#ifndef MASKING_LOGIC_H
#define MASKING_LOGIC_H

#include <cstring>
#include <iostream>
#include <string>

using namespace std;

class MaskingLogic {
public:
    static void MaskCCCDForClient(char* buffer, size_t bufferSize = 20);
    static void MaskPhoneForClient(char* buffer, size_t bufferSize = 20);
    static void MaskToThreeStarForClient(char* buffer, size_t bufferSize = 20);
};

#endif // MASKING_LOGIC_H
