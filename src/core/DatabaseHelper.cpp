#include "DatabaseHelper.h"

#include "Base64.h"
#include "Blowfish.h"
#include "PasswordHasher.h"
#include "SecureRandom.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#endif

#include "mysql_connection.h"
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>

using namespace std;

namespace {

// [GROUP: Role Constants]
constexpr int kRoleUser = 1;
constexpr int kRoleDoctor = 2;
constexpr int kRoleAdmin = 3;

// [GROUP: Database Helpers]
void RollbackQuietly(sql::Connection* conn) {
    if (conn == nullptr) {
        return;
    }

    try {
        conn->rollback();
    } catch (...) {
    }
}

// [GROUP: Validation Helpers]
bool IsDigitsOnly(const string& value) {
    for (char ch : value) {
        if (ch < '0' || ch > '9') {
            return false;
        }
    }
    return !value.empty();
}

// [GROUP: Validation Helpers]
bool LooksLikeEmail(const string& value) {
    const size_t atPos = value.find('@');
    if (atPos == string::npos || atPos == 0 || atPos == value.size() - 1) {
        return false;
    }

    return value.find('.', atPos) != string::npos;
}

// [GROUP: Validation Helpers]
bool IsValidRoleValue(int role) {
    return role == kRoleUser || role == kRoleAdmin || role == kRoleDoctor;
}

// [GROUP: SQL Error Mapping]
bool IsDuplicateKeyError(const sql::SQLException& e) {
    return e.getErrorCode() == 1062;
}

// [GROUP: SQL Error Mapping]
string ToLowerCopy(string value) {
    transform(value.begin(), value.end(), value.begin(),
              [](unsigned char ch) { return static_cast<char>(tolower(ch)); });
    return value;
}

// [GROUP: SQL Error Mapping]
string BuildDuplicateValueMessage(const string& sqlMessage) {
    const string normalized = ToLowerCopy(sqlMessage);

    if (normalized.find("users.username") != string::npos ||
        normalized.find("username") != string::npos) {
        return "Username already exists";
    }

    return "Duplicate value violates a unique constraint";
}

// [GROUP: Schema Helpers]
bool EnsureIndexDropped(sql::Connection* conn,
                        const string& schemaName,
                        const string& tableName,
                        const string& indexName) {
    if (conn == nullptr) {
        return false;
    }

    try {
        unique_ptr<sql::PreparedStatement> checkStmt(conn->prepareStatement(
            "SELECT COUNT(*) AS total "
            "FROM information_schema.statistics "
            "WHERE table_schema = ? AND table_name = ? AND index_name = ?"));
        checkStmt->setString(1, schemaName);
        checkStmt->setString(2, tableName);
        checkStmt->setString(3, indexName);
        unique_ptr<sql::ResultSet> checkRes(checkStmt->executeQuery());
        if (!checkRes->next() || checkRes->getInt("total") == 0) {
            return true;
        }

        unique_ptr<sql::Statement> dropStmt(conn->createStatement());
        dropStmt->execute("ALTER TABLE " + tableName + " DROP INDEX " + indexName);
        return true;
    } catch (const sql::SQLException& e) {
        cerr << "[ERROR] Failed to drop index '" << indexName << "': " << e.what()
             << " (Code: " << e.getErrorCode()
             << ", State: " << e.getSQLState() << ")" << endl;
        return false;
    }
}

// [GROUP: Schema Helpers]
bool EnsureColumnDropped(sql::Connection* conn,
                         const string& schemaName,
                         const string& tableName,
                         const string& columnName) {
    if (conn == nullptr) {
        return false;
    }

    try {
        unique_ptr<sql::PreparedStatement> checkStmt(conn->prepareStatement(
            "SELECT COUNT(*) AS total "
            "FROM information_schema.columns "
            "WHERE table_schema = ? AND table_name = ? AND column_name = ?"));
        checkStmt->setString(1, schemaName);
        checkStmt->setString(2, tableName);
        checkStmt->setString(3, columnName);
        unique_ptr<sql::ResultSet> checkRes(checkStmt->executeQuery());
        if (!checkRes->next() || checkRes->getInt("total") == 0) {
            return true;
        }

        unique_ptr<sql::Statement> dropStmt(conn->createStatement());
        dropStmt->execute("ALTER TABLE " + tableName + " DROP COLUMN " + columnName);
        return true;
    } catch (const sql::SQLException& e) {
        cerr << "[ERROR] Failed to drop column '" << columnName << "': " << e.what()
             << " (Code: " << e.getErrorCode()
             << ", State: " << e.getSQLState() << ")" << endl;
        return false;
    }
}

// [GROUP: Secure Memory Helpers]
void SecureWipeBuffer(void* buffer, size_t length) {
    if (buffer == nullptr || length == 0) {
        return;
    }
#ifdef _WIN32
    SecureZeroMemory(buffer, length);
#else
    volatile unsigned char* ptr = static_cast<volatile unsigned char*>(buffer);
    while (length-- > 0) {
        *ptr++ = 0;
    }
#endif
}

// [GROUP: Secure Memory Helpers]
void SecureWipeStringInPlace(string& value) {
    if (!value.empty()) {
        SecureWipeBuffer(value.data(), value.size());
    }
    value.clear();
}

// [GROUP: Secure Memory Helpers]
void SecureWipeBytes(vector<uint8_t>& value) {
    if (!value.empty()) {
        SecureWipeBuffer(value.data(), value.size());
    }
    value.clear();
}

// [GROUP: Cipher Decoding Helpers]
bool IsHexCipherText(const string& value) {
    if (value.empty() || (value.size() % 16) != 0) {
        return false;
    }

    for (const unsigned char ch : value) {
        if (!isxdigit(ch)) {
            return false;
        }
    }

    return true;
}

// [GROUP: Cipher Decoding Helpers]
string DecodeCipherForBlowfish(const string& storedValue) {
    string decoded;
    if (Base64::Decode(storedValue, decoded) && IsHexCipherText(decoded)) {
        return decoded;
    }

    return storedValue;
}

// [GROUP: Cipher Decoding Helpers]
string DecryptMedicalFieldWithDoctorKek(const string& storedValue, const string& doctorKek) {
    if (storedValue.empty() || doctorKek.empty()) {
        return storedValue;
    }

    try {
        const string cipherHex = DecodeCipherForBlowfish(storedValue);
        if (!IsHexCipherText(cipherHex)) {
            return storedValue;
        }

        Blowfish cipher(doctorKek);
        return cipher.DecryptString(cipherHex);
    } catch (...) {
        return storedValue;
    }
}

// [GROUP: DER Encoding Helpers]
void AppendDerLength(vector<uint8_t>& output, size_t length) {
    if (length < 0x80) {
        output.push_back(static_cast<uint8_t>(length));
        return;
    }

    vector<uint8_t> encoded;
    size_t current = length;
    while (current > 0) {
        encoded.push_back(static_cast<uint8_t>(current & 0xFF));
        current >>= 8;
    }

    output.push_back(static_cast<uint8_t>(0x80 | encoded.size()));
    for (auto it = encoded.rbegin(); it != encoded.rend(); ++it) {
        output.push_back(*it);
    }
}

// [GROUP: DER Encoding Helpers]
void AppendDerInteger(vector<uint8_t>& output, const uint8_t* data, size_t length) {
    size_t start = 0;
    while (start + 1 < length && data[start] == 0) {
        ++start;
    }

    vector<uint8_t> value;
    if (length == 0) {
        value.push_back(0);
    } else {
        value.assign(data + start, data + length);
    }

    if (value.empty()) {
        value.push_back(0);
    }

    if ((value.front() & 0x80) != 0) {
        value.insert(value.begin(), 0);
    }

    output.push_back(0x02);
    AppendDerLength(output, value.size());
    output.insert(output.end(), value.begin(), value.end());
}

// [GROUP: DER Encoding Helpers]
vector<uint8_t> WrapAsDerSequence(const vector<uint8_t>& content) {
    vector<uint8_t> sequence;
    sequence.push_back(0x30);
    AppendDerLength(sequence, content.size());
    sequence.insert(sequence.end(), content.begin(), content.end());
    return sequence;
}

// [GROUP: PEM Helpers]
string EncodePem(const string& label, const vector<uint8_t>& derBytes) {
    const string binary(reinterpret_cast<const char*>(derBytes.data()), derBytes.size());
    const string base64 = Base64::Encode(binary);

    string pem;
    pem += "-----BEGIN " + label + "-----\n";
    for (size_t index = 0; index < base64.size(); index += 64) {
        pem += base64.substr(index, 64);
        pem += "\n";
    }
    pem += "-----END " + label + "-----\n";
    return pem;
}

// [GROUP: DER Parsing Helpers]
bool ReadDerLength(const vector<uint8_t>& der, size_t& offset, size_t& length) {
    if (offset >= der.size()) {
        return false;
    }

    const uint8_t first = der[offset++];
    if ((first & 0x80) == 0) {
        length = first;
        return (offset + length) <= der.size();
    }

    const size_t byteCount = first & 0x7F;
    if (byteCount == 0 || byteCount > sizeof(size_t) || offset + byteCount > der.size()) {
        return false;
    }

    length = 0;
    for (size_t i = 0; i < byteCount; ++i) {
        length = (length << 8) | der[offset++];
    }
    return (offset + length) <= der.size();
}

// [GROUP: DER Parsing Helpers]
bool ReadDerInteger(const vector<uint8_t>& der, size_t& offset, vector<uint8_t>& integerBytes) {
    integerBytes.clear();
    if (offset >= der.size() || der[offset++] != 0x02) {
        return false;
    }

    size_t integerLength = 0;
    if (!ReadDerLength(der, offset, integerLength)) {
        return false;
    }

    integerBytes.assign(der.data() + offset, der.data() + offset + integerLength);
    offset += integerLength;
    return true;
}

// [GROUP: PEM Helpers]
bool DecodePemBlock(const string& pem,
                    const string& label,
                    vector<uint8_t>& derBytes,
                    string& error) {
    derBytes.clear();
    const string beginMarker = "-----BEGIN " + label + "-----";
    const string endMarker = "-----END " + label + "-----";

    const size_t beginPos = pem.find(beginMarker);
    if (beginPos == string::npos) {
        error = "PEM begin marker not found";
        return false;
    }

    const size_t contentStart = beginPos + beginMarker.size();
    const size_t endPos = pem.find(endMarker, contentStart);
    if (endPos == string::npos) {
        error = "PEM end marker not found";
        return false;
    }

    string base64Body;
    base64Body.reserve(endPos - contentStart);
    for (size_t i = contentStart; i < endPos; ++i) {
        const unsigned char ch = static_cast<unsigned char>(pem[i]);
        if (!isspace(ch)) {
            base64Body.push_back(static_cast<char>(ch));
        }
    }

    string derBinary;
    if (!Base64::Decode(base64Body, derBinary) || derBinary.empty()) {
        error = "Failed to decode PEM base64 body";
        return false;
    }

    derBytes.assign(derBinary.begin(), derBinary.end());
    return true;
}

// [GROUP: RSA Conversion Helpers]
bool ConvertPublicKeyPemToBlob(const string& publicKeyPem,
                               vector<uint8_t>& publicBlob,
                               string& error) {
#ifdef _WIN32
    publicBlob.clear();

    vector<uint8_t> derBytes;
    if (!DecodePemBlock(publicKeyPem, "RSA PUBLIC KEY", derBytes, error)) {
        return false;
    }

    size_t offset = 0;
    if (offset >= derBytes.size() || derBytes[offset++] != 0x30) {
        error = "Invalid RSA public key DER sequence";
        return false;
    }

    size_t sequenceLength = 0;
    if (!ReadDerLength(derBytes, offset, sequenceLength) || (offset + sequenceLength) != derBytes.size()) {
        error = "Invalid RSA public key DER length";
        return false;
    }

    vector<uint8_t> modulus;
    vector<uint8_t> publicExponent;
    if (!ReadDerInteger(derBytes, offset, modulus) || !ReadDerInteger(derBytes, offset, publicExponent)) {
        error = "Invalid RSA public key DER integer fields";
        return false;
    }

    while (modulus.size() > 1 && modulus.front() == 0) {
        modulus.erase(modulus.begin());
    }
    while (publicExponent.size() > 1 && publicExponent.front() == 0) {
        publicExponent.erase(publicExponent.begin());
    }

    if (modulus.empty() || publicExponent.empty()) {
        error = "RSA public key modulus/exponent is empty";
        return false;
    }

    // BCrypt expects BCRYPT_RSAKEY_BLOB = header + exponent + modulus.
    BCRYPT_RSAKEY_BLOB header{};
    header.Magic = BCRYPT_RSAPUBLIC_MAGIC;
    header.BitLength = static_cast<ULONG>(modulus.size() * 8);
    header.cbPublicExp = static_cast<ULONG>(publicExponent.size());
    header.cbModulus = static_cast<ULONG>(modulus.size());
    header.cbPrime1 = 0;
    header.cbPrime2 = 0;

    publicBlob.resize(sizeof(BCRYPT_RSAKEY_BLOB) + publicExponent.size() + modulus.size());
    memcpy(publicBlob.data(), &header, sizeof(BCRYPT_RSAKEY_BLOB));

    size_t blobOffset = sizeof(BCRYPT_RSAKEY_BLOB);
    memcpy(publicBlob.data() + blobOffset, publicExponent.data(), publicExponent.size());
    blobOffset += publicExponent.size();
    memcpy(publicBlob.data() + blobOffset, modulus.data(), modulus.size());
    return true;
#else
    (void)publicKeyPem;
    (void)publicBlob;
    error = "RSA public key conversion is currently implemented for Windows builds only";
    return false;
#endif
}

// [GROUP: RSA Conversion Helpers]
bool ConvertPrivateKeyPemToBlob(const string& privateKeyPem,
                                vector<uint8_t>& privateBlob,
                                string& error) {
#ifdef _WIN32
    privateBlob.clear();

    vector<uint8_t> derBytes;
    if (!DecodePemBlock(privateKeyPem, "RSA PRIVATE KEY", derBytes, error)) {
        return false;
    }

    size_t offset = 0;
    if (offset >= derBytes.size() || derBytes[offset++] != 0x30) {
        error = "Invalid RSA private key DER sequence";
        return false;
    }

    size_t sequenceLength = 0;
    if (!ReadDerLength(derBytes, offset, sequenceLength) || (offset + sequenceLength) != derBytes.size()) {
        error = "Invalid RSA private key DER length";
        return false;
    }

    vector<uint8_t> version;
    vector<uint8_t> modulus;
    vector<uint8_t> publicExponent;
    vector<uint8_t> privateExponent;
    vector<uint8_t> prime1;
    vector<uint8_t> prime2;
    vector<uint8_t> exponent1;
    vector<uint8_t> exponent2;
    vector<uint8_t> coefficient;

    if (!ReadDerInteger(derBytes, offset, version) ||
        !ReadDerInteger(derBytes, offset, modulus) ||
        !ReadDerInteger(derBytes, offset, publicExponent) ||
        !ReadDerInteger(derBytes, offset, privateExponent) ||
        !ReadDerInteger(derBytes, offset, prime1) ||
        !ReadDerInteger(derBytes, offset, prime2) ||
        !ReadDerInteger(derBytes, offset, exponent1) ||
        !ReadDerInteger(derBytes, offset, exponent2) ||
        !ReadDerInteger(derBytes, offset, coefficient)) {
        error = "Invalid RSA private key DER integer fields";
        return false;
    }

    if (version.empty() || version.back() != 0) {
        error = "Unsupported RSA private key version";
        return false;
    }

    auto trimLeadingZero = [](vector<uint8_t>& value) {
        while (value.size() > 1 && value.front() == 0) {
            value.erase(value.begin());
        }
    };

    trimLeadingZero(modulus);
    trimLeadingZero(publicExponent);
    trimLeadingZero(privateExponent);
    trimLeadingZero(prime1);
    trimLeadingZero(prime2);
    trimLeadingZero(exponent1);
    trimLeadingZero(exponent2);
    trimLeadingZero(coefficient);

    if (modulus.empty() || publicExponent.empty() || privateExponent.empty() || prime1.empty() || prime2.empty()) {
        error = "RSA private key components are incomplete";
        return false;
    }

    BCRYPT_RSAKEY_BLOB header{};
    header.Magic = BCRYPT_RSAFULLPRIVATE_MAGIC;
    header.BitLength = static_cast<ULONG>(modulus.size() * 8);
    header.cbPublicExp = static_cast<ULONG>(publicExponent.size());
    header.cbModulus = static_cast<ULONG>(modulus.size());
    header.cbPrime1 = static_cast<ULONG>(prime1.size());
    header.cbPrime2 = static_cast<ULONG>(prime2.size());

    const size_t totalSize =
        sizeof(BCRYPT_RSAKEY_BLOB) +
        publicExponent.size() +
        modulus.size() +
        prime1.size() +
        prime2.size() +
        exponent1.size() +
        exponent2.size() +
        coefficient.size() +
        privateExponent.size();

    privateBlob.resize(totalSize);
    memcpy(privateBlob.data(), &header, sizeof(BCRYPT_RSAKEY_BLOB));

    size_t blobOffset = sizeof(BCRYPT_RSAKEY_BLOB);
    auto copyPart = [&](const vector<uint8_t>& part) {
        memcpy(privateBlob.data() + blobOffset, part.data(), part.size());
        blobOffset += part.size();
    };

    // Order must follow BCRYPT_RSAFULLPRIVATE_BLOB documentation exactly.
    copyPart(publicExponent);
    copyPart(modulus);
    copyPart(prime1);
    copyPart(prime2);
    copyPart(exponent1);
    copyPart(exponent2);
    copyPart(coefficient);
    copyPart(privateExponent);

    return true;
#else
    (void)privateKeyPem;
    (void)privateBlob;
    error = "RSA private key conversion is currently implemented for Windows builds only";
    return false;
#endif
}

// [GROUP: RSA Encryption Helpers]
bool RsaEncryptWithPublicKeyPem(const string& publicKeyPem,
                                const string& plaintext,
                                string& encryptedBase64,
                                string& error) {
#ifdef _WIN32
    encryptedBase64.clear();
    error.clear();

    vector<uint8_t> publicBlob;
    if (!ConvertPublicKeyPemToBlob(publicKeyPem, publicBlob, error)) {
        return false;
    }

    vector<uint8_t> plaintextBytes(plaintext.begin(), plaintext.end());
    if (plaintextBytes.empty()) {
        error = "RSA plaintext must not be empty";
        return false;
    }

    BCRYPT_ALG_HANDLE algorithmHandle = nullptr;
    BCRYPT_KEY_HANDLE keyHandle = nullptr;

    auto cleanup = [&]() {
        if (keyHandle != nullptr) {
            BCryptDestroyKey(keyHandle);
            keyHandle = nullptr;
        }
        if (algorithmHandle != nullptr) {
            BCryptCloseAlgorithmProvider(algorithmHandle, 0);
            algorithmHandle = nullptr;
        }
        SecureWipeBytes(publicBlob);
        SecureWipeBytes(plaintextBytes);
    };

    NTSTATUS status = BCryptOpenAlgorithmProvider(&algorithmHandle, BCRYPT_RSA_ALGORITHM, nullptr, 0);
    if (status < 0) {
        error = "Failed to open BCrypt RSA provider for encryption";
        cleanup();
        return false;
    }

    status = BCryptImportKeyPair(
        algorithmHandle,
        nullptr,
        BCRYPT_RSAPUBLIC_BLOB,
        &keyHandle,
        publicBlob.data(),
        static_cast<ULONG>(publicBlob.size()),
        0);
    if (status < 0) {
        error = "Failed to import RSA public key";
        cleanup();
        return false;
    }

    BCRYPT_OAEP_PADDING_INFO oaepInfo{};
    oaepInfo.pszAlgId = BCRYPT_SHA256_ALGORITHM;

    ULONG encryptedSize = 0;
    status = BCryptEncrypt(
        keyHandle,
        plaintextBytes.data(),
        static_cast<ULONG>(plaintextBytes.size()),
        &oaepInfo,
        nullptr,
        0,
        nullptr,
        0,
        &encryptedSize,
        BCRYPT_PAD_OAEP);
    if (status < 0 || encryptedSize == 0) {
        error = "Failed to calculate RSA encrypted size";
        cleanup();
        return false;
    }

    vector<uint8_t> encryptedBytes(encryptedSize);
    status = BCryptEncrypt(
        keyHandle,
        plaintextBytes.data(),
        static_cast<ULONG>(plaintextBytes.size()),
        &oaepInfo,
        nullptr,
        0,
        encryptedBytes.data(),
        static_cast<ULONG>(encryptedBytes.size()),
        &encryptedSize,
        BCRYPT_PAD_OAEP);
    if (status < 0 || encryptedSize == 0) {
        SecureWipeBytes(encryptedBytes);
        error = "RSA encryption failed";
        cleanup();
        return false;
    }

    encryptedBase64 = Base64::Encode(
        string(reinterpret_cast<const char*>(encryptedBytes.data()), encryptedSize));
    SecureWipeBytes(encryptedBytes);
    cleanup();
    return true;
#else
    (void)publicKeyPem;
    (void)plaintext;
    (void)encryptedBase64;
    error = "RSA encryption is currently implemented for Windows builds only";
    return false;
#endif
}

// [GROUP: RSA Encryption Helpers]
bool RsaDecryptWithPrivateBlobBase64(const string& privateKeyBlobBase64,
                                     const string& encryptedBase64,
                                     string& plaintext,
                                     string& error) {
#ifdef _WIN32
    plaintext.clear();
    error.clear();

    vector<uint8_t> privateBlob;
    string privateBlobBinary;
    // Accept both the current binary-blob format and legacy PEM payloads.
    if (Base64::Decode(privateKeyBlobBase64, privateBlobBinary) && !privateBlobBinary.empty()) {
        privateBlob.assign(privateBlobBinary.begin(), privateBlobBinary.end());
        SecureWipeStringInPlace(privateBlobBinary);
    } else {
        string convertError;
        if (!ConvertPrivateKeyPemToBlob(privateKeyBlobBase64, privateBlob, convertError)) {
            error = "Failed to decode encrypted private key payload: " + convertError;
            return false;
        }
    }

    string encryptedBinary;
    if (!Base64::Decode(encryptedBase64, encryptedBinary) || encryptedBinary.empty()) {
        SecureWipeBytes(privateBlob);
        error = "Failed to decode wrapped medical DEK";
        return false;
    }
    vector<uint8_t> encryptedBytes(encryptedBinary.begin(), encryptedBinary.end());
    SecureWipeStringInPlace(encryptedBinary);

    BCRYPT_ALG_HANDLE algorithmHandle = nullptr;
    BCRYPT_KEY_HANDLE keyHandle = nullptr;

    auto cleanup = [&]() {
        if (keyHandle != nullptr) {
            BCryptDestroyKey(keyHandle);
            keyHandle = nullptr;
        }
        if (algorithmHandle != nullptr) {
            BCryptCloseAlgorithmProvider(algorithmHandle, 0);
            algorithmHandle = nullptr;
        }
        SecureWipeBytes(privateBlob);
        SecureWipeBytes(encryptedBytes);
    };

    NTSTATUS status = BCryptOpenAlgorithmProvider(&algorithmHandle, BCRYPT_RSA_ALGORITHM, nullptr, 0);
    if (status < 0) {
        error = "Failed to open BCrypt RSA provider for decryption";
        cleanup();
        return false;
    }

    status = BCryptImportKeyPair(
        algorithmHandle,
        nullptr,
        BCRYPT_RSAFULLPRIVATE_BLOB,
        &keyHandle,
        privateBlob.data(),
        static_cast<ULONG>(privateBlob.size()),
        0);
    if (status < 0) {
        status = BCryptImportKeyPair(
            algorithmHandle,
            nullptr,
            BCRYPT_RSAPRIVATE_BLOB,
            &keyHandle,
            privateBlob.data(),
            static_cast<ULONG>(privateBlob.size()),
            0);
    }
    if (status < 0) {
        error = "Failed to import RSA private key";
        cleanup();
        return false;
    }

    BCRYPT_OAEP_PADDING_INFO oaepInfo{};
    oaepInfo.pszAlgId = BCRYPT_SHA256_ALGORITHM;

    ULONG plaintextSize = 0;
    status = BCryptDecrypt(
        keyHandle,
        encryptedBytes.data(),
        static_cast<ULONG>(encryptedBytes.size()),
        &oaepInfo,
        nullptr,
        0,
        nullptr,
        0,
        &plaintextSize,
        BCRYPT_PAD_OAEP);
    if (status < 0 || plaintextSize == 0) {
        error = "Failed to calculate RSA decrypted size";
        cleanup();
        return false;
    }

    vector<uint8_t> plaintextBytes(plaintextSize);
    status = BCryptDecrypt(
        keyHandle,
        encryptedBytes.data(),
        static_cast<ULONG>(encryptedBytes.size()),
        &oaepInfo,
        nullptr,
        0,
        plaintextBytes.data(),
        static_cast<ULONG>(plaintextBytes.size()),
        &plaintextSize,
        BCRYPT_PAD_OAEP);
    if (status < 0 || plaintextSize == 0) {
        SecureWipeBytes(plaintextBytes);
        error = "RSA decryption failed";
        cleanup();
        return false;
    }

    plaintext.assign(reinterpret_cast<const char*>(plaintextBytes.data()), plaintextSize);
    SecureWipeBytes(plaintextBytes);
    cleanup();
    return true;
#else
    (void)privateKeyBlobBase64;
    (void)encryptedBase64;
    (void)plaintext;
    error = "RSA decryption is currently implemented for Windows builds only";
    return false;
#endif
}

// [GROUP: RSA Key Generation Helpers]
bool GenerateRsaKeyPairMaterial(string& publicKeyPem, string& privateKeyBlobBase64, string& error) {
#ifdef _WIN32
    publicKeyPem.clear();
    privateKeyBlobBase64.clear();
    error.clear();

    BCRYPT_ALG_HANDLE algorithmHandle = nullptr;
    BCRYPT_KEY_HANDLE keyHandle = nullptr;

    auto cleanup = [&]() {
        if (keyHandle != nullptr) {
            BCryptDestroyKey(keyHandle);
            keyHandle = nullptr;
        }
        if (algorithmHandle != nullptr) {
            BCryptCloseAlgorithmProvider(algorithmHandle, 0);
            algorithmHandle = nullptr;
        }
    };

    const NTSTATUS openStatus = BCryptOpenAlgorithmProvider(&algorithmHandle, BCRYPT_RSA_ALGORITHM, nullptr, 0);
    if (openStatus < 0) {
        error = "Failed to open BCrypt RSA provider";
        cleanup();
        return false;
    }

    // Project baseline: RSA-2048 for compatibility and acceptable performance.
    const NTSTATUS generateStatus = BCryptGenerateKeyPair(algorithmHandle, &keyHandle, 2048, 0);
    if (generateStatus < 0) {
        error = "Failed to generate RSA key pair";
        cleanup();
        return false;
    }

    const NTSTATUS finalizeStatus = BCryptFinalizeKeyPair(keyHandle, 0);
    if (finalizeStatus < 0) {
        error = "Failed to finalize RSA key pair";
        cleanup();
        return false;
    }

    DWORD publicBlobSize = 0;
    NTSTATUS exportStatus = BCryptExportKey(
        keyHandle,
        nullptr,
        BCRYPT_RSAPUBLIC_BLOB,
        nullptr,
        0,
        &publicBlobSize,
        0);
    if (exportStatus < 0 || publicBlobSize <= sizeof(BCRYPT_RSAKEY_BLOB)) {
        error = "Failed to query RSA public blob size";
        cleanup();
        return false;
    }

    vector<uint8_t> publicBlob(publicBlobSize);
    exportStatus = BCryptExportKey(
        keyHandle,
        nullptr,
        BCRYPT_RSAPUBLIC_BLOB,
        publicBlob.data(),
        static_cast<ULONG>(publicBlob.size()),
        &publicBlobSize,
        0);
    if (exportStatus < 0) {
        error = "Failed to export RSA public blob";
        cleanup();
        return false;
    }

    DWORD privateBlobSize = 0;
    exportStatus = BCryptExportKey(
        keyHandle,
        nullptr,
        BCRYPT_RSAFULLPRIVATE_BLOB,
        nullptr,
        0,
        &privateBlobSize,
        0);
    if (exportStatus < 0 || privateBlobSize <= sizeof(BCRYPT_RSAKEY_BLOB)) {
        error = "Failed to query RSA private blob size";
        cleanup();
        return false;
    }

    vector<uint8_t> privateBlob(privateBlobSize);
    exportStatus = BCryptExportKey(
        keyHandle,
        nullptr,
        BCRYPT_RSAFULLPRIVATE_BLOB,
        privateBlob.data(),
        static_cast<ULONG>(privateBlob.size()),
        &privateBlobSize,
        0);
    if (exportStatus < 0) {
        error = "Failed to export RSA private blob";
        cleanup();
        return false;
    }

    const auto* publicHeader = reinterpret_cast<const BCRYPT_RSAKEY_BLOB*>(publicBlob.data());
    if (publicHeader->Magic != BCRYPT_RSAPUBLIC_MAGIC) {
        error = "Invalid RSA public blob magic";
        cleanup();
        return false;
    }

    size_t offset = sizeof(BCRYPT_RSAKEY_BLOB);
    auto readPublicPart = [&](size_t bytes, const uint8_t*& outPart) -> bool {
        if (offset + bytes > publicBlob.size()) {
            return false;
        }
        outPart = publicBlob.data() + offset;
        offset += bytes;
        return true;
    };

    const uint8_t* publicExponent = nullptr;
    const uint8_t* modulus = nullptr;
    if (!readPublicPart(publicHeader->cbPublicExp, publicExponent) ||
        !readPublicPart(publicHeader->cbModulus, modulus)) {
        error = "RSA public blob layout is invalid";
        cleanup();
        return false;
    }

    vector<uint8_t> publicContent;
    AppendDerInteger(publicContent, modulus, publicHeader->cbModulus);
    AppendDerInteger(publicContent, publicExponent, publicHeader->cbPublicExp);
    const vector<uint8_t> publicDer = WrapAsDerSequence(publicContent);

    publicKeyPem = EncodePem("RSA PUBLIC KEY", publicDer);
    privateKeyBlobBase64 = Base64::Encode(
        string(reinterpret_cast<const char*>(privateBlob.data()), privateBlobSize));

    SecureWipeBytes(publicBlob);
    SecureWipeBytes(privateBlob);

    cleanup();
    return true;
#else
    (void)publicKeyPem;
    (void)privateKeyBlobBase64;
    error = "RSA key generation is currently implemented for Windows builds only";
    return false;
#endif
}

}  // namespace

