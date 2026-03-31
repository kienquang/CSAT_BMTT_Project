#include "NetworkClient.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <cstring>

#include "../../core/DatabaseHelper.h"
#include "../../core/Blowfish.h"
#include "../../core/EnvConfig.h"
#include "../../shared/NetworkData.h"

#pragma comment(lib, "ws2_32.lib")

using namespace std;

namespace {

template <size_t N>
void CopyToBuffer(char (&dest)[N], const string& value) {
    memset(dest, 0, N);
    if (!value.empty()) {
        strncpy_s(dest, N, value.c_str(), _TRUNCATE);
    }
}

void InitializePacket(PacketData& packet) {
    memset(&packet, 0, sizeof(PacketData));
    packet.protocolVersion = PROTOCOL_VERSION;
}

nhanvien PacketToEmployee(const PacketData& packet) {
    nhanvien employee;
    employee.id = packet.employeeId;
    employee.ten_nv = packet.employeeName;
    employee.vai_tro = packet.employeeRole;
    employee.cccd_cipher = packet.cccd;
    employee.sdt_cipher = packet.phone;
    employee.matkhau_cipher = packet.passwordMasked;
    employee.luong_cipher = packet.salary;
    return employee;
}

void EmployeeToPacket(const nhanvien& employee, const string& passwordPlaintext, PacketData& packet) {
    InitializePacket(packet);
    packet.employeeId = employee.id;
    CopyToBuffer(packet.employeeName, employee.ten_nv);
    CopyToBuffer(packet.employeeRole, employee.vai_tro);
    CopyToBuffer(packet.cccd, employee.cccd_cipher);
    CopyToBuffer(packet.phone, employee.sdt_cipher);
    CopyToBuffer(packet.salary, employee.luong_cipher);
    CopyToBuffer(packet.password, passwordPlaintext);
}

bool EncryptLoginPayload(const string& cccd, const string& password, PacketData& request, string& error) {
    const string loginKey = EnvConfig::GetString("APP_LOGIN_BLOWFISH_KEY");
    if (loginKey.empty()) {
        error = "Missing APP_LOGIN_BLOWFISH_KEY in client .env";
        return false;
    }

    Blowfish cipher(loginKey);
    const string encryptedPayload = cipher.EncryptString(cccd + password);
    if (!WriteLoginCiphertext(request, encryptedPayload)) {
        error = "Encrypted login payload exceeds packet capacity";
        return false;
    }

    request.dataType = DATATYPE_LOGIN_BLOWFISH;
    return true;
}

} // namespace

// ======== ADMIN PAYLOAD ENCRYPT/DECRYPT HELPERS ========

namespace {

string SerializeEmployeePlaintext(const nhanvien& employee, const string& passwordPlaintext) {
    // Fields are expected to be plaintext here
    return employee.ten_nv + "|" + employee.vai_tro + "|" + employee.cccd_cipher + "|" +
           employee.sdt_cipher + "|" + passwordPlaintext + "|" + employee.luong_cipher;
}

bool EncryptAdminEmployeePayload(const nhanvien& employee, const string& passwordPlaintext,
                                 PacketData& request, string& error) {
    const string Key = EnvConfig::GetString("APP_LOGIN_BLOWFISH_KEY");
    if (Key.empty()) {
        error = "Missing APP_LOGIN_BLOWFISH_KEY in client .env";
        return false;
    }

    const string plain = SerializeEmployeePlaintext(employee, passwordPlaintext);
    Blowfish cipher(Key);
    const string encrypted = cipher.EncryptString(plain);

    if (!WriteLoginCiphertext(request, encrypted)) {
        error = "Encrypted admin payload exceeds packet capacity";
        return false;
    }

    request.dataType = DATATYPE_ADMIN_BLOWFISH;
    return true;
}

bool DecryptAdminEmployeePayload(const PacketData& packet, nhanvien& employee, string& passwordPlaintext, string& error) {
    if (packet.dataType != DATATYPE_ADMIN_BLOWFISH) {
        error = "Unexpected data type for admin payload";
        return false;
    }

    const string adminKey = EnvConfig::GetString("APP_LOGIN_BLOWFISH_KEY");
    if (adminKey.empty()) {
        error = "Missing APP_LOGIN_BLOWFISH_KEY in client .env";
        return false;
    }

    const string encrypted = ReadLoginCiphertext(packet);
    Blowfish cipher(adminKey);
    const string plain = cipher.DecryptString(encrypted);

    // Parse delimited payload: name|role|cccd|phone|password|salary
    size_t pos = 0, prev = 0;
    string parts[6];
    int idx = 0;
    while ((pos = plain.find('|', prev)) != string::npos && idx < 5) {
        parts[idx++] = plain.substr(prev, pos - prev);
        prev = pos + 1;
    }
    parts[idx++] = plain.substr(prev);

    if (idx != 6) {
        error = "Malformed admin payload";
        return false;
    }

    employee.ten_nv = parts[0];
    employee.vai_tro = parts[1];
    employee.cccd_cipher = parts[2];
    employee.sdt_cipher = parts[3];
    passwordPlaintext = parts[4];
    employee.luong_cipher = parts[5];
    return true;
}

} // namespace

