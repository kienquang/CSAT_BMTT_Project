#include "MaskingLogic.h"

using namespace std;

void MaskingLogic::MaskPhone(char* buffer, size_t bufferSize) {
    if (buffer == nullptr) {
        cerr << "[ERROR] MaskPhone: Buffer null pointer" << endl;
        return;
    }

    size_t len = strlen(buffer);

    // Điều kiện: SĐT hợp lệ phải có ít nhất 10 ký tự (0912345678)
    if (len < 10) {
        cerr << "[WARNING] MaskPhone: Phone number length < 10, skip masking" << endl;
        return;
    }

    // Mask plaintext: Giữ 2 ký tự đầu + 3 ký tự cuối
    // VD: 0912345678 → 09****5678
    int startMask = 2;
    int maskCount = len - 5;  // Che giữa, giữ 2 đầu + 3 cuối

    for (int i = startMask; i < startMask + maskCount; i++) {
        buffer[i] = '*';
    }

    cout << "[MASK] Phone: " << string(buffer) << endl;
}

void MaskingLogic::MaskCCCD(char* buffer, size_t bufferSize) {
    if (buffer == nullptr) {
        cerr << "[ERROR] MaskCCCD: Buffer null pointer" << endl;
        return;
    }

    size_t len = strlen(buffer);

    // CCCD must be at least 12 characters
    if (len < 12) {
        cerr << "[WARNING] MaskCCCD: CCCD length < 12, skip masking" << endl;
        return;
    }

    // Mask plaintext CCCD: Keep only last 4 characters
    // Example: 001202037855 → ****037855
    int keepLast = 4;
    int maskCount = len - keepLast;

    for (int i = 0; i < maskCount; i++) {
        buffer[i] = '*';
    }

    cout << "[MASK] CCCD: " << string(buffer) << endl;
}

void MaskingLogic::MaskSalary(char* buffer, size_t bufferSize) {
    if (buffer == nullptr) {
        cerr << "[ERROR] MaskSalary: Buffer null pointer" << endl;
        return;
    }

    size_t len = strlen(buffer);

    // Salary must be at least 1 character
    if (len < 1) {
        cerr << "[WARNING] MaskSalary: salary length < 1, skip masking" << endl;
        return;
    }

    // Mask entire salary as "***"
    // Example: "50000000" → "***"
    if (bufferSize >= 4) {
        strcpy(buffer, "***");
        cout << "[MASK] Salary: ***" << endl;
    } else {
        cerr << "[ERROR] MaskSalary: Buffer size too small" << endl;
    }
}

bool MaskingLogic::IsAlreadyMasked(const char* buffer) {
    if (buffer == nullptr) {
        return false;
    }

    // Check if buffer contains '*'
    for (size_t i = 0; i < strlen(buffer); i++) {
        if (buffer[i] == '*') {
            return true;
        }
    }

    return false;
}

void MaskingLogic::PrintBuffer(const char* buffer, const char* label) {
    if (buffer == nullptr) {
        cerr << "[" << label << "] Buffer is NULL" << endl;
        return;
    }

    cout << "[" << label << "] Value: '" << buffer << "'" << endl;
    
    // Print each character with position
    cout << "  Details: ";
    for (size_t i = 0; i < strlen(buffer); i++) {
        if (buffer[i] == '*') {
            cout << "[*]";  // Highlight asterisk
        } else {
            cout << "[" << buffer[i] << "]";
        }
    }
    cout << endl;
}
