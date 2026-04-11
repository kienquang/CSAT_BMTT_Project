#include "Base64.h"

#include <array>
#include <cstdint>

namespace {

constexpr char kEncodeTable[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

std::array<int, 256> BuildDecodeTable() {
    std::array<int, 256> table{};
    table.fill(-1);

    for (int i = 0; i < 64; ++i) {
        table[static_cast<unsigned char>(kEncodeTable[i])] = i;
    }

    return table;
}

}  // namespace

namespace Base64 {

std::string Encode(const std::string& input) {
    if (input.empty()) {
        return "";
    }

    std::string output;
    output.reserve(((input.size() + 2) / 3) * 4);

    for (size_t i = 0; i < input.size(); i += 3) {
        const size_t remaining = input.size() - i;
        const uint32_t b0 = static_cast<unsigned char>(input[i]);
        const uint32_t b1 = remaining > 1 ? static_cast<unsigned char>(input[i + 1]) : 0;
        const uint32_t b2 = remaining > 2 ? static_cast<unsigned char>(input[i + 2]) : 0;

        const uint32_t triple = (b0 << 16) | (b1 << 8) | b2;

        output.push_back(kEncodeTable[(triple >> 18) & 0x3F]);
        output.push_back(kEncodeTable[(triple >> 12) & 0x3F]);
        output.push_back(remaining > 1 ? kEncodeTable[(triple >> 6) & 0x3F] : '=');
        output.push_back(remaining > 2 ? kEncodeTable[triple & 0x3F] : '=');
    }

    return output;
}

bool Decode(const std::string& input, std::string& output) {
    output.clear();

    if (input.empty()) {
        return true;
    }

    if ((input.size() % 4) != 0) {
        return false;
    }

    static const std::array<int, 256> kDecodeTable = BuildDecodeTable();

    size_t paddingCount = 0;
    if (!input.empty() && input.back() == '=') {
        paddingCount = 1;
    }
    if (input.size() >= 2 && input[input.size() - 2] == '=') {
        paddingCount = 2;
    }

    output.reserve((input.size() / 4) * 3 - paddingCount);

    for (size_t i = 0; i < input.size(); i += 4) {
        int values[4] = {0, 0, 0, 0};

        for (int j = 0; j < 4; ++j) {
            const unsigned char ch = static_cast<unsigned char>(input[i + j]);
            if (ch == '=') {
                values[j] = -2;
                continue;
            }

            const int decoded = kDecodeTable[ch];
            if (decoded < 0) {
                return false;
            }

            values[j] = decoded;
        }

        if (values[0] < 0 || values[1] < 0) {
            return false;
        }

        if (values[2] == -2 && values[3] != -2) {
            return false;
        }

        if ((values[2] == -2 || values[3] == -2) && (i + 4 != input.size())) {
            return false;
        }

        const uint32_t triple =
            (static_cast<uint32_t>(values[0]) << 18) |
            (static_cast<uint32_t>(values[1]) << 12) |
            (static_cast<uint32_t>(values[2] > -1 ? values[2] : 0) << 6) |
            static_cast<uint32_t>(values[3] > -1 ? values[3] : 0);

        output.push_back(static_cast<char>((triple >> 16) & 0xFF));

        if (values[2] >= 0) {
            output.push_back(static_cast<char>((triple >> 8) & 0xFF));
        }

        if (values[3] >= 0) {
            output.push_back(static_cast<char>(triple & 0xFF));
        }
    }

    return true;
}

}  // namespace Base64