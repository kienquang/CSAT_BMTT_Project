#include <iostream>
#include <windows.h>
#include <iomanip>
#include <cctype>
#include "DatabaseHelper.h"

using namespace std;

// ========== GLOBAL VARIABLE ==========
string currentUserRole = "User";  // Vai tro cua user hien tai (Admin hoac User)

// ========== COLOR FUNCTIONS ==========
void SetColor(int color) {
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
}

void PrintHeader(const string& title) {
    SetColor(14);  // Yellow
    cout << "\n" << string(60, '=') << endl;
    cout << "   " << title << endl;
    cout << string(60, '=') << "\n" << endl;
    SetColor(7);   // White
    cout.flush();
}

void PrintSuccess(const string& msg) {
    SetColor(10);  // Green
    cout << "[SUCCESS] " << msg << endl;
    SetColor(7);
    cout.flush();
}

void PrintError(const string& msg) {
    SetColor(12);  // Red
    cout << "[ERROR] " << msg << endl;
    SetColor(7);
    cout.flush();
}

void PrintInfo(const string& msg) {
    SetColor(11);  // Cyan
    cout << "[INFO] " << msg << endl;
    SetColor(7);
    cout.flush();
}

// ========== DISPLAY FUNCTIONS ==========
void DisplayNhanVienTable(const vector<nhanvien>& employees) {
    if (employees.empty()) {
        PrintInfo("Khong co du lieu nhan vien");
        return;
    }

    SetColor(11);  // Cyan
    cout << left 
         << setw(5) << "ID"
         << setw(20) << "Ten Nhan Vien"
         << setw(10) << "Vai Tro"
         << setw(20) << "CCCD"
         << setw(18) << "So DT"
         << setw(12) << "Mat Khau"
         << setw(12) << "Luong"
         << endl;
    cout << string(95, '-') << endl;
    SetColor(7);

    for (const auto& emp : employees) {
        cout << left 
             << setw(5) << emp.id
             << setw(20) << emp.ten_nv
             << setw(10) << emp.vai_tro
             << setw(20) << emp.cccd_cipher
             << setw(18) << emp.sdt_cipher
             << setw(12) << emp.matkhau_cipher             
             << setw(12) << emp.luong_cipher
             << endl;
    }
    cout << string(95, '-') << endl;
}

// ========== MENU FUNCTIONS ==========
void DisplayMainMenu() {
    PrintHeader("QUAN LY DU LIEU NHAN VIEN - Role-Based Masking");
    cout << "*** DANG DANG NHAP VAS: " << currentUserRole << " ***" << endl << endl;
    cout << "1. Xem tat ca nhan vien (voi role-based masking)" << endl;
    cout << "2. Tim kiem nhan vien theo ID (voi role-based masking)" << endl;
    cout << "3. Tim kiem nhan vien theo vai tro (voi role-based masking)" << endl;
    cout << "4. Them nhan vien moi" << endl;
    cout << "5. Cap nhat thong tin nhan vien" << endl;
    cout << "6. Xoa nhan vien" << endl;
    cout << "7. Xem thong ke" << endl;
    cout << "8. Thay doi vai tro (Admin/User)" << endl;
    cout << "9. Xem database views" << endl;
    cout << "0. Thoat chuong trinh" << endl;
    cout << "\nChon chuc nang (0-9): ";
}

// ========== ACTION FUNCTIONS ==========
void ViewAllNhanVien(DatabaseHelper& dbHelper) {
    PrintHeader("DANH SACH TAT CA NHAN VIEN");
    cout << "*** Dang xem voi vai tro: " << currentUserRole << " ***\n" << endl;
    vector<nhanvien> employees = dbHelper.GetAllNhanVienWithRole(currentUserRole);
    DisplayNhanVienTable(employees);
}

void SearchNhanVienById(DatabaseHelper& dbHelper) {
    PrintHeader("TIM KIEM NHAN VIEN THEO ID");
    cout << "*** Dang xem voi vai tro: " << currentUserRole << " ***\n" << endl;
    cout << "Nhap ID nhan vien: ";
    int id;
    cin >> id;
    cin.ignore();

    nhanvien emp;
    if (dbHelper.GetNhanVienByIdWithRole(id, emp, currentUserRole)) {
        cout << "\n" << string(60, '-') << endl;
        SetColor(11);
        cout << left 
             << setw(15) << "ID"
             << ": " << emp.id << endl
             << setw(15) << "Ten Nhan Vien"
             << ": " << emp.ten_nv << endl
             << setw(15) << "Vai Tro"
             << ": " << emp.vai_tro << endl
             << setw(15) << "CCCD (ma hoa)"
             << ": " << emp.cccd_cipher << endl
             << setw(15) << "So DT (ma hoa)"
             << ":" << emp.sdt_cipher << endl
             << setw(15) << "Mat khau (ma hoa)"
             << ": " << emp.sdt_cipher << endl
             << setw(15) << "Luong (ma hoa)"
             << ": " << emp.luong_cipher << endl;
        SetColor(7);
        cout << string(60, '-') << "\n" << endl;
        PrintSuccess("Tim thay nhan vien!");
    } else {
        PrintError("Khong tim thay nhan vien voi ID = " + to_string(id));
    }
}

