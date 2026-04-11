#include "masking.h"

#include <algorithm>
#include <cctype>

namespace {

bool IsDigitsOnly(const std::string& value) {
    if (value.empty()) {
        return false;
    }

    for (char ch : value) {
        if (!std::isdigit(static_cast<unsigned char>(ch))) {
            return false;
        }
    }
    return true;
}

std::string MakeStars(size_t count) {
    return std::string(count, '*');
}

}  // namespace

namespace masking {

std::string masking_phone(const std::string& value) {
    // Encrypted values are not digit-only, so we mask by expected phone length.
    if (IsDigitsOnly(value)) {
        return MakeStars(value.size());
    }
    return MakeStars(10);
}

std::string masking_cccd(const std::string& value) {
    // Encrypted values are not digit-only, so we mask by expected CCCD length.
    if (IsDigitsOnly(value)) {
        return MakeStars(value.size());
    }
    return MakeStars(12);
}

std::string masking_email(const std::string& value) {
    (void)value;
    return "******@*****.com";
}

std::string masking_to_three_star(const std::string& value) {
    (void)value;
    return "***";
}

}  // namespace masking
