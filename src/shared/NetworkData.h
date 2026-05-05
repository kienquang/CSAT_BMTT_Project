#ifndef NETWORK_DATA_H
#define NETWORK_DATA_H

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
    char name[128];
    char password[128];
    char cccd[64];
    char phone[64];
    char email[96];
    char encryptedDek[256];
    char message[128];
};

#pragma pack()

static_assert(sizeof(PacketData) <= 1024, "PacketData size exceeds expected limit");

// [GROUP: Protocol Version]
#define PROTOCOL_VERSION 2

// [GROUP: Request Codes]
#define REQ_LOGIN            1
#define REQ_FETCH_PROFILE    2
#define REQ_REGISTER         3
#define REQ_UPDATE_PROFILE   4
#define REQ_DELETE_ACCOUNT   5
#define REQ_GET_TOTAL        6
#define REQ_LOGOUT           7
#define REQ_ADMIN_LIST_USERS 8
#define REQ_ADMIN_VIEW_MY_INFO 9
#define REQ_ADMIN_DELETE_USER 10
#define REQ_ADMIN_UPDATE_USER_ROLE 11
#define REQ_ADMIN_LIST_MEDICAL_RECORDS 12
#define REQ_DOCTOR_LIST_MY_MEDICAL_RECORDS 13
#define REQ_DOCTOR_CREATE_MEDICAL_RECORD 14
#define REQ_DOCTOR_VIEW_MEDICAL_RECORD_DETAIL 15

// [GROUP: Payload Types]
#define DATATYPE_PLAINTEXT 1

// [GROUP: Response Status Codes]
#define STATUS_SUCCESS       0
#define STATUS_ERROR         1
#define STATUS_NOTFOUND      2
#define STATUS_UNAUTHORIZED  3

#endif  // NETWORK_DATA_H
