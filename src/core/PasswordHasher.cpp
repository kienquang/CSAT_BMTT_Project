#include "PasswordHasher.h"
#include "SecureRandom.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <argon2.h>

using namespace std;

namespace {

constexpr uint32_t kTimeCost = 3;
constexpr uint32_t kMemoryCostKiB = 1 << 16;
constexpr uint32_t kParallelism = 1;
constexpr size_t kSaltLength = 16;
constexpr size_t kHashLength = 32;
constexpr size_t kKekLength = 32;

array<uint8_t, kSaltLength> GenerateSalt() {
    const vector<uint8_t> randomSalt = SecureRandom::RandomBytes(kSaltLength);
    array<uint8_t, kSaltLength> salt{};
    copy(randomSalt.begin(), randomSalt.end(), salt.begin());
    return salt;
}

string BuildDerivationSalt(const string& username) {
    string normalized = username;
    for (char& ch : normalized) {
        ch = static_cast<char>(tolower(static_cast<unsigned char>(ch)));
    }

    string salt = "kek:" + normalized;
    while (salt.size() < kSaltLength) {
        salt += "#";
        salt += normalized;
    }

    salt.resize(kSaltLength);
    return salt;
}

string BytesToHex(const uint8_t* data, size_t length) {
    stringstream stream;
    stream << hex << setfill('0');
    for (size_t i = 0; i < length; ++i) {
        stream << setw(2) << static_cast<unsigned int>(data[i]);
    }
    return stream.str();
}

}  // namespace

namespace PasswordHasher {

string HashPassword(const string& passwordPlaintext) {
    if (passwordPlaintext.empty()) {
        throw runtime_error("Password must not be empty");
    }

    const auto salt = GenerateSalt();
    const size_t encodedLength =
        argon2_encodedlen(kTimeCost, kMemoryCostKiB, kParallelism,
                          static_cast<uint32_t>(salt.size()),
                          static_cast<uint32_t>(kHashLength), Argon2_id) +
        1;

    vector<char> encoded(encodedLength, '\0');
    const int rc = argon2id_hash_encoded(
        kTimeCost,
        kMemoryCostKiB,
        kParallelism,
        passwordPlaintext.data(),
        passwordPlaintext.size(),
        salt.data(),
        salt.size(),
        kHashLength,
        encoded.data(),
        encoded.size());

    if (rc != ARGON2_OK) {
        throw runtime_error(argon2_error_message(rc));
    }

    return string(encoded.data());
}

bool VerifyPassword(const string& passwordPlaintext, const string& encodedHash) {
    if (passwordPlaintext.empty() || encodedHash.empty()) {
        return false;
    }

    const int rc = argon2id_verify(
        encodedHash.c_str(),
        passwordPlaintext.data(),
        passwordPlaintext.size());

    return rc == ARGON2_OK;
}

string DeriveKeyEncryptionKey(const string& passwordPlaintext, const string& username) {
    if (passwordPlaintext.empty()) {
        throw runtime_error("Password must not be empty for KEK derivation");
    }

    if (username.empty()) {
        throw runtime_error("Username must not be empty for KEK derivation");
    }

    const string salt = BuildDerivationSalt(username);
    array<uint8_t, kKekLength> rawKey{};

    const int rc = argon2id_hash_raw(
        kTimeCost,
        kMemoryCostKiB,
        kParallelism,
        passwordPlaintext.data(),
        passwordPlaintext.size(),
        salt.data(),
        salt.size(),
        rawKey.data(),
        rawKey.size());

    if (rc != ARGON2_OK) {
        throw runtime_error(argon2_error_message(rc));
    }

    return BytesToHex(rawKey.data(), rawKey.size());
}

}  // namespace PasswordHasher
