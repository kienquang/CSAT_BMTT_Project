#ifndef MASKING_LOGIC_H
#define MASKING_LOGIC_H

#include <cstring>
#include <iostream>

using namespace std;

class MaskingLogic {
public:

    static void MaskPhone(char* buffer, size_t bufferSize = 20);

    static void MaskCCCD(char* buffer, size_t bufferSize = 20);

    static void MaskSalary(char* buffer, size_t bufferSize = 20);


    // Kiem tra xem buffer co chua du lieu da duoc mo phong khong
    // Return: true neu phat hien '*', false neu du lieu goc
    static bool IsAlreadyMasked(const char* buffer);

    // In ra buffer de debug
    static void PrintBuffer(const char* buffer, const char* label);
};

#endif // MASKING_LOGIC_H
