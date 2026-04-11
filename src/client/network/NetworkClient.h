#ifndef NETWORK_CLIENT_H
#define NETWORK_CLIENT_H

#include <string>
#include <vector>

#include "../../core/DatabaseHelper.h"

struct PacketData;

class NetworkClient {
public:
    struct LoginResult {
        bool success = false;
        int userId = -1;
        int role = 1;
        std::string username;
        std::string message;
    };

    NetworkClient();
    ~NetworkClient();

    bool Connect(const std::string& host, int port, std::string& error);
    void Disconnect();
    bool IsConnected() const;

    bool Login(const std::string& username, const std::string& password, LoginResult& result, std::string& error);
    bool Logout(std::string& error);

    bool FetchProfile(PersonalRecord& record, std::string& error);
    bool Register(const PersonalRecord& record, const std::string& passwordPlaintext, std::string& error);
    bool UpdateProfile(const PersonalRecord& record, const std::string& passwordPlaintext, std::string& error);
    bool DeleteAccount(std::string& error);
    bool GetTotalUsers(int& total, std::string& error);
    bool FetchEncryptedUserList(std::vector<PersonalRecord>& records, std::string& error);

private:
    bool SendAll(const char* data, int totalBytes);
    bool RecvAll(char* data, int totalBytes);
    bool SendRequest(const PacketData& request, PacketData& response, std::string& error);

    std::string host_;
    int port_;
    bool connected_;
    std::string currentUsername_;
    int currentUserId_;
    unsigned long long socketValue_;
};

#endif  // NETWORK_CLIENT_H
