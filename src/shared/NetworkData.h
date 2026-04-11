#ifndef NETWORK_DATA_H
#define NETWORK_DATA_H

#include <cstring>
#include <string>

#pragma pack(1)

struct PacketData {
    int protocolVersion;
    int requestType;
    int status;
    int userId;
    int recordId;
    int role;
    int gender;
    int dataType;
    int recordCount;

    char username[64];
    char password[128];
    char cccd[64];
    char phone[64];
    char email[96];
    char encryptedDek[256];
    char message[128];
};

#pragma pack()

static_assert(sizeof(PacketData) <= 1024, "PacketData size exceeds expected limit");

inline size_t GetLoginCipherCapacity() {
    PacketData packet{};
    return sizeof(packet.username) +
           sizeof(packet.password) +
           sizeof(packet.cccd) +
           sizeof(packet.phone) +
           sizeof(packet.email) +
           sizeof(packet.encryptedDek) +
           sizeof(packet.message);
}

inline void ClearLoginCipherFields(PacketData& packet) {
    memset(packet.username, 0, sizeof(packet.username));
    memset(packet.password, 0, sizeof(packet.password));
    memset(packet.cccd, 0, sizeof(packet.cccd));
    memset(packet.phone, 0, sizeof(packet.phone));
    memset(packet.email, 0, sizeof(packet.email));
    memset(packet.encryptedDek, 0, sizeof(packet.encryptedDek));
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
    CopyLoginCipherChunk(packet.username, sizeof(packet.username), cipherText, offset);
    CopyLoginCipherChunk(packet.password, sizeof(packet.password), cipherText, offset);
    CopyLoginCipherChunk(packet.cccd, sizeof(packet.cccd), cipherText, offset);
    CopyLoginCipherChunk(packet.phone, sizeof(packet.phone), cipherText, offset);
    CopyLoginCipherChunk(packet.email, sizeof(packet.email), cipherText, offset);
    CopyLoginCipherChunk(packet.encryptedDek, sizeof(packet.encryptedDek), cipherText, offset);
    CopyLoginCipherChunk(packet.message, sizeof(packet.message), cipherText, offset);
    return offset == cipherText.size();
}

inline void AppendLoginCipherChunk(std::string& cipherText, const char* src, size_t srcSize) {
    cipherText.append(src, strnlen_s(src, srcSize));
}

inline std::string ReadLoginCiphertext(const PacketData& packet) {
    std::string cipherText;
    cipherText.reserve(GetLoginCipherCapacity());
    AppendLoginCipherChunk(cipherText, packet.username, sizeof(packet.username));
    AppendLoginCipherChunk(cipherText, packet.password, sizeof(packet.password));
    AppendLoginCipherChunk(cipherText, packet.cccd, sizeof(packet.cccd));
    AppendLoginCipherChunk(cipherText, packet.phone, sizeof(packet.phone));
    AppendLoginCipherChunk(cipherText, packet.email, sizeof(packet.email));
    AppendLoginCipherChunk(cipherText, packet.encryptedDek, sizeof(packet.encryptedDek));
    AppendLoginCipherChunk(cipherText, packet.message, sizeof(packet.message));
    return cipherText;
}

#define PROTOCOL_VERSION 2

#define REQ_LOGIN            1
#define REQ_FETCH_PROFILE    2
#define REQ_REGISTER         3
#define REQ_UPDATE_PROFILE   4
#define REQ_DELETE_ACCOUNT   5
#define REQ_GET_TOTAL        6
#define REQ_LOGOUT           7
#define REQ_ADMIN_LIST_USERS 8

#define DATATYPE_PLAINTEXT       1
#define DATATYPE_LOGIN_BLOWFISH  2

#define STATUS_SUCCESS       0
#define STATUS_ERROR         1
#define STATUS_NOTFOUND      2
#define STATUS_UNAUTHORIZED  3

#endif  // NETWORK_DATA_H
