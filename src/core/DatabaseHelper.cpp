#include "DatabaseHelper.h"
#include "MaskingLogic.h"
#include "Blowfish.h"
#include <iostream>
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
        
        cout << "[DATABASE] Da ket noi toi MySQL: " << HOST << ":" << PORT << " [DB: " << DATABASE << "]" << endl;
        return true; 
    } 
    catch (sql::SQLException &e) {
        cerr << "[ERROR] Loi ket noi MySQL: " << e.what() 
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
        cout << "[DATABASE] Da ngan ket noi" << endl;
    }
}

// ======== QUERY EXECUTION ========

bool DatabaseHelper::ExecuteQuery(const string& query) {
    if (!IsConnected()) {
        cerr << "[ERROR] Khong ket noi duoc voi database" << endl;
        return false;
    }

    try {
        sql::Connection* conn = (sql::Connection*)connection;
        sql::Statement* stmt = conn->createStatement();
        stmt->execute(query);
        delete stmt;
        
        cout << "[DATABASE] Thuc thi query: " << query.substr(0, 50) << "..." << endl;
        return true;
    }
    catch (sql::SQLException &e) {
        cerr << "[ERROR] Loi thuc thi query: " << e.what() 
             << " (Code: " << e.getErrorCode() 
             << ", State: " << e.getSQLState() << ")" << endl;
        return false;
    }
}

// ======== TABLE OPERATIONS ========

bool DatabaseHelper::CreateTableNhanVien() {
    string createTableSQL = 
        "CREATE TABLE IF NOT EXISTS nhanvien ( "
        "id INT AUTO_INCREMENT PRIMARY KEY, "
        "ten_nv VARCHAR(100) NOT NULL COMMENT 'Ten nhan vien', "
        "vai_tro VARCHAR(50) NOT NULL CHECK (vai_tro IN ('Admin', 'User')), "
        "cccd_cipher VARCHAR(256) NOT NULL UNIQUE COMMENT 'CCCD ma hoa (Blowfish)', "
        "matkhau_cipher VARCHAR(256) NOT NULL COMMENT 'Mat khau ma hoa (Blowfish)', "
        "sdt_cipher VARCHAR(256) NOT NULL UNIQUE COMMENT 'So dien thoai ma hoa (Blowfish)', "
        "luong_cipher VARCHAR(256) NOT NULL COMMENT 'Luong ma hoa (Blowfish)', "
        "INDEX idx_vai_tro (vai_tro) "
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;";

    if (!ExecuteQuery(createTableSQL)) {
        return false;
    }

    // Check if default Admin account exists
    try {
        sql::Connection* conn = (sql::Connection*)connection;
        sql::Statement* stmt = conn->createStatement();
        sql::ResultSet* res = stmt->executeQuery("SELECT COUNT(*) as admin_count FROM nhanvien WHERE vai_tro = 'Admin';");
        
        int adminCount = 0;
        if (res->next()) {
            adminCount = res->getInt("admin_count");
        }
        
        delete res;
        delete stmt;
        
        // If no Admin account exists, create default Admin
        if (adminCount == 0) {
            cout << "[DATABASE] No Admin account found, creating default Admin account..." << endl;
            
            // Default Admin credentials
            string defaultAdminName = "Administrator";
            string defaultCCCD = "012345678912";
            string defaultPhone = "0123456789";
            string defaultPassword = "11111111";
            string defaultSalary = "0";
            
            // Encrypt using Blowfish
            Blowfish cipher(EncryptionConfig::GetBlowfishKey());
            string cccd_encrypted = cipher.EncryptString(defaultCCCD);
            string phone_encrypted = cipher.EncryptString(defaultPhone);
            string password_encrypted = cipher.EncryptString(defaultPassword);
            string salary_encrypted = cipher.EncryptString(defaultSalary);
            
            // Insert default Admin
            string insertAdminSQL = 
                "INSERT INTO nhanvien (ten_nv, vai_tro, cccd_cipher, matkhau_cipher, sdt_cipher, luong_cipher) "
                "VALUES ('" + defaultAdminName + "', 'Admin', '" + cccd_encrypted + "', '" + password_encrypted + "', '" + 
                phone_encrypted + "', '" + salary_encrypted + "');";
            
            if (ExecuteQuery(insertAdminSQL)) {
                cout << "[DATABASE] Default Admin account created successfully!" << endl;
                cout << "[DATABASE] Default Admin Credentials:" << endl;
                cout << "           CCCD: " << defaultCCCD << endl;
                cout << "           Password: " << defaultPassword << endl;
            } else {
                cerr << "[ERROR] Failed to create default Admin account" << endl;
            }
        }
        
        return true;
    }
    catch (sql::SQLException &e) {
        cerr << "[ERROR] Error checking/creating default Admin: " << e.what() << endl;
        return false;
    }
}

