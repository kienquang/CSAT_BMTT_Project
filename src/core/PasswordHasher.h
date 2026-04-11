#ifndef PASSWORD_HASHER_H
#define PASSWORD_HASHER_H

#include <string>

namespace PasswordHasher {

std::string HashPassword(const std::string& passwordPlaintext);
bool VerifyPassword(const std::string& passwordPlaintext, const std::string& encodedHash);
std::string DeriveKeyEncryptionKey(const std::string& passwordPlaintext, const std::string& username);

}  // namespace PasswordHasher

#endif  // PASSWORD_HASHER_H
