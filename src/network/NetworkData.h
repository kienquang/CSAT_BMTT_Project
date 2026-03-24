#ifndef NETWORK_DATA_H
#define NETWORK_DATA_H

#include <cstring>

// ======== SHARED NETWORK PACKET DEFINITION ========
// Sử dụng cho giao tiếp giữa TCP_Client và TCP_Server
// Kích thước cố định: 64 bytes (pragma pack để không padding)

#pragma pack(1)

struct PacketData {
    // [0-3] User Role
    int userRole;              // 1 = Admin (nhận plaintext sau decrypt)
                               // 2 = User  (nhận masked data)

    // [4-7] Request Type
    int requestType;           // 1 = Query CCCD
                               // 2 = Query Phone
                               // 3 = Query Salary
                               // 4 = Login request
                               // etc.

    // [8-27] CCCD Data (20 bytes)
    // - If userRole = Admin: HEX string (e.g., "A3F7B2C1E5F8A4B9...")
    // - If userRole = User: Masked string (e.g., "0012***7855")
    char cccd[20];

    // [28-47] Phone Data (20 bytes)
    // Same format as cccd above
    char phone[20];

    // [48-67] Salary Data (20 bytes)
    // Same format as cccd above
    char salary[20];

    // [68-71] Data Type
    int dataType;              // 1 = Encrypted HEX (Admin)
                               // 2 = Masked plaintext (User)

    // [72-75] Status Code
    int status;                // 0 = Success
                               // 1 = Error
                               // 2 = Not found

    // [76-79] Timestamp
    int timestamp;             // Unix timestamp

    // Total: 80 bytes (should be 64 initially, expand as needed)
};

#pragma pack()

// ======== STATIC ASSERTION ========
// Verify struct size at compile time
static_assert(sizeof(PacketData) <= 256, "PacketData size exceeds 256 bytes!");

// ======== ROLE CONSTANTS ========
#define ROLE_ADMIN 1
#define ROLE_USER 2

// ======== REQUEST TYPE CONSTANTS ========
#define REQ_QUERY_CCCD   1
#define REQ_QUERY_PHONE  2
#define REQ_QUERY_SALARY 3
#define REQ_LOGIN        4

// ======== DATA TYPE CONSTANTS ========
#define DATATYPE_ENCRYPTED 1
#define DATATYPE_MASKED    2

// ======== STATUS CODE CONSTANTS ========
#define STATUS_SUCCESS   0
#define STATUS_ERROR     1
#define STATUS_NOTFOUND  2

#endif // NETWORK_DATA_H