// [GROUP: Lifecycle]
DatabaseHelper::DatabaseHelper(const string& host, int port,
                               const string& user, const string& password,
                               const string& database)
    : HOST(host),
      PORT(port),
      USER(user),
      PASSWORD(password),
      DATABASE(database),
      connection(nullptr) {
}

// [GROUP: Lifecycle]
DatabaseHelper::~DatabaseHelper() {
    Disconnect();
}

// [GROUP: Connection And Schema]
bool DatabaseHelper::Connect() {
    try {
        sql::Driver* driver = get_driver_instance();
        connection = driver->connect("tcp://" + HOST + ":" + to_string(PORT), USER, PASSWORD);

        sql::Connection* conn = static_cast<sql::Connection*>(connection);
        conn->setSchema(DATABASE);
        conn->setAutoCommit(true);

        cout << "[DATABASE] Connected to MySQL " << HOST << ":" << PORT
             << " [DB: " << DATABASE << "]" << endl;
        return true;
    } catch (sql::SQLException& e) {
        cerr << "[ERROR] MySQL connection failed: " << e.what()
             << " (Code: " << e.getErrorCode()
             << ", State: " << e.getSQLState() << ")" << endl;
        return false;
    }
}

// [GROUP: Lifecycle]
bool DatabaseHelper::IsConnected() {
    return connection != nullptr;
}

