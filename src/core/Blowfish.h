#ifndef BLOWFISH_H
#define BLOWFISH_H

#include <string>
#include <cstdint>

// Khai bao Class Blowfish
class Blowfish {
private:
    uint32_t P[18];
    uint32_t S[4][256];

    // Cac ham noi bo
    uint32_t F(uint32_t x);
    void InitializeDefaultArrays();
    void EncryptBlock(uint32_t& L, uint32_t& R);
    void DecryptBlock(uint32_t& L, uint32_t& R);

public:
    // Khoi tao thuat toan voi Khoa (Key)
    Blowfish(const std::string& key);

    // 2 ham giao tiep chinh yeu cho he thong Database
    std::string EncryptString(const std::string& text);
    std::string DecryptString(const std::string& hexText);
};

#endif // BLOWFISH_H