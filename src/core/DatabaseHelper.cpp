#include "DatabaseHelper.h"
#include "MaskingLogic.h"
#include "Blowfish.h"
#include <iostream>
#include <sstream>
#include <cstring>
#include <vector>
#include <string>

// MySQL Connector/C++ includes
#include "mysql_connection.h"
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>

using namespace std;

// ======== CONSTRUCTOR & DESTRUCTOR ========

DatabaseHelper::DatabaseHelper(const string& host, int port,
                               const string& user, const string& password,
                               const string& database)
    : HOST(host), PORT(port), USER(user), PASSWORD(password), 
      DATABASE(database), connection(nullptr) {
}

DatabaseHelper::~DatabaseHelper() {
    Disconnect();
}

// ======== CONNECTION MANAGEMENT ========

bool DatabaseHelper::Connect() {
    try {
        sql::Driver* driver = get_driver_instance();
        connection = driver->connect("tcp://" + HOST + ":" + to_string(PORT), USER, PASSWORD);
        
        sql::Connection* conn = (sql::Connection*)connection;
        conn->setSchema(DATABASE);
        conn->setAutoCommit(true);  // Enable autocommit to ensure data visibility
        
        cout << "[DATABASE] Đã kết nối tới MySQL: " << HOST << ":" << PORT << " [DB: " << DATABASE << "]" << endl;
        return true; 
    } 
    catch (sql::SQLException &e) {
        cerr << "[ERROR] Lỗi kết nối MySQL: " << e.what() 
             << " (Code: " << e.getErrorCode() 
             << ", State: " << e.getSQLState() << ")" << endl;
        return false;
    }
}

bool DatabaseHelper::IsConnected() {
    return connection != nullptr;
}

void DatabaseHelper::Disconnect() {
    if (connection != nullptr) {
        sql::Connection* conn = (sql::Connection*)connection;
        delete conn;
        connection = nullptr;
        cout << "[DATABASE] Đã ngắt kết nối" << endl;
    }
}

// ======== QUERY EXECUTION ========

bool DatabaseHelper::ExecuteQuery(const string& query) {
    if (!IsConnected()) {
        cerr << "[ERROR] Không kết nối được với database" << endl;
        return false;
    }

    try {
        sql::Connection* conn = (sql::Connection*)connection;
        sql::Statement* stmt = conn->createStatement();
        stmt->execute(query);
        delete stmt;
        
        cout << "[DATABASE] Thực thi query: " << query.substr(0, 50) << "..." << endl;
        return true;
    }
    catch (sql::SQLException &e) {
        cerr << "[ERROR] Lỗi thực thi query: " << e.what() 
             << " (Code: " << e.getErrorCode() 
             << ", State: " << e.getSQLState() << ")" << endl;
        return false;
    }
}

bool DatabaseHelper::ExecuteInsert(const string& query) {
    return ExecuteQuery(query);
}

// ======== TABLE OPERATIONS ========

bool DatabaseHelper::CreateTableNhanVien() {
    string createTableSQL = 
        "CREATE TABLE IF NOT EXISTS nhanvien ( "
        "id INT AUTO_INCREMENT PRIMARY KEY, "
        "ten_nv VARCHAR(100) NOT NULL COMMENT 'Tên nhân viên', "
        "vai_tro VARCHAR(50) NOT NULL CHECK (vai_tro IN ('Admin', 'User')), "
        "cccd_cipher VARCHAR(256) NOT NULL COMMENT 'CCCD mã hóa', "
        "sdt_cipher VARCHAR(256) NOT NULL COMMENT 'Số điện thoại mã hóa', "
        "luong_cipher VARCHAR(256) NOT NULL COMMENT 'Lương mã hóa', "
        "INDEX idx_vai_tro (vai_tro) "
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;";

    return ExecuteQuery(createTableSQL);
}

// ======== INSERT OPERATIONS ========

bool DatabaseHelper::InsertNhanVien(const string& ten_nv, const string& vai_tro,
                                    const string& cccd_plaintext, 
                                    const string& sdt_plaintext,
                                    const string& luong_plaintext) {
    if (!IsConnected()) {
        cerr << "[ERROR] Không kết nối được database" << endl;
        return false;
    }

    // ======== VALIDATION ========
    // CCCD phải chính xác 12 ký tự
    if (cccd_plaintext.length() != 12) {
        cerr << "[ERROR] CCCD phải chính xác 12 ký tự (hiện tại: " << cccd_plaintext.length() << " ký tự)" << endl;
        return false;
    }

    // SDT phải ít nhất 10 ký tự
    if (sdt_plaintext.length() < 10) {
        cerr << "[ERROR] Số điện thoại phải ít nhất 10 ký tự (hiện tại: " << sdt_plaintext.length() << " ký tự)" << endl;
        return false;
    }

    // Lương phải ít nhất 7 ký tự
    if (luong_plaintext.length() < 7) {
        cerr << "[ERROR] Lương phải ít nhất 7 ký tự (hiện tại: " << luong_plaintext.length() << " ký tự)" << endl;
        return false;
    }

    // Mã hóa
    Blowfish cipher("MatMaHoc@NIST2025");
    string cccd_hex = cipher.EncryptString(cccd_plaintext);
    string sdt_hex = cipher.EncryptString(sdt_plaintext);
    string luong_hex = cipher.EncryptString(luong_plaintext);

    try {
        sql::Connection* conn = (sql::Connection*)connection;
        sql::PreparedStatement* pstmt = conn->prepareStatement(
            "INSERT INTO nhanvien (ten_nv, vai_tro, cccd_cipher, sdt_cipher, luong_cipher) "
            "VALUES (?, ?, ?, ?, ?)"
        );

        pstmt->setString(1, ten_nv);
        pstmt->setString(2, vai_tro);
        pstmt->setString(3, cccd_hex);
        pstmt->setString(4, sdt_hex);
        pstmt->setString(5, luong_hex);

        pstmt->execute();
        delete pstmt;
        
        // COMMIT the transaction
        conn->commit();

        cout << "[DATABASE] Thêm nhân viên: " << ten_nv << " (" << vai_tro << ")" << endl;
        cout << "[DATABASE]   CCCD HEX: " << cccd_hex << endl;
        cout << "[DATABASE]   SDT HEX: " << sdt_hex << endl;
        cout << "[DATABASE]   LƯƠNG HEX: " << luong_hex << endl;
        return true;
    }
    catch (sql::SQLException &e) {
        cerr << "[ERROR] Lỗi thêm nhân viên: " << e.what() 
             << " (Code: " << e.getErrorCode() << ")" << endl;
        return false;
    }
}

// ======== QUERY OPERATIONS ========

bool DatabaseHelper::GetNhanVienById(int id, nhanvien& result) {
    if (!IsConnected()) {
        cerr << "[ERROR] Không kết nối được database" << endl;
        return false;
    }

    try {
        sql::Connection* conn = (sql::Connection*)connection;
        sql::PreparedStatement* pstmt = conn->prepareStatement(
            "SELECT id, ten_nv, vai_tro, cccd_cipher, sdt_cipher, luong_cipher "
            "FROM nhanvien WHERE id = ?"
        );
        pstmt->setInt(1, id);

        sql::ResultSet* res = pstmt->executeQuery();

        if (res->next()) {
            result.id = res->getInt("id");
            result.ten_nv = res->getString("ten_nv");
            result.vai_tro = res->getString("vai_tro");
            
            // Get encrypted data and decrypt first, then mask
            string cccd_encrypted = res->getString("cccd_cipher");
            string sdt_encrypted = res->getString("sdt_cipher");
            string luong_encrypted = res->getString("luong_cipher");
            
            // Decrypt using Blowfish
            Blowfish cipher("MatMaHoc@NIST2025");
            string cccd_plain = cipher.DecryptString(cccd_encrypted);
            string sdt_plain = cipher.DecryptString(sdt_encrypted);
            string luong_plain = cipher.DecryptString(luong_encrypted);
            
            // Convert to char array and apply masking on plaintext
            char cccd_buffer[256];
            char sdt_buffer[256];
            char luong_buffer[256];
            
            strcpy(cccd_buffer, cccd_plain.c_str());
            strcpy(sdt_buffer, sdt_plain.c_str());
            strcpy(luong_buffer, luong_plain.c_str());
            
            // Now mask the plaintext data
            MaskingLogic::MaskCCCD(cccd_buffer, sizeof(cccd_buffer));
            MaskingLogic::MaskPhone(sdt_buffer, sizeof(sdt_buffer));
            MaskingLogic::MaskSalary(luong_buffer, sizeof(luong_buffer));
            
            result.cccd_cipher = string(cccd_buffer);
            result.sdt_cipher = string(sdt_buffer);
            result.luong_cipher = string(luong_buffer);

            delete res;
            delete pstmt;
            cout << "[DATABASE] Query nhân viên ID: " << id << " → Tìm thấy" << endl;
            return true;
        }

        delete res;
        delete pstmt;
        cout << "[DATABASE] Query nhân viên ID: " << id << " → Không tìm thấy" << endl;
        return false;
    }
    catch (sql::SQLException &e) {
        cerr << "[ERROR] Lỗi query nhân viên: " << e.what() 
             << " (Code: " << e.getErrorCode() << ")" << endl;
        return false;
    }
}

vector<nhanvien> DatabaseHelper::GetAllNhanVien() {
    vector<nhanvien> employees;

    if (!IsConnected()) {
        cerr << "[ERROR] Không kết nối được database" << endl;
        return employees;
    }

    try {
        sql::Connection* conn = (sql::Connection*)connection;
        sql::Statement* stmt = conn->createStatement();
        sql::ResultSet* res = stmt->executeQuery(
            "SELECT id, ten_nv, vai_tro, cccd_cipher, sdt_cipher, luong_cipher "
            "FROM nhanvien ORDER BY id ASC;"
        );

        while (res->next()) {
            nhanvien emp;
            emp.id = res->getInt("id");
            emp.ten_nv = res->getString("ten_nv");
            emp.vai_tro = res->getString("vai_tro");
            
            // Get encrypted data and decrypt first, then mask
            string cccd_encrypted = res->getString("cccd_cipher");
            string sdt_encrypted = res->getString("sdt_cipher");
            string luong_encrypted = res->getString("luong_cipher");
            
            // Decrypt using Blowfish
            Blowfish cipher("MatMaHoc@NIST2025");
            string cccd_plain = cipher.DecryptString(cccd_encrypted);
            string sdt_plain = cipher.DecryptString(sdt_encrypted);
            string luong_plain = cipher.DecryptString(luong_encrypted);
            
            // Convert to char array and apply masking on plaintext
            char cccd_buffer[256];
            char sdt_buffer[256];
            char luong_buffer[256];
            
            strcpy(cccd_buffer, cccd_plain.c_str());
            strcpy(sdt_buffer, sdt_plain.c_str());
            strcpy(luong_buffer, luong_plain.c_str());
            
            // Now mask the plaintext data
            MaskingLogic::MaskCCCD(cccd_buffer, sizeof(cccd_buffer));
            MaskingLogic::MaskPhone(sdt_buffer, sizeof(sdt_buffer));
            MaskingLogic::MaskSalary(luong_buffer, sizeof(luong_buffer));
            
            emp.cccd_cipher = string(cccd_buffer);
            emp.sdt_cipher = string(sdt_buffer);
            emp.luong_cipher = string(luong_buffer);
            
            employees.push_back(emp);
        }

        delete res;
        delete stmt;
        cout << "[DATABASE] Lấy tất cả nhân viên: " << employees.size() << " bản ghi" << endl;
        return employees;
    }
    catch (sql::SQLException &e) {
        cerr << "[ERROR] Lỗi lấy tất cả nhân viên: " << e.what() 
             << " (Code: " << e.getErrorCode() << ")" << endl;
        return employees;
    }
}

vector<nhanvien> DatabaseHelper::GetNhanVienByRole(const string& vai_tro) {
    vector<nhanvien> employees;

    if (!IsConnected()) {
        cerr << "[ERROR] Không kết nối được database" << endl;
        return employees;
    }

    try {
        sql::Connection* conn = (sql::Connection*)connection;
        sql::PreparedStatement* pstmt = conn->prepareStatement(
            "SELECT id, ten_nv, vai_tro, cccd_cipher, sdt_cipher, luong_cipher "
            "FROM nhanvien WHERE vai_tro = ? ORDER BY id ASC;"
        );
        pstmt->setString(1, vai_tro);

        sql::ResultSet* res = pstmt->executeQuery();

        while (res->next()) {
            nhanvien emp;
            emp.id = res->getInt("id");
            emp.ten_nv = res->getString("ten_nv");
            emp.vai_tro = res->getString("vai_tro");
            
            // Get encrypted data and decrypt first, then mask
            string cccd_encrypted = res->getString("cccd_cipher");
            string sdt_encrypted = res->getString("sdt_cipher");
            string luong_encrypted = res->getString("luong_cipher");
            
            // Decrypt using Blowfish
            Blowfish cipher("MatMaHoc@NIST2025");
            string cccd_plain = cipher.DecryptString(cccd_encrypted);
            string sdt_plain = cipher.DecryptString(sdt_encrypted);
            string luong_plain = cipher.DecryptString(luong_encrypted);
            
            // Convert to char array and apply masking on plaintext
            char cccd_buffer[256];
            char sdt_buffer[256];
            char luong_buffer[256];
            
            strcpy(cccd_buffer, cccd_plain.c_str());
            strcpy(sdt_buffer, sdt_plain.c_str());
            strcpy(luong_buffer, luong_plain.c_str());
            
            // Now mask the plaintext data
            MaskingLogic::MaskCCCD(cccd_buffer, sizeof(cccd_buffer));
            MaskingLogic::MaskPhone(sdt_buffer, sizeof(sdt_buffer));
            MaskingLogic::MaskSalary(luong_buffer, sizeof(luong_buffer));
            
            emp.cccd_cipher = string(cccd_buffer);
            emp.sdt_cipher = string(sdt_buffer);
            emp.luong_cipher = string(luong_buffer);
            
            employees.push_back(emp);
        }

        delete res;
        delete pstmt;
        cout << "[DATABASE] Lấy nhân viên theo vai trò '" << vai_tro 
             << "': " << employees.size() << " bản ghi" << endl;
        return employees;
    }
    catch (sql::SQLException &e) {
        cerr << "[ERROR] Lỗi lấy nhân viên theo vai trò: " << e.what() 
             << " (Code: " << e.getErrorCode() << ")" << endl;
        return employees;
    }
}

// ======== UPDATE OPERATIONS ========

bool DatabaseHelper::UpdateNhanVien(int id, const string& ten_nv, const string& vai_tro,
                                    const string& cccd_plaintext, const string& sdt_plaintext,
                                    const string& luong_plaintext) {
    if (!IsConnected()) {
        cerr << "[ERROR] Không kết nối được database" << endl;
        return false;
    }

    Blowfish cipher("MatMaHoc@NIST2025");
    string cccd_hex = cipher.EncryptString(cccd_plaintext);
    string sdt_hex = cipher.EncryptString(sdt_plaintext);
    string luong_hex = cipher.EncryptString(luong_plaintext);

    try {
        sql::Connection* conn = (sql::Connection*)connection;
        sql::PreparedStatement* pstmt = conn->prepareStatement(
            "UPDATE nhanvien SET ten_nv = ?, vai_tro = ?, cccd_cipher = ?, "
            "sdt_cipher = ?, luong_cipher = ? WHERE id = ?"
        );

        pstmt->setString(1, ten_nv);
        pstmt->setString(2, vai_tro);
        pstmt->setString(3, cccd_hex);
        pstmt->setString(4, sdt_hex);
        pstmt->setString(5, luong_hex);
        pstmt->setInt(6, id);

        pstmt->execute();
        delete pstmt;
        
        // COMMIT the transaction
        conn->commit();

        cout << "[DATABASE] Cập nhật nhân viên ID: " << id << " thành công" << endl;
        return true;
    }
    catch (sql::SQLException &e) {
        cerr << "[ERROR] Lỗi cập nhật nhân viên: " << e.what() 
             << " (Code: " << e.getErrorCode() << ")" << endl;
        return false;
    }
}

// ======== DELETE OPERATIONS ========

bool DatabaseHelper::DeleteNhanVienById(int id) {
    if (!IsConnected()) {
        cerr << "[ERROR] Không kết nối được database" << endl;
        return false;
    }

    try {
        sql::Connection* conn = (sql::Connection*)connection;
        sql::PreparedStatement* pstmt = conn->prepareStatement(
            "DELETE FROM nhanvien WHERE id = ?"
        );
        pstmt->setInt(1, id);
        pstmt->execute();
        delete pstmt;
        
        // COMMIT the transaction
        conn->commit();

        cout << "[DATABASE] Xóa nhân viên ID: " << id << " thành công" << endl;
        return true;
    }
    catch (sql::SQLException &e) {
        cerr << "[ERROR] Lỗi xóa nhân viên: " << e.what() 
             << " (Code: " << e.getErrorCode() << ")" << endl;
        return false;
    }
}

bool DatabaseHelper::DeleteAllNhanVien() {
    if (!IsConnected()) {
        cerr << "[ERROR] Không kết nối được database" << endl;
        return false;
    }

    cout << "[DATABASE] Xóa tất cả nhân viên (CẨN THẬN!)" << endl;
    string deleteSQL = "DELETE FROM nhanvien;";
    return ExecuteQuery(deleteSQL);
}

// ======== UTILITY FUNCTIONS ========

bool DatabaseHelper::NhanVienExists(int id) {
    nhanvien result;
    return GetNhanVienById(id, result);
}

int DatabaseHelper::GetTotalNhanVien() {
    if (!IsConnected()) {
        return 0;
    }

    try {
        sql::Connection* conn = (sql::Connection*)connection;
        sql::Statement* stmt = conn->createStatement();
        sql::ResultSet* res = stmt->executeQuery("SELECT COUNT(*) as total FROM nhanvien;");

        int total = 0;
        if (res->next()) {
            total = res->getInt("total");
        }

        delete res;
        delete stmt;
        return total;
    }
    catch (sql::SQLException &e) {
        cerr << "[ERROR] Lỗi đếm nhân viên: " << e.what() 
             << " (Code: " << e.getErrorCode() << ")" << endl;
        return 0;
    }
}