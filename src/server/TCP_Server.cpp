#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>

#include <cstring>
#include <string>

#include "../core/Blowfish.h"
#include "../core/DatabaseHelper.h"
#include "../core/EnvConfig.h"
#include "../core/masking.h"
#include "../shared/NetworkData.h"

#pragma comment(lib, "ws2_32.lib")

using namespace std;

namespace {

constexpr int kBacklog = 5;

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
    packet.status = STATUS_ERROR;
}

bool SendAll(SOCKET socket, const char* data, int totalBytes) {
    int sentBytes = 0;
    while (sentBytes < totalBytes) {
        const int sent = send(socket, data + sentBytes, totalBytes - sentBytes, 0);
        if (sent == SOCKET_ERROR) {
            return false;
        }
        sentBytes += sent;
    }
    return true;
}

bool RecvAll(SOCKET socket, char* data, int totalBytes) {
    int receivedBytes = 0;
    while (receivedBytes < totalBytes) {
        const int received = recv(socket, data + receivedBytes, totalBytes - receivedBytes, 0);
        if (received <= 0) {
            return false;
        }
        receivedBytes += received;
    }
    return true;
}

bool DecryptLoginPayload(const PacketData& request, string& username, string& password, string& error) {
    if (request.dataType != DATATYPE_LOGIN_BLOWFISH) {
        error = "Login request must use Blowfish-encrypted payload.";
        return false;
    }

    const string loginKey = EnvConfig::GetString("APP_LOGIN_BLOWFISH_KEY");
    if (loginKey.empty()) {
        error = "Server is missing APP_LOGIN_BLOWFISH_KEY";
        return false;
    }

    const string encryptedPayload = ReadLoginCiphertext(request);
    if (encryptedPayload.empty()) {
        error = "Encrypted login payload is empty";
        return false;
    }

    Blowfish cipher(loginKey);
    const string plainPayload = cipher.DecryptString(encryptedPayload);
    const size_t separator = plainPayload.find('|');
    if (separator == string::npos) {
        error = "Encrypted login payload is invalid";
        return false;
    }

    username = plainPayload.substr(0, separator);
    password = plainPayload.substr(separator + 1);
    return !username.empty();
}

PersonalRecord PacketToRecord(const PacketData& request) {
    PersonalRecord record;
    record.userId = request.userId;
    record.recordId = request.recordId;
    record.username = request.username;
    record.role = request.role;
    record.gender = request.gender;
    record.cccd = request.cccd;
    record.phone = request.phone;
    record.email = request.email;
    record.encryptedDek = request.encryptedDek;
    return record;
}

void RecordToPacket(const PersonalRecord& record, PacketData& response) {
    response.userId = record.userId;
    response.recordId = record.recordId;
    response.role = record.role;
    response.gender = record.gender;
    CopyToBuffer(response.username, record.username);
    CopyToBuffer(response.cccd, record.cccd);
    CopyToBuffer(response.phone, record.phone);
    CopyToBuffer(response.email, record.email);
    CopyToBuffer(response.encryptedDek, record.encryptedDek);
}

PacketData BuildProtocolErrorResponse(const PacketData& request, const string& message) {
    PacketData response;
    InitializePacket(response);
    response.requestType = request.requestType;
    response.status = STATUS_ERROR;
    CopyToBuffer(response.message, message);
    return response;
}

PacketData BuildUnauthorizedResponse(int requestType, const string& message) {
    PacketData response;
    InitializePacket(response);
    response.requestType = requestType;
    response.status = STATUS_UNAUTHORIZED;
    CopyToBuffer(response.message, message);
    return response;
}

