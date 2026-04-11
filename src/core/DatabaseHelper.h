#ifndef DATABASE_HELPER_H
#define DATABASE_HELPER_H

#include <string>
#include <vector>

struct PersonalRecord {
    int userId = -1;
    int recordId = -1;
    std::string username;
    std::string name;
    int role = 1;
    int gender = 0;
    std::string cccd;
    std::string phone;
    std::string email;
    std::string encryptedDek;
};

struct AuthenticatedUser {
    int id = -1;
    std::string username;
    int role = 1;

    bool IsValid() const {
        return id >= 0;
    }
};

struct MedicalRecordSummary {
    int recordId = -1;
    int patientId = -1;
    std::string patientName;
    int doctorId = -1;
    std::string visitDate;
    std::string department;
    std::string diagnosisCipher;
    std::string prescriptionCipher;
};

class DatabaseHelper {
private:
    std::string HOST;
    int PORT;
    std::string USER;
    std::string PASSWORD;
    std::string DATABASE;
    void* connection;

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
    DatabaseHelper(const std::string& host, int port,
                   const std::string& user, const std::string& password,
                   const std::string& database);
    ~DatabaseHelper();

    bool Connect();
    bool InitializeSchema();
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
    bool DeleteUserById(int userId);
    bool UserExistsById(int userId);
    bool UpdateUserRoleById(int userId, int newRole);
    bool CreateMedicalRecord(int patientId,
                             int doctorId,
                             const std::string& visitDate,
                             const std::string& department,
                             const std::string& diagnosisPlaintext,
                             const std::string& prescriptionPlaintext,
                             const std::string& doctorKek);
    int GetTotalUsers();
    int GetTotalMedicalRecords();
    int GetTotalMedicalRecordsForDoctor(int doctorId);
    bool GetEncryptedUserRecordByOffset(int offset, PersonalRecord& record);
    bool GetMedicalRecordSummaryByOffset(int offset, MedicalRecordSummary& record);
    bool GetMedicalRecordSummaryByDoctorOffset(int doctorId, int offset, MedicalRecordSummary& record);
    bool GetMedicalRecordDetailForDoctor(int doctorId,
                                         int recordId,
                                         const std::string& doctorKek,
                                         MedicalRecordSummary& record);
    AuthenticatedUser AuthenticateUser(const std::string& username,
                                       const std::string& passwordPlaintext,
                                       std::string& derivedSessionKek);
    bool IsConnected();
    void Disconnect();
};

#endif  // DATABASE_HELPER_H
