#ifndef DATABASE_HELPER_H
#define DATABASE_HELPER_H

#include <string>
#include <vector>
#include "Blowfish.h"
#include "MaskingLogic.h"
#include "EncryptionConfig.h"

using namespace std;

// Struct dai dien cho mot nhan vien trong database
struct nhanvien {
    int id;                      // ID (auto-increment tu MySQL)
    string ten_nv;               // Ten nhan vien (plaintext)
    string vai_tro;              // Vai tro: "Admin" hoac "User"
    string cccd_cipher;          // CCCD duoc ma hoa (HEX format)
    string sdt_cipher;          // Mat khau duoc ma hoa (HEX format)
    string matkhau_cipher;          // So dien thoai duoc ma hoa (HEX format)
    string luong_cipher;         // Luong duoc ma hoa (HEX format)
};

// database helper class
class DatabaseHelper {
private:
    string HOST;
    int PORT;
    string USER;
    string PASSWORD;
    string DATABASE;
    void* connection;  // Se cast ve sql::Connection* khi su dung

    bool ExecuteQuery(const string& query);
    bool ExecuteInsert(const string& query);

    // ======== ENCRYPTION/DECRYPTION HELPERS ========
    // Encrypt all sensitive fields
    void EncryptNhanVienData(const string& cccd, const string& sdt,
                              const string& matkhau, const string& luong,
                              string& cccd_encrypted, string& sdt_encrypted,
                              string& matkhau_encrypted, string& luong_encrypted);

    // Decrypt all sensitive fields
    void DecryptNhanVienData(const string& cccd_encrypted, const string& sdt_encrypted,
                              const string& matkhau_encrypted, const string& luong_encrypted,
                              string& cccd_plain, string& sdt_plain,
                              string& matkhau_plain, string& luong_plain);

    // Apply role-based masking to decrypted data
    void ApplyRoleBasedMasking(string& cccd, string& sdt, string& matkhau, string& luong,
                                const string& currentUserRole);

    // Combined decrypt + mask operation
    void ProcessNhanVienFieldsWithRole(const string& cccd_encrypted, const string& sdt_encrypted,
                                        const string& matkhau_encrypted, const string& luong_encrypted,
                                        string& cccd_masked, string& sdt_masked,
                                        string& matkhau_masked, string& luong_masked,
                                        const string& currentUserRole);
    
public:
    // Constructor - Khoi tao thong tin ket noi
    DatabaseHelper(const string& host, int port, 
                   const string& user, const string& password, 
                   const string& database);

    // Destructor - Dong ket noi
    ~DatabaseHelper();

    bool Connect();

    // Tao bang NhanVien neu chua ton tai
    bool CreateTableNhanVien();

    // Tao database views cho role-based masking
    bool CreateRoleBasedViews();
    
    // Them mot nhan vien moi
    bool InsertNhanVien(const string& ten_nv, const string& vai_tro,
                        const string& cccd_plaintext, 
                        const string& sdt_plaintext,
                        const string& matkhau_plaintext,
                        const string& luong_plaintext);

    // ======== HAM TRUY VAN CO VAI TRO (ROLE-BASED MASKING) ========
    // Lay nhan vien theo ID voi masking dua vao vai tro cua user hien tai
    bool GetNhanVienByIdWithRole(int id, nhanvien& result, const string& currentUserRole);

    // Lay tat ca nhan vien voi masking dua vao vai tro cua user
    vector<nhanvien> GetAllNhanVienWithRole(const string& currentUserRole);

    // Lay nhan vien theo vai tro voi masking
    vector<nhanvien> GetNhanVienByRoleWithMask(const string& vai_tro, const string& currentUserRole);



    // Cap nhat thong tin nhan vien
    bool UpdateNhanVien(int id, const string& ten_nv, const string& vai_tro,
                        const string& cccd_plaintext, const string& sdt_plaintext, const string& matkhau_plaintext, const string& luong_plaintext);
                        

    // Xoa nhan vien theo ID
    bool DeleteNhanVienById(int id);

    // Xoa tat ca nhan vien (can than!)
    bool DeleteAllNhanVien();

    // Dem tong so nhan vien trong DB
    int GetTotalNhanVien();

    // ======== AUTHENTICATION ========
    // Xac thuc dang nhap (CCCD + Password)
    // Tra ve: nhanvien struct neu thanh cong, id = -1 neu that bai
    nhanvien AuthenticateUser(const string& cccd_plaintext, const string& matkhau_plaintext);

    // Kiem tra ket noi con song khong
    bool IsConnected();

    // Dong ket noi
    void Disconnect();
};

#endif 
// DATABASE_HELPER_H