PacketData HandleLogin(DatabaseHelper& dbHelper, const PacketData& request,
                       bool& isAuthenticated, int& sessionUserId,
                       int& sessionRole, string& sessionUsername, string& sessionKek) {
    PacketData response;
    InitializePacket(response);
    response.requestType = request.requestType;

    string username;
    string password;
    string decryptError;
    if (!DecryptLoginPayload(request, username, password, decryptError)) {
        response.status = STATUS_ERROR;
        CopyToBuffer(response.message, decryptError);
        return response;
    }

    string derivedSessionKek;
    AuthenticatedUser user = dbHelper.AuthenticateUser(username, password, derivedSessionKek);
    if (!user.IsValid()) {
        response.status = STATUS_UNAUTHORIZED;
        CopyToBuffer(response.message, "Login failed. Username or password is incorrect.");
        return response;
    }

    isAuthenticated = true;
    sessionUserId = user.id;
    sessionRole = user.role;
    sessionUsername = user.username;
    sessionKek = derivedSessionKek;

    response.status = STATUS_SUCCESS;
    response.userId = user.id;
    response.role = user.role;
    CopyToBuffer(response.username, user.username);
    CopyToBuffer(response.message, "Login successful.");
    return response;
}

PacketData HandleFetchProfile(DatabaseHelper& dbHelper, bool isAuthenticated,
                              int sessionUserId, const string& sessionKek) {
    PacketData response;
    InitializePacket(response);
    response.requestType = REQ_FETCH_PROFILE;

    if (!isAuthenticated) {
        response = BuildUnauthorizedResponse(REQ_FETCH_PROFILE, "Please login first.");
        return response;
    }

    PersonalRecord record;
    if (!dbHelper.GetPersonalRecordForUser(sessionUserId, sessionKek, record)) {
        response.status = STATUS_NOTFOUND;
        CopyToBuffer(response.message, "Profile record was not found.");
        return response;
    }

    response.status = STATUS_SUCCESS;
    response.dataType = DATATYPE_PLAINTEXT;
    RecordToPacket(record, response);
    CopyToBuffer(response.message, "Profile loaded successfully.");
    return response;
}

PacketData HandleRegister(DatabaseHelper& dbHelper, const PacketData& request, bool isAuthenticated) {
    PacketData response;
    InitializePacket(response);
    response.requestType = request.requestType;

    if (isAuthenticated) {
        response.status = STATUS_ERROR;
        CopyToBuffer(response.message, "Logout before registering a new account.");
        return response;
    }

    const PersonalRecord record = PacketToRecord(request);
    const bool created = dbHelper.RegisterUser(
        record.username,
        request.password,
        record.gender,
        record.cccd,
        record.phone,
        record.email);

    response.status = created ? STATUS_SUCCESS : STATUS_ERROR;
    CopyToBuffer(response.message, created ? "Registration successful." : "Registration failed.");
    return response;
}

PacketData HandleUpdateProfile(DatabaseHelper& dbHelper, const PacketData& request,
                               bool isAuthenticated, int sessionUserId,
                               string& sessionUsername, string& sessionKek) {
    PacketData response;
    InitializePacket(response);
    response.requestType = request.requestType;

    if (!isAuthenticated) {
        return BuildUnauthorizedResponse(request.requestType, "Please login first.");
    }

    const PersonalRecord record = PacketToRecord(request);
    string updatedSessionKek;
    const bool updated = dbHelper.UpdatePersonalRecordForUser(
        sessionUserId,
        sessionKek,
        record.username,
        request.password,
        request.password[0] != '\0',
        record.gender,
        record.cccd,
        record.phone,
        record.email,
        updatedSessionKek);

    response.status = updated ? STATUS_SUCCESS : STATUS_ERROR;
    if (updated) {
        sessionUsername = record.username;
        if (!updatedSessionKek.empty()) {
            sessionKek = updatedSessionKek;
        }
        response.userId = sessionUserId;
        CopyToBuffer(response.username, sessionUsername);
        CopyToBuffer(response.message, "Profile updated successfully.");
    } else {
        CopyToBuffer(response.message, "Profile update failed.");
    }

    return response;
}

