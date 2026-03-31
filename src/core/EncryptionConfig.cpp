#include "EncryptionConfig.h"

#include "EnvConfig.h"

#include <stdexcept>
#include <string>

namespace EncryptionConfig {

std::string GetBlowfishKey() {
    const std::string key = EnvConfig::GetString("APP_BLOWFISH_KEY");
    if (key.empty()) {
        throw std::runtime_error("Missing required APP_BLOWFISH_KEY in .env");
    }
    return key;
}

}  // namespace EncryptionConfig
