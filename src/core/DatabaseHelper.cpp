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
constexpr int kRoleDoctor = 2;
constexpr int kRoleAdmin = 3;

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
    return role == kRoleUser || role == kRoleAdmin || role == kRoleDoctor;
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

    return storedValue;
}

string DecryptMedicalFieldWithDoctorKek(const string& storedValue, const string& doctorKek) {
    if (storedValue.empty() || doctorKek.empty()) {
        return storedValue;
    }

    try {
        const string cipherHex = DecodeCipherForBlowfish(storedValue);
        if (!IsHexCipherText(cipherHex)) {
            return storedValue;
        }

        Blowfish cipher(doctorKek);
        return cipher.DecryptString(cipherHex);
    } catch (...) {
        return storedValue;
    }
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
        "name VARCHAR(255) NULL, "
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

    const string createMedicalRecordsTable =
        "CREATE TABLE IF NOT EXISTS medical_records ("
        "id INT AUTO_INCREMENT PRIMARY KEY, "
        "patient_id INT NOT NULL, "
        "doctor_id INT NOT NULL, "
        "visit_date DATE NOT NULL, "
        "department VARCHAR(100) NULL, "
        "patient_encrypted_dek VARCHAR(255) NOT NULL, "
        "doctor_encrypted_dek VARCHAR(255) NOT NULL, "
        "diagnosis_cipher TEXT NOT NULL, "
        "prescription_cipher TEXT NOT NULL, "
        "INDEX idx_medical_records_patient_id (patient_id), "
        "INDEX idx_medical_records_doctor_id (doctor_id), "
        "INDEX idx_medical_records_visit_date (visit_date), "
        "CONSTRAINT fk_medical_records_patient FOREIGN KEY (patient_id) "
        "REFERENCES users(id) ON DELETE CASCADE, "
        "CONSTRAINT fk_medical_records_doctor FOREIGN KEY (doctor_id) "
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

    const string addNameColumnIfMissing =
        "ALTER TABLE users "
        "ADD COLUMN IF NOT EXISTS name VARCHAR(255) NULL AFTER username;";

    const string normalizeRoleColumn =
        "ALTER TABLE users "
        "MODIFY COLUMN role TINYINT NOT NULL DEFAULT 1;";

    const string fixInvalidRoleValues =
        "UPDATE users SET role = 1 WHERE role NOT IN (1, 2, 3);";

    if (!ExecuteQuery(createUsersTable) ||
        !ExecuteQuery(createPersonalRecordsTable) ||
        !ExecuteQuery(createMedicalRecordsTable) ||
        !ExecuteQuery(migrateCipherColumns) ||
        !ExecuteQuery(addRoleColumnIfMissing) ||
        !ExecuteQuery(addNameColumnIfMissing) ||
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
            "System Administrator",
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
                                  const string& name,
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

    if (name.empty()) {
        cerr << "[ERROR] Name must not be empty" << endl;
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
        cerr << "[ERROR] Role must be 1 (user), 2 (doctor), or 3 (admin)" << endl;
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
            "INSERT INTO users (username, name, password_hash, role) VALUES (?, ?, ?, ?)"));
        insertUser->setString(1, username);
        insertUser->setString(2, name);
        insertUser->setString(3, passwordHash);
        insertUser->setInt(4, role);
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
            "SELECT u.id AS user_id, u.username, COALESCE(u.name, '') AS name, u.role, pr.id AS record_id, pr.Gender, "
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
        record.name = res->getString("name");
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
                                                 const string& newName,
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

    if (newName.empty()) {
        cerr << "[ERROR] Name must not be empty" << endl;
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
                "UPDATE users SET username = ?, name = ?, password_hash = ? WHERE id = ?"));
            updateUser->setString(1, newUsername);
            updateUser->setString(2, newName);
            updateUser->setString(3, newPasswordHash);
            updateUser->setInt(4, userId);
            updateUser->execute();
        } else {
            unique_ptr<sql::PreparedStatement> updateUser(conn->prepareStatement(
                "UPDATE users SET username = ?, name = ? WHERE id = ?"));
            updateUser->setString(1, newUsername);
            updateUser->setString(2, newName);
            updateUser->setInt(3, userId);
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
        const int affectedRows = pstmt->executeUpdate();
        return affectedRows > 0;
    } catch (const exception& e) {
        cerr << "[ERROR] DeleteUserById failed: " << e.what() << endl;
        return false;
    }
}

bool DatabaseHelper::UserExistsById(int userId) {
    if (!IsConnected()) {
        cerr << "[ERROR] Database is not connected" << endl;
        return false;
    }

    if (userId <= 0) {
        return false;
    }

    try {
        sql::Connection* conn = static_cast<sql::Connection*>(connection);
        unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
            "SELECT 1 FROM users WHERE id = ? LIMIT 1"));
        pstmt->setInt(1, userId);
        unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        return res->next();
    } catch (const exception& e) {
        cerr << "[ERROR] UserExistsById failed: " << e.what() << endl;
        return false;
    }
}

bool DatabaseHelper::UpdateUserRoleById(int userId, int newRole) {
    if (!IsConnected()) {
        cerr << "[ERROR] Database is not connected" << endl;
        return false;
    }

    if (userId <= 0) {
        cerr << "[ERROR] User id must be positive" << endl;
        return false;
    }

    if (!IsValidRoleValue(newRole)) {
        cerr << "[ERROR] Role must be 1 (user), 2 (doctor), or 3 (admin)" << endl;
        return false;
    }

    try {
        sql::Connection* conn = static_cast<sql::Connection*>(connection);
        unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
            "UPDATE users SET role = ? WHERE id = ?"));
        pstmt->setInt(1, newRole);
        pstmt->setInt(2, userId);
        const int affectedRows = pstmt->executeUpdate();
        return affectedRows > 0;
    } catch (const exception& e) {
        cerr << "[ERROR] UpdateUserRoleById failed: " << e.what() << endl;
        return false;
    }
}

