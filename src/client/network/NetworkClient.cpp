#include "NetworkClient.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <cstring>

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

bool EncryptLoginPayload(const string& username, const string& password, PacketData& request, string& error) {
    const string loginKey = EnvConfig::GetString("APP_LOGIN_BLOWFISH_KEY");
    if (loginKey.empty()) {
        error = "Missing APP_LOGIN_BLOWFISH_KEY in client .env";
        return false;
    }

    Blowfish cipher(loginKey);
    const string encryptedPayload = cipher.EncryptString(username + "|" + password);
    if (!WriteLoginCiphertext(request, encryptedPayload)) {
        error = "Encrypted login payload exceeds packet capacity";
        return false;
    }

    request.dataType = DATATYPE_LOGIN_BLOWFISH;
    return true;
}

}  // namespace

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
    currentUsername_.clear();
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

bool NetworkClient::Login(const string& username, const string& password, LoginResult& result, string& error) {
    PacketData request;
    PacketData response;
    InitializePacket(request);
    request.requestType = REQ_LOGIN;
    if (!EncryptLoginPayload(username, password, request, error)) {
        return false;
    }

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

bool NetworkClient::FetchProfile(PersonalRecord& record, string& error) {
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

bool NetworkClient::UpdateProfile(const PersonalRecord& record, const string& passwordPlaintext, string& error) {
    PacketData request;
    PacketData response;
    RecordToPacket(record, passwordPlaintext, request);
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
