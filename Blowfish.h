#ifndef BLOWFISH_H
#define BLOWFISH_H

#include <string>
#include <cstdint>

// Khai báo Class Blowfish
class Blowfish {
private:
    uint32_t P[18];
    uint32_t S[4][256];

    // Các hàm nội bộ 
    uint32_t F(uint32_t x);
    void InitializeDefaultArrays();
    void EncryptBlock(uint32_t& L, uint32_t& R);
    void DecryptBlock(uint32_t& L, uint32_t& R);

public:
    // Khởi tạo thuật toán với Khóa (Key)
    Blowfish(const std::string& key);

    // 2 hàm giao tiếp chính yếu cho hệ thống Database
    std::string EncryptString(const std::string& text);
    std::string DecryptString(const std::string& hexText);
};

#endif // BLOWFISH_H