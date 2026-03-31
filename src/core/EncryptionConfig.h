#ifndef ENCRYPTION_CONFIG_H
#define ENCRYPTION_CONFIG_H

#include <string>

namespace EncryptionConfig {
    std::string GetBlowfishKey();

    constexpr int ENCRYPTION_BUFFER_SIZE = 256;
}  // namespace EncryptionConfig

#endif  // ENCRYPTION_CONFIG_H
