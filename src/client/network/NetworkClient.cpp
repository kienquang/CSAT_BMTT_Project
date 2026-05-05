#include "NetworkClient.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <cstring>

#include "../../core/EnvConfig.h"
#include "../../shared/NetworkData.h"

#pragma comment(lib, "ws2_32.lib")

using namespace std;

namespace {

// [GROUP: Packet Field Helpers]
template <size_t N>
void CopyToBuffer(char (&dest)[N], const string& value) {
    memset(dest, 0, N);
    if (!value.empty()) {
        strncpy_s(dest, N, value.c_str(), _TRUNCATE);
    }
}

// [GROUP: Packet Initialization And Mapping]
void InitializePacket(PacketData& packet) {
    memset(&packet, 0, sizeof(PacketData));
    packet.protocolVersion = PROTOCOL_VERSION;
}

// [GROUP: Profile Serialization Helpers]
PersonalRecord PacketToRecord(const PacketData& packet) {
    PersonalRecord record;
    record.userId = packet.userId;
    record.recordId = packet.recordId;
    record.username = packet.username;
    record.name = packet.name;
    record.role = packet.role;
    record.gender = packet.gender;
    record.cccd = packet.cccd;
    record.phone = packet.phone;
    record.email = packet.email;
    record.encryptedDek = packet.encryptedDek;
    return record;
}

void RecordToPacket(const PersonalRecord& record, const string& passwordPlaintext, PacketData& packet) {
    InitializePacket(packet);
    packet.userId = record.userId;
    packet.recordId = record.recordId;
    packet.role = record.role;
    packet.gender = record.gender;
    CopyToBuffer(packet.username, record.username);
    CopyToBuffer(packet.name, record.name);
    CopyToBuffer(packet.password, passwordPlaintext);
    CopyToBuffer(packet.cccd, record.cccd);
    CopyToBuffer(packet.phone, record.phone);
    CopyToBuffer(packet.email, record.email);
    CopyToBuffer(packet.encryptedDek, record.encryptedDek);
}

<<<<<<< HEAD
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

    const string Key = EnvConfig::GetString("APP_LOGIN_BLOWFISH_KEY");
    if (Key.empty()) {
        error = "Missing APP_LOGIN_BLOWFISH_KEY in client .env";
        return false;
    }

    const string encrypted = ReadLoginCiphertext(packet);
    Blowfish cipher(Key);
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
=======
}  // namespace
>>>>>>> 20c0ea2f1ec5799d4fd4d5f44e7c257922ea3bf9

NetworkClient::NetworkClient()
    : connected_(false),
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
    cout << "wsaData.wVersion:" << wsaData.wVersion << endl;
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

    const bool insecureSkipVerify = EnvConfig::GetInt("APP_TLS_INSECURE_SKIP_VERIFY", 1) != 0;
    if (!tlsSocket_.InitializeClient(socketHandle, host, insecureSkipVerify, error)) {
        closesocket(socketHandle);
        WSACleanup();
        return false;
    }

    connected_ = true;
    socketValue_ = static_cast<unsigned long long>(socketHandle);
    return true;
}

void NetworkClient::Disconnect() {
    if (!connected_) {
        return;
    }

    SOCKET socketHandle = static_cast<SOCKET>(socketValue_);
    tlsSocket_.Shutdown();
    closesocket(socketHandle);
    connected_ = false;
    socketValue_ = static_cast<unsigned long long>(INVALID_SOCKET);
    currentUsername_.clear();
    currentUserId_ = -1;
    WSACleanup();
}

bool NetworkClient::IsConnected() const {
    return connected_;
}

bool NetworkClient::SendAll(const char* data, int totalBytes) {
    return tlsSocket_.SendAll(data, totalBytes);
}

