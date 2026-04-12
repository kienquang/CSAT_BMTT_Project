#ifndef DATABASE_HELPER_H
#define DATABASE_HELPER_H

#include <string>
#include <vector>

// [GROUP: Shared Data Models]
struct PersonalRecord {
    int userId = -1;
    int recordId = -1;
    std::string username;
    std::string name;
    int role = 1;
    int gender = 0;
    // Decrypted personal fields returned to caller when session KEK is available.
    std::string cccd;
    std::string phone;
    std::string email;
    // Stored envelope key for personal record data (ciphertext in DB format).
    std::string encryptedDek;
};

// [GROUP: Shared Data Models]
struct AuthenticatedUser {
    int id = -1;
    std::string username;
    int role = 1;

    bool IsValid() const {
        return id >= 0;
    }
};

// [GROUP: Shared Data Models]
struct MedicalRecordSummary {
    int recordId = -1;
    int patientId = -1;
    std::string patientName;
    int doctorId = -1;
    std::string doctorName;
    std::string visitDate;
    std::string department;
    // For list APIs these can be ciphertext payloads; for detail APIs they are plaintext.
    std::string diagnosisCipher;
    std::string prescriptionCipher;
};

// [GROUP: Database Service]
class DatabaseHelper {
private:
    std::string HOST;
    int PORT;
    std::string USER;
    std::string PASSWORD;
    std::string DATABASE;
    void* connection;

    // [GROUP: Internal Helpers]
    bool ExecuteQuery(const std::string& query);
    std::string GenerateRandomDek() const;
    std::string WrapDek(const std::string& dekPlaintext, const std::string& kek) const;
    std::string UnwrapDek(const std::string& encryptedDek, const std::string& kek) const;
    void EncryptPersonalData(const std::string& dekPlaintext,
                             const std::string& cccdPlaintext,
                             const std::string& phonePlaintext,
                             const std::string& emailPlaintext,
                             std::string& cccdCiphertext,
                             std::string& phoneCiphertext,
                             std::string& emailCiphertext) const;
    void DecryptPersonalData(const std::string& dekPlaintext,
                             const std::string& cccdCiphertext,
                             const std::string& phoneCiphertext,
                             const std::string& emailCiphertext,
                             std::string& cccdPlaintext,
                             std::string& phonePlaintext,
                             std::string& emailPlaintext) const;
    bool BootstrapDefaultUser();

public:
    // [GROUP: Lifecycle]
    DatabaseHelper(const std::string& host, int port,
                   const std::string& user, const std::string& password,
                   const std::string& database);
    ~DatabaseHelper();

    // [GROUP: Connection And Schema]
    bool Connect();
    bool InitializeSchema();

    // [GROUP: User Profile Operations]
    bool RegisterUser(const std::string& username,
                      const std::string& name,
                      const std::string& passwordPlaintext,
                      int gender,
                      const std::string& cccdPlaintext,
                      const std::string& phonePlaintext,
                      const std::string& emailPlaintext,
                      int role = 1);
    bool GetPersonalRecordForUser(int userId,
                                  const std::string& sessionKek,
                                  PersonalRecord& record);
    bool UpdatePersonalRecordForUser(int userId,
                                     const std::string& currentSessionKek,
                                     const std::string& newUsername,
                                     const std::string& newName,
                                     const std::string& newPasswordPlaintext,
                                     bool updatePassword,
                                     int newGender,
                                     const std::string& newCccdPlaintext,
                                     const std::string& newPhonePlaintext,
                                     const std::string& newEmailPlaintext,
                                     std::string& updatedSessionKek);

    // [GROUP: Account Management]
    bool DeleteUserById(int userId);
    bool UserExistsById(int userId);
    bool UpdateUserRoleById(int userId, int newRole);

    // [GROUP: Medical Record Operations]
    bool CreateMedicalRecord(int patientId,
                             int doctorId,
                             const std::string& visitDate,
                             const std::string& department,
                             const std::string& diagnosisPlaintext,
                             const std::string& prescriptionPlaintext);

    // [GROUP: Query Operations]
    int GetTotalUsers();
    int GetTotalMedicalRecords();
    int GetTotalMedicalRecordsForDoctor(int doctorId);
    int GetTotalMedicalRecordsForPatient(int patientId);
    bool GetEncryptedUserRecordByOffset(int offset, PersonalRecord& record);
    bool GetMedicalRecordSummaryByOffset(int offset, MedicalRecordSummary& record);
    bool GetMedicalRecordSummaryByDoctorOffset(int doctorId, int offset, MedicalRecordSummary& record);
    bool GetMedicalRecordSummaryByPatientOffset(int patientId, int offset, MedicalRecordSummary& record);
    bool GetMedicalRecordDetailForDoctor(int doctorId,
                                         int recordId,
                                         const std::string& doctorKek,
                                         MedicalRecordSummary& record);
    bool GetMedicalRecordDetailForPatient(int patientId,
                                          int recordId,
                                          const std::string& patientKek,
                                          MedicalRecordSummary& record);

    // [GROUP: Authentication]
    AuthenticatedUser AuthenticateUser(const std::string& username,
                                       const std::string& passwordPlaintext,
                                       std::string& derivedSessionKek);

    // [GROUP: Lifecycle]
    bool IsConnected();
    void Disconnect();
};

#endif  // DATABASE_HELPER_H