// [GROUP: Lifecycle]
void DatabaseHelper::Disconnect() {
    if (connection != nullptr) {
        sql::Connection* conn = static_cast<sql::Connection*>(connection);
        delete conn;
        connection = nullptr;
        cout << "[DATABASE] Disconnected" << endl;
    }
}

// [GROUP: Internal Helpers]
bool DatabaseHelper::ExecuteQuery(const string& query) {
    if (!IsConnected()) {
        cerr << "[ERROR] Database is not connected" << endl;
        return false;
    }

    try {
        sql::Connection* conn = static_cast<sql::Connection*>(connection);
        unique_ptr<sql::Statement> stmt(conn->createStatement());
        stmt->execute(query);
        return true;
    } catch (sql::SQLException& e) {
        cerr << "[ERROR] Query failed: " << e.what()
             << " (Code: " << e.getErrorCode()
             << ", State: " << e.getSQLState() << ")" << endl;
        return false;
    }
}

// [GROUP: Internal Crypto Helpers]
string DatabaseHelper::GenerateRandomDek() const {
    const vector<uint8_t> randomBytes = SecureRandom::RandomBytes(32);

    stringstream stream;
    stream << hex << setfill('0');
    for (uint8_t byte : randomBytes) {
        stream << setw(2) << static_cast<unsigned int>(byte);
    }
    return stream.str();
}