// ======== ENCRYPTION/DECRYPTION HELPERS ========

void DatabaseHelper::EncryptNhanVienData(const string& cccd, const string& sdt,
                                         const string& matkhau, const string& luong,
                                         string& cccd_encrypted, string& sdt_encrypted,
                                         string& matkhau_encrypted, string& luong_encrypted) {
    Blowfish cipher(EncryptionConfig::GetBlowfishKey());
    cccd_encrypted = cipher.EncryptString(cccd);
    sdt_encrypted = cipher.EncryptString(sdt);
    matkhau_encrypted = cipher.EncryptString(matkhau);
    luong_encrypted = cipher.EncryptString(luong);
}

void DatabaseHelper::DecryptNhanVienData(const string& cccd_encrypted, const string& sdt_encrypted,
                                         const string& matkhau_encrypted, const string& luong_encrypted,
                                         string& cccd_plain, string& sdt_plain,
                                         string& matkhau_plain, string& luong_plain) {
    Blowfish cipher(EncryptionConfig::GetBlowfishKey());
    cccd_plain = cipher.DecryptString(cccd_encrypted);
    sdt_plain = cipher.DecryptString(sdt_encrypted);
    matkhau_plain = cipher.DecryptString(matkhau_encrypted);
    luong_plain = cipher.DecryptString(luong_encrypted);
}

void DatabaseHelper::ApplyClientMasking(string& cccd, string& sdt, string& matkhau, string& luong) {
    // Convert strings to char buffers
    char cccd_buffer[256];
    char sdt_buffer[256];
    char matkhau_buffer[256];
    char luong_buffer[256];
    
    strcpy(cccd_buffer, cccd.c_str());
    strcpy(sdt_buffer, sdt.c_str());
    strcpy(matkhau_buffer, matkhau.c_str());
    strcpy(luong_buffer, luong.c_str());
    
    // Apply masking
    MaskingLogic::MaskCCCDForClient(cccd_buffer, sizeof(cccd_buffer));
    MaskingLogic::MaskPhoneForClient(sdt_buffer, sizeof(sdt_buffer));
    MaskingLogic::MaskToThreeStarForClient(matkhau_buffer, sizeof(matkhau_buffer));
    MaskingLogic::MaskToThreeStarForClient(luong_buffer, sizeof(luong_buffer));
    
    // Convert back to strings
    cccd = string(cccd_buffer);
    sdt = string(sdt_buffer);
    matkhau = string(matkhau_buffer);
    luong = string(luong_buffer);
}

void DatabaseHelper::ProcessNhanVienFieldsForClient(const string& cccd_encrypted, const string& sdt_encrypted,
                                                    const string& matkhau_encrypted, const string& luong_encrypted,
                                                    string& cccd_masked, string& sdt_masked,
                                                    string& matkhau_masked, string& luong_masked) {
    // Decrypt
    string cccd_plain, sdt_plain, matkhau_plain, luong_plain;
    DecryptNhanVienData(cccd_encrypted, sdt_encrypted, matkhau_encrypted, luong_encrypted,
                        cccd_plain, sdt_plain, matkhau_plain, luong_plain);
    
    // Apply masking
    ApplyClientMasking(cccd_plain, sdt_plain, matkhau_plain, luong_plain);
    
    // Return masked data
    cccd_masked = cccd_plain;
    sdt_masked = sdt_plain;
    matkhau_masked = matkhau_plain;
    luong_masked = luong_plain;
}

// ======== INSERT OPERATIONS ========

bool DatabaseHelper::InsertNhanVien(const string& ten_nv, const string& vai_tro,
                                    const string& cccd_plaintext, 
                                    const string& sdt_plaintext,
                                    const string& matkhau_plaintext,
                                    const string& luong_plaintext) {
    if (!IsConnected()) {
        cerr << "[ERROR] Khong ket noi duoc database" << endl;
        return false;
    }

    // ======== VALIDATION ========
    // CCCD phai chinh xac 12 ky tu
    if (cccd_plaintext.length() != 12) {
        cerr << "[ERROR] CCCD phai chinh xac 12 ky tu (hien tai: " << cccd_plaintext.length() << " ky tu)" << endl;
        return false;
    }

    // SDT phai it nhat 10 ky tu
    if (sdt_plaintext.length() < 10) {
        cerr << "[ERROR] So dien thoai phai it nhat 10 ky tu (hien tai: " << sdt_plaintext.length() << " ky tu)" << endl;
        return false;
    }
    // Mat khau phai it nhat 8 ky tu
    if (matkhau_plaintext.length() < 8) {
        cerr << "[ERROR] Mat khau phai it nhat 8 ky tu (hien tai: " << matkhau_plaintext.length() << " ky tu)" << endl;
        return false;
    }

    // Luong phai it nhat 7 ky tu
    if (luong_plaintext.length() < 7) {
        cerr << "[ERROR] Luong phai it nhat 7 ky tu (hien tai: " << luong_plaintext.length() << " ky tu)" << endl;
        return false;
    }

    // Ma hoa using helper
    string cccd_hex, sdt_hex, matkhau_hex, luong_hex;
    EncryptNhanVienData(cccd_plaintext, sdt_plaintext, matkhau_plaintext, luong_plaintext,
                       cccd_hex, sdt_hex, matkhau_hex, luong_hex);

    try {
        sql::Connection* conn = (sql::Connection*)connection;
        sql::PreparedStatement* pstmt = conn->prepareStatement(
            "INSERT INTO nhanvien (ten_nv, vai_tro, cccd_cipher, matkhau_cipher, sdt_cipher, luong_cipher) "
            "VALUES (?, ?, ?, ?, ?, ?)"
        );

        pstmt->setString(1, ten_nv);
        pstmt->setString(2, vai_tro);
        pstmt->setString(3, cccd_hex);             // Encrypted CCCD
        pstmt->setString(4, matkhau_hex);          // Encrypted Password
        pstmt->setString(5, sdt_hex);              // Encrypted SDT
        pstmt->setString(6, luong_hex);            // Encrypted Salary

        pstmt->execute();
        delete pstmt;
        
        // COMMIT the transaction
        conn->commit();

        cout << "[DATABASE] Them nhan vien: " << ten_nv << " (" << vai_tro << ")" << endl;
        cout << "[DATABASE]   CCCD HEX: " << cccd_hex << endl;
        cout << "[DATABASE]   SDT: " << sdt_plaintext << endl;
        cout << "[DATABASE]   SDT HEX: " << sdt_hex << endl;
        return true;
    }
    catch (sql::SQLException &e) {
        cerr << "[ERROR] Loi them nhan vien: " << e.what() 
             << " (Code: " << e.getErrorCode() << ")" << endl;
        return false;
    }
}

// ======== UPDATE OPERATIONS ========

bool DatabaseHelper::UpdateNhanVien(int id, const string& ten_nv, const string& vai_tro,
                                    const string& cccd_plaintext, const string& sdt_plaintext,
                                    const string& matkhau_plaintext, const string& luong_plaintext) {
    if (!IsConnected()) {
        cerr << "[ERROR] Khong ket noi duoc database" << endl;
        return false;
    }

    // Ma hoa using helper
    string cccd_hex, sdt_hex, matkhau_hex, luong_hex;
    EncryptNhanVienData(cccd_plaintext, sdt_plaintext, matkhau_plaintext, luong_plaintext,
                       cccd_hex, sdt_hex, matkhau_hex, luong_hex);

    try {
        sql::Connection* conn = (sql::Connection*)connection;
        sql::PreparedStatement* pstmt = conn->prepareStatement(
            "UPDATE nhanvien SET ten_nv = ?, vai_tro = ?, cccd_cipher = ?, "
            "sdt_cipher = ?, matkhau_cipher = ?, luong_cipher = ? WHERE id = ?"
        );

        pstmt->setString(1, ten_nv);
        pstmt->setString(2, vai_tro);
        pstmt->setString(3, cccd_hex);             // Encrypted CCCD
        pstmt->setString(4, sdt_hex);              // Encrypted SDT
        pstmt->setString(5, matkhau_hex);          // Encrypted Password
        pstmt->setString(6, luong_hex);            // Encrypted Salary
        pstmt->setInt(7, id);

        pstmt->execute();
        delete pstmt;
        
        // COMMIT the transaction
        conn->commit();

        cout << "[DATABASE] Cap nhat nhan vien ID: " << id << " thanh cong" << endl;
        return true;
    }
    catch (sql::SQLException &e) {
        cerr << "[ERROR] Loi cap nhat nhan vien: " << e.what()
             << " (Code: " << e.getErrorCode() << ")" << endl;
        return false;
    }
}

// ======== DELETE OPERATIONS ========

bool DatabaseHelper::DeleteNhanVienById(int id) {
    if (!IsConnected()) {
        cerr << "[ERROR] Khong ket noi duoc database" << endl;
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

        cout << "[DATABASE] Xoa nhan vien ID: " << id << " thanh cong" << endl;
        return true;
    }
    catch (sql::SQLException &e) {
        cerr << "[ERROR] Loi xoa nhan vien: " << e.what() 
             << " (Code: " << e.getErrorCode() << ")" << endl;
        return false;
    }
}

// ======== UTILITY FUNCTIONS ========

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

// ======== CLIENT PAYLOAD QUERY OPERATIONS ========

vector<nhanvien> DatabaseHelper::GetAllNhanVienForClient() {
    vector<nhanvien> employees;

    if (!IsConnected()) {
        cerr << "[ERROR] Khong ket noi duoc database" << endl;
        return employees;
    }

    try {
        sql::Connection* conn = (sql::Connection*)connection;
        sql::Statement* stmt = conn->createStatement();
        sql::ResultSet* res = stmt->executeQuery(
            "SELECT id, ten_nv, vai_tro, cccd_cipher, matkhau_cipher, sdt_cipher, luong_cipher "
            "FROM nhanvien ORDER BY id ASC;"
        );

        while (res->next()) {
            nhanvien emp;
            emp.id = res->getInt("id");
            emp.ten_nv = res->getString("ten_nv");
            emp.vai_tro = res->getString("vai_tro");
            
            // Get encrypted data
            string cccd_encrypted = res->getString("cccd_cipher");
            string sdt_encrypted = res->getString("sdt_cipher");
            string matkhau_encrypted = res->getString("matkhau_cipher");
            string luong_encrypted = res->getString("luong_cipher");
            
            // Process: decrypt + mask using helper
            string cccd_masked, sdt_masked, matkhau_masked, luong_masked;
            ProcessNhanVienFieldsForClient(cccd_encrypted, sdt_encrypted, matkhau_encrypted, luong_encrypted,
                                           cccd_masked, sdt_masked, matkhau_masked, luong_masked);
            
            emp.cccd_cipher = cccd_masked;
            emp.sdt_cipher = sdt_masked;
            emp.matkhau_cipher = matkhau_masked;
            emp.luong_cipher = luong_masked;
            
            employees.push_back(emp);
        }

        delete res;
        delete stmt;
        cout << "[DATABASE] Lay tat ca nhan vien masked cho client: " 
             << employees.size() << " bản ghi" << endl;
        return employees;
    }
    catch (sql::SQLException &e) {
        cerr << "[ERROR] Lỗi lấy tất cả nhân viên: " << e.what() 
             << " (Code: " << e.getErrorCode() << ")" << endl;
        return employees;
    }
}

// ======== AUTHENTICATION ========

nhanvien DatabaseHelper::AuthenticateUser(const string& cccd_plaintext, const string& matkhau_plaintext) {
    nhanvien result;
    result.id = -1;  // Default failed state
    
    if (!IsConnected()) {
        cerr << "[ERROR] Database not connected" << endl;
        return result;
    }
    
    try {
        sql::Connection* conn = (sql::Connection*)connection;
        sql::Statement* stmt = conn->createStatement();
        sql::ResultSet* res = stmt->executeQuery(
            "SELECT id, ten_nv, vai_tro, cccd_cipher, matkhau_cipher, sdt_cipher, luong_cipher "
            "FROM nhanvien ORDER BY id ASC;"
        );
        
        while (res->next()) {
            // Get encrypted data
            string cccd_encrypted = res->getString("cccd_cipher");
            string matkhau_encrypted = res->getString("matkhau_cipher");
            string sdt_encrypted = res->getString("sdt_cipher");
            string luong_encrypted = res->getString("luong_cipher");
            
            // Decrypt using helper
            string cccd_decrypted, matkhau_decrypted, sdt_decrypted, luong_decrypted;
            DecryptNhanVienData(cccd_encrypted, sdt_encrypted, matkhau_encrypted, luong_encrypted,
                               cccd_decrypted, sdt_decrypted, matkhau_decrypted, luong_decrypted);
            
            // Compare with input
            if (cccd_plaintext == cccd_decrypted && matkhau_plaintext == matkhau_decrypted) {
                // Found matching user
                result.id = res->getInt("id");
                result.ten_nv = res->getString("ten_nv");
                result.vai_tro = res->getString("vai_tro");
                result.cccd_cipher = cccd_encrypted;
                result.sdt_cipher = sdt_encrypted;
                result.matkhau_cipher = matkhau_encrypted;
                result.luong_cipher = luong_encrypted;
                
                cout << "[AUTH] User authenticated: ID=" << result.id 
                     << ", Name=" << result.ten_nv 
                     << ", Role=" << result.vai_tro << endl;
                
                delete res;
                delete stmt;
                return result;
            }
        }
        
        // Not found
        cerr << "[AUTH] Authentication failed for CCCD: " << cccd_plaintext << endl;
        delete res;
        delete stmt;
        return result;
    }
    catch (sql::SQLException &e) {
        cerr << "[ERROR] Authentication query error: " << e.what() 
             << " (Code: " << e.getErrorCode() << ")" << endl;
        return result;
    }
}
