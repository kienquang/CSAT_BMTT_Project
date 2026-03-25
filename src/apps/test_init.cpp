#include <iostream>
#include "DatabaseHelper.h"

using namespace std;

int main() {
    cout << "======= DATABASE INITIALIZATION TEST =======" << endl;
    
    // Create database helper
    DatabaseHelper dbHelper("localhost", 3306, "root", "", "csatbmtt");
    
    cout << "[TEST] Connecting to database..." << endl;
    if (!dbHelper.Connect()) {
        cerr << "[ERROR] Failed to connect to database" << endl;
        return 1;
    }
    cout << "[SUCCESS] Connected to database" << endl;
    
    cout << "\n[TEST] Creating table nhanvien..." << endl;
    if (!dbHelper.CreateTableNhanVien()) {
        cerr << "[ERROR] Failed to create table" << endl;
        return 1;
    }
    cout << "[SUCCESS] Table created successfully" << endl;
    
    cout << "\n[TEST] Creating role-based views..." << endl;
    if (!dbHelper.CreateRoleBasedViews()) {
        cerr << "[ERROR] Failed to create views" << endl;
        return 1;
    }
    cout << "[SUCCESS] Views created successfully" << endl;
    
    cout << "\n[TEST] Checking data..." << endl;
    int totalEmployees = dbHelper.GetTotalNhanVien();
    cout << "[INFO] Total employees in database: " << totalEmployees << endl;
    
    if (totalEmployees > 0) {
        cout << "[SUCCESS] Default admin account created!" << endl;
    }
    
    cout << "\n======= TEST COMPLETED SUCCESSFULLY =======" << endl;
    return 0;
}