bool DatabaseHelper::CreateMedicalRecord(int patientId,
                                         int doctorId,
                                         const string& visitDate,
                                         const string& department,
                                         const string& diagnosisPlaintext,
                                         const string& prescriptionPlaintext,
                                         const string& doctorKek) {
    if (!IsConnected()) {
        cerr << "[ERROR] Database is not connected" << endl;
        return false;
    }

    if (patientId <= 0 || doctorId <= 0) {
        cerr << "[ERROR] Patient id and doctor id must be positive" << endl;
        return false;
    }

    if (visitDate.empty()) {
        cerr << "[ERROR] Visit date is required" << endl;
        return false;
    }

    if (diagnosisPlaintext.empty() || prescriptionPlaintext.empty()) {
        cerr << "[ERROR] Diagnosis and prescription are required" << endl;
        return false;
    }

    if (doctorKek.empty()) {
        cerr << "[ERROR] Doctor KEK is required" << endl;
        return false;
    }

    try {
        sql::Connection* conn = static_cast<sql::Connection*>(connection);

        unique_ptr<sql::PreparedStatement> patientStmt(conn->prepareStatement(
            "SELECT role FROM users WHERE id = ?"));
        patientStmt->setInt(1, patientId);
        unique_ptr<sql::ResultSet> patientRes(patientStmt->executeQuery());
        if (!patientRes->next()) {
            cerr << "[ERROR] Patient account does not exist" << endl;
            return false;
        }

        const int patientRole = patientRes->getInt("role");
        if (patientRole != kRoleUser) {
            cerr << "[ERROR] Target patient must have user role" << endl;
            return false;
        }

        unique_ptr<sql::PreparedStatement> doctorStmt(conn->prepareStatement(
            "SELECT role FROM users WHERE id = ?"));
        doctorStmt->setInt(1, doctorId);
        unique_ptr<sql::ResultSet> doctorRes(doctorStmt->executeQuery());
        if (!doctorRes->next()) {
            cerr << "[ERROR] Doctor account does not exist" << endl;
            return false;
        }

        const int doctorRole = doctorRes->getInt("role");
        if (doctorRole != kRoleDoctor) {
            cerr << "[ERROR] Creator must have doctor role" << endl;
            return false;
        }

        const string medicalDek = GenerateRandomDek();
        const string doctorEncryptedDek = WrapDek(medicalDek, doctorKek);
        // Patient-side unwrap flow is not wired yet, keep a wrapped DEK payload instead of placeholder.
        const string patientEncryptedDek = doctorEncryptedDek;

        Blowfish cipher(medicalDek);
        const string diagnosisCipher = Base64::Encode(cipher.EncryptString(diagnosisPlaintext));
        const string prescriptionCipher = Base64::Encode(cipher.EncryptString(prescriptionPlaintext));

        unique_ptr<sql::PreparedStatement> insertStmt(conn->prepareStatement(
            "INSERT INTO medical_records "
            "(patient_id, doctor_id, visit_date, department, patient_encrypted_dek, doctor_encrypted_dek, diagnosis_cipher, prescription_cipher) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));
        insertStmt->setInt(1, patientId);
        insertStmt->setInt(2, doctorId);
        insertStmt->setString(3, visitDate);
        insertStmt->setString(4, department);
        insertStmt->setString(5, patientEncryptedDek);
        insertStmt->setString(6, doctorEncryptedDek);
        insertStmt->setString(7, diagnosisCipher);
        insertStmt->setString(8, prescriptionCipher);
        const int affectedRows = insertStmt->executeUpdate();
        return affectedRows > 0;
    } catch (const exception& e) {
        cerr << "[ERROR] CreateMedicalRecord failed: " << e.what() << endl;
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

int DatabaseHelper::GetTotalMedicalRecords() {
    if (!IsConnected()) {
        return 0;
    }

    try {
        sql::Connection* conn = static_cast<sql::Connection*>(connection);
        unique_ptr<sql::Statement> stmt(conn->createStatement());
        unique_ptr<sql::ResultSet> res(stmt->executeQuery("SELECT COUNT(*) AS total FROM medical_records"));
        if (res->next()) {
            return res->getInt("total");
        }
    } catch (const exception& e) {
        cerr << "[ERROR] GetTotalMedicalRecords failed: " << e.what() << endl;
    }

    return 0;
}

int DatabaseHelper::GetTotalMedicalRecordsForDoctor(int doctorId) {
    if (!IsConnected()) {
        return 0;
    }

    if (doctorId <= 0) {
        return 0;
    }

    try {
        sql::Connection* conn = static_cast<sql::Connection*>(connection);
        unique_ptr<sql::PreparedStatement> stmt(conn->prepareStatement(
            "SELECT COUNT(*) AS total FROM medical_records WHERE doctor_id = ?"));
        stmt->setInt(1, doctorId);
        unique_ptr<sql::ResultSet> res(stmt->executeQuery());
        if (res->next()) {
            return res->getInt("total");
        }
    } catch (const exception& e) {
        cerr << "[ERROR] GetTotalMedicalRecordsForDoctor failed: " << e.what() << endl;
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

bool DatabaseHelper::GetMedicalRecordSummaryByOffset(int offset, MedicalRecordSummary& record) {
    record = MedicalRecordSummary{};

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
            "SELECT mr.id, mr.patient_id, mr.doctor_id, DATE_FORMAT(mr.visit_date, '%Y-%m-%d') AS visit_date, "
            "COALESCE(mr.department, '') AS department, "
            "COALESCE(NULLIF(u.name, ''), u.username) AS patient_name, "
            "mr.diagnosis_cipher, mr.prescription_cipher "
            "FROM medical_records mr "
            "INNER JOIN users u ON u.id = mr.patient_id "
            "ORDER BY mr.id ASC "
            "LIMIT 1 OFFSET ?"));
        pstmt->setInt(1, offset);
        unique_ptr<sql::ResultSet> res(pstmt->executeQuery());

        if (!res->next()) {
            return false;
        }

        record.recordId = res->getInt("id");
        record.patientId = res->getInt("patient_id");
        record.patientName = res->getString("patient_name");
        record.doctorId = res->getInt("doctor_id");
        record.visitDate = res->getString("visit_date");
        record.department = res->getString("department");
        record.diagnosisCipher = res->getString("diagnosis_cipher");
        record.prescriptionCipher = res->getString("prescription_cipher");
        return true;
    } catch (const exception& e) {
        cerr << "[ERROR] GetMedicalRecordSummaryByOffset failed: " << e.what() << endl;
        return false;
    }
}

bool DatabaseHelper::GetMedicalRecordSummaryByDoctorOffset(int doctorId, int offset, MedicalRecordSummary& record) {
    record = MedicalRecordSummary{};

    if (!IsConnected()) {
        cerr << "[ERROR] Database is not connected" << endl;
        return false;
    }

    if (doctorId <= 0 || offset < 0) {
        return false;
    }

    try {
        sql::Connection* conn = static_cast<sql::Connection*>(connection);
        unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
            "SELECT mr.id, mr.patient_id, mr.doctor_id, DATE_FORMAT(mr.visit_date, '%Y-%m-%d') AS visit_date, "
            "COALESCE(mr.department, '') AS department, "
            "COALESCE(NULLIF(u.name, ''), u.username) AS patient_name, "
            "mr.diagnosis_cipher, mr.prescription_cipher "
            "FROM medical_records mr "
            "INNER JOIN users u ON u.id = mr.patient_id "
            "WHERE mr.doctor_id = ? "
            "ORDER BY mr.id ASC "
            "LIMIT 1 OFFSET ?"));
        pstmt->setInt(1, doctorId);
        pstmt->setInt(2, offset);
        unique_ptr<sql::ResultSet> res(pstmt->executeQuery());

        if (!res->next()) {
            return false;
        }

        record.recordId = res->getInt("id");
        record.patientId = res->getInt("patient_id");
        record.patientName = res->getString("patient_name");
        record.doctorId = res->getInt("doctor_id");
        record.visitDate = res->getString("visit_date");
        record.department = res->getString("department");
        record.diagnosisCipher = res->getString("diagnosis_cipher");
        record.prescriptionCipher = res->getString("prescription_cipher");
        return true;
    } catch (const exception& e) {
        cerr << "[ERROR] GetMedicalRecordSummaryByDoctorOffset failed: " << e.what() << endl;
        return false;
    }
}

bool DatabaseHelper::GetMedicalRecordDetailForDoctor(int doctorId,
                                                     int recordId,
                                                     const string& doctorKek,
                                                     MedicalRecordSummary& record) {
    record = MedicalRecordSummary{};

    if (!IsConnected()) {
        cerr << "[ERROR] Database is not connected" << endl;
        return false;
    }

    if (doctorId <= 0 || recordId <= 0 || doctorKek.empty()) {
        return false;
    }

    try {
        sql::Connection* conn = static_cast<sql::Connection*>(connection);
        unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
            "SELECT mr.id, mr.patient_id, mr.doctor_id, DATE_FORMAT(mr.visit_date, '%Y-%m-%d') AS visit_date, "
            "COALESCE(mr.department, '') AS department, "
            "COALESCE(NULLIF(u.name, ''), u.username) AS patient_name, "
            "mr.doctor_encrypted_dek, mr.diagnosis_cipher, mr.prescription_cipher "
            "FROM medical_records mr "
            "INNER JOIN users u ON u.id = mr.patient_id "
            "WHERE mr.id = ? AND mr.doctor_id = ?"));
        pstmt->setInt(1, recordId);
        pstmt->setInt(2, doctorId);
        unique_ptr<sql::ResultSet> res(pstmt->executeQuery());

        if (!res->next()) {
            return false;
        }

        const string medicalDek = UnwrapDek(res->getString("doctor_encrypted_dek"), doctorKek);

        record.recordId = res->getInt("id");
        record.patientId = res->getInt("patient_id");
        record.patientName = res->getString("patient_name");
        record.doctorId = res->getInt("doctor_id");
        record.visitDate = res->getString("visit_date");
        record.department = res->getString("department");
        record.diagnosisCipher = DecryptMedicalFieldWithDoctorKek(res->getString("diagnosis_cipher"), medicalDek);
        record.prescriptionCipher = DecryptMedicalFieldWithDoctorKek(res->getString("prescription_cipher"), medicalDek);
        return true;
    } catch (const exception& e) {
        cerr << "[ERROR] GetMedicalRecordDetailForDoctor failed: " << e.what() << endl;
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