void SearchNhanVienByRole(DatabaseHelper& dbHelper) {
    PrintHeader("TIM KIEM NHAN VIEN THEO VAI TRO");
    cout << "*** Dang xem voi vai tro: " << currentUserRole << " ***\n" << endl;
    cout << "Nhap vai tro (Admin/User): ";
    string role;
    getline(cin, role);

    vector<nhanvien> employees = dbHelper.GetNhanVienByRoleWithMask(role, currentUserRole);
    DisplayNhanVienTable(employees);
}

void AddNhanVien(DatabaseHelper& dbHelper) {
    PrintHeader("THEM NHAN VIEN MOI");
    
    string ten_nv, vai_tro, cccd, sdt, matkhau, luong;
    
    cout << "Ten nhan vien: ";
    getline(cin, ten_nv);
    
    cout << "Vai tro (Admin/User): ";
    getline(cin, vai_tro);
    
    cout << "CCCD: ";
    getline(cin, cccd);
    
    cout << "So dien thoai: ";
    getline(cin, sdt);

    cout << "Mat khau: ";
    getline(cin, matkhau);
    
    cout << "Luong: ";
    getline(cin, luong);

    if (dbHelper.InsertNhanVien(ten_nv, vai_tro, cccd, sdt, matkhau, luong)) {
        PrintSuccess("Them nhan vien thanh cong!");
    } else {
        PrintError("Them nhan vien that bai!");
    }
}

void UpdateNhanVien(DatabaseHelper& dbHelper) {
    PrintHeader("CAP NHAT THONG TIN NHAN VIEN");
    
    cout << "Nhap ID nhan vien can cap nhat: ";
    int id;
    cin >> id;
    cin.ignore();

    nhanvien emp;
    if (!dbHelper.GetNhanVienByIdWithRole(id, emp, currentUserRole)) {
        PrintError("Khong tim thay nhan vien voi ID = " + to_string(id));
        return;
    }

    PrintSuccess("Tim thay nhan vien: " + emp.ten_nv);
    cout << "\nNhap thong tin moi (de trong de giu nguyen):\n";

    string ten_nv, vai_tro, cccd, sdt, matkhau, luong;
    
    cout << "Ten nhan vien (" << emp.ten_nv << "): ";
    getline(cin, ten_nv);
    if (ten_nv.empty()) ten_nv = emp.ten_nv;
    
    cout << "Vai tro (" << emp.vai_tro << "): ";
    getline(cin, vai_tro);
    if (vai_tro.empty()) vai_tro = emp.vai_tro;
    
    cout << "CCCD (khong hien thi): ";
    getline(cin, cccd);
    if (cccd.empty()) cccd = emp.cccd_cipher;
    
    cout << "So dien thoai (khong hien thi): ";
    getline(cin, sdt);
    if (sdt.empty()) sdt = emp.sdt_cipher;

    cout << "Mat khau (khong hien thi): ";
    getline(cin, matkhau);
    if (matkhau.empty()) matkhau = emp.matkhau_cipher;
    
    cout << "Luong (khong hien thi): ";
    getline(cin, luong);
    if (luong.empty()) luong = emp.luong_cipher;

    if (dbHelper.UpdateNhanVien(id, ten_nv, vai_tro, cccd, sdt, matkhau, luong)) {
        PrintSuccess("Cap nhat nhan vien thanh cong!");
    } else {
        PrintError("Cap nhat nhan vien that bai!");
    }
}

void DeleteNhanVien(DatabaseHelper& dbHelper) {
    PrintHeader("XOA NHAN VIEN");
    
    cout << "Nhap ID nhan vien can xoa: ";
    int id;
    cin >> id;
    cin.ignore();

    nhanvien emp;
    if (!dbHelper.GetNhanVienByIdWithRole(id, emp, currentUserRole)) {
        PrintError("Khong tim thay nhan vien voi ID = " + to_string(id));
        return;
    }

    PrintInfo("Nhan vien: " + emp.ten_nv + " (" + emp.vai_tro + ")");
    cout << "Ban co chac chan muon xoa? (y/n): ";
    char confirm;
    cin >> confirm;
    cin.ignore();

    if (tolower(confirm) == 'y') {
        if (dbHelper.DeleteNhanVienById(id)) {
            PrintSuccess("Xoa nhan vien thanh cong!");
        } else {
            PrintError("Xoa nhan vien that bai!");
        }
    } else {
        PrintInfo("Huy bo thao tac xoa");
    }
}

void DisplayStatistics(DatabaseHelper& dbHelper) {
    PrintHeader("THONG KE DU LIEU");
    
    int total = dbHelper.GetTotalNhanVien();
    vector<nhanvien> admins = dbHelper.GetNhanVienByRoleWithMask("Admin", currentUserRole);
    vector<nhanvien> users = dbHelper.GetNhanVienByRoleWithMask("User", currentUserRole);

    SetColor(11);
    cout << left << setw(25) << "Tong so nhan vien:" 
         << total << " nguoi" << endl;
    cout << setw(25) << "So Admin:" 
         << admins.size() << " nguoi" << endl;
    cout << setw(25) << "So User:" 
         << users.size() << " nguoi" << endl;
    cout << setw(25) << "Vai tro hien tai:" 
         << currentUserRole << endl;
    SetColor(7);
    cout << endl;
}

void ChangeUserRole() {
    PrintHeader("THAY DOI VAI TRO DANG NHAP");
    cout << "Vai tro hien tai: " << currentUserRole << endl << endl;
    cout << "1. Chuyen sang Admin" << endl;
    cout << "2. Chuyen sang User" << endl;
    cout << "0. Huy bo" << endl;
    cout << "\nChon (0-2): ";
    
    int choice;
    cin >> choice;
    cin.ignore();
    
    switch (choice) {
        case 1:
            currentUserRole = "Admin";
            PrintSuccess("Chuyen sang vai tro Admin!");
            break;
        case 2:
            currentUserRole = "User";
            PrintSuccess("Chuyen sang vai tro User!");
            break;
        case 0:
            PrintInfo("Huy bo");
            break;
        default:
            PrintError("Lua chon khong hop le!");
    }
}

void DisplayDatabaseViews() {
    PrintHeader("THONG TIN DATABASE VIEWS");
    SetColor(11);
    cout << "\nCac view da duoc tao:\n" << endl;
    cout << "1. v_nhanvien_user:" << endl;
    cout << "   - Dung cho User: Che giau tat ca du lieu nhay cam" << endl;
    cout << "   - Cot: id, ten_nv, vai_tro, cccd_cipher_encrypted, sdt_cipher_encrypted, luong_cipher_encrypted" << endl << endl;
    
    cout << "2. v_nhanvien_admin:" << endl;
    cout << "   - Dung cho Admin: Hien thi tat ca du lieu goc (encrypted)" << endl;
    cout << "   - Cot: id, ten_nv, vai_tro, cccd_cipher, sdt_cipher, luong_cipher" << endl << endl;
    
    cout << "Ghi chu:" << endl;
    cout << "   - He thong su dung role-based masking trong C++ code" << endl;
    cout << "   - Decrypt -> Mask theo role -> Gui cho user" << endl;
    cout << "   - Views co the dung truc tiep tu SQL client neu can" << endl;
    SetColor(7);
    cout << endl;
}

// ========== MAIN PROGRAM ==========
int main() {
    try {
        PrintHeader("KET NOI CO SO DU LIEU");
        
        DatabaseHelper dbHelper("127.0.0.1", 3306, "root", "", "csatbmtt");
        
        cout << "Dang ket noi toi database..." << endl;
        cout.flush();
        if (!dbHelper.Connect()) {
            PrintError("Khong the ket noi toi database!");
            PrintError("Kiem tra:");
            PrintError("  - MySQL server co chay khong?");
            PrintError("  - Username/Password co dung khong?");
            PrintError("  - Database 'csatbmtt' da duoc tao chua?");
            cout << "\nNhan ENTER de thoat...";
            cout.flush();
            cin.get();
            return 1;
        }

        PrintSuccess("Ket noi thanh cong!");

        if (!dbHelper.CreateTableNhanVien()) {
            PrintError("Khong the tao bang NhanVien!");
            cout << "\nNhan ENTER de thoat...";
            cout.flush();
            cin.get();
            return 1;
        }

        PrintSuccess("Bang NhanVien san sang!");

        // Tao database views cho role-based masking
        PrintHeader("TAO DATABASE VIEWS");
        if (!dbHelper.CreateRoleBasedViews()) {
            PrintError("Khong the tao database views!");
            cout << "\nNhan ENTER de thoat...";
            cout.flush();
            cin.get();
            return 1;
        }
        PrintSuccess("Database views da tao thanh cong!");

        // Main menu loop
        int choice;
        while (true) {
            DisplayMainMenu();
            cin >> choice;
            cin.ignore();

            switch (choice) {
                case 1:
                    ViewAllNhanVien(dbHelper);
                    break;
                case 2:
                    SearchNhanVienById(dbHelper);
                    break;
                case 3:
                    SearchNhanVienByRole(dbHelper);
                    break;
                case 4:
                    AddNhanVien(dbHelper);
                    break;
                case 5:
                    UpdateNhanVien(dbHelper);
                    break;
                case 6:
                    DeleteNhanVien(dbHelper);
                    break;
                case 7:
                    DisplayStatistics(dbHelper);
                    break;
                case 8:
                    ChangeUserRole();
                    break;
                case 9:
                    DisplayDatabaseViews();
                    break;
                case 0:
                    PrintInfo("Dang dong ket noi...");
                    dbHelper.Disconnect();
                    PrintSuccess("Chuong trinh da thoat!");
                    return 0;
                default:
                    PrintError("Lua chon khong hop le! Vui long chon lai.");
            }
            
            cout << "\nNhan ENTER de tiep tuc...";
            cout.flush();
            cin.get();
        }

    } catch (exception& e) {
        PrintError("Co loi xay ra: " + string(e.what()));
        cout << "\nNhan ENTER de thoat...";
        cin.get();
        return 1;
    }
}
