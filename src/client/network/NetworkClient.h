#ifndef NETWORK_CLIENT_H
#define NETWORK_CLIENT_H

#include <string>
#include <vector>

struct PacketData;
struct nhanvien;

class NetworkClient {
public:
    struct LoginResult {
        bool success = false;
        std::string userRole;
        int userId = -1;
        std::string userName;
        std::string message;
    };

    NetworkClient();
    ~NetworkClient();

    bool Connect(const std::string& host, int port, std::string& error);
    void Disconnect();
    bool IsConnected() const;

    bool Login(const std::string& cccd, const std::string& password, LoginResult& result, std::string& error);
    bool Logout(std::string& error);

    bool FetchAllEmployees(std::vector<nhanvien>& employees, std::string& error);
    bool AddEmployee(const nhanvien& employee, const std::string& passwordPlaintext, std::string& error);
    bool UpdateEmployee(const nhanvien& employee, const std::string& passwordPlaintext, std::string& error);
    bool DeleteEmployee(int employeeId, std::string& error);
    bool GetTotalEmployees(int& total, std::string& error);

private:
    bool SendAll(const char* data, int totalBytes);
    bool RecvAll(char* data, int totalBytes);
    bool SendRequest(const struct PacketData& request, struct PacketData& response, std::string& error);

    std::string host_;
    int port_;
    bool connected_;
    std::string currentRole_;
    std::string currentUserName_;
    int currentUserId_;
    unsigned long long socketValue_;
};

#endif // NETWORK_CLIENT_H
