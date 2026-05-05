#ifndef BLOWFISH_H
#define BLOWFISH_H

#include <string>
#include <cstdint>

// [GROUP: Cipher Class]
class Blowfish {
private:
    uint32_t P[18];
    uint32_t S[4][256];

    // [GROUP: Internal Helpers]
    uint32_t F(uint32_t x);
    void InitializeDefaultArrays();
    void EncryptBlock(uint32_t& L, uint32_t& R);
    void DecryptBlock(uint32_t& L, uint32_t& R);

public:
    // [GROUP: Public API]
    Blowfish(const std::string& key);

    // [GROUP: Public API]
    std::string EncryptString(const std::string& text);
    std::string DecryptString(const std::string& hexText);
};

#endif  // BLOWFISH_H