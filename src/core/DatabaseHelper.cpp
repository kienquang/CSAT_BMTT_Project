#include "DatabaseHelper.h"

#include "Base64.h"
#include "Blowfish.h"
#include "PasswordHasher.h"
#include "SecureRandom.h"

#include <cctype>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>

#include "mysql_connection.h"
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>

using namespace std;

namespace {

constexpr int kRoleUser = 1;
constexpr int kRoleAdmin = 2;

void RollbackQuietly(sql::Connection* conn) {
    if (conn == nullptr) {
        return;
    }

    try {
        conn->rollback();
    } catch (...) {
    }
}

bool IsDigitsOnly(const string& value) {
    for (char ch : value) {
        if (ch < '0' || ch > '9') {
            return false;
        }
    }
    return !value.empty();
}

bool LooksLikeEmail(const string& value) {
    const size_t atPos = value.find('@');
    if (atPos == string::npos || atPos == 0 || atPos == value.size() - 1) {
        return false;
    }

    return value.find('.', atPos) != string::npos;
}

bool IsValidRoleValue(int role) {
    return role == kRoleUser || role == kRoleAdmin;
}

bool IsHexCipherText(const string& value) {
    if (value.empty() || (value.size() % 16) != 0) {
        return false;
    }

    for (const unsigned char ch : value) {
        if (!isxdigit(ch)) {
            return false;
        }
    }

    return true;
}

string DecodeCipherForBlowfish(const string& storedValue) {
    string decoded;
    if (Base64::Decode(storedValue, decoded) && IsHexCipherText(decoded)) {
        return decoded;
    }

    // Backward compatibility: keep supporting legacy hex ciphertext already in DB.
    return storedValue;
}

}  // namespace

DatabaseHelper::DatabaseHelper(const string& host, int port,
                               const string& user, const string& password,
                               const string& database)
    : HOST(host),
      PORT(port),
      USER(user),
      PASSWORD(password),
      DATABASE(database),
      connection(nullptr) {
}

DatabaseHelper::~DatabaseHelper() {
    Disconnect();
}

bool DatabaseHelper::Connect() {
    try {
        sql::Driver* driver = get_driver_instance();
        connection = driver->connect("tcp://" + HOST + ":" + to_string(PORT), USER, PASSWORD);

        sql::Connection* conn = static_cast<sql::Connection*>(connection);
        conn->setSchema(DATABASE);
        conn->setAutoCommit(true);

        cout << "[DATABASE] Connected to MySQL " << HOST << ":" << PORT
             << " [DB: " << DATABASE << "]" << endl;
        return true;
    } catch (sql::SQLException& e) {
        cerr << "[ERROR] MySQL connection failed: " << e.what()
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
        sql::Connection* conn = static_cast<sql::Connection*>(connection);
        delete conn;
        connection = nullptr;
        cout << "[DATABASE] Disconnected" << endl;
    }
}

bool DatabaseHelper::ExecuteQuery(const string& query) {
    if (!IsConnected()) {
        cerr << "[ERROR] Database is not connected" << endl;
        return false;
    }

    try {
        sql::Connection* conn = static_cast<sql::Connection*>(connection);
        unique_ptr<sql::Statement> stmt(conn->createStatement());
        stmt->execute(query);
        return true;
    } catch (sql::SQLException& e) {
        cerr << "[ERROR] Query failed: " << e.what()
             << " (Code: " << e.getErrorCode()
             << ", State: " << e.getSQLState() << ")" << endl;
        return false;
    }
}

string DatabaseHelper::GenerateRandomDek() const {
    const vector<uint8_t> randomBytes = SecureRandom::RandomBytes(32);

    stringstream stream;
    stream << hex << setfill('0');
    for (uint8_t byte : randomBytes) {
        stream << setw(2) << static_cast<unsigned int>(byte);
    }
    return stream.str();
}

string DatabaseHelper::WrapDek(const string& dekPlaintext, const string& kek) const {
    Blowfish cipher(kek);
    return Base64::Encode(cipher.EncryptString(dekPlaintext));
}

string DatabaseHelper::UnwrapDek(const string& encryptedDek, const string& kek) const {
    Blowfish cipher(kek);
    const string dekCipherHex = DecodeCipherForBlowfish(encryptedDek);
    return cipher.DecryptString(dekCipherHex);
}

void DatabaseHelper::EncryptPersonalData(const string& dekPlaintext,
                                         const string& cccdPlaintext,
                                         const string& phonePlaintext,
                                         const string& emailPlaintext,
                                         string& cccdCiphertext,
                                         string& phoneCiphertext,
                                         string& emailCiphertext) const {
    Blowfish cipher(dekPlaintext);
    cccdCiphertext = Base64::Encode(cipher.EncryptString(cccdPlaintext));
    phoneCiphertext = Base64::Encode(cipher.EncryptString(phonePlaintext));
    emailCiphertext = Base64::Encode(cipher.EncryptString(emailPlaintext));
}

void DatabaseHelper::DecryptPersonalData(const string& dekPlaintext,
                                         const string& cccdCiphertext,
                                         const string& phoneCiphertext,
                                         const string& emailCiphertext,
                                         string& cccdPlaintext,
                                         string& phonePlaintext,
                                         string& emailPlaintext) const {
    Blowfish cipher(dekPlaintext);

    const string cccdCipherHex = DecodeCipherForBlowfish(cccdCiphertext);
    const string phoneCipherHex = DecodeCipherForBlowfish(phoneCiphertext);
    const string emailCipherHex = DecodeCipherForBlowfish(emailCiphertext);

    cccdPlaintext = cipher.DecryptString(cccdCipherHex);
    phonePlaintext = cipher.DecryptString(phoneCipherHex);
    emailPlaintext = cipher.DecryptString(emailCipherHex);
}

bool DatabaseHelper::InitializeSchema() {
    const string createUsersTable =
        "CREATE TABLE IF NOT EXISTS users ("
        "id INT AUTO_INCREMENT PRIMARY KEY, "
        "username VARCHAR(255) NOT NULL UNIQUE, "
        "password_hash VARCHAR(255) NOT NULL, "
        "role TINYINT NOT NULL DEFAULT 1, "
        "INDEX idx_users_username (username)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;";

    const string createPersonalRecordsTable =
        "CREATE TABLE IF NOT EXISTS personal_records ("
        "id INT AUTO_INCREMENT PRIMARY KEY, "
        "user_id INT NOT NULL, "
        "Gender TINYINT UNSIGNED NOT NULL, "
        "encrypted_dek VARCHAR(255) NOT NULL, "
        "CCCD_cipher TEXT NOT NULL, "
        "SDT_cipher TEXT NOT NULL, "
        "Email_cipher TEXT NOT NULL, "
        "UNIQUE KEY uk_personal_records_user_id (user_id), "
        "INDEX idx_personal_records_user_id (user_id), "
        "CONSTRAINT fk_personal_records_users FOREIGN KEY (user_id) "
        "REFERENCES users(id) ON DELETE CASCADE"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;";

    const string migrateCipherColumns =
        "ALTER TABLE personal_records "
        "MODIFY COLUMN CCCD_cipher TEXT NOT NULL, "
        "MODIFY COLUMN SDT_cipher TEXT NOT NULL, "
        "MODIFY COLUMN Email_cipher TEXT NOT NULL;";

    const string addRoleColumnIfMissing =
        "ALTER TABLE users "
        "ADD COLUMN IF NOT EXISTS role TINYINT NOT NULL DEFAULT 1;";

    const string normalizeRoleColumn =
        "ALTER TABLE users "
        "MODIFY COLUMN role TINYINT NOT NULL DEFAULT 1;";

    const string fixInvalidRoleValues =
        "UPDATE users SET role = 1 WHERE role NOT IN (1, 2);";

    if (!ExecuteQuery(createUsersTable) ||
        !ExecuteQuery(createPersonalRecordsTable) ||
        !ExecuteQuery(migrateCipherColumns) ||
        !ExecuteQuery(addRoleColumnIfMissing) ||
        !ExecuteQuery(normalizeRoleColumn) ||
        !ExecuteQuery(fixInvalidRoleValues)) {
        return false;
    }

    return BootstrapDefaultUser();
}

bool DatabaseHelper::BootstrapDefaultUser() {
    if (!IsConnected()) {
        return false;
    }

    try {
        sql::Connection* conn = static_cast<sql::Connection*>(connection);
        unique_ptr<sql::Statement> stmt(conn->createStatement());
        unique_ptr<sql::ResultSet> res(stmt->executeQuery("SELECT COUNT(*) AS total FROM users;"));
        int totalUsers = 0;
        if (res->next()) {
            totalUsers = res->getInt("total");
        }

        if (totalUsers > 0) {
            return true;
        }

        cout << "[DATABASE] No users found. Bootstrapping default account 'admin'." << endl;
        return RegisterUser(
            "admin",
            "admin12345",
            1,
            "012345678912",
            "0123456789",
            "admin@example.com",
            kRoleAdmin);
    } catch (const exception& e) {
        cerr << "[ERROR] Bootstrap failed: " << e.what() << endl;
        return false;
    }
}

bool DatabaseHelper::RegisterUser(const string& username,
                                  const string& passwordPlaintext,
                                  int gender,
                                  const string& cccdPlaintext,
                                  const string& phonePlaintext,
                                  const string& emailPlaintext,
                                  int role) {
    if (!IsConnected()) {
        cerr << "[ERROR] Database is not connected" << endl;
        return false;
    }

    if (username.empty()) {
        cerr << "[ERROR] Username must not be empty" << endl;
        return false;
    }

    if (passwordPlaintext.size() < 8) {
        cerr << "[ERROR] Password must be at least 8 characters" << endl;
        return false;
    }

    if (gender != 1 && gender != 2) {
        cerr << "[ERROR] Gender must be 1 or 2" << endl;
        return false;
    }

    if (!IsValidRoleValue(role)) {
        cerr << "[ERROR] Role must be 1 (user) or 2 (admin)" << endl;
        return false;
    }

    if (cccdPlaintext.size() != 12 || !IsDigitsOnly(cccdPlaintext)) {
        cerr << "[ERROR] CCCD must be exactly 12 digits" << endl;
        return false;
    }

    if (phonePlaintext.size() < 10 || !IsDigitsOnly(phonePlaintext)) {
        cerr << "[ERROR] Phone must contain at least 10 digits" << endl;
        return false;
    }

    if (!LooksLikeEmail(emailPlaintext)) {
        cerr << "[ERROR] Email format is invalid" << endl;
        return false;
    }

    sql::Connection* conn = static_cast<sql::Connection*>(connection);

    try {
        const string passwordHash = PasswordHasher::HashPassword(passwordPlaintext);
        const string kek = PasswordHasher::DeriveKeyEncryptionKey(passwordPlaintext, username);
        const string dekPlaintext = GenerateRandomDek();
        const string encryptedDek = WrapDek(dekPlaintext, kek);

        string cccdCiphertext;
        string phoneCiphertext;
        string emailCiphertext;
        EncryptPersonalData(dekPlaintext, cccdPlaintext, phonePlaintext, emailPlaintext,
                            cccdCiphertext, phoneCiphertext, emailCiphertext);

        conn->setAutoCommit(false);

        unique_ptr<sql::PreparedStatement> insertUser(conn->prepareStatement(
            "INSERT INTO users (username, password_hash, role) VALUES (?, ?, ?)"));
        insertUser->setString(1, username);
        insertUser->setString(2, passwordHash);
        insertUser->setInt(3, role);
        insertUser->execute();

        unique_ptr<sql::Statement> identityStmt(conn->createStatement());
        unique_ptr<sql::ResultSet> identityRes(identityStmt->executeQuery("SELECT LAST_INSERT_ID() AS id"));
        if (!identityRes->next()) {
            throw runtime_error("Failed to obtain inserted user id");
        }

        const int userId = identityRes->getInt("id");
        unique_ptr<sql::PreparedStatement> insertRecord(conn->prepareStatement(
            "INSERT INTO personal_records "
            "(user_id, Gender, encrypted_dek, CCCD_cipher, SDT_cipher, Email_cipher) "
            "VALUES (?, ?, ?, ?, ?, ?)"));
        insertRecord->setInt(1, userId);
        insertRecord->setInt(2, gender);
        insertRecord->setString(3, encryptedDek);
        insertRecord->setString(4, cccdCiphertext);
        insertRecord->setString(5, phoneCiphertext);
        insertRecord->setString(6, emailCiphertext);
        insertRecord->execute();

        conn->commit();
        conn->setAutoCommit(true);

        cout << "[DATABASE] Registered user: " << username << endl;
        return true;
    } catch (const exception& e) {
        RollbackQuietly(conn);
        conn->setAutoCommit(true);
        cerr << "[ERROR] RegisterUser failed: " << e.what() << endl;
        return false;
    }
}

