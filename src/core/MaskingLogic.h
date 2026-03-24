#ifndef MASKING_LOGIC_H
#define MASKING_LOGIC_H

#include <cstring>
#include <iostream>

using namespace std;

// ======== MASKING UTILITY MODULE ========
// Hàm này kiểm soát che giấu dữ liệu nhạy cảm trước khi gửi cho User
// TUYỆT ĐỐI không dùng <regex> - chỉ dùng vòng lặp for
// Ghi đè trực tiếp bộ đệm cục bộ với dấu sao '*'

class MaskingLogic {
public:
    // ======== HÀM CHE GIẤU PHỔ BIẾN ========
    
    // Che giấu số điện thoại: "0912345678" → "091****678"
    // Tham số: buffer = con trỏ tới mảng char chứa SĐT plaintext
    // Kích thước: 20 bytes chuẩn từ NetworkData.h
    // Hành động: Ghi đè các ký tự nhạy cảm bằng '*' trực tiếp trên buffer
    static void MaskPhone(char* buffer, size_t bufferSize = 20);

    // Che giấu CCCD: "001202037855" → "0012***7855"
    // Tương tự maskPhone nhưng cho CCCD (12 chữ số)
    static void MaskCCCD(char* buffer, size_t bufferSize = 20);

    // Che giấu Lương (4 chữ số cuối): "50000000" → "5000****"
    static void MaskSalary(char* buffer, size_t bufferSize = 20);

    // ======== HÀM KIỂM CHỨNG ========

    // Kiểm tra xem buffer có chứa dữ liệu đã được mô phỏng không
    // Return: true nếu phát hiện '*', false nếu dữ liệu gốc
    static bool IsAlreadyMasked(const char* buffer);

    // In ra buffer để debug
    static void PrintBuffer(const char* buffer, const char* label);
};

#endif // MASKING_LOGIC_H
