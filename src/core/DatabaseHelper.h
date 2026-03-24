#ifndef DATABASE_HELPER_H
#define DATABASE_HELPER_H

#include <string>
#include <vector>
#include "Blowfish.h"

using namespace std;

// Struct đại diện cho một nhân viên trong database
struct nhanvien {
    int id;                      // ID (auto-increment từ MySQL)
    string ten_nv;               // Tên nhân viên (plaintext)
    string vai_tro;              // Vai trò: "Admin" hoặc "User"
    string cccd_cipher;          // CCCD được mã hóa (HEX format)
    string sdt_cipher;           // Số điện thoại được mã hóa (HEX format)
    string luong_cipher;         // Lương được mã hóa (HEX format)
};

// database helper class
class DatabaseHelper {
private:
    string HOST;
    int PORT;
    string USER;
    string PASSWORD;
    string DATABASE;
    void* connection;  // Sẽ cast về sql::Connection* khi sử dụng

    bool ExecuteQuery(const string& query);
    bool ExecuteInsert(const string& query);
    
public:
    // Constructor - Khởi tạo thông tin kết nối
    DatabaseHelper(const string& host, int port, 
                   const string& user, const string& password, 
                   const string& database);

    // Destructor - Đóng kết nối
    ~DatabaseHelper();

    bool Connect();

    // Tạo bảng NhanVien nếu chưa tồn tại
    bool CreateTableNhanVien();
    
    // Thêm một nhân viên mới
    bool InsertNhanVien(const string& ten_nv, const string& vai_tro,
                        const string& cccd_plaintext, 
                        const string& sdt_plaintext,
                        const string& luong_plaintext);

    bool GetNhanVienById(int id, nhanvien& result);

    // Lấy tất cả nhân viên
    vector<nhanvien> GetAllNhanVien();

    // Lấy nhân viên theo vai trò (Admin hoặc User)
    vector<nhanvien> GetNhanVienByRole(const string& vai_tro);


    // Cập nhật thông tin nhân viên
    bool UpdateNhanVien(int id, const string& ten_nv, const string& vai_tro,
                        const string& cccd_plaintext, const string& sdt_plaintext,
                        const string& luong_plaintext);
                        

    // Xóa nhân viên theo ID
    bool DeleteNhanVienById(int id);

    // Xóa tất cả nhân viên (cẩn thận!)
    bool DeleteAllNhanVien();


    // Kiểm tra xem nhân viên có tồn tại không
    bool NhanVienExists(int id);

    // Đếm tổng số nhân viên trong DB
    int GetTotalNhanVien();

    // Kiểm tra kết nối còn sống không
    bool IsConnected();

    // Đóng kết nối
    void Disconnect();
};

#endif 
// DATABASE_HELPER_H
