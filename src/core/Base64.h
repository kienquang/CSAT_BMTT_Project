#ifndef BASE64_H
#define BASE64_H

#include <string>

namespace Base64 {

// [GROUP: Public API]
std::string Encode(const std::string& input);
bool Decode(const std::string& input, std::string& output);

}  // namespace Base64

#endif  // BASE64_H