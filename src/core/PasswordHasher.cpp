#include "PasswordHasher.h"
#include "Base64.h"
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

// [GROUP: Argon2 Constants]
constexpr uint32_t kTimeCost = 3;
constexpr uint32_t kMemoryCostKiB = 1 << 16;
constexpr uint32_t kParallelism = 1;
constexpr size_t kSaltLength = 16;
constexpr size_t kHashLength = 32;
constexpr size_t kKekLength = 32;

struct Argon2EncodedParts {
    uint32_t memoryCostKiB = 0;
    uint32_t timeCost = 0;
    uint32_t parallelism = 0;
    string saltBytes;
};

// [GROUP: Internal Helpers]
array<uint8_t, kSaltLength> GenerateSalt() {
    const vector<uint8_t> randomSalt = SecureRandom::RandomBytes(kSaltLength);
    array<uint8_t, kSaltLength> salt{};
    copy(randomSalt.begin(), randomSalt.end(), salt.begin());
    return salt;
}

// [GROUP: Internal Helpers]
vector<string> Split(const string& value, char delimiter) {
    vector<string> parts;
    string current;
    for (char ch : value) {
        if (ch == delimiter) {
            parts.push_back(current);
            current.clear();
            continue;
        }
        current.push_back(ch);
    }
    parts.push_back(current);
    return parts;
}

// [GROUP: Internal Helpers]
string NormalizeBase64Padding(string encoded) {
    while ((encoded.size() % 4) != 0) {
        encoded.push_back('=');
    }
    return encoded;
}

// [GROUP: Internal Helpers]
Argon2EncodedParts ParseArgon2EncodedHash(const string& encodedHash) {
    const vector<string> sections = Split(encodedHash, '$');
    if (sections.size() < 6) {
        throw runtime_error("Encoded hash format is invalid");
    }

    // Expected format: $argon2id$v=19$m=...,t=...,p=...$<salt_b64>$<hash_b64>
    if (sections[1] != "argon2id") {
        throw runtime_error("Only argon2id hashes are supported for KEK derivation");
    }

    const string& paramsSection = sections[3];
    const vector<string> params = Split(paramsSection, ',');

    Argon2EncodedParts parts;
    for (const string& param : params) {
        const size_t equalsPos = param.find('=');
        if (equalsPos == string::npos || equalsPos == 0 || equalsPos == param.size() - 1) {
            continue;
        }

        const string key = param.substr(0, equalsPos);
        const string value = param.substr(equalsPos + 1);
        if (key == "m") {
            parts.memoryCostKiB = static_cast<uint32_t>(stoul(value));
        } else if (key == "t") {
            parts.timeCost = static_cast<uint32_t>(stoul(value));
        } else if (key == "p") {
            parts.parallelism = static_cast<uint32_t>(stoul(value));
        }
    }

    if (parts.memoryCostKiB == 0 || parts.timeCost == 0 || parts.parallelism == 0) {
        throw runtime_error("Encoded hash does not contain valid Argon2 parameters");
    }

    const string saltBase64 = NormalizeBase64Padding(sections[4]);
    if (!Base64::Decode(saltBase64, parts.saltBytes) || parts.saltBytes.empty()) {
        throw runtime_error("Failed to decode Argon2 salt");
    }

    return parts;
}

// [GROUP: Internal Helpers]
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

// [GROUP: Public API]
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

// [GROUP: Public API]
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

// [GROUP: Public API]
string DeriveKeyEncryptionKey(const string& passwordPlaintext, const string& encodedHash) {
    if (passwordPlaintext.empty()) {
        throw runtime_error("Password must not be empty for KEK derivation");
    }

    if (encodedHash.empty()) {
        throw runtime_error("Encoded hash must not be empty for KEK derivation");
    }

    const Argon2EncodedParts parsed = ParseArgon2EncodedHash(encodedHash);
    array<uint8_t, kKekLength> rawKey{};

    const int rc = argon2id_hash_raw(
        parsed.timeCost,
        parsed.memoryCostKiB,
        parsed.parallelism,
        passwordPlaintext.data(),
        passwordPlaintext.size(),
        parsed.saltBytes.data(),
        parsed.saltBytes.size(),
        rawKey.data(),
        rawKey.size());

    if (rc != ARGON2_OK) {
        throw runtime_error(argon2_error_message(rc));
    }

    return BytesToHex(rawKey.data(), rawKey.size());
}

}  // namespace PasswordHasher
