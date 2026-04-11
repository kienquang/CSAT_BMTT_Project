#ifndef SECURE_RANDOM_H
#define SECURE_RANDOM_H

#include <cstddef>
#include <cstdint>
#include <vector>

namespace SecureRandom {

bool Fill(uint8_t* buffer, size_t length);
std::vector<uint8_t> RandomBytes(size_t length);

}  // namespace SecureRandom

#endif  // SECURE_RANDOM_H