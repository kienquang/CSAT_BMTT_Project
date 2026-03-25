#include "DatabaseHelper.h"
#include <iostream>
#include <sstream>
#include <cstring>
#include "EncryptionConfig.h"

// MySQL Connector/C++ includes
#include "mysql_connection.h"
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>

using namespace std;

// ======== CONSTRUCTOR & DESTRUCTOR ========

DatabaseHelper::DatabaseHelper(const string& host, int port,
                               const string& user, const string& password,
                               const string& database)
    : HOST(host), PORT(port), USER(user), PASSWORD(password), 
      DATABASE(database), connection(nullptr) {
    // Khoi tao tung truong
}

DatabaseHelper::~DatabaseHelper() {
    Disconnect();
}

// CONNECTION MANAGEMENT ========

bool DatabaseHelper::Connect() {
    try {
        sql::Driver* driver = get_driver_instance();
        connection = driver->connect("tcp://" + HOST + ":" + to_string(PORT), USER, PASSWORD);  
        
        sql::Connection* conn = (sql::Connection*)connection;
        conn->setSchema(DATABASE);
        
        cout << "[DATABASE] Da ket noi toi MySQL: " << HOST << ":" << PORT << " [DB: " << DATABASE << "]" << endl;
        return true; 
    } 
    catch (exception& e) {
        cerr << "[ERROR] Loi ket noi MySQL: " << e.what() << endl;
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
        cerr << "[ERROR] Loi thuc thi query: " << e.what() << endl;
        return false;
    }
}

bool DatabaseHelper::ExecuteInsert(const string& query) {
    return ExecuteQuery(query);
}

// ======== TABLE OPERATIONS ========

bool DatabaseHelper::CreateTableNhanVien() {
    string createTableSQL = 
        "CREATE TABLE IF NOT EXISTS NhanVien ( "
        "id INT AUTO_INCREMENT PRIMARY KEY, "
        "ten_nv VARCHAR(100) NOT NULL COMMENT 'Tên nhân viên', "
        "vai_tro VARCHAR(50) NOT NULL CHECK (vai_tro IN ('Admin', 'User')), "
        "cccd_cipher VARCHAR(256) NOT NULL COMMENT 'CCCD mã hóa', "
        "sdt_cipher VARCHAR(256) NOT NULL COMMENT 'Số điện thoại mã hóa', "
        "luong_cipher VARCHAR(256) NOT NULL COMMENT 'Lương mã hóa', "
        "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP, "
        "INDEX idx_vai_tro (vai_tro) "
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;";

    if (!ExecuteQuery(createTableSQL)) {
        cerr << "[ERROR] Không thể tạo bảng NhanVien" << endl;
        return false;
    }
    
    cout << "[DATABASE] Bảng NhanVien đã được tạo/kiểm tra" << endl;
    return true;
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

    // STEP 1: Mã hóa plaintext thành HEX bằng Blowfish
    Blowfish cipher(EncryptionConfig::BLOWFISH_KEY);
    
    string cccd_hex = cipher.EncryptString(cccd_plaintext);
    string sdt_hex = cipher.EncryptString(sdt_plaintext);
    string luong_hex = cipher.EncryptString(luong_plaintext);

    // STEP 2: Xây dựng câu SQL INSERT
    stringstream ss;
    ss << "INSERT INTO NhanVien (ten_nv, vai_tro, cccd, cccd_cipher, sdt, sdt_cipher, luong_cipher) "
       << "VALUES ('"
       << ten_nv << "', '"
       << vai_tro << "', '"
       << cccd_plaintext << "', '"
       << cccd_hex << "', '"
       << sdt_plaintext << "', '"
       << sdt_hex << "', '"
       << luong_hex << "');";
    
    string insertSQL = ss.str();

    // STEP 3: Thực thi câu SQL
    // TODO (B): Replace with real MySQL execution:
    // sql::Connection* conn = (sql::Connection*)connection;
    // sql::Statement* stmt = conn->createStatement();
    // stmt->execute(insertSQL);
    // delete stmt;

    cout << "[DATABASE] Thêm nhân viên: " << ten_nv << " (" << vai_tro << ")" << endl;
    cout << "[DATABASE]   CCCD HEX: " << cccd_hex << endl;
    cout << "[DATABASE]   SDT HEX: " << sdt_hex << endl;
    cout << "[DATABASE]   LƯƠNG HEX: " << luong_hex << endl;

    return true;
}

// ======== QUERY OPERATIONS ========

bool DatabaseHelper::GetNhanVienById(int id, NhanVien& result) {
    if (!IsConnected()) {
        cerr << "[ERROR] Không kết nối được database" << endl;
        return false;
    }

    // STEP 1: Xây dựng SQL SELECT
    stringstream ss;
    ss << "SELECT id, ten_nv, vai_tro, cccd_cipher, sdt_cipher, luong_cipher, created_at "
       << "FROM NhanVien WHERE id = " << id << ";";
    
    string selectSQL = ss.str();

    try {
        // TODO (B): Replace with real MySQL query:
        // sql::Connection* conn = (sql::Connection*)connection;
        // sql::Statement* stmt = conn->createStatement();
        // sql::ResultSet* res = stmt->executeQuery(selectSQL);
        // 
        // if (res->next()) {
        //     result.id = res->getInt("id");
        //     result.ten_nv = res->getString("ten_nv");
        //     result.vai_tro = res->getString("vai_tro");
        //     result.cccd_cipher = res->getString("cccd_cipher");  // Still HEX
        //     result.sdt_cipher = res->getString("sdt_cipher");    // Still HEX
        //     result.luong_cipher = res->getString("luong_cipher");  // Still HEX
        //     result.created_at = res->getString("created_at");
        //     delete res;
        //     delete stmt;
        //     return true;
        // }
        // delete res;
        // delete stmt;

        cout << "[DATABASE] Query nhân viên ID: " << id << endl;
        // TODO (B): Remove the dummy data below after implementing above
        result.id = id;
        result.ten_nv = "Demo Employee";
        result.vai_tro = "Admin";
        result.cccd_cipher = "A3F7B2C1E5F8A4B9A3F7B2C1E5F8A4B9";  // Demo HEX
        result.sdt_cipher = "F1D2E3A4B5C6D7E8F1D2E3A4B5C6D7E8";
        result.luong_cipher = "C5B6A7D8E9F0A1B2C5B6A7D8E9F0A1B2";
        result.created_at = "2026-03-21 10:00:00";

        return true;
    }
    catch (exception& e) {
        cerr << "[ERROR] Lỗi query nhân viên: " << e.what() << endl;
        return false;
    }
}

vector<NhanVien> DatabaseHelper::GetAllNhanVien() {
    vector<NhanVien> employees;

    if (!IsConnected()) {
        cerr << "[ERROR] Không kết nối được database" << endl;
        return employees;
    }

    // TODO (B): Replace with real MySQL query:
    // string selectSQL = "SELECT id, ten_nv, vai_tro, cccd_cipher, sdt_cipher, luong_cipher, created_at "
    //                    "FROM NhanVien ORDER BY id ASC;";
    //
    // sql::Connection* conn = (sql::Connection*)connection;
    // sql::Statement* stmt = conn->createStatement();
    // sql::ResultSet* res = stmt->executeQuery(selectSQL);
    //
    // while (res->next()) {
    //     NhanVien emp;
    //     emp.id = res->getInt("id");
    //     emp.ten_nv = res->getString("ten_nv");
    //     emp.vai_tro = res->getString("vai_tro");
    //     emp.cccd_cipher = res->getString("cccd_cipher");
    //     emp.sdt_cipher = res->getString("sdt_cipher");
    //     emp.luong_cipher = res->getString("luong_cipher");
    //     emp.created_at = res->getString("created_at");
    //     employees.push_back(emp);
    // }

    cout << "[DATABASE] Lấy tất cả nhân viên" << endl;
    return employees;
}

vector<NhanVien> DatabaseHelper::GetNhanVienByRole(const string& vai_tro) {
    vector<NhanVien> employees;

    if (!IsConnected()) {
        cerr << "[ERROR] Không kết nối được database" << endl;
        return employees;
    }

    // TODO (B): Replace with real MySQL query:
    // stringstream ss;
    // ss << "SELECT id, ten_nv, vai_tro, cccd_cipher, sdt_cipher, luong_cipher, created_at "
    //    << "FROM NhanVien WHERE vai_tro = '" << vai_tro << "' ORDER BY id ASC;";
    //
    // sql::Connection* conn = (sql::Connection*)connection;
    // sql::Statement* stmt = conn->createStatement();
    // sql::ResultSet* res = stmt->executeQuery(ss.str());
    //
    // while (res->next()) {
    //     NhanVien emp;
    //     emp.id = res->getInt("id");
    //     emp.ten_nv = res->getString("ten_nv");
    //     emp.vai_tro = res->getString("vai_tro");
    //     emp.cccd_cipher = res->getString("cccd_cipher");
    //     emp.sdt_cipher = res->getString("sdt_cipher");
    //     emp.luong_cipher = res->getString("luong_cipher");
    //     emp.created_at = res->getString("created_at");
    //     employees.push_back(emp);
    // }

    cout << "[DATABASE] Lấy nhân viên theo vai trò: " << vai_tro << endl;
    return employees;
}

// ======== UPDATE OPERATIONS ========

bool DatabaseHelper::UpdateNhanVien(int id, const string& ten_nv, const string& vai_tro,
                                    const string& cccd_plaintext, const string& sdt_plaintext,
                                    const string& luong_plaintext) {
    if (!IsConnected()) {
        cerr << "[ERROR] Không kết nối được database" << endl;
        return false;
    }

    // STEP 1: Mã hóa plaintext
    Blowfish cipher(EncryptionConfig::BLOWFISH_KEY);
    string cccd_hex = cipher.EncryptString(cccd_plaintext);
    string sdt_hex = cipher.EncryptString(sdt_plaintext);
    string luong_hex = cipher.EncryptString(luong_plaintext);

    // STEP 2: Xây dựng SQL UPDATE
    stringstream ss;
    ss << "UPDATE NhanVien SET "
       << "ten_nv = '" << ten_nv << "', "
       << "vai_tro = '" << vai_tro << "', "
       << "cccd = '" << cccd_plaintext << "', "
       << "cccd_cipher = '" << cccd_hex << "', "
       << "sdt = '" << sdt_plaintext << "', "
       << "sdt_cipher = '" << sdt_hex << "', "
       << "luong_cipher = '" << luong_hex << "' "
       << "WHERE id = " << id << ";";

    // TODO (B): Execute update query
    cout << "[DATABASE] Cập nhật nhân viên ID: " << id << endl;
    return true;
}

// ======== DELETE OPERATIONS ========

bool DatabaseHelper::DeleteNhanVienById(int id) {
    if (!IsConnected()) {
        cerr << "[ERROR] Không kết nối được database" << endl;
        return false;
    }

    stringstream ss;
    ss << "DELETE FROM NhanVien WHERE id = " << id << ";";
    
    // TODO (B): Execute delete query
    cout << "[DATABASE] Xóa nhân viên ID: " << id << endl;
    return true;
}

bool DatabaseHelper::DeleteAllNhanVien() {
    if (!IsConnected()) {
        cerr << "[ERROR] Không kết nối được database" << endl;
        return false;
    }

    string deleteSQL = "DELETE FROM NhanVien;";
    
    // TODO (B): Execute delete query
    cout << "[DATABASE] Xóa tất cả nhân viên (CẨN THẬN!)" << endl;
    return true;
}

// ======== UTILITY FUNCTIONS ========

bool DatabaseHelper::NhanVienExists(int id) {
    NhanVien result;
    return GetNhanVienById(id, result);
}

int DatabaseHelper::GetTotalNhanVien() {
    if (!IsConnected()) {
        return 0;
    }

    // TODO (B): Replace with real count query:
    // string countSQL = "SELECT COUNT(*) as total FROM NhanVien;";
    // sql::Connection* conn = (sql::Connection*)connection;
    // sql::Statement* stmt = conn->createStatement();
    // sql::ResultSet* res = stmt->executeQuery(countSQL);
    // if (res->next()) {
    //     int total = res->getInt("total");
    //     delete res;
    //     delete stmt;
    //     return total;
    // }

    return 0;
}