bool DatabaseHelper::GetPersonalRecordForUser(int userId,
                                              const string& sessionKek,
                                              PersonalRecord& record) {
    record = PersonalRecord{};

    if (!IsConnected()) {
        cerr << "[ERROR] Database is not connected" << endl;
        return false;
    }

    try {
        sql::Connection* conn = static_cast<sql::Connection*>(connection);
        unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
            "SELECT u.id AS user_id, u.username, u.role, pr.id AS record_id, pr.Gender, "
            "pr.encrypted_dek, pr.CCCD_cipher, pr.SDT_cipher, pr.Email_cipher "
            "FROM users u "
            "INNER JOIN personal_records pr ON pr.user_id = u.id "
            "WHERE u.id = ?"));
        pstmt->setInt(1, userId);
        unique_ptr<sql::ResultSet> res(pstmt->executeQuery());

        if (!res->next()) {
            return false;
        }

        const string encryptedDek = res->getString("encrypted_dek");
        const string dekPlaintext = UnwrapDek(encryptedDek, sessionKek);

        record.userId = res->getInt("user_id");
        record.recordId = res->getInt("record_id");
        record.username = res->getString("username");
        record.role = res->getInt("role");
        record.gender = res->getInt("Gender");
        record.encryptedDek = encryptedDek;
        DecryptPersonalData(
            dekPlaintext,
            res->getString("CCCD_cipher"),
            res->getString("SDT_cipher"),
            res->getString("Email_cipher"),
            record.cccd,
            record.phone,
            record.email);

        return true;
    } catch (const exception& e) {
        cerr << "[ERROR] GetPersonalRecordForUser failed: " << e.what() << endl;
        return false;
    }
}

bool DatabaseHelper::UpdatePersonalRecordForUser(int userId,
                                                 const string& currentSessionKek,
                                                 const string& newUsername,
                                                 const string& newPasswordPlaintext,
                                                 bool updatePassword,
                                                 int newGender,
                                                 const string& newCccdPlaintext,
                                                 const string& newPhonePlaintext,
                                                 const string& newEmailPlaintext,
                                                 string& updatedSessionKek) {
    updatedSessionKek.clear();

    if (!IsConnected()) {
        cerr << "[ERROR] Database is not connected" << endl;
        return false;
    }

    if (newUsername.empty()) {
        cerr << "[ERROR] Username must not be empty" << endl;
        return false;
    }

    if (newGender != 1 && newGender != 2) {
        cerr << "[ERROR] Gender must be 1 or 2" << endl;
        return false;
    }

    if (newCccdPlaintext.size() != 12 || !IsDigitsOnly(newCccdPlaintext)) {
        cerr << "[ERROR] CCCD must be exactly 12 digits" << endl;
        return false;
    }

    if (newPhonePlaintext.size() < 10 || !IsDigitsOnly(newPhonePlaintext)) {
        cerr << "[ERROR] Phone must contain at least 10 digits" << endl;
        return false;
    }

    if (!LooksLikeEmail(newEmailPlaintext)) {
        cerr << "[ERROR] Email format is invalid" << endl;
        return false;
    }

    if (updatePassword && newPasswordPlaintext.size() < 8) {
        cerr << "[ERROR] New password must be at least 8 characters" << endl;
        return false;
    }

    sql::Connection* conn = static_cast<sql::Connection*>(connection);

    try {
        unique_ptr<sql::PreparedStatement> selectCurrent(conn->prepareStatement(
            "SELECT u.username, pr.id AS record_id, pr.encrypted_dek "
            "FROM users u "
            "INNER JOIN personal_records pr ON pr.user_id = u.id "
            "WHERE u.id = ?"));
        selectCurrent->setInt(1, userId);
        unique_ptr<sql::ResultSet> current(selectCurrent->executeQuery());
        if (!current->next()) {
            return false;
        }

        const int recordId = current->getInt("record_id");
        const string oldUsername = current->getString("username");
        const string currentEncryptedDek = current->getString("encrypted_dek");
        const string dekPlaintext = UnwrapDek(currentEncryptedDek, currentSessionKek);

        if (!updatePassword && newUsername != oldUsername) {
            throw runtime_error("Username change requires current password re-entry");
        }

        string finalSessionKek = currentSessionKek;
        string newPasswordHash;
        if (updatePassword) {
            newPasswordHash = PasswordHasher::HashPassword(newPasswordPlaintext);
            finalSessionKek = PasswordHasher::DeriveKeyEncryptionKey(newPasswordPlaintext, newUsername);
        }

        string encryptedDek = currentEncryptedDek;
        if (finalSessionKek != currentSessionKek || newUsername != oldUsername) {
            encryptedDek = WrapDek(dekPlaintext, finalSessionKek);
        }

        string cccdCiphertext;
        string phoneCiphertext;
        string emailCiphertext;
        EncryptPersonalData(dekPlaintext, newCccdPlaintext, newPhonePlaintext, newEmailPlaintext,
                            cccdCiphertext, phoneCiphertext, emailCiphertext);

        conn->setAutoCommit(false);

        if (updatePassword) {
            unique_ptr<sql::PreparedStatement> updateUser(conn->prepareStatement(
                "UPDATE users SET username = ?, password_hash = ? WHERE id = ?"));
            updateUser->setString(1, newUsername);
            updateUser->setString(2, newPasswordHash);
            updateUser->setInt(3, userId);
            updateUser->execute();
        } else {
            unique_ptr<sql::PreparedStatement> updateUser(conn->prepareStatement(
                "UPDATE users SET username = ? WHERE id = ?"));
            updateUser->setString(1, newUsername);
            updateUser->setInt(2, userId);
            updateUser->execute();
        }

        unique_ptr<sql::PreparedStatement> updateRecord(conn->prepareStatement(
            "UPDATE personal_records "
            "SET Gender = ?, encrypted_dek = ?, CCCD_cipher = ?, SDT_cipher = ?, Email_cipher = ? "
            "WHERE id = ?"));
        updateRecord->setInt(1, newGender);
        updateRecord->setString(2, encryptedDek);
        updateRecord->setString(3, cccdCiphertext);
        updateRecord->setString(4, phoneCiphertext);
        updateRecord->setString(5, emailCiphertext);
        updateRecord->setInt(6, recordId);
        updateRecord->execute();

        conn->commit();
        conn->setAutoCommit(true);

        updatedSessionKek = finalSessionKek;
        return true;
    } catch (const exception& e) {
        RollbackQuietly(conn);
        conn->setAutoCommit(true);
        updatedSessionKek.clear();
        cerr << "[ERROR] UpdatePersonalRecordForUser failed: " << e.what() << endl;
        return false;
    }
}

bool DatabaseHelper::DeleteUserById(int userId) {
    if (!IsConnected()) {
        cerr << "[ERROR] Database is not connected" << endl;
        return false;
    }

    try {
        sql::Connection* conn = static_cast<sql::Connection*>(connection);
        unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
            "DELETE FROM users WHERE id = ?"));
        pstmt->setInt(1, userId);
        pstmt->execute();
        return true;
    } catch (const exception& e) {
        cerr << "[ERROR] DeleteUserById failed: " << e.what() << endl;
        return false;
    }
}

int DatabaseHelper::GetTotalUsers() {
    if (!IsConnected()) {
        return 0;
    }

    try {
        sql::Connection* conn = static_cast<sql::Connection*>(connection);
        unique_ptr<sql::Statement> stmt(conn->createStatement());
        unique_ptr<sql::ResultSet> res(stmt->executeQuery("SELECT COUNT(*) AS total FROM users"));
        if (res->next()) {
            return res->getInt("total");
        }
    } catch (const exception& e) {
        cerr << "[ERROR] GetTotalUsers failed: " << e.what() << endl;
    }

    return 0;
}

bool DatabaseHelper::GetEncryptedUserRecordByOffset(int offset, PersonalRecord& record) {
    record = PersonalRecord{};

    if (!IsConnected()) {
        cerr << "[ERROR] Database is not connected" << endl;
        return false;
    }

    if (offset < 0) {
        return false;
    }

    try {
        sql::Connection* conn = static_cast<sql::Connection*>(connection);
        unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
            "SELECT u.id AS user_id, u.username, u.role, pr.id AS record_id, pr.Gender, "
            "pr.encrypted_dek, pr.CCCD_cipher, pr.SDT_cipher, pr.Email_cipher "
            "FROM users u "
            "INNER JOIN personal_records pr ON pr.user_id = u.id "
            "ORDER BY u.id ASC "
            "LIMIT 1 OFFSET ?"));
        pstmt->setInt(1, offset);
        unique_ptr<sql::ResultSet> res(pstmt->executeQuery());

        if (!res->next()) {
            return false;
        }

        record.userId = res->getInt("user_id");
        record.recordId = res->getInt("record_id");
        record.username = res->getString("username");
        record.role = res->getInt("role");
        record.gender = res->getInt("Gender");
        record.encryptedDek = res->getString("encrypted_dek");
        record.cccd = res->getString("CCCD_cipher");
        record.phone = res->getString("SDT_cipher");
        record.email = res->getString("Email_cipher");
        return true;
    } catch (const exception& e) {
        cerr << "[ERROR] GetEncryptedUserRecordByOffset failed: " << e.what() << endl;
        return false;
    }
}

AuthenticatedUser DatabaseHelper::AuthenticateUser(const string& username,
                                                   const string& passwordPlaintext,
                                                   string& derivedSessionKek) {
    derivedSessionKek.clear();
    AuthenticatedUser result;

    if (!IsConnected()) {
        cerr << "[ERROR] Database is not connected" << endl;
        return result;
    }

    try {
        sql::Connection* conn = static_cast<sql::Connection*>(connection);
        unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
            "SELECT id, username, password_hash, role FROM users WHERE username = ?"));
        pstmt->setString(1, username);
        unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        if (!res->next()) {
            return result;
        }

        const string storedHash = res->getString("password_hash");
        if (!PasswordHasher::VerifyPassword(passwordPlaintext, storedHash)) {
            return result;
        }

        result.id = res->getInt("id");
        result.username = res->getString("username");
        result.role = res->getInt("role");
        derivedSessionKek = PasswordHasher::DeriveKeyEncryptionKey(passwordPlaintext, result.username);
        return result;
    } catch (const exception& e) {
        cerr << "[ERROR] AuthenticateUser failed: " << e.what() << endl;
        derivedSessionKek.clear();
        return result;
    }
}
