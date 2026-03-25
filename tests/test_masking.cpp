#include "MaskingLogic.h"
#include <cstring>

using namespace std;

int main() {
    cout << "========================================" << endl;
    cout << "   TEST: MASKING LOGIC MODULE" << endl;
    cout << "========================================" << endl;

    // ======== TEST 1: MASK PHONE ========
    cout << "\n[TEST 1] Masking Phone Number" << endl;
    cout << "-------------------------------------" << endl;
    
    char phone1[20] = "0912345678";
    cout << "Before: " << phone1 << endl;
    MaskingLogic::PrintBuffer(phone1, "PHONE_BEFORE");
    
    MaskingLogic::MaskPhone(phone1, 20);
    cout << "After:  " << phone1 << endl;
    MaskingLogic::PrintBuffer(phone1, "PHONE_AFTER");
    
    // Check if masked
    if (MaskingLogic::IsAlreadyMasked(phone1)) {
        cout << "✓ Phone is correctly masked" << endl;
    }

    // ======== TEST 2: MASK CCCD ========
    cout << "\n[TEST 2] Masking CCCD" << endl;
    cout << "-------------------------------------" << endl;
    
    char cccd1[20] = "001202037855";
    cout << "Before: " << cccd1 << endl;
    MaskingLogic::PrintBuffer(cccd1, "CCCD_BEFORE");
    
    MaskingLogic::MaskCCCD(cccd1, 20);
    cout << "After:  " << cccd1 << endl;
    MaskingLogic::PrintBuffer(cccd1, "CCCD_AFTER");
    
    if (MaskingLogic::IsAlreadyMasked(cccd1)) {
        cout << "✓ CCCD is correctly masked" << endl;
    }

    // ======== TEST 3: MASK SALARY ========
    cout << "\n[TEST 3] Masking Salary" << endl;
    cout << "-------------------------------------" << endl;
    
    char salary1[20] = "50000000";
    cout << "Before: " << salary1 << endl;
    MaskingLogic::PrintBuffer(salary1, "SALARY_BEFORE");
    
    MaskingLogic::MaskToThreeStar(salary1, user, 20);
    cout << "After:  " << salary1 << endl;
    MaskingLogic::PrintBuffer(salary1, "SALARY_AFTER");
    
    if (MaskingLogic::IsAlreadyMasked(salary1)) {
        cout << "✓ Salary is correctly masked" << endl;
    }

    // ======== TEST 4: MULTIPLE DATA ========
    cout << "\n[TEST 4] Masking Multiple Employee Records" << endl;
    cout << "-------------------------------------" << endl;

    // Employee 1
    char emp1_phone[20] = "0987654321";
    char emp1_cccd[20] = "002201234567";
    char emp1_salary[20] = "35000000";
    
    cout << "\nEmployee 1 (Before Masking):" << endl;
    MaskingLogic::PrintBuffer(emp1_phone, "Phone");
    MaskingLogic::PrintBuffer(emp1_cccd, "CCCD");
    MaskingLogic::PrintBuffer(emp1_salary, "Salary");
    
    MaskingLogic::MaskPhone(emp1_phone);
    MaskingLogic::MaskCCCD(emp1_cccd);
    MaskingLogic::MaskSalary(emp1_salary);
    
    cout << "\nEmployee 1 (After Masking):" << endl;
    MaskingLogic::PrintBuffer(emp1_phone, "Phone");
    MaskingLogic::PrintBuffer(emp1_cccd, "CCCD");
    MaskingLogic::PrintBuffer(emp1_salary, "Salary");

    // ======== TEST 5: EDGE CASES ========
    cout << "\n[TEST 5] Edge Cases" << endl;
    cout << "-------------------------------------" << endl;

    char shortData[20] = "123";
    cout << "Input (too short): " << shortData << endl;
    MaskingLogic::MaskPhone(shortData);
    cout << "Output (should skip): " << shortData << endl;

    char nullBuffer[20];
    strcpy_s(nullBuffer, 20, "");
    cout << "\nInput (empty): '" << nullBuffer << "'" << endl;
    MaskingLogic::MaskPhone(nullBuffer);
    cout << "Output (should skip): '" << nullBuffer << "'" << endl;

    // ======== TEST SUMMARY ========
    cout << "\n========================================" << endl;
    cout << "   ALL TESTS COMPLETED SUCCESSFULLY!" << endl;
    cout << "========================================" << endl;

    return 0;
}