NetworkClient::NetworkClient()
    : port_(0),
      connected_(false),
      currentUserId_(-1),
      socketValue_(static_cast<unsigned long long>(INVALID_SOCKET)) {
}

NetworkClient::~NetworkClient() {
    Disconnect();
}

bool NetworkClient::Connect(const string& host, int port, string& error) {
    if (connected_) {
        return true;
    }

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        error = "WSAStartup failed";
        return false;
    }

    SOCKET socketHandle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socketHandle == INVALID_SOCKET) {
        error = "Failed to create socket";
        WSACleanup();
        return false;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(static_cast<u_short>(port));
    if (inet_pton(AF_INET, host.c_str(), &serverAddr.sin_addr) != 1) {
        error = "Invalid server address";
        closesocket(socketHandle);
        WSACleanup();
        return false;
    }

    if (connect(socketHandle, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
        error = "Failed to connect to server";
        closesocket(socketHandle);
        WSACleanup();
        return false;
    }

    host_ = host;
    port_ = port;
    connected_ = true;
    socketValue_ = static_cast<unsigned long long>(socketHandle);
    return true;
}

void NetworkClient::Disconnect() {
    if (!connected_) {
        return;
    }

    SOCKET socketHandle = static_cast<SOCKET>(socketValue_);
    closesocket(socketHandle);
    connected_ = false;
    socketValue_ = static_cast<unsigned long long>(INVALID_SOCKET);
    currentRole_.clear();
    currentUserName_.clear();
    currentUserId_ = -1;
    WSACleanup();
}

bool NetworkClient::IsConnected() const {
    return connected_;
}

bool NetworkClient::SendAll(const char* data, int totalBytes) {
    SOCKET socketHandle = static_cast<SOCKET>(socketValue_);
    int sentBytes = 0;
    while (sentBytes < totalBytes) {
        const int sent = send(socketHandle, data + sentBytes, totalBytes - sentBytes, 0);
        if (sent == SOCKET_ERROR) {
            return false;
        }
        sentBytes += sent;
    }
    return true;
}

bool NetworkClient::RecvAll(char* data, int totalBytes) {
    SOCKET socketHandle = static_cast<SOCKET>(socketValue_);
    int receivedBytes = 0;
    while (receivedBytes < totalBytes) {
        const int received = recv(socketHandle, data + receivedBytes, totalBytes - receivedBytes, 0);
        if (received <= 0) {
            return false;
        }
        receivedBytes += received;
    }
    return true;
}

bool NetworkClient::SendRequest(const PacketData& request, PacketData& response, string& error) {
    if (!connected_) {
        error = "Client is not connected";
        return false;
    }

    if (!SendAll(reinterpret_cast<const char*>(&request), sizeof(PacketData))) {
        error = "Failed to send request";
        return false;
    }

    if (!RecvAll(reinterpret_cast<char*>(&response), sizeof(PacketData))) {
        error = "Failed to receive response";
        return false;
    }

    return true;
}

bool NetworkClient::Login(const string& cccd, const string& password, LoginResult& result, string& error) {
    PacketData request;
    PacketData response;
    InitializePacket(request);
    request.requestType = REQ_LOGIN;
    if (!EncryptLoginPayload(cccd, password, request, error)) {
        return false;
    }

    if (!SendRequest(request, response, error)) {
        return false;
    }

    result.success = response.status == STATUS_SUCCESS;
    result.userRole = response.employeeRole;
    result.userId = response.employeeId;
    result.userName = response.employeeName;
    result.message = response.message;

    if (result.success) {
        currentRole_ = result.userRole;
        currentUserId_ = result.userId;
        currentUserName_ = result.userName;
    }

    return true;
}

bool NetworkClient::Logout(string& error) {
    if (!connected_) {
        return true;
    }

    PacketData request;
    PacketData response;
    InitializePacket(request);
    request.requestType = REQ_LOGOUT;

    if (!SendRequest(request, response, error)) {
        return false;
    }

    currentRole_.clear();
    currentUserName_.clear();
    currentUserId_ = -1;
    return response.status == STATUS_SUCCESS;
}

bool NetworkClient::FetchAllEmployees(vector<nhanvien>& employees, string& error) {
    employees.clear();

    PacketData request;
    PacketData header;
    InitializePacket(request);
    request.requestType = REQ_LIST_EMPLOYEES;

    if (!SendRequest(request, header, error)) {
        return false;
    }

    if (header.status != STATUS_SUCCESS) {
        error = header.message;
        return false;
    }

    const int count = header.recordCount;
    for (int i = 0; i < count; ++i) {
        PacketData employeePacket;
        if (!RecvAll(reinterpret_cast<char*>(&employeePacket), sizeof(PacketData))) {
            error = "Failed to receive employee list item";
            employees.clear();
            return false;
        }

        if (header.dataType == DATATYPE_ADMIN_BLOWFISH) {
            nhanvien emp;
            string passwordPlain;
            if (!DecryptAdminEmployeePayload(employeePacket, emp, passwordPlain, error)) {
                employees.clear();
                return false;
            }
            emp.id = employeePacket.employeeId;
            // Hiển thị password đã giải mã cho Admin (UI đọc từ matkhau_cipher)
            emp.matkhau_cipher = passwordPlain;
            employees.push_back(emp);
        } else {
            employees.push_back(PacketToEmployee(employeePacket));
        }
    }

    return true;
}

bool NetworkClient::AddEmployee(const nhanvien& employee, const string& passwordPlaintext, string& error) {
    PacketData request;
    PacketData response;
    const bool useAdminEncryption = currentRole_ == "Admin";

    if (useAdminEncryption) {
        InitializePacket(request);
        request.requestType = REQ_ADD_EMPLOYEE;
        if (!EncryptAdminEmployeePayload(employee, passwordPlaintext, request, error)) {
            return false;
        }
    } else {
        EmployeeToPacket(employee, passwordPlaintext, request);
        request.requestType = REQ_ADD_EMPLOYEE;
    }

    if (!SendRequest(request, response, error)) {
        return false;
    }

    if (response.status != STATUS_SUCCESS) {
        error = response.message;
        return false;
    }

    return true;
}

bool NetworkClient::UpdateEmployee(const nhanvien& employee, const string& passwordPlaintext, string& error) {
    PacketData request;
    PacketData response;
    const bool useAdminEncryption = currentRole_ == "Admin";

    if (useAdminEncryption) {
        InitializePacket(request);
        request.requestType = REQ_UPDATE_EMPLOYEE;
        request.employeeId = employee.id;
        if (!EncryptAdminEmployeePayload(employee, passwordPlaintext, request, error)) {
            return false;
        }
    } else {
        EmployeeToPacket(employee, passwordPlaintext, request);
        request.requestType = REQ_UPDATE_EMPLOYEE;
    }

    if (!SendRequest(request, response, error)) {
        return false;
    }

    if (response.status != STATUS_SUCCESS) {
        error = response.message;
        return false;
    }

    return true;
}

bool NetworkClient::DeleteEmployee(int employeeId, string& error) {
    PacketData request;
    PacketData response;
    InitializePacket(request);
    request.requestType = REQ_DELETE_EMPLOYEE;
    request.employeeId = employeeId;

    if (!SendRequest(request, response, error)) {
        return false;
    }

    if (response.status != STATUS_SUCCESS) {
        error = response.message;
        return false;
    }

    return true;
}

bool NetworkClient::GetTotalEmployees(int& total, string& error) {
    PacketData request;
    PacketData response;
    InitializePacket(request);
    request.requestType = REQ_GET_TOTAL;

    if (!SendRequest(request, response, error)) {
        return false;
    }

    if (response.status != STATUS_SUCCESS) {
        error = response.message;
        return false;
    }

    total = response.recordCount;
    return true;
}
