#include "SecureRandom.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#else
#include <fstream>
#endif

namespace SecureRandom {

// [GROUP: Public API]
bool Fill(uint8_t* buffer, size_t length) {
    if (length == 0) {
        return true;
    }

    if (buffer == nullptr) {
        return false;
    }

#ifdef _WIN32
    size_t offset = 0;
    const size_t maxChunk = static_cast<size_t>(std::numeric_limits<ULONG>::max());

    while (offset < length) {
        const size_t remaining = length - offset;
        const ULONG chunkSize = static_cast<ULONG>(std::min(remaining, maxChunk));

        const NTSTATUS status = BCryptGenRandom(
            nullptr,
            reinterpret_cast<PUCHAR>(buffer + offset),
            chunkSize,
            BCRYPT_USE_SYSTEM_PREFERRED_RNG);

        if (status < 0) {
            return false;
        }

        offset += chunkSize;
    }

    return true;
#else
    std::ifstream urandom("/dev/urandom", std::ios::in | std::ios::binary);
    if (!urandom) {
        return false;
    }

    urandom.read(reinterpret_cast<char*>(buffer), static_cast<std::streamsize>(length));
    return urandom.good();
#endif
}

// [GROUP: Public API]
std::vector<uint8_t> RandomBytes(size_t length) {
    std::vector<uint8_t> bytes(length);
    if (!Fill(bytes.data(), bytes.size())) {
        throw std::runtime_error("Failed to generate secure random bytes");
    }
    return bytes;
}

}  // namespace SecureRandom