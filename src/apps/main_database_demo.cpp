#include <iostream>
#include <iomanip>
#include <cstring>
#include "DatabaseHelper.h"
#include "Blowfish.h"
#include "EncryptionConfig.h"

using namespace std;

// ======== HELPER FUNCTIONS ========

void DisplayEmployee(const nhanvien& emp) {
    cout << "\n┌─────────────────────────────────────────────────┐" << endl;
    cout << "│ THONG TIN NHAN VIEN" << endl;
    cout << "├─────────────────────────────────────────────────┤" << endl;
    cout << "│ ID:          " << emp.id << endl;
    cout << "│ Ten:         " << emp.ten_nv << endl;
    cout << "│ Vai tro:     " << emp.vai_tro << endl;
    cout << "│ CCCD (HEX):  " << emp.cccd_cipher << endl;
    cout << "│ SDT (HEX):   " << emp.sdt_cipher << endl;
    cout << "│ Luong (HEX): " << emp.luong_cipher << endl;
    cout << "└─────────────────────────────────────────────────┘\n" << endl;
}

void TestEncryptionDecryption() {
    cout << "\n========== TEST ENCRYPTION/DECRYPTION ==========" << endl;
    
    Blowfish cipher(EncryptionConfig::BLOWFISH_KEY);
    
    // Test data
    string plaintext = "001202037855";
    cout << "Plaintext:       " << plaintext << endl;
    
    // Encrypt
    string encrypted_hex = cipher.EncryptString(plaintext);
    cout << "Encrypted (HEX): " << encrypted_hex << endl;
    
    // Decrypt
    string decrypted = cipher.DecryptString(encrypted_hex);
    cout << "Decrypted:       " << decrypted << endl;
    
    // Verify
    if (decrypted == plaintext) {
        cout << "✓ PASS: Encrypt/Decrypt round-trip successful!" << endl;
    } else {
        cout << "✗ FAIL: Mismatch detected!" << endl;
    }
    cout << "===============================================\n" << endl;
}

void InsertDummyData(DatabaseHelper& dbHelper) {
    cout << "\n========== THÊM DỮ LIỆU TẠM ==========" << endl;
    
    // Dữ liệu tạm
    struct {
        string name;
        string role;
        string cccd;
        string phone;
        string salary;
    } dummies[] = {
        {"Nguyễn Văn A", "Admin", "001202037855", "0912345678", "50000000"},
        {"Trần Thị B", "User", "001203041234", "0913456789", "40000000"},
        {"Phạm Văn C", "Admin", "001204051234", "0914567890", "55000000"},
        {"Hoàng Thị D", "User", "001205061234", "0915678901", "35000000"},
        {"Lê Văn E", "Admin", "001206071234", "0916789012", "48000000"},
    };

    int count = sizeof(dummies) / sizeof(dummies[0]);
    
    for (int i = 0; i < count; i++) {
        bool success = dbHelper.InsertNhanVien(
            dummies[i].name,
            dummies[i].role,
            dummies[i].cccd,
            dummies[i].phone,
            "Default123",
            dummies[i].salary
        );
        
        if (success) {
            cout << "✓ Thêm: " << dummies[i].name << endl;
        } else {
            cout << "✗ Lỗi: " << dummies[i].name << endl;
        }
    }
    
    cout << "===================================\n" << endl;
}

// ======== MAIN MENU ========

int main() {
    cout << "\n╔════════════════════════════════════════════╗" << endl;
    cout << "║   DATABASE DEMO - CSAT_BMTT_PROJECT      ║" << endl;
    cout << "║   Member B: Database Integration         ║" << endl;
    cout << "╚════════════════════════════════════════════╝\n" << endl;

    // Khởi tạo DatabaseHelper
    DatabaseHelper dbHelper("127.0.0.1", 3306, "root", "root", "csat_project");

    // Menu chính
    bool running = true;
    while (running) {
        cout << "\n========== DATABASE MENU ==========" << endl;
        cout << "1. Kết nối & Tạo bảng" << endl;
        cout << "2. Thêm nhân viên" << endl;
        cout << "3. Tìm nhân viên theo ID" << endl;
        cout << "4. Cập nhật nhân viên" << endl;
        cout << "5. Xóa nhân viên" << endl;
        cout << "6. Thêm dữ liệu tạm (5 employees)" << endl;
        cout << "7. Test Encryption/Decryption" << endl;
        cout << "8. Hiển thị tất cả nhân viên" << endl;
        cout << "9. Đóng kết nối" << endl;
        cout << "0. Thoát" << endl;
        cout << "===================================" << endl;
        cout << "Chọn: ";

        int choice;
        cin >> choice;
        cin.ignore();  // Clear input buffer

        switch (choice) {
            case 1: {
                cout << "\n[ACTION] Kết nối tới MySQL..." << endl;
                if (dbHelper.Connect()) {
                    cout << "✓ Kết nối thành công!" << endl;
                    if (dbHelper.CreateTableNhanVien()) {
                        cout << "✓ Bảng NhanVien đã sẵn sàng!" << endl;
                    }
                } else {
                    cout << "✗ Lỗi kết nối!" << endl;
                }
                break;
            }

            case 2: {
                cout << "\n[ACTION] Thêm nhân viên mới" << endl;
                string name, role, cccd, phone, salary;
                
                cout << "Tên: ";
                getline(cin, name);
                cout << "Vai trò (Admin/User): ";
                getline(cin, role);
                cout << "CCCD (12 số): ";
                getline(cin, cccd);
                cout << "Số điện thoại (10 số): ";
                getline(cin, phone);
                cout << "Mat khau: ";
                string password;
                getline(cin, password);
                cout << "Luong: ";
                getline(cin, salary);
                
                if (dbHelper.InsertNhanVien(name, role, cccd, phone, password, salary)) {
                    cout << "✓ Thêm thành công!" << endl;
                } else {
                    cout << "✗ Lỗi khi thêm!" << endl;
                }
                break;
            }

            case 3: {
                cout << "\n[ACTION] Tìm nhân viên" << endl;
                cout << "ID: ";
                int id;
                cin >> id;
                
                nhanvien emp;
                if (dbHelper.GetNhanVienByIdWithRole(id, emp, "Admin")) {
                    DisplayEmployee(emp);
                    
                    // Giải mã để hiển thị plaintext
                    Blowfish cipher(EncryptionConfig::BLOWFISH_KEY);
                    string cccdPlain = cipher.DecryptString(emp.cccd_cipher);
                    string sdtPlain = cipher.DecryptString(emp.sdt_cipher);
                    string luongPlain = cipher.DecryptString(emp.luong_cipher);
                    
                    cout << "\n📋 GIẢI MÃ (Plaintext - chỉ demo):" << endl;
                    cout << "  CCCD:  " << cccdPlain << endl;
                    cout << "  SDT:   " << sdtPlain << endl;
                    cout << "  Lương: " << luongPlain << endl;
                } else {
                    cout << "✗ Không tìm thấy nhân viên!" << endl;
                }
                break;
            }

            case 4: {
                cout << "\n[ACTION] Cập nhật nhân viên" << endl;
                cout << "ID: ";
                int id;
                cin >> id;
                cin.ignore();
                
                string name, role, cccd, phone, password, salary;
                cout << "Tên: ";
                getline(cin, name);
                cout << "Vai trò: ";
                getline(cin, role);
                cout << "CCCD: ";
                getline(cin, cccd);
                cout << "SĐT: ";
                getline(cin, phone);
                cout << "Mật khẩu: ";
                getline(cin, password);
                cout << "Lương: ";
                getline(cin, salary);
                
                if (dbHelper.UpdateNhanVien(id, name, role, cccd, phone, password, salary)) {
                    cout << "✓ Cập nhật thành công!" << endl;
                } else {
                    cout << "✗ Lỗi cập nhật!" << endl;
                }
                break;
            }

            case 5: {
                cout << "\n[ACTION] Xóa nhân viên" << endl;
                cout << "ID: ";
                int id;
                cin >> id;
                
                if (dbHelper.DeleteNhanVienById(id)) {
                    cout << "✓ Xóa thành công!" << endl;
                } else {
                    cout << "✗ Lỗi xóa!" << endl;
                }
                break;
            }

            case 6: {
                InsertDummyData(dbHelper);
                break;
            }

            case 7: {
                TestEncryptionDecryption();
                break;
            }

            case 8: {
                cout << "\n[ACTION] Lấy tất cả nhân viên" << endl;
                vector<nhanvien> employees = dbHelper.GetAllNhanVienWithRole("Admin");
                
                if (employees.size() == 0) {
                    cout << "Không có dữ liệu trong bảng!" << endl;
                } else {
                    cout << "Tổng: " << employees.size() << " nhân viên\n" << endl;
                    for (const auto& emp : employees) {
                        cout << "ID: " << emp.id << " | Tên: " << emp.ten_nv 
                             << " | Vai trò: " << emp.vai_tro << endl;
                    }
                }
                break;
            }

            case 9: {
                cout << "\n[ACTION] Đóng kết nối..." << endl;
                dbHelper.Disconnect();
                cout << "✓ Kết nối đã được đóng!" << endl;
                break;
            }

            case 0: {
                cout << "\nTạm biệt! 👋" << endl;
                running = false;
                break;
            }

            default: {
                cout << "Lựa chọn không hợp lệ!" << endl;
            }
        }
    }

    return 0;
}
