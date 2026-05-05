#ifndef MASKING_H
#define MASKING_H

#include <string>

namespace masking {

// [GROUP: Public API]
std::string masking_phone(const std::string& value);
std::string masking_cccd(const std::string& value);
std::string masking_email(const std::string& value);
std::string masking_to_three_star(const std::string& value);

}  // namespace masking

#endif  // MASKING_H
