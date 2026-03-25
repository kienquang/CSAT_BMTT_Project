#ifndef ENCRYPTION_CONFIG_H
#define ENCRYPTION_CONFIG_H

#include <string>

using namespace std;

// ======== ENCRYPTION CONFIGURATION ========
// Centralized configuration for all encryption keys and settings
// This file contains all sensitive configuration values used across the application

namespace EncryptionConfig {
    // ======== BLOWFISH CIPHER KEY ========
    // Key used for encrypting/decrypting sensitive data (CCCD, Phone, Password, Salary)
    // WARNING: This key should be stored securely and never hardcoded in production
    static const string BLOWFISH_KEY = "MatMaHoc@NIST2025";
    
    // Alias for documentation clarity
    static const string& DATABASE_ENCRYPTION_KEY = BLOWFISH_KEY;
    
    // ======== OTHER CONFIGURATION CONSTANTS ========
    // Can be extended with other security settings in the future
    static const int ENCRYPTION_BUFFER_SIZE = 256;
    
}  // namespace EncryptionConfig

#endif  // ENCRYPTION_CONFIG_H
