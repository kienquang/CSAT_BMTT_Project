#include "MaskingLogic.h"

using namespace std;

void MaskingLogic::MaskCCCDForClient(char* buffer, size_t bufferSize) {
    if (buffer == nullptr) {
        cerr << "[ERROR] MaskCCCDForClient: Buffer null pointer" << endl;
        return;
    }

    const size_t len = strlen(buffer);
    if (len < 12) {
        cerr << "[WARNING] MaskCCCDForClient: CCCD length < 12, skip masking" << endl;
        return;
    }

    (void)bufferSize;
    const int keepLast = 4;
    const int maskCount = static_cast<int>(len) - keepLast;
    for (int i = 0; i < maskCount; i++) {
        buffer[i] = '*';
    }
}

void MaskingLogic::MaskPhoneForClient(char* buffer, size_t bufferSize) {
    if (buffer == nullptr) {
        cerr << "[ERROR] MaskPhoneForClient: Buffer null pointer" << endl;
        return;
    }

    const size_t len = strlen(buffer);
    if (len < 10) {
        cerr << "[WARNING] MaskPhoneForClient: Phone number length < 10, skip masking" << endl;
        return;
    }

    (void)bufferSize;
    const int startMask = 2;
    const int maskCount = static_cast<int>(len) - 6;
    for (int i = startMask; i < startMask + maskCount; i++) {
        buffer[i] = '*';
    }
}

void MaskingLogic::MaskToThreeStarForClient(char* buffer, size_t bufferSize) {
    if (buffer == nullptr) {
        cerr << "[ERROR] MaskToThreeStarForClient: Buffer null pointer" << endl;
        return;
    }

    const size_t len = strlen(buffer);
    if (len < 1) {
        cerr << "[WARNING] MaskToThreeStarForClient: length < 1, skip masking" << endl;
        return;
    }

    if (bufferSize >= 4) {
        strcpy(buffer, "***");
    } else {
        cerr << "[ERROR] MaskToThreeStarForClient: Buffer size too small" << endl;
    }
}