// [GROUP: Internal Crypto Helpers]
string DatabaseHelper::WrapDek(const string& dekPlaintext, const string& kek) const {
    Blowfish cipher(kek);
    return Base64::Encode(cipher.EncryptString(dekPlaintext));
}
string DatabaseHelper::UnwrapDek(const string& encryptedDek, const string& kek) const {
    Blowfish cipher(kek);
    const string dekCipherHex = DecodeCipherForBlowfish(encryptedDek);
    return cipher.DecryptString(dekCipherHex);
}

// [GROUP: Internal Crypto Helpers]
void DatabaseHelper::EncryptPersonalData(const string& dekPlaintext,
                                         const string& cccdPlaintext,
                                         const string& phonePlaintext,
                                         const string& emailPlaintext,
                                         string& cccdCiphertext,
                                         string& phoneCiphertext,
                                         string& emailCiphertext) const {
    Blowfish cipher(dekPlaintext);
    cccdCiphertext = Base64::Encode(cipher.EncryptString(cccdPlaintext));
    phoneCiphertext = Base64::Encode(cipher.EncryptString(phonePlaintext));
    emailCiphertext = Base64::Encode(cipher.EncryptString(emailPlaintext));
}

// [GROUP: Internal Crypto Helpers]
void DatabaseHelper::DecryptPersonalData(const string& dekPlaintext,
                                         const string& cccdCiphertext,
                                         const string& phoneCiphertext,
                                         const string& emailCiphertext,
                                         string& cccdPlaintext,
                                         string& phonePlaintext,
                                         string& emailPlaintext) const {
    Blowfish cipher(dekPlaintext);

    const string cccdCipherHex = DecodeCipherForBlowfish(cccdCiphertext);
    const string phoneCipherHex = DecodeCipherForBlowfish(phoneCiphertext);
    const string emailCipherHex = DecodeCipherForBlowfish(emailCiphertext);

    cccdPlaintext = cipher.DecryptString(cccdCipherHex);
    phonePlaintext = cipher.DecryptString(phoneCipherHex);
    emailPlaintext = cipher.DecryptString(emailCipherHex);
}

// [GROUP: Connection And Schema]
bool DatabaseHelper::InitializeSchema() {
    const string createUsersTable =
        "CREATE TABLE IF NOT EXISTS users ("
        "id INT AUTO_INCREMENT PRIMARY KEY, "
        "username VARCHAR(255) NOT NULL UNIQUE, "
        "name VARCHAR(255) NULL, "
        "password_hash VARCHAR(255) NOT NULL, "
        "role TINYINT NOT NULL DEFAULT 1, "
        "private_key_cipher TEXT NULL, "
        "public_key TEXT NULL, "
        "INDEX idx_users_username (username)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;";

    const string createPersonalRecordsTable =
        "CREATE TABLE IF NOT EXISTS personal_records ("
        "id INT AUTO_INCREMENT PRIMARY KEY, "
        "user_id INT NOT NULL, "
        "Gender TINYINT UNSIGNED NOT NULL, "
        "encrypted_dek VARCHAR(255) NOT NULL, "
        "CCCD_cipher TEXT NOT NULL, "
        "SDT_cipher TEXT NOT NULL, "
        "Email_cipher TEXT NOT NULL, "
        "UNIQUE KEY uk_personal_records_user_id (user_id), "
        "INDEX idx_personal_records_user_id (user_id), "
        "CONSTRAINT fk_personal_records_users FOREIGN KEY (user_id) "
        "REFERENCES users(id) ON DELETE CASCADE"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;";

    const string createMedicalRecordsTable =
        "CREATE TABLE IF NOT EXISTS medical_records ("
        "id INT AUTO_INCREMENT PRIMARY KEY, "
        "patient_id INT NOT NULL, "
        "doctor_id INT NOT NULL, "
        "visit_date DATE NOT NULL, "
        "department VARCHAR(100) NULL, "
        "patient_encrypted_dek TEXT NOT NULL, "
        "doctor_encrypted_dek TEXT NOT NULL, "
        "diagnosis_cipher TEXT NOT NULL, "
        "prescription_cipher TEXT NOT NULL, "
        "INDEX idx_medical_records_patient_id (patient_id), "
        "INDEX idx_medical_records_doctor_id (doctor_id), "
        "INDEX idx_medical_records_visit_date (visit_date), "
        "CONSTRAINT fk_medical_records_patient FOREIGN KEY (patient_id) "
        "REFERENCES users(id) ON DELETE CASCADE, "
        "CONSTRAINT fk_medical_records_doctor FOREIGN KEY (doctor_id) "
        "REFERENCES users(id) ON DELETE CASCADE"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;";

    const string migrateCipherColumns =
        "ALTER TABLE personal_records "
        "MODIFY COLUMN CCCD_cipher TEXT NOT NULL, "
        "MODIFY COLUMN SDT_cipher TEXT NOT NULL, "
        "MODIFY COLUMN Email_cipher TEXT NOT NULL;";

    const string migrateMedicalDekColumns =
        "ALTER TABLE medical_records "
        "MODIFY COLUMN patient_encrypted_dek TEXT NOT NULL, "
        "MODIFY COLUMN doctor_encrypted_dek TEXT NOT NULL;";

    const string addRoleColumnIfMissing =
        "ALTER TABLE users "
        "ADD COLUMN IF NOT EXISTS role TINYINT NOT NULL DEFAULT 1;";

    const string addNameColumnIfMissing =
        "ALTER TABLE users "
        "ADD COLUMN IF NOT EXISTS name VARCHAR(255) NULL AFTER username;";

    const string addPrivateKeyCipherColumnIfMissing =
        "ALTER TABLE users "
        "ADD COLUMN IF NOT EXISTS private_key_cipher TEXT NULL AFTER role;";

    const string addPublicKeyColumnIfMissing =
        "ALTER TABLE users "
        "ADD COLUMN IF NOT EXISTS public_key TEXT NULL AFTER private_key_cipher;";

    const string normalizeRoleColumn =
        "ALTER TABLE users "
        "MODIFY COLUMN role TINYINT NOT NULL DEFAULT 1;";

    const string fixInvalidRoleValues =
        "UPDATE users SET role = 1 WHERE role NOT IN (1, 2, 3);";

    // Keep startup idempotent: create-if-missing + migrate-if-needed in one pass.
    if (!ExecuteQuery(createUsersTable) ||
        !ExecuteQuery(createPersonalRecordsTable) ||
        !ExecuteQuery(createMedicalRecordsTable) ||
        !ExecuteQuery(migrateCipherColumns) ||
        !ExecuteQuery(migrateMedicalDekColumns) ||
        !ExecuteQuery(addRoleColumnIfMissing) ||
        !ExecuteQuery(addNameColumnIfMissing) ||
        !ExecuteQuery(addPrivateKeyCipherColumnIfMissing) ||
        !ExecuteQuery(addPublicKeyColumnIfMissing) ||
        !ExecuteQuery(normalizeRoleColumn) ||
        !ExecuteQuery(fixInvalidRoleValues)) {
        return false;
    }

    sql::Connection* conn = static_cast<sql::Connection*>(connection);
    if (!EnsureIndexDropped(conn, DATABASE, "personal_records", "uk_personal_records_cccd_hash") ||
        !EnsureIndexDropped(conn, DATABASE, "personal_records", "uk_personal_records_sdt_hash") ||
        !EnsureIndexDropped(conn, DATABASE, "personal_records", "uk_personal_records_email_hash") ||
        !EnsureColumnDropped(conn, DATABASE, "personal_records", "CCCD_hash") ||
        !EnsureColumnDropped(conn, DATABASE, "personal_records", "SDT_hash") ||
        !EnsureColumnDropped(conn, DATABASE, "personal_records", "Email_hash")) {
        return false;
    }

    return BootstrapDefaultUser();
}

