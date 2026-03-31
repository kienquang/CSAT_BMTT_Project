#ifndef NETWORK_DATA_H
#define NETWORK_DATA_H

#include <cstring>
#include <string>

// ======== SHARED NETWORK PACKET DEFINITION ========
// Client gui request den server.
// Server tra ve du lieu da duoc mask theo chinh sach truoc khi gui cho client.

#pragma pack(1)

struct PacketData {
    int protocolVersion;
    int requestType;
    int status;
    int userRole;
    int employeeId;
    int dataType;
    int recordCount;

    char cccd[20];
    char password[32];

    char employeeName[48];
    char employeeRole[16];
    char phone[20];
    char salary[20];
    char passwordMasked[20];
    char message[48];
};

#pragma pack()

static_assert(sizeof(PacketData) <= 256, "PacketData size exceeds 256 bytes!");

inline size_t GetLoginCipherCapacity() {
    PacketData packet{};
    return sizeof(packet.cccd) +
           sizeof(packet.password) +
           sizeof(packet.employeeName) +
           sizeof(packet.employeeRole) +
           sizeof(packet.phone) +
           sizeof(packet.salary) +
           sizeof(packet.passwordMasked) +
           sizeof(packet.message);
}

inline void ClearLoginCipherFields(PacketData& packet) {
    memset(packet.cccd, 0, sizeof(packet.cccd));
    memset(packet.password, 0, sizeof(packet.password));
    memset(packet.employeeName, 0, sizeof(packet.employeeName));
    memset(packet.employeeRole, 0, sizeof(packet.employeeRole));
    memset(packet.phone, 0, sizeof(packet.phone));
    memset(packet.salary, 0, sizeof(packet.salary));
    memset(packet.passwordMasked, 0, sizeof(packet.passwordMasked));
    memset(packet.message, 0, sizeof(packet.message));
}

inline void CopyLoginCipherChunk(char* dest, size_t destSize, const std::string& cipherText, size_t& offset) {
    const size_t remaining = cipherText.size() - offset;
    const size_t chunkSize = remaining < destSize ? remaining : destSize;
    if (chunkSize > 0) {
        memcpy(dest, cipherText.data() + offset, chunkSize);
        offset += chunkSize;
    }
    if (chunkSize < destSize) {
        memset(dest + chunkSize, 0, destSize - chunkSize);
    }
}

inline bool WriteLoginCiphertext(PacketData& packet, const std::string& cipherText) {
    if (cipherText.size() > GetLoginCipherCapacity()) {
        return false;
    }

    ClearLoginCipherFields(packet);

    size_t offset = 0;
    CopyLoginCipherChunk(packet.cccd, sizeof(packet.cccd), cipherText, offset);
    CopyLoginCipherChunk(packet.password, sizeof(packet.password), cipherText, offset);
    CopyLoginCipherChunk(packet.employeeName, sizeof(packet.employeeName), cipherText, offset);
    CopyLoginCipherChunk(packet.employeeRole, sizeof(packet.employeeRole), cipherText, offset);
    CopyLoginCipherChunk(packet.phone, sizeof(packet.phone), cipherText, offset);
    CopyLoginCipherChunk(packet.salary, sizeof(packet.salary), cipherText, offset);
    CopyLoginCipherChunk(packet.passwordMasked, sizeof(packet.passwordMasked), cipherText, offset);
    CopyLoginCipherChunk(packet.message, sizeof(packet.message), cipherText, offset);
    return offset == cipherText.size();
}

inline void AppendLoginCipherChunk(std::string& cipherText, const char* src, size_t srcSize) {
    cipherText.append(src, strnlen_s(src, srcSize));
}

inline std::string ReadLoginCiphertext(const PacketData& packet) {
    std::string cipherText;
    cipherText.reserve(GetLoginCipherCapacity());
    AppendLoginCipherChunk(cipherText, packet.cccd, sizeof(packet.cccd));
    AppendLoginCipherChunk(cipherText, packet.password, sizeof(packet.password));
    AppendLoginCipherChunk(cipherText, packet.employeeName, sizeof(packet.employeeName));
    AppendLoginCipherChunk(cipherText, packet.employeeRole, sizeof(packet.employeeRole));
    AppendLoginCipherChunk(cipherText, packet.phone, sizeof(packet.phone));
    AppendLoginCipherChunk(cipherText, packet.salary, sizeof(packet.salary));
    AppendLoginCipherChunk(cipherText, packet.passwordMasked, sizeof(packet.passwordMasked));
    AppendLoginCipherChunk(cipherText, packet.message, sizeof(packet.message));
    return cipherText;
}

#define PROTOCOL_VERSION 1

// ======== ROLE CONSTANTS ========
#define ROLE_ADMIN 1
#define ROLE_USER 2

// ======== REQUEST TYPE CONSTANTS ========
#define REQ_LOGIN            1
#define REQ_LIST_EMPLOYEES   2
#define REQ_ADD_EMPLOYEE     3
#define REQ_UPDATE_EMPLOYEE  4
#define REQ_DELETE_EMPLOYEE  5
#define REQ_GET_TOTAL        6
#define REQ_LOGOUT           7

// ======== DATA TYPE CONSTANTS ========
#define DATATYPE_PLAINTEXT       1
#define DATATYPE_MASKED          2
#define DATATYPE_LOGIN_BLOWFISH  3
#define DATATYPE_ADMIN_BLOWFISH  4

// ======== STATUS CODE CONSTANTS ========
#define STATUS_SUCCESS       0
#define STATUS_ERROR         1
#define STATUS_NOTFOUND      2
#define STATUS_UNAUTHORIZED  3

#endif // NETWORK_DATA_H
