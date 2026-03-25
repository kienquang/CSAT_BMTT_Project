#ifndef MASKING_LOGIC_H
#define MASKING_LOGIC_H

#include <cstring>
#include <iostream>
#include <string>

using namespace std;

// ======== ENUM DINH NGHIA VAI TRO ========
enum class UserRole {
    ADMIN,  // Admin: Co quyen xem du lieu goc (plaintext)
    USER    // User: Chi xem du lieu da mask
};

// ======== MASKING UTILITY MODULE ========
// Ham nay kiem soat che giau du lieu nhay cam truoc khi gui cho User
// TUYET DOI khong dung <regex> - chi dung vong lap for
// Ghi de truc tiep bo dem cuc bo voi dau sao '*'

class MaskingLogic {
public:
    // ======== HAM CHE GIAU PHO BIEN (KHONG TON ROLE) ========
    
    // Che giau so dien thoai: "0912345678" → "091****678"
    // Tham so: buffer = con tro toi mang char chua SDT plaintext
    // Kich thuoc: 20 bytes chuan tu NetworkData.h
    // Hanh dong: Ghi de cac ky tu nhay cam bang '*' truc tiep tren buffer
    static void MaskPhone(char* buffer, size_t bufferSize = 20);

    // Che giau CCCD: "001202037855" → "0012***7855"
    // Tuong tu maskPhone nhung cho CCCD (12 chu so)
    static void MaskCCCD(char* buffer, size_t bufferSize = 20);

    // Che giau thành 3 ky tu '*': "50000000" → "***"
    static void MaskToThreeStar(char* buffer, UserRole role, size_t bufferSize = 20);

    // ======== HAM CHE GIAU THEO VAI TRO (ROLE-BASED) ========
    
    // Che giau CCCD theo vai tro: Admin → plaintext, User → mask all "****037855"
    static void MaskCCCDByRole(char* buffer, UserRole role, size_t bufferSize = 20);

    // Che giau SDT theo vai tro: Admin → plaintext, User → "09****5678"
    static void MaskPhoneByRole(char* buffer, UserRole role, size_t bufferSize = 20);

    // Che giau theo vai tro: Admin → plaintext, User → "***"
    static void MaskToThreeStarByRole(char* buffer, UserRole role, size_t bufferSize = 20);

    // ======== HAM UTILITY ========

    // Chuyen string role thanh enum
    static UserRole StringToRole(const string& roleStr);

    // Kiem tra xem buffer co chua du lieu da duoc mo phong khong
    // Return: true neu phat hien '*', false neu du lieu goc
    static bool IsAlreadyMasked(const char* buffer);

    // In ra buffer de debug
    static void PrintBuffer(const char* buffer, const char* label);
};

#endif // MASKING_LOGIC_H
