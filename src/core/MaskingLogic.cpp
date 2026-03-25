#include "MaskingLogic.h"

using namespace std;

void MaskingLogic::MaskPhone(char* buffer, size_t bufferSize) {
    if (buffer == nullptr) {
        cerr << "[ERROR] MaskPhone: Buffer null pointer" << endl;
        return;
    }

    size_t len = strlen(buffer);

    // Dieu kien: SDT hop le phai co it nhat 10 ky tu (0912345678)
    if (len < 10) {
        cerr << "[WARNING] MaskPhone: Phone number length < 10, skip masking" << endl;
        return;
    }

    // Mask plaintext: Giu 2 ky tu dau + 3 ky tu cuoi
    // VD: 0912345678 → 09****5678
    int startMask = 2;
    int maskCount = len - 5;  // Che giua, giu 2 dau + 3 cuoi

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

void MaskingLogic::MaskToThreeStar(char* buffer, UserRole role, size_t bufferSize) {
    if (buffer == nullptr) {
        cerr << "[ERROR] MaskToThreeStar: Buffer null pointer" << endl;
        return;
    }

    size_t len = strlen(buffer);

    // Salary must be at least 1 character
    if (len < 1) {
        cerr << "[WARNING] MaskToThreeStar: length < 1, skip masking" << endl;
        return;
    }

    // Mask entire salary as "***"
    // Example: "50000000" → "***"
    if (bufferSize >= 4) {
        strcpy(buffer, "***");
        cout << "[MASK] Salary: ***" << endl;
    } else {
        cerr << "[ERROR] MaskToThreeStar: Buffer size too small" << endl;
    }
}

// ======== HAM CHE GIAU THEO VAI TRO ========

UserRole MaskingLogic::StringToRole(const string& roleStr) {
    if (roleStr == "Admin") {
        return UserRole::ADMIN;
    }
    return UserRole::USER;
}

void MaskingLogic::MaskCCCDByRole(char* buffer, UserRole role, size_t bufferSize) {
    if (buffer == nullptr) {
        cerr << "[ERROR] MaskCCCDByRole: Buffer null pointer" << endl;
        return;
    }

    size_t len = strlen(buffer);

    if (len < 12) {
        cerr << "[WARNING] MaskCCCDByRole: CCCD length < 12, skip masking" << endl;
        return;
    }

    if (role == UserRole::ADMIN) {
        // Admin: Hien thi plaintext
        cout << "[MASK] CCCD (Admin - plaintext): " << string(buffer) << endl;
    } else {
        // User: Che toan bo ngoai 4 chu so cuoi
        // Example: 001202037855 → ****037855
        int keepLast = 4;
        int maskCount = len - keepLast;

        for (int i = 0; i < maskCount; i++) {
            buffer[i] = '*';
        }
        cout << "[MASK] CCCD (User - masked): " << string(buffer) << endl;
    }
}

void MaskingLogic::MaskPhoneByRole(char* buffer, UserRole role, size_t bufferSize) {
    if (buffer == nullptr) {
        cerr << "[ERROR] MaskPhoneByRole: Buffer null pointer" << endl;
        return;
    }

    size_t len = strlen(buffer);

    if (len < 10) {
        cerr << "[WARNING] MaskPhoneByRole: Phone number length < 10, skip masking" << endl;
        return;
    }

    if (role == UserRole::ADMIN) {
        // Admin: Hien thi plaintext
        cout << "[MASK] Phone (Admin - plaintext): " << string(buffer) << endl;
    } else {
        // User: Giu 2 ky tu dau + 4 ky tu cuoi, che phan giu
        // Example: 0912345678 → 09****5678
        int startMask = 2;
        int maskCount = len - 6;  // Che giua, giu 2 dau + 4 cuoi

        for (int i = startMask; i < startMask + maskCount; i++) {
            buffer[i] = '*';
        }
        cout << "[MASK] Phone (User - masked): " << string(buffer) << endl;
    }
}

void MaskingLogic::MaskToThreeStarByRole(char* buffer, UserRole role, size_t bufferSize) {
    if (buffer == nullptr) {
        cerr << "[ERROR] MaskToThreeStarByRole: Buffer null pointer" << endl;
        return;
    }

    size_t len = strlen(buffer);

    if (len < 1) {
        cerr << "[WARNING] MaskToThreeStarByRole: length < 1, skip masking" << endl;
        return;
    }

    if (role == UserRole::ADMIN) {
        // Admin: Hien thi plaintext
        cout << "[MASK] Salary (Admin - plaintext): " << string(buffer) << endl;
    } else {
        // User: Che toan bo thanh "***"
        if (bufferSize >= 4) {
            strcpy(buffer, "***");
            cout << "[MASK] Salary (User - masked): ***" << endl;
        } else {
            cerr << "[ERROR] Mask: Buffer size too small" << endl;
        }
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