// [GROUP: Connection And Schema]
bool DatabaseHelper::BootstrapDefaultUser() {
    if (!IsConnected()) {
        return false;
    }

    try {
        sql::Connection* conn = static_cast<sql::Connection*>(connection);
        unique_ptr<sql::Statement> stmt(conn->createStatement());
        unique_ptr<sql::ResultSet> res(stmt->executeQuery("SELECT COUNT(*) AS total FROM users;"));
        int totalUsers = 0;
        if (res->next()) {
            totalUsers = res->getInt("total");
        }

        if (totalUsers > 0) {
            return true;
        }

        cout << "[DATABASE] No users found. Bootstrapping default account 'admin'." << endl;
        return RegisterUser(
            "admin",
            "System Administrator",
            "admin12345",
            1,
            "012345678912",
            "0123456789",
            "admin@example.com",
            kRoleAdmin);
    } catch (const exception& e) {
        cerr << "[ERROR] Bootstrap failed: " << e.what() << endl;
        return false;
    }
}

// [GROUP: User Profile Operations]
bool DatabaseHelper::RegisterUser(const string& username,
                                  const string& name,
                                  const string& passwordPlaintext,
                                  int gender,
                                  const string& cccdPlaintext,
                                  const string& phonePlaintext,
                                  const string& emailPlaintext,
                                  int role) {
    if (!IsConnected()) {
        cerr << "[ERROR] Database is not connected" << endl;
        return false;
    }

    if (username.empty()) {
        cerr << "[ERROR] Username must not be empty" << endl;
        return false;
    }

    if (name.empty()) {
        cerr << "[ERROR] Name must not be empty" << endl;
        return false;
    }

    if (passwordPlaintext.size() < 8) {
        cerr << "[ERROR] Password must be at least 8 characters" << endl;
        return false;
    }

    if (gender != 1 && gender != 2) {
        cerr << "[ERROR] Gender must be 1 or 2" << endl;
        return false;
    }

    if (!IsValidRoleValue(role)) {
        cerr << "[ERROR] Role must be 1 (user), 2 (doctor), or 3 (admin)" << endl;
        return false;
    }

    if (cccdPlaintext.size() != 12 || !IsDigitsOnly(cccdPlaintext)) {
        cerr << "[ERROR] CCCD must be exactly 12 digits" << endl;
        return false;
    }

    if (phonePlaintext.size() < 10 || !IsDigitsOnly(phonePlaintext)) {
        cerr << "[ERROR] Phone must contain at least 10 digits" << endl;
        return false;
    }

    if (!LooksLikeEmail(emailPlaintext)) {
        cerr << "[ERROR] Email format is invalid" << endl;
        return false;
    }

    sql::Connection* conn = static_cast<sql::Connection*>(connection);

    try {
        // 1) Build password hash and KEK used to protect user-owned key material.
        const string passwordHash = PasswordHasher::HashPassword(passwordPlaintext);
        const string kek = PasswordHasher::DeriveKeyEncryptionKey(passwordPlaintext, passwordHash);
        string publicKeyPem;
        string privateKeyBlobBase64;
        string rsaError;
        if (!GenerateRsaKeyPairMaterial(publicKeyPem, privateKeyBlobBase64, rsaError)) {
            throw runtime_error("RSA key generation failed: " + rsaError);
        }

        const string privateKeyCipher = WrapDek(privateKeyBlobBase64, kek);
        SecureWipeStringInPlace(privateKeyBlobBase64);

        // 2) Personal PII is encrypted by a per-user DEK, and that DEK is wrapped by KEK.
        const string dekPlaintext = GenerateRandomDek();
        const string encryptedDek = WrapDek(dekPlaintext, kek);

        string cccdCiphertext;
        string phoneCiphertext;
        string emailCiphertext;
        EncryptPersonalData(dekPlaintext, cccdPlaintext, phonePlaintext, emailPlaintext,
                            cccdCiphertext, phoneCiphertext, emailCiphertext);

        conn->setAutoCommit(false);

        unique_ptr<sql::PreparedStatement> insertUser(conn->prepareStatement(
            "INSERT INTO users (username, name, password_hash, role, private_key_cipher, public_key) VALUES (?, ?, ?, ?, ?, ?)"));
        insertUser->setString(1, username);
        insertUser->setString(2, name);
        insertUser->setString(3, passwordHash);
        insertUser->setInt(4, role);
        insertUser->setString(5, privateKeyCipher);
        insertUser->setString(6, publicKeyPem);
        insertUser->execute();

        unique_ptr<sql::Statement> identityStmt(conn->createStatement());
        unique_ptr<sql::ResultSet> identityRes(identityStmt->executeQuery("SELECT LAST_INSERT_ID() AS id"));
        if (!identityRes->next()) {
            throw runtime_error("Failed to obtain inserted user id");
        }

        const int userId = identityRes->getInt("id");
        unique_ptr<sql::PreparedStatement> insertRecord(conn->prepareStatement(
            "INSERT INTO personal_records "
            "(user_id, Gender, encrypted_dek, CCCD_cipher, SDT_cipher, Email_cipher) "
            "VALUES (?, ?, ?, ?, ?, ?)"));
        insertRecord->setInt(1, userId);
        insertRecord->setInt(2, gender);
        insertRecord->setString(3, encryptedDek);
        insertRecord->setString(4, cccdCiphertext);
        insertRecord->setString(5, phoneCiphertext);
        insertRecord->setString(6, emailCiphertext);
        insertRecord->execute();

        conn->commit();
        conn->setAutoCommit(true);

        cout << "[DATABASE] Registered user: " << username << endl;
        return true;
    } catch (sql::SQLException& e) {
        RollbackQuietly(conn);
        conn->setAutoCommit(true);

        if (IsDuplicateKeyError(e)) {
            cerr << "[ERROR] RegisterUser failed: " << BuildDuplicateValueMessage(e.what()) << endl;
        } else {
            cerr << "[ERROR] RegisterUser failed: " << e.what()
                 << " (Code: " << e.getErrorCode()
                 << ", State: " << e.getSQLState() << ")" << endl;
        }
        return false;
    } catch (const exception& e) {
        RollbackQuietly(conn);
        conn->setAutoCommit(true);
        cerr << "[ERROR] RegisterUser failed: " << e.what() << endl;
        return false;
    }
}

bool DatabaseHelper::GetPersonalRecordForUser(int userId,
                                              const string& sessionKek,
                                              PersonalRecord& record) {
    record = PersonalRecord{};

    if (!IsConnected()) {
        cerr << "[ERROR] Database is not connected" << endl;
        return false;
    }

    try {
        sql::Connection* conn = static_cast<sql::Connection*>(connection);
        unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
            "SELECT u.id AS user_id, u.username, COALESCE(u.name, '') AS name, u.role, pr.id AS record_id, pr.Gender, "
            "pr.encrypted_dek, pr.CCCD_cipher, pr.SDT_cipher, pr.Email_cipher "
            "FROM users u "
            "INNER JOIN personal_records pr ON pr.user_id = u.id "
            "WHERE u.id = ?"));
        pstmt->setInt(1, userId);
        unique_ptr<sql::ResultSet> res(pstmt->executeQuery());

        if (!res->next()) {
            return false;
        }

        const string encryptedDek = res->getString("encrypted_dek");
        const string dekPlaintext = UnwrapDek(encryptedDek, sessionKek);

        record.userId = res->getInt("user_id");
        record.recordId = res->getInt("record_id");
        record.username = res->getString("username");
        record.name = res->getString("name");
        record.role = res->getInt("role");
        record.gender = res->getInt("Gender");
        record.encryptedDek = encryptedDek;
        DecryptPersonalData(
            dekPlaintext,
            res->getString("CCCD_cipher"),
            res->getString("SDT_cipher"),
            res->getString("Email_cipher"),
            record.cccd,
            record.phone,
            record.email);

        return true;
    } catch (const exception& e) {
        cerr << "[ERROR] GetPersonalRecordForUser failed: " << e.what() << endl;
        return false;
    }
}

bool DatabaseHelper::UpdatePersonalRecordForUser(int userId,
                                                 const string& currentSessionKek,
                                                 const string& newUsername,
                                                 const string& newName,
                                                 const string& newPasswordPlaintext,
                                                 bool updatePassword,
                                                 int newGender,
                                                 const string& newCccdPlaintext,
                                                 const string& newPhonePlaintext,
                                                 const string& newEmailPlaintext,
                                                 string& updatedSessionKek) {
    updatedSessionKek.clear();

    if (!IsConnected()) {
        cerr << "[ERROR] Database is not connected" << endl;
        return false;
    }

    if (newUsername.empty()) {
        cerr << "[ERROR] Username must not be empty" << endl;
        return false;
    }

    if (newName.empty()) {
        cerr << "[ERROR] Name must not be empty" << endl;
        return false;
    }

    if (newGender != 1 && newGender != 2) {
        cerr << "[ERROR] Gender must be 1 or 2" << endl;
        return false;
    }

    if (newCccdPlaintext.size() != 12 || !IsDigitsOnly(newCccdPlaintext)) {
        cerr << "[ERROR] CCCD must be exactly 12 digits" << endl;
        return false;
    }

    if (newPhonePlaintext.size() < 10 || !IsDigitsOnly(newPhonePlaintext)) {
        cerr << "[ERROR] Phone must contain at least 10 digits" << endl;
        return false;
    }

    if (!LooksLikeEmail(newEmailPlaintext)) {
        cerr << "[ERROR] Email format is invalid" << endl;
        return false;
    }

    if (updatePassword && newPasswordPlaintext.size() < 8) {
        cerr << "[ERROR] New password must be at least 8 characters" << endl;
        return false;
    }

    sql::Connection* conn = static_cast<sql::Connection*>(connection);

    try {
        unique_ptr<sql::PreparedStatement> selectCurrent(conn->prepareStatement(
            "SELECT u.username, u.private_key_cipher, pr.id AS record_id, pr.encrypted_dek "
            "FROM users u "
            "INNER JOIN personal_records pr ON pr.user_id = u.id "
            "WHERE u.id = ?"));
        selectCurrent->setInt(1, userId);
        unique_ptr<sql::ResultSet> current(selectCurrent->executeQuery());
        if (!current->next()) {
            return false;
        }

        const int recordId = current->getInt("record_id");
        const string oldUsername = current->getString("username");
        const string currentPrivateKeyCipher = current->getString("private_key_cipher");
        const string currentEncryptedDek = current->getString("encrypted_dek");
        const string dekPlaintext = UnwrapDek(currentEncryptedDek, currentSessionKek);

        if (!updatePassword && newUsername != oldUsername) {
            throw runtime_error("Username change requires current password re-entry");
        }

        string finalSessionKek = currentSessionKek;
        string newPasswordHash;
        if (updatePassword) {
            newPasswordHash = PasswordHasher::HashPassword(newPasswordPlaintext);
            finalSessionKek = PasswordHasher::DeriveKeyEncryptionKey(newPasswordPlaintext, newPasswordHash);
        }

        string encryptedDek = currentEncryptedDek;
        string privateKeyCipher = currentPrivateKeyCipher;
        if (finalSessionKek != currentSessionKek || newUsername != oldUsername) {
            // Rewrap personal DEK when KEK context changes.
            encryptedDek = WrapDek(dekPlaintext, finalSessionKek);
        }

        if (finalSessionKek != currentSessionKek) {
            // Keep private RSA key encrypted at rest under the latest KEK.
            string privateKeyBlobBase64 = UnwrapDek(currentPrivateKeyCipher, currentSessionKek);
            privateKeyCipher = WrapDek(privateKeyBlobBase64, finalSessionKek);
            SecureWipeStringInPlace(privateKeyBlobBase64);
        }

        string cccdCiphertext;
        string phoneCiphertext;
        string emailCiphertext;
        EncryptPersonalData(dekPlaintext, newCccdPlaintext, newPhonePlaintext, newEmailPlaintext,
                            cccdCiphertext, phoneCiphertext, emailCiphertext);

        conn->setAutoCommit(false);

        if (updatePassword) {
            unique_ptr<sql::PreparedStatement> updateUser(conn->prepareStatement(
                "UPDATE users SET username = ?, name = ?, password_hash = ?, private_key_cipher = ? WHERE id = ?"));
            updateUser->setString(1, newUsername);
            updateUser->setString(2, newName);
            updateUser->setString(3, newPasswordHash);
            updateUser->setString(4, privateKeyCipher);
            updateUser->setInt(5, userId);
            updateUser->execute();
        } else {
            unique_ptr<sql::PreparedStatement> updateUser(conn->prepareStatement(
                "UPDATE users SET username = ?, name = ? WHERE id = ?"));
            updateUser->setString(1, newUsername);
            updateUser->setString(2, newName);
            updateUser->setInt(3, userId);
            updateUser->execute();
        }

        unique_ptr<sql::PreparedStatement> updateRecord(conn->prepareStatement(
            "UPDATE personal_records "
            "SET Gender = ?, encrypted_dek = ?, CCCD_cipher = ?, SDT_cipher = ?, Email_cipher = ? "
            "WHERE id = ?"));
        updateRecord->setInt(1, newGender);
        updateRecord->setString(2, encryptedDek);
        updateRecord->setString(3, cccdCiphertext);
        updateRecord->setString(4, phoneCiphertext);
        updateRecord->setString(5, emailCiphertext);
        updateRecord->setInt(6, recordId);
        updateRecord->execute();

        conn->commit();
        conn->setAutoCommit(true);

        updatedSessionKek = finalSessionKek;
        return true;
    } catch (sql::SQLException& e) {
        RollbackQuietly(conn);
        conn->setAutoCommit(true);
        updatedSessionKek.clear();

        if (IsDuplicateKeyError(e)) {
            cerr << "[ERROR] UpdatePersonalRecordForUser failed: " << BuildDuplicateValueMessage(e.what()) << endl;
        } else {
            cerr << "[ERROR] UpdatePersonalRecordForUser failed: " << e.what()
                 << " (Code: " << e.getErrorCode()
                 << ", State: " << e.getSQLState() << ")" << endl;
        }
        return false;
    } catch (const exception& e) {
        RollbackQuietly(conn);
        conn->setAutoCommit(true);
        updatedSessionKek.clear();
        cerr << "[ERROR] UpdatePersonalRecordForUser failed: " << e.what() << endl;
        return false;
    }
}

bool DatabaseHelper::DeleteUserById(int userId) {
    if (!IsConnected()) {
        cerr << "[ERROR] Database is not connected" << endl;
        return false;
    }

    try {
        sql::Connection* conn = static_cast<sql::Connection*>(connection);
        unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
            "DELETE FROM users WHERE id = ?"));
        pstmt->setInt(1, userId);
        const int affectedRows = pstmt->executeUpdate();
        return affectedRows > 0;
    } catch (const exception& e) {
        cerr << "[ERROR] DeleteUserById failed: " << e.what() << endl;
        return false;
    }
}

bool DatabaseHelper::UserExistsById(int userId) {
    if (!IsConnected()) {
        cerr << "[ERROR] Database is not connected" << endl;
        return false;
    }

    if (userId <= 0) {
        return false;
    }

    try {
        sql::Connection* conn = static_cast<sql::Connection*>(connection);
        unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
            "SELECT 1 FROM users WHERE id = ? LIMIT 1"));
        pstmt->setInt(1, userId);
        unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        return res->next();
    } catch (const exception& e) {
        cerr << "[ERROR] UserExistsById failed: " << e.what() << endl;
        return false;
    }
}

bool DatabaseHelper::UpdateUserRoleById(int userId, int newRole) {
    if (!IsConnected()) {
        cerr << "[ERROR] Database is not connected" << endl;
        return false;
    }

    if (userId <= 0) {
        cerr << "[ERROR] User id must be positive" << endl;
        return false;
    }

    if (!IsValidRoleValue(newRole)) {
        cerr << "[ERROR] Role must be 1 (user), 2 (doctor), or 3 (admin)" << endl;
        return false;
    }

    try {
        sql::Connection* conn = static_cast<sql::Connection*>(connection);
        unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
            "UPDATE users SET role = ? WHERE id = ?"));
        pstmt->setInt(1, newRole);
        pstmt->setInt(2, userId);
        const int affectedRows = pstmt->executeUpdate();
        return affectedRows > 0;
    } catch (const exception& e) {
        cerr << "[ERROR] UpdateUserRoleById failed: " << e.what() << endl;
        return false;
    }
}

bool DatabaseHelper::CreateMedicalRecord(int patientId,
                                         int doctorId,
                                         const string& visitDate,
                                         const string& department,
                                         const string& diagnosisPlaintext,
                                         const string& prescriptionPlaintext) {
    if (!IsConnected()) {
        cerr << "[ERROR] Database is not connected" << endl;
        return false;
    }

    if (patientId <= 0 || doctorId <= 0) {
        cerr << "[ERROR] Patient id and doctor id must be positive" << endl;
        return false;
    }

    if (visitDate.empty()) {
        cerr << "[ERROR] Visit date is required" << endl;
        return false;
    }

    if (diagnosisPlaintext.empty() || prescriptionPlaintext.empty()) {
        cerr << "[ERROR] Diagnosis and prescription are required" << endl;
        return false;
    }

    try {
        sql::Connection* conn = static_cast<sql::Connection*>(connection);

        unique_ptr<sql::PreparedStatement> patientStmt(conn->prepareStatement(
            "SELECT role, public_key FROM users WHERE id = ?"));
        patientStmt->setInt(1, patientId);
        unique_ptr<sql::ResultSet> patientRes(patientStmt->executeQuery());
        if (!patientRes->next()) {
            cerr << "[ERROR] Patient account does not exist" << endl;
            return false;
        }

        const int patientRole = patientRes->getInt("role");
        if (patientRole != kRoleUser) {
            cerr << "[ERROR] Target patient must have user role" << endl;
            return false;
        }

        const string patientPublicKey = patientRes->getString("public_key");
        if (patientPublicKey.empty()) {
            cerr << "[ERROR] Patient public key is missing" << endl;
            return false;
        }

        unique_ptr<sql::PreparedStatement> doctorStmt(conn->prepareStatement(
            "SELECT role, public_key FROM users WHERE id = ?"));
        doctorStmt->setInt(1, doctorId);
        unique_ptr<sql::ResultSet> doctorRes(doctorStmt->executeQuery());
        if (!doctorRes->next()) {
            cerr << "[ERROR] Doctor account does not exist" << endl;
            return false;
        }

        const int doctorRole = doctorRes->getInt("role");
        if (doctorRole != kRoleDoctor) {
            cerr << "[ERROR] Creator must have doctor role" << endl;
            return false;
        }

        const string doctorPublicKey = doctorRes->getString("public_key");
        if (doctorPublicKey.empty()) {
            cerr << "[ERROR] Doctor public key is missing" << endl;
            return false;
        }

        // Envelope encryption for record payload:
        // one random DEK encrypts diagnosis/prescription, then DEK is RSA-wrapped per recipient.
        string medicalDek = GenerateRandomDek();

        string doctorEncryptedDek;
        string patientEncryptedDek;
        string rsaError;
        if (!RsaEncryptWithPublicKeyPem(doctorPublicKey, medicalDek, doctorEncryptedDek, rsaError)) {
            cerr << "[ERROR] Failed to wrap DEK for doctor: " << rsaError << endl;
            SecureWipeStringInPlace(medicalDek);
            return false;
        }

        if (!RsaEncryptWithPublicKeyPem(patientPublicKey, medicalDek, patientEncryptedDek, rsaError)) {
            cerr << "[ERROR] Failed to wrap DEK for patient: " << rsaError << endl;
            SecureWipeStringInPlace(medicalDek);
            return false;
        }

        Blowfish cipher(medicalDek);
        const string diagnosisCipher = Base64::Encode(cipher.EncryptString(diagnosisPlaintext));
        const string prescriptionCipher = Base64::Encode(cipher.EncryptString(prescriptionPlaintext));
        SecureWipeStringInPlace(medicalDek);

        unique_ptr<sql::PreparedStatement> insertStmt(conn->prepareStatement(
            "INSERT INTO medical_records "
            "(patient_id, doctor_id, visit_date, department, patient_encrypted_dek, doctor_encrypted_dek, diagnosis_cipher, prescription_cipher) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));
        insertStmt->setInt(1, patientId);
        insertStmt->setInt(2, doctorId);
        insertStmt->setString(3, visitDate);
        insertStmt->setString(4, department);
        insertStmt->setString(5, patientEncryptedDek);
        insertStmt->setString(6, doctorEncryptedDek);
        insertStmt->setString(7, diagnosisCipher);
        insertStmt->setString(8, prescriptionCipher);
        const int affectedRows = insertStmt->executeUpdate();
        return affectedRows > 0;
    } catch (const exception& e) {
        cerr << "[ERROR] CreateMedicalRecord failed: " << e.what() << endl;
        return false;
    }
}

int DatabaseHelper::GetTotalUsers() {
    if (!IsConnected()) {
        return 0;
    }

    try {
        sql::Connection* conn = static_cast<sql::Connection*>(connection);
        unique_ptr<sql::Statement> stmt(conn->createStatement());
        unique_ptr<sql::ResultSet> res(stmt->executeQuery("SELECT COUNT(*) AS total FROM users"));
        if (res->next()) {
            return res->getInt("total");
        }
    } catch (const exception& e) {
        cerr << "[ERROR] GetTotalUsers failed: " << e.what() << endl;
    }

    return 0;
}

int DatabaseHelper::GetTotalMedicalRecords() {
    if (!IsConnected()) {
        return 0;
    }

    try {
        sql::Connection* conn = static_cast<sql::Connection*>(connection);
        unique_ptr<sql::Statement> stmt(conn->createStatement());
        unique_ptr<sql::ResultSet> res(stmt->executeQuery("SELECT COUNT(*) AS total FROM medical_records"));
        if (res->next()) {
            return res->getInt("total");
        }
    } catch (const exception& e) {
        cerr << "[ERROR] GetTotalMedicalRecords failed: " << e.what() << endl;
    }

    return 0;
}

int DatabaseHelper::GetTotalMedicalRecordsForDoctor(int doctorId) {
    if (!IsConnected()) {
        return 0;
    }

    if (doctorId <= 0) {
        return 0;
    }

    try {
        sql::Connection* conn = static_cast<sql::Connection*>(connection);
        unique_ptr<sql::PreparedStatement> stmt(conn->prepareStatement(
            "SELECT COUNT(*) AS total FROM medical_records WHERE doctor_id = ?"));
        stmt->setInt(1, doctorId);
        unique_ptr<sql::ResultSet> res(stmt->executeQuery());
        if (res->next()) {
            return res->getInt("total");
        }
    } catch (const exception& e) {
        cerr << "[ERROR] GetTotalMedicalRecordsForDoctor failed: " << e.what() << endl;
    }

    return 0;
}

int DatabaseHelper::GetTotalMedicalRecordsForPatient(int patientId) {
    if (!IsConnected()) {
        return 0;
    }

    if (patientId <= 0) {
        return 0;
    }

    try {
        sql::Connection* conn = static_cast<sql::Connection*>(connection);
        unique_ptr<sql::PreparedStatement> stmt(conn->prepareStatement(
            "SELECT COUNT(*) AS total FROM medical_records WHERE patient_id = ?"));
        stmt->setInt(1, patientId);
        unique_ptr<sql::ResultSet> res(stmt->executeQuery());
        if (res->next()) {
            return res->getInt("total");
        }
    } catch (const exception& e) {
        cerr << "[ERROR] GetTotalMedicalRecordsForPatient failed: " << e.what() << endl;
    }

    return 0;
}

bool DatabaseHelper::GetEncryptedUserRecordByOffset(int offset, PersonalRecord& record) {
    record = PersonalRecord{};

    if (!IsConnected()) {
        cerr << "[ERROR] Database is not connected" << endl;
        return false;
    }

    if (offset < 0) {
        return false;
    }

    try {
        sql::Connection* conn = static_cast<sql::Connection*>(connection);
        unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
            "SELECT u.id AS user_id, u.username, u.role, pr.id AS record_id, pr.Gender, "
            "pr.encrypted_dek, pr.CCCD_cipher, pr.SDT_cipher, pr.Email_cipher "
            "FROM users u "
            "INNER JOIN personal_records pr ON pr.user_id = u.id "
            "ORDER BY u.id ASC "
            "LIMIT 1 OFFSET ?"));
        pstmt->setInt(1, offset);
        unique_ptr<sql::ResultSet> res(pstmt->executeQuery());

        if (!res->next()) {
            return false;
        }

        record.userId = res->getInt("user_id");
        record.recordId = res->getInt("record_id");
        record.username = res->getString("username");
        record.role = res->getInt("role");
        record.gender = res->getInt("Gender");
        record.encryptedDek = res->getString("encrypted_dek");
        record.cccd = res->getString("CCCD_cipher");
        record.phone = res->getString("SDT_cipher");
        record.email = res->getString("Email_cipher");
        return true;
    } catch (const exception& e) {
        cerr << "[ERROR] GetEncryptedUserRecordByOffset failed: " << e.what() << endl;
        return false;
    }
}

bool DatabaseHelper::GetMedicalRecordSummaryByOffset(int offset, MedicalRecordSummary& record) {
    record = MedicalRecordSummary{};

    if (!IsConnected()) {
        cerr << "[ERROR] Database is not connected" << endl;
        return false;
    }

    if (offset < 0) {
        return false;
    }

    try {
        sql::Connection* conn = static_cast<sql::Connection*>(connection);
        unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
            "SELECT mr.id, mr.patient_id, mr.doctor_id, DATE_FORMAT(mr.visit_date, '%Y-%m-%d') AS visit_date, "
            "COALESCE(mr.department, '') AS department, "
            "COALESCE(NULLIF(u.name, ''), u.username) AS patient_name, "
            "COALESCE(NULLIF(d.name, ''), d.username) AS doctor_name, "
            "mr.diagnosis_cipher, mr.prescription_cipher "
            "FROM medical_records mr "
            "INNER JOIN users u ON u.id = mr.patient_id "
            "INNER JOIN users d ON d.id = mr.doctor_id "
            "ORDER BY mr.id ASC "
            "LIMIT 1 OFFSET ?"));
        pstmt->setInt(1, offset);
        unique_ptr<sql::ResultSet> res(pstmt->executeQuery());

        if (!res->next()) {
            return false;
        }

        record.recordId = res->getInt("id");
        record.patientId = res->getInt("patient_id");
        record.patientName = res->getString("patient_name");
        record.doctorId = res->getInt("doctor_id");
        record.doctorName = res->getString("doctor_name");
        record.visitDate = res->getString("visit_date");
        record.department = res->getString("department");
        record.diagnosisCipher = res->getString("diagnosis_cipher");
        record.prescriptionCipher = res->getString("prescription_cipher");
        return true;
    } catch (const exception& e) {
        cerr << "[ERROR] GetMedicalRecordSummaryByOffset failed: " << e.what() << endl;
        return false;
    }
}

bool DatabaseHelper::GetMedicalRecordSummaryByDoctorOffset(int doctorId, int offset, MedicalRecordSummary& record) {
    record = MedicalRecordSummary{};

    if (!IsConnected()) {
        cerr << "[ERROR] Database is not connected" << endl;
        return false;
    }

    if (doctorId <= 0 || offset < 0) {
        return false;
    }

    try {
        sql::Connection* conn = static_cast<sql::Connection*>(connection);
        unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
            "SELECT mr.id, mr.patient_id, mr.doctor_id, DATE_FORMAT(mr.visit_date, '%Y-%m-%d') AS visit_date, "
            "COALESCE(mr.department, '') AS department, "
            "COALESCE(NULLIF(u.name, ''), u.username) AS patient_name, "
            "COALESCE(NULLIF(d.name, ''), d.username) AS doctor_name, "
            "mr.diagnosis_cipher, mr.prescription_cipher "
            "FROM medical_records mr "
            "INNER JOIN users u ON u.id = mr.patient_id "
            "INNER JOIN users d ON d.id = mr.doctor_id "
            "WHERE mr.doctor_id = ? "
            "ORDER BY mr.id ASC "
            "LIMIT 1 OFFSET ?"));
        pstmt->setInt(1, doctorId);
        pstmt->setInt(2, offset);
        unique_ptr<sql::ResultSet> res(pstmt->executeQuery());

        if (!res->next()) {
            return false;
        }

        record.recordId = res->getInt("id");
        record.patientId = res->getInt("patient_id");
        record.patientName = res->getString("patient_name");
        record.doctorId = res->getInt("doctor_id");
        record.doctorName = res->getString("doctor_name");
        record.visitDate = res->getString("visit_date");
        record.department = res->getString("department");
        record.diagnosisCipher = res->getString("diagnosis_cipher");
        record.prescriptionCipher = res->getString("prescription_cipher");
        return true;
    } catch (const exception& e) {
        cerr << "[ERROR] GetMedicalRecordSummaryByDoctorOffset failed: " << e.what() << endl;
        return false;
    }
}

bool DatabaseHelper::GetMedicalRecordSummaryByPatientOffset(int patientId, int offset, MedicalRecordSummary& record) {
    record = MedicalRecordSummary{};

    if (!IsConnected()) {
        cerr << "[ERROR] Database is not connected" << endl;
        return false;
    }

    if (patientId <= 0 || offset < 0) {
        return false;
    }

    try {
        sql::Connection* conn = static_cast<sql::Connection*>(connection);
        unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
            "SELECT mr.id, mr.patient_id, mr.doctor_id, DATE_FORMAT(mr.visit_date, '%Y-%m-%d') AS visit_date, "
            "COALESCE(mr.department, '') AS department, "
            "COALESCE(NULLIF(u.name, ''), u.username) AS patient_name, "
            "COALESCE(NULLIF(d.name, ''), d.username) AS doctor_name, "
            "mr.diagnosis_cipher, mr.prescription_cipher "
            "FROM medical_records mr "
            "INNER JOIN users u ON u.id = mr.patient_id "
            "INNER JOIN users d ON d.id = mr.doctor_id "
            "WHERE mr.patient_id = ? "
            "ORDER BY mr.id ASC "
            "LIMIT 1 OFFSET ?"));
        pstmt->setInt(1, patientId);
        pstmt->setInt(2, offset);
        unique_ptr<sql::ResultSet> res(pstmt->executeQuery());

        if (!res->next()) {
            return false;
        }

        record.recordId = res->getInt("id");
        record.patientId = res->getInt("patient_id");
        record.patientName = res->getString("patient_name");
        record.doctorId = res->getInt("doctor_id");
        record.doctorName = res->getString("doctor_name");
        record.visitDate = res->getString("visit_date");
        record.department = res->getString("department");
        record.diagnosisCipher = res->getString("diagnosis_cipher");
        record.prescriptionCipher = res->getString("prescription_cipher");
        return true;
    } catch (const exception& e) {
        cerr << "[ERROR] GetMedicalRecordSummaryByPatientOffset failed: " << e.what() << endl;
        return false;
    }
}

bool DatabaseHelper::GetMedicalRecordDetailForDoctor(int doctorId,
                                                     int recordId,
                                                     const string& doctorKek,
                                                     MedicalRecordSummary& record) {
    record = MedicalRecordSummary{};

    if (!IsConnected()) {
        cerr << "[ERROR] Database is not connected" << endl;
        return false;
    }

    if (doctorId <= 0 || recordId <= 0 || doctorKek.empty()) {
        return false;
    }

    try {
        sql::Connection* conn = static_cast<sql::Connection*>(connection);
        unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
            "SELECT mr.id, mr.patient_id, mr.doctor_id, DATE_FORMAT(mr.visit_date, '%Y-%m-%d') AS visit_date, "
            "COALESCE(mr.department, '') AS department, "
            "COALESCE(NULLIF(u.name, ''), u.username) AS patient_name, "
            "COALESCE(NULLIF(ud.name, ''), ud.username) AS doctor_name, "
            "mr.doctor_encrypted_dek, mr.diagnosis_cipher, mr.prescription_cipher, "
            "ud.private_key_cipher AS private_key_cipher "
            "FROM medical_records mr "
            "INNER JOIN users u ON u.id = mr.patient_id "
            "INNER JOIN users ud ON ud.id = mr.doctor_id "
            "WHERE mr.id = ? AND mr.doctor_id = ?"));
        pstmt->setInt(1, recordId);
        pstmt->setInt(2, doctorId);
        unique_ptr<sql::ResultSet> res(pstmt->executeQuery());

        if (!res->next()) {
            return false;
        }

        // Unwrap chain: KEK -> private RSA key -> medical DEK -> medical fields.
        string privateKeyBlobBase64 = UnwrapDek(res->getString("private_key_cipher"), doctorKek);
        string medicalDek;
        string rsaError;
        if (!RsaDecryptWithPrivateBlobBase64(privateKeyBlobBase64,
                                             res->getString("doctor_encrypted_dek"),
                                             medicalDek,
                                             rsaError)) {
            SecureWipeStringInPlace(privateKeyBlobBase64);
            cerr << "[ERROR] Failed to unwrap doctor medical DEK: " << rsaError << endl;
            return false;
        }
        SecureWipeStringInPlace(privateKeyBlobBase64);

        record.recordId = res->getInt("id");
        record.patientId = res->getInt("patient_id");
        record.patientName = res->getString("patient_name");
        record.doctorId = res->getInt("doctor_id");
        record.doctorName = res->getString("doctor_name");
        record.visitDate = res->getString("visit_date");
        record.department = res->getString("department");
        record.diagnosisCipher = DecryptMedicalFieldWithDoctorKek(res->getString("diagnosis_cipher"), medicalDek);
        record.prescriptionCipher = DecryptMedicalFieldWithDoctorKek(res->getString("prescription_cipher"), medicalDek);
        SecureWipeStringInPlace(medicalDek);
        return true;
    } catch (const exception& e) {
        cerr << "[ERROR] GetMedicalRecordDetailForDoctor failed: " << e.what() << endl;
        return false;
    }
}

bool DatabaseHelper::GetMedicalRecordDetailForPatient(int patientId,
                                                      int recordId,
                                                      const string& patientKek,
                                                      MedicalRecordSummary& record) {
    record = MedicalRecordSummary{};

    if (!IsConnected()) {
        cerr << "[ERROR] Database is not connected" << endl;
        return false;
    }

    if (patientId <= 0 || recordId <= 0 || patientKek.empty()) {
        return false;
    }

    try {
        sql::Connection* conn = static_cast<sql::Connection*>(connection);
        unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
            "SELECT mr.id, mr.patient_id, mr.doctor_id, DATE_FORMAT(mr.visit_date, '%Y-%m-%d') AS visit_date, "
            "COALESCE(mr.department, '') AS department, "
            "COALESCE(NULLIF(u.name, ''), u.username) AS patient_name, "
            "COALESCE(NULLIF(d.name, ''), d.username) AS doctor_name, "
            "mr.patient_encrypted_dek, mr.diagnosis_cipher, mr.prescription_cipher, "
            "up.private_key_cipher AS private_key_cipher "
            "FROM medical_records mr "
            "INNER JOIN users u ON u.id = mr.patient_id "
            "INNER JOIN users d ON d.id = mr.doctor_id "
            "INNER JOIN users up ON up.id = mr.patient_id "
            "WHERE mr.id = ? AND mr.patient_id = ?"));
        pstmt->setInt(1, recordId);
        pstmt->setInt(2, patientId);
        unique_ptr<sql::ResultSet> res(pstmt->executeQuery());

        if (!res->next()) {
            return false;
        }

        // Unwrap chain: KEK -> private RSA key -> medical DEK -> medical fields.
        string privateKeyBlobBase64 = UnwrapDek(res->getString("private_key_cipher"), patientKek);
        string medicalDek;
        string rsaError;
        if (!RsaDecryptWithPrivateBlobBase64(privateKeyBlobBase64,
                                             res->getString("patient_encrypted_dek"),
                                             medicalDek,
                                             rsaError)) {
            SecureWipeStringInPlace(privateKeyBlobBase64);
            cerr << "[ERROR] Failed to unwrap patient medical DEK: " << rsaError << endl;
            return false;
        }
        SecureWipeStringInPlace(privateKeyBlobBase64);

        record.recordId = res->getInt("id");
        record.patientId = res->getInt("patient_id");
        record.patientName = res->getString("patient_name");
        record.doctorId = res->getInt("doctor_id");
        record.doctorName = res->getString("doctor_name");
        record.visitDate = res->getString("visit_date");
        record.department = res->getString("department");
        record.diagnosisCipher = DecryptMedicalFieldWithDoctorKek(res->getString("diagnosis_cipher"), medicalDek);
        record.prescriptionCipher = DecryptMedicalFieldWithDoctorKek(res->getString("prescription_cipher"), medicalDek);
        SecureWipeStringInPlace(medicalDek);
        return true;
    } catch (const exception& e) {
        cerr << "[ERROR] GetMedicalRecordDetailForPatient failed: " << e.what() << endl;
        return false;
    }
}

AuthenticatedUser DatabaseHelper::AuthenticateUser(const string& username,
                                                   const string& passwordPlaintext,
                                                   string& derivedSessionKek) {
    derivedSessionKek.clear();
    AuthenticatedUser result;

    if (!IsConnected()) {
        cerr << "[ERROR] Database is not connected" << endl;
        return result;
    }

    try {
        sql::Connection* conn = static_cast<sql::Connection*>(connection);
        unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
            "SELECT id, username, password_hash, role FROM users WHERE username = ?"));
        pstmt->setString(1, username);
        unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        if (!res->next()) {
            return result;
        }

        const string storedHash = res->getString("password_hash");
        if (!PasswordHasher::VerifyPassword(passwordPlaintext, storedHash)) {
            return result;
        }

        result.id = res->getInt("id");
        result.username = res->getString("username");
        result.role = res->getInt("role");
        derivedSessionKek = PasswordHasher::DeriveKeyEncryptionKey(passwordPlaintext, storedHash);
        return result;
    } catch (const exception& e) {
        cerr << "[ERROR] AuthenticateUser failed: " << e.what() << endl;
        derivedSessionKek.clear();
        return result;
    }
}