PacketData HandleDeleteAccount(DatabaseHelper& dbHelper, bool isAuthenticated,
                               int& sessionUserId, string& sessionUsername,
                               string& sessionKek, bool& shouldCloseConnection) {
    PacketData response;
    InitializePacket(response);
    response.requestType = REQ_DELETE_ACCOUNT;

    if (!isAuthenticated) {
        return BuildUnauthorizedResponse(REQ_DELETE_ACCOUNT, "Please login first.");
    }

    const bool deleted = dbHelper.DeleteUserById(sessionUserId);
    response.status = deleted ? STATUS_SUCCESS : STATUS_ERROR;
    CopyToBuffer(response.message, deleted ? "Account deleted successfully." : "Account deletion failed.");

    if (deleted) {
        sessionUserId = -1;
        sessionUsername.clear();
        sessionKek.clear();
        shouldCloseConnection = true;
    }

    return response;
}

PacketData HandleGetTotal(DatabaseHelper& dbHelper) {
    PacketData response;
    InitializePacket(response);
    response.requestType = REQ_GET_TOTAL;
    response.status = STATUS_SUCCESS;
    response.recordCount = dbHelper.GetTotalUsers();
    CopyToBuffer(response.message, "Loaded total users.");
    return response;
}

PacketData HandleAdminListUsers(DatabaseHelper& dbHelper,
                                bool isAuthenticated,
                                int sessionRole,
                                int sessionUserId,
                                const string& sessionKek,
                                const PacketData& request) {
    PacketData response;
    InitializePacket(response);
    response.requestType = REQ_ADMIN_LIST_USERS;

    if (!isAuthenticated) {
        return BuildUnauthorizedResponse(REQ_ADMIN_LIST_USERS, "Please login first.");
    }

    if (sessionRole != 2) {
        return BuildUnauthorizedResponse(REQ_ADMIN_LIST_USERS, "Admin role is required.");
    }

    const int offset = request.recordId < 0 ? 0 : request.recordId;
    response.recordCount = dbHelper.GetTotalUsers();

    PersonalRecord record;
    if (!dbHelper.GetEncryptedUserRecordByOffset(offset, record)) {
        response.status = STATUS_NOTFOUND;
        CopyToBuffer(response.message, "No more users.");
        return response;
    }

    (void)sessionUserId;
    (void)sessionKek;

    record.cccd = masking::masking_cccd(record.cccd);
    record.phone = masking::masking_phone(record.phone);
    record.email = masking::masking_email(record.email);

    response.status = STATUS_SUCCESS;
    response.dataType = DATATYPE_PLAINTEXT;
    RecordToPacket(record, response);
    CopyToBuffer(response.message, "Sensitive fields were masked.");
    return response;
}

PacketData HandleLogout(bool& isAuthenticated, int& sessionUserId,
                        int& sessionRole, string& sessionUsername, string& sessionKek) {
    PacketData response;
    InitializePacket(response);
    response.requestType = REQ_LOGOUT;
    response.status = STATUS_SUCCESS;
    CopyToBuffer(response.message, "Logged out.");

    isAuthenticated = false;
    sessionUserId = -1;
    sessionRole = 0;
    sessionUsername.clear();
    sessionKek.clear();
    return response;
}

}  // namespace

int main() {
    const string dbHost = EnvConfig::GetString("APP_DB_HOST", "127.0.0.1");
    const int dbPort = EnvConfig::GetInt("APP_DB_PORT", 3306);
    const string dbUser = EnvConfig::GetString("APP_DB_USER", "root");
    const string dbPassword = EnvConfig::GetString("APP_DB_PASSWORD", "");
    const string dbName = EnvConfig::GetString("APP_DB_NAME", "csatbmtt");
    const int serverPort = EnvConfig::GetInt("APP_SERVER_PORT", 8080);

    cout << "[SERVER] TCP Server starting on port " << serverPort << "..." << endl;

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "[SERVER] WSAStartup failed" << endl;
        return 1;
    }

    DatabaseHelper dbHelper(dbHost, dbPort, dbUser, dbPassword, dbName);
    if (!dbHelper.Connect()) {
        cerr << "[SERVER] Database connection failed" << endl;
        WSACleanup();
        return 1;
    }

    if (!dbHelper.InitializeSchema()) {
        cerr << "[SERVER] Database schema initialization failed" << endl;
        dbHelper.Disconnect();
        WSACleanup();
        return 1;
    }

    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET) {
        cerr << "[SERVER] Failed to create listening socket" << endl;
        dbHelper.Disconnect();
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(static_cast<u_short>(serverPort));
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(listenSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
        cerr << "[SERVER] Bind failed on port " << serverPort << endl;
        closesocket(listenSocket);
        dbHelper.Disconnect();
        WSACleanup();
        return 1;
    }

    if (listen(listenSocket, kBacklog) == SOCKET_ERROR) {
        cerr << "[SERVER] Listen failed" << endl;
        closesocket(listenSocket);
        dbHelper.Disconnect();
        WSACleanup();
        return 1;
    }

    cout << "[SERVER] Listening on 0.0.0.0:" << serverPort << endl;

    while (true) {
        sockaddr_in clientAddr{};
        int clientAddrLen = sizeof(clientAddr);
        SOCKET clientSocket = accept(listenSocket, reinterpret_cast<sockaddr*>(&clientAddr), &clientAddrLen);
        if (clientSocket == INVALID_SOCKET) {
            cerr << "[SERVER] Accept failed" << endl;
            continue;
        }

        char clientIp[INET_ADDRSTRLEN] = {};
        inet_ntop(AF_INET, &clientAddr.sin_addr, clientIp, sizeof(clientIp));
        cout << "[SERVER] Client connected from " << clientIp << ":" << ntohs(clientAddr.sin_port) << endl;

        bool isAuthenticated = false;
        int sessionUserId = -1;
        int sessionRole = 0;
        string sessionUsername;
        string sessionKek;

        while (true) {
            PacketData request;
            if (!RecvAll(clientSocket, reinterpret_cast<char*>(&request), sizeof(PacketData))) {
                cerr << "[SERVER] Receive failed or client disconnected" << endl;
                break;
            }

            bool shouldCloseConnection = false;
            PacketData response;
            if (request.protocolVersion != PROTOCOL_VERSION) {
                response = BuildProtocolErrorResponse(request, "Protocol version is not compatible.");
            } else {
                switch (request.requestType) {
                case REQ_LOGIN:
                    response = HandleLogin(dbHelper, request, isAuthenticated, sessionUserId, sessionRole, sessionUsername, sessionKek);
                    break;
                case REQ_FETCH_PROFILE:
                    response = HandleFetchProfile(dbHelper, isAuthenticated, sessionUserId, sessionKek);
                    break;
                case REQ_REGISTER:
                    response = HandleRegister(dbHelper, request, isAuthenticated);
                    break;
                case REQ_UPDATE_PROFILE:
                    response = HandleUpdateProfile(dbHelper, request, isAuthenticated, sessionUserId, sessionUsername, sessionKek);
                    break;
                case REQ_DELETE_ACCOUNT:
                    response = HandleDeleteAccount(dbHelper, isAuthenticated, sessionUserId, sessionUsername, sessionKek, shouldCloseConnection);
                    if (response.status == STATUS_SUCCESS) {
                        isAuthenticated = false;
                        sessionRole = 0;
                    }
                    break;
                case REQ_GET_TOTAL:
                    response = HandleGetTotal(dbHelper);
                    break;
                case REQ_ADMIN_LIST_USERS:
                    response = HandleAdminListUsers(dbHelper, isAuthenticated, sessionRole, sessionUserId, sessionKek, request);
                    break;
                case REQ_LOGOUT:
                    response = HandleLogout(isAuthenticated, sessionUserId, sessionRole, sessionUsername, sessionKek);
                    break;
                default:
                    response = BuildProtocolErrorResponse(request, "Unsupported request type.");
                    break;
                }
            }

            if (!SendAll(clientSocket, reinterpret_cast<const char*>(&response), sizeof(PacketData))) {
                cerr << "[SERVER] Send failed" << endl;
                break;
            }

            if (request.requestType == REQ_LOGOUT || shouldCloseConnection) {
                break;
            }
        }

        closesocket(clientSocket);
    }

    closesocket(listenSocket);
    dbHelper.Disconnect();
    WSACleanup();
    return 0;
}
