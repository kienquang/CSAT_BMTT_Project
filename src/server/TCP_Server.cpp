#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>

#include <cstring>
#include <string>

#include "../core/Blowfish.h"
#include "../core/DatabaseHelper.h"
#include "../core/EnvConfig.h"
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

int RoleStringToInt(const string& role) {
    return role == "Admin" ? ROLE_ADMIN : ROLE_USER;
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

bool DecryptLoginPayload(const PacketData& request, string& cccd, string& password, string& error) {
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
    if (plainPayload.size() < 12) {
        error = "Encrypted login payload is invalid";
        return false;
    }

    cccd = plainPayload.substr(0, 12);
    password = plainPayload.substr(12);
    return true;
}

PacketData HandleLogin(DatabaseHelper& dbHelper, const PacketData& request,
                       bool& isAuthenticated, int& sessionRole, int& sessionUserId) {
    PacketData response;
    InitializePacket(response);
    response.requestType = request.requestType;

    string cccdPlaintext;
    string passwordPlaintext;
    string decryptError;
    if (!DecryptLoginPayload(request, cccdPlaintext, passwordPlaintext, decryptError)) {
        response.status = STATUS_ERROR;
        CopyToBuffer(response.message, decryptError);
        return response;
    }

    nhanvien user = dbHelper.AuthenticateUser(cccdPlaintext, passwordPlaintext);
    if (user.id == -1) {
        response.status = STATUS_UNAUTHORIZED;
        CopyToBuffer(response.message, "Dang nhap that bai. CCCD hoac mat khau sai.");
        return response;
    }

    isAuthenticated = true;
    sessionRole = RoleStringToInt(user.vai_tro);
    sessionUserId = user.id;

    response.status = STATUS_SUCCESS;
    response.userRole = sessionRole;
    response.employeeId = sessionUserId;
    response.dataType = DATATYPE_PLAINTEXT;
    CopyToBuffer(response.employeeName, user.ten_nv);
    CopyToBuffer(response.employeeRole, user.vai_tro);
    CopyToBuffer(response.message, "Dang nhap thanh cong.");

    return response;
}

bool IsAdminSession(bool isAuthenticated, int sessionRole) {
    return isAuthenticated && sessionRole == ROLE_ADMIN;
}

PacketData HandleMutationDenied(int requestType, const string& message) {
    PacketData response;
    InitializePacket(response);
    response.requestType = requestType;
    response.status = STATUS_UNAUTHORIZED;
    CopyToBuffer(response.message, message);
    return response;
}

PacketData HandleAddEmployee(DatabaseHelper& dbHelper, const PacketData& request,
                             bool isAuthenticated, int sessionRole) {
    if (!IsAdminSession(isAuthenticated, sessionRole)) {
        return HandleMutationDenied(request.requestType, "Chi Admin moi duoc them nhan vien.");
    }

    PacketData response;
    InitializePacket(response);
    response.requestType = request.requestType;

    const bool inserted = dbHelper.InsertNhanVien(
        request.employeeName,
        request.employeeRole,
        request.cccd,
        request.phone,
        request.password,
        request.salary
    );

    response.status = inserted ? STATUS_SUCCESS : STATUS_ERROR;
    CopyToBuffer(response.message, inserted ? "Them nhan vien thanh cong." : "Them nhan vien that bai.");
    return response;
}

PacketData HandleUpdateEmployee(DatabaseHelper& dbHelper, const PacketData& request,
                                bool isAuthenticated, int sessionRole) {
    if (!IsAdminSession(isAuthenticated, sessionRole)) {
        return HandleMutationDenied(request.requestType, "Chi Admin moi duoc cap nhat nhan vien.");
    }

    PacketData response;
    InitializePacket(response);
    response.requestType = request.requestType;

    const bool updated = dbHelper.UpdateNhanVien(
        request.employeeId,
        request.employeeName,
        request.employeeRole,
        request.cccd,
        request.phone,
        request.password,
        request.salary
    );

    response.status = updated ? STATUS_SUCCESS : STATUS_ERROR;
    CopyToBuffer(response.message, updated ? "Cap nhat nhan vien thanh cong." : "Cap nhat nhan vien that bai.");
    return response;
}

PacketData HandleDeleteEmployee(DatabaseHelper& dbHelper, const PacketData& request,
                                bool isAuthenticated, int sessionRole) {
    if (!IsAdminSession(isAuthenticated, sessionRole)) {
        return HandleMutationDenied(request.requestType, "Chi Admin moi duoc xoa nhan vien.");
    }

    PacketData response;
    InitializePacket(response);
    response.requestType = request.requestType;

    const bool deleted = dbHelper.DeleteNhanVienById(request.employeeId);
    response.status = deleted ? STATUS_SUCCESS : STATUS_ERROR;
    CopyToBuffer(response.message, deleted ? "Xoa nhan vien thanh cong." : "Xoa nhan vien that bai.");
    return response;
}

PacketData HandleGetTotal(DatabaseHelper& dbHelper, bool isAuthenticated) {
    PacketData response;
    InitializePacket(response);
    response.requestType = REQ_GET_TOTAL;

    if (!isAuthenticated) {
        response.status = STATUS_UNAUTHORIZED;
        CopyToBuffer(response.message, "Ban can dang nhap truoc khi truy van.");
        return response;
    }

    response.status = STATUS_SUCCESS;
    response.recordCount = dbHelper.GetTotalNhanVien();
    CopyToBuffer(response.message, "Lay tong so nhan vien thanh cong.");
    return response;
}

bool SendEmployeeListResponse(SOCKET clientSocket, DatabaseHelper& dbHelper,
                              bool isAuthenticated, int sessionRole, string& error) {
    PacketData header;
    InitializePacket(header);
    header.requestType = REQ_LIST_EMPLOYEES;

    if (!isAuthenticated) {
        header.status = STATUS_UNAUTHORIZED;
        CopyToBuffer(header.message, "Ban can dang nhap truoc khi truy van.");
        return SendAll(clientSocket, reinterpret_cast<const char*>(&header), sizeof(PacketData));
    }

    const int responseDataType = sessionRole == ROLE_USER ? DATATYPE_MASKED : DATATYPE_PLAINTEXT;
    vector<nhanvien> employees = dbHelper.GetAllNhanVienForClient(sessionRole);
    header.status = STATUS_SUCCESS;
    header.userRole = sessionRole;
    header.dataType = responseDataType;
    header.recordCount = static_cast<int>(employees.size());
    CopyToBuffer(header.message, "Lay danh sach nhan vien thanh cong.");

    if (!SendAll(clientSocket, reinterpret_cast<const char*>(&header), sizeof(PacketData))) {
        error = "Failed to send employee list header";
        return false;
    }

    for (const auto& employee : employees) {
        PacketData item;
        InitializePacket(item);
        item.requestType = REQ_LIST_EMPLOYEES;
        item.status = STATUS_SUCCESS;
        item.userRole = sessionRole;
        item.employeeId = employee.id;
        item.dataType = responseDataType;
        CopyToBuffer(item.employeeName, employee.ten_nv);
        CopyToBuffer(item.employeeRole, employee.vai_tro);
        CopyToBuffer(item.cccd, employee.cccd_cipher);
        CopyToBuffer(item.phone, employee.sdt_cipher);
        CopyToBuffer(item.salary, employee.luong_cipher);
        CopyToBuffer(item.passwordMasked, employee.matkhau_cipher);

        if (!SendAll(clientSocket, reinterpret_cast<const char*>(&item), sizeof(PacketData))) {
            error = "Failed to send employee list item";
            return false;
        }
    }

    return true;
}

PacketData HandleLogout(bool& isAuthenticated, int& sessionRole, int& sessionUserId) {
    PacketData response;
    InitializePacket(response);
    response.requestType = REQ_LOGOUT;
    response.status = STATUS_SUCCESS;
    CopyToBuffer(response.message, "Da dang xuat.");

    isAuthenticated = false;
    sessionRole = ROLE_USER;
    sessionUserId = -1;
    return response;
}

PacketData BuildProtocolErrorResponse(const PacketData& request, const string& message) {
    PacketData response;
    InitializePacket(response);
    response.requestType = request.requestType;
    response.status = STATUS_ERROR;
    CopyToBuffer(response.message, message);
    return response;
}

} // namespace

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

    dbHelper.CreateTableNhanVien();

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
        int sessionRole = ROLE_USER;
        int sessionUserId = -1;

        while (true) {
            PacketData request;
            if (!RecvAll(clientSocket, reinterpret_cast<char*>(&request), sizeof(PacketData))) {
                cerr << "[SERVER] Receive failed or client disconnected" << endl;
                break;
            }

            PacketData response;
            string error;
            if (request.protocolVersion != PROTOCOL_VERSION) {
                response = BuildProtocolErrorResponse(request, "Protocol version khong tuong thich.");
            } else {
                switch (request.requestType) {
                case REQ_LOGIN:
                    response = HandleLogin(dbHelper, request, isAuthenticated, sessionRole, sessionUserId);
                    break;
                case REQ_LIST_EMPLOYEES:
                    if (!SendEmployeeListResponse(clientSocket, dbHelper, isAuthenticated, sessionRole, error)) {
                        cerr << "[SERVER] " << error << endl;
                    }
                    continue;
                case REQ_ADD_EMPLOYEE:
                    response = HandleAddEmployee(dbHelper, request, isAuthenticated, sessionRole);
                    break;
                case REQ_UPDATE_EMPLOYEE:
                    response = HandleUpdateEmployee(dbHelper, request, isAuthenticated, sessionRole);
                    break;
                case REQ_DELETE_EMPLOYEE:
                    response = HandleDeleteEmployee(dbHelper, request, isAuthenticated, sessionRole);
                    break;
                case REQ_GET_TOTAL:
                    response = HandleGetTotal(dbHelper, isAuthenticated);
                    break;
                case REQ_LOGOUT:
                    response = HandleLogout(isAuthenticated, sessionRole, sessionUserId);
                    break;
                default:
                    response = BuildProtocolErrorResponse(request, "Request type khong ho tro.");
                    break;
                }
            }

            if (!SendAll(clientSocket, reinterpret_cast<const char*>(&response), sizeof(PacketData))) {
                cerr << "[SERVER] Send failed" << endl;
                break;
            }

            if (request.requestType == REQ_LOGOUT) {
                cout << "[SERVER] Client requested logout" << endl;
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