bool NetworkClient::RecvAll(char* data, int totalBytes) {
    return tlsSocket_.RecvAll(data, totalBytes);
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

bool NetworkClient::Login(const string& username, const string& password, LoginResult& result, string& error) {
    PacketData request;
    PacketData response;
    InitializePacket(request);
    request.requestType = REQ_LOGIN;
    request.dataType = DATATYPE_PLAINTEXT;
    CopyToBuffer(request.username, username);
    CopyToBuffer(request.password, password);

    if (!SendRequest(request, response, error)) {
        return false;
    }

    result.success = response.status == STATUS_SUCCESS;
    result.userId = response.userId;
    result.role = response.role;
    result.username = response.username;
    result.message = response.message;

    if (result.success) {
        currentUserId_ = result.userId;
        currentUsername_ = result.username;
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

    currentUsername_.clear();
    currentUserId_ = -1;
    return response.status == STATUS_SUCCESS;
}

<<<<<<< HEAD
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
=======
bool NetworkClient::FetchProfile(PersonalRecord& record, string& error) {
>>>>>>> 20c0ea2f1ec5799d4fd4d5f44e7c257922ea3bf9
    PacketData request;
    PacketData response;
    InitializePacket(request);
    request.requestType = REQ_FETCH_PROFILE;

    if (!SendRequest(request, response, error)) {
        return false;
    }

    if (response.status != STATUS_SUCCESS) {
        error = response.message;
        return false;
    }

    record = PacketToRecord(response);
    return true;
}

bool NetworkClient::Register(const PersonalRecord& record, const string& passwordPlaintext, string& error) {
    PacketData request;
    PacketData response;
    RecordToPacket(record, passwordPlaintext, request);
    request.requestType = REQ_REGISTER;

    if (!SendRequest(request, response, error)) {
        return false;
    }

    if (response.status != STATUS_SUCCESS) {
        error = response.message;
        return false;
    }

    return true;
}

bool NetworkClient::UpdateProfile(const PersonalRecord& record,
                                  const string& passwordPlaintext,
                                  string& error,
                                  const string& currentPasswordPlaintext) {
    PacketData request;
    PacketData response;
    RecordToPacket(record, passwordPlaintext, request);
    CopyToBuffer(request.message, currentPasswordPlaintext);
    request.requestType = REQ_UPDATE_PROFILE;

    if (!SendRequest(request, response, error)) {
        return false;
    }

    if (response.status != STATUS_SUCCESS) {
        error = response.message;
        return false;
    }

    if (response.userId >= 0) {
        currentUserId_ = response.userId;
    }
    if (response.username[0] != '\0') {
        currentUsername_ = response.username;
    }

    return true;
}

bool NetworkClient::DeleteAccount(string& error) {
    PacketData request;
    PacketData response;
    InitializePacket(request);
    request.requestType = REQ_DELETE_ACCOUNT;

    if (!SendRequest(request, response, error)) {
        return false;
    }

    if (response.status != STATUS_SUCCESS) {
        error = response.message;
        return false;
    }

    currentUsername_.clear();
    currentUserId_ = -1;
    return true;
}

bool NetworkClient::GetTotalUsers(int& total, string& error) {
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

bool NetworkClient::FetchEncryptedUserList(vector<PersonalRecord>& records, string& error) {
    records.clear();

    int totalHint = -1;
    for (int offset = 0;; ++offset) {
        PacketData request;
        PacketData response;
        InitializePacket(request);
        request.requestType = REQ_ADMIN_LIST_USERS;
        request.recordId = offset;

        if (!SendRequest(request, response, error)) {
            return false;
        }

        if (response.status == STATUS_NOTFOUND) {
            break;
        }

        if (response.status != STATUS_SUCCESS) {
            error = response.message;
            return false;
        }

        records.push_back(PacketToRecord(response));

        if (response.recordCount >= 0) {
            totalHint = response.recordCount;
        }

        if (totalHint >= 0 && static_cast<int>(records.size()) >= totalHint) {
            break;
        }
    }

    return true;
}

bool NetworkClient::FetchMedicalRecordList(vector<MedicalRecordListItem>& records, string& error) {
    records.clear();

    int totalHint = -1;
    for (int offset = 0;; ++offset) {
        PacketData request;
        PacketData response;
        InitializePacket(request);
        request.requestType = REQ_ADMIN_LIST_MEDICAL_RECORDS;
        request.recordId = offset;

        if (!SendRequest(request, response, error)) {
            return false;
        }

        if (response.status == STATUS_NOTFOUND) {
            break;
        }

        if (response.status != STATUS_SUCCESS) {
            error = response.message;
            return false;
        }

        MedicalRecordListItem item;
        item.recordId = response.recordId;
        item.patientId = response.userId;
        item.patientName = response.name;
        item.doctorId = response.role;
        item.doctorName = response.encryptedDek;
        item.visitDate = response.username;
        item.department = response.cccd;
        item.diagnosis = response.phone;
        item.prescription = response.email;
        records.push_back(item);

        if (response.recordCount >= 0) {
            totalHint = response.recordCount;
        }

        if (totalHint >= 0 && static_cast<int>(records.size()) >= totalHint) {
            break;
        }
    }

    return true;
}

bool NetworkClient::FetchMyMedicalRecordList(vector<MedicalRecordListItem>& records, string& error) {
    records.clear();

    int totalHint = -1;
    for (int offset = 0;; ++offset) {
        PacketData request;
        PacketData response;
        InitializePacket(request);
        request.requestType = REQ_DOCTOR_LIST_MY_MEDICAL_RECORDS;
        request.recordId = offset;

        if (!SendRequest(request, response, error)) {
            return false;
        }

        if (response.status == STATUS_NOTFOUND) {
            break;
        }

        if (response.status != STATUS_SUCCESS) {
            error = response.message;
            return false;
        }

        MedicalRecordListItem item;
        item.recordId = response.recordId;
        item.patientId = response.userId;
        item.patientName = response.name;
        item.doctorId = response.role;
        item.doctorName = response.encryptedDek;
        item.visitDate = response.username;
        item.department = response.cccd;
        item.diagnosis = response.phone;
        item.prescription = response.email;
        records.push_back(item);

        if (response.recordCount >= 0) {
            totalHint = response.recordCount;
        }

        if (totalHint >= 0 && static_cast<int>(records.size()) >= totalHint) {
            break;
        }
    }

    return true;
}

bool NetworkClient::CreateMedicalRecord(int patientId,
                                        const string& visitDate,
                                        const string& department,
                                        const string& diagnosis,
                                        const string& prescription,
                                        string& error) {
    if (patientId <= 0) {
        error = "Patient id must be positive";
        return false;
    }

    if (visitDate.empty() || diagnosis.empty() || prescription.empty()) {
        error = "Visit date, diagnosis and prescription are required";
        return false;
    }

    PacketData request;
    PacketData response;
    InitializePacket(request);
    request.requestType = REQ_DOCTOR_CREATE_MEDICAL_RECORD;
    request.userId = patientId;
    CopyToBuffer(request.username, visitDate);
    CopyToBuffer(request.cccd, department);
    CopyToBuffer(request.phone, diagnosis);
    CopyToBuffer(request.email, prescription);

    if (!SendRequest(request, response, error)) {
        return false;
    }

    if (response.status != STATUS_SUCCESS) {
        error = response.message;
        return false;
    }

    return true;
}

bool NetworkClient::FetchMedicalRecordDetailForDoctor(int medicalRecordId,
                                                      const string& passwordPlaintext,
                                                      MedicalRecordListItem& record,
                                                      string& error) {
    if (medicalRecordId <= 0) {
        error = "Medical record id must be positive";
        return false;
    }

    if (passwordPlaintext.empty()) {
        error = "Password must not be empty";
        return false;
    }

    PacketData request;
    PacketData response;
    InitializePacket(request);
    request.requestType = REQ_DOCTOR_VIEW_MEDICAL_RECORD_DETAIL;
    request.recordId = medicalRecordId;
    CopyToBuffer(request.password, passwordPlaintext);

    if (!SendRequest(request, response, error)) {
        return false;
    }

    if (response.status != STATUS_SUCCESS) {
        error = response.message;
        return false;
    }

    record.recordId = response.recordId;
    record.patientId = response.userId;
    record.patientName = response.name;
    record.doctorId = response.role;
    record.doctorName = response.encryptedDek;
    record.visitDate = response.username;
    record.department = response.cccd;
    record.diagnosis = response.phone;
    record.prescription = response.email;
    return true;
}

bool NetworkClient::FetchMyInfoForAdmin(const string& passwordPlaintext, PersonalRecord& record, string& error) {
    if (passwordPlaintext.empty()) {
        error = "Password must not be empty";
        return false;
    }

    PacketData request;
    PacketData response;
    InitializePacket(request);
    request.requestType = REQ_ADMIN_VIEW_MY_INFO;
    CopyToBuffer(request.password, passwordPlaintext);

    if (!SendRequest(request, response, error)) {
        return false;
    }

    if (response.status != STATUS_SUCCESS) {
        error = response.message;
        return false;
    }

    record = PacketToRecord(response);
    return true;
}

bool NetworkClient::AdminDeleteUser(int userId, string& error) {
    if (userId <= 0) {
        error = "Invalid user id";
        return false;
    }

    PacketData request;
    PacketData response;
    InitializePacket(request);
    request.requestType = REQ_ADMIN_DELETE_USER;
    request.userId = userId;

    if (!SendRequest(request, response, error)) {
        return false;
    }

    if (response.status != STATUS_SUCCESS) {
        error = response.message;
        return false;
    }

    return true;
}

bool NetworkClient::AdminUpdateUserRole(int userId, int newRole, string& error) {
    if (userId <= 0) {
        error = "Invalid user id";
        return false;
    }

    PacketData request;
    PacketData response;
    InitializePacket(request);
    request.requestType = REQ_ADMIN_UPDATE_USER_ROLE;
    request.userId = userId;
    request.role = newRole;

    if (!SendRequest(request, response, error)) {
        return false;
    }

    if (response.status != STATUS_SUCCESS) {
        error = response.message;
        return false;
    }

    return true;
}
