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


    // Kiểm tra xem buffer có chứa dữ liệu đã được mô phỏng không
    // Return: true nếu phát hiện '*', false nếu dữ liệu gốc
    static bool IsAlreadyMasked(const char* buffer);

    // In ra buffer để debug
    static void PrintBuffer(const char* buffer, const char* label);
};

#endif // MASKING_LOGIC_H
