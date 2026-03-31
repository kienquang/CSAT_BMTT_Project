#ifndef DATABASE_HELPER_H
#define DATABASE_HELPER_H

#include <string>
#include <vector>
#include "Blowfish.h"
#include "MaskingLogic.h"
#include "EncryptionConfig.h"

using namespace std;

struct nhanvien {
    int id;
    string ten_nv;
    string vai_tro;
    string cccd_cipher;
    string sdt_cipher;
    string matkhau_cipher;
    string luong_cipher;
};

class DatabaseHelper {
private:
    string HOST;
    int PORT;
    string USER;
    string PASSWORD;
    string DATABASE;
    void* connection;

    bool ExecuteQuery(const string& query);
    void EncryptNhanVienData(const string& cccd, const string& sdt,
                             const string& matkhau, const string& luong,
                             string& cccd_encrypted, string& sdt_encrypted,
                             string& matkhau_encrypted, string& luong_encrypted);
    void DecryptNhanVienData(const string& cccd_encrypted, const string& sdt_encrypted,
                             const string& matkhau_encrypted, const string& luong_encrypted,
                             string& cccd_plain, string& sdt_plain,
                             string& matkhau_plain, string& luong_plain);
    void ApplyClientMasking(string& cccd, string& sdt, string& matkhau, string& luong);
    void ProcessNhanVienFieldsForClient(const string& cccd_encrypted, const string& sdt_encrypted,
                                        const string& matkhau_encrypted, const string& luong_encrypted,
                                        string& cccd_masked, string& sdt_masked,
                                        string& matkhau_masked, string& luong_masked);

public:
    DatabaseHelper(const string& host, int port,
                   const string& user, const string& password,
                   const string& database);
    ~DatabaseHelper();

    bool Connect();
    bool CreateTableNhanVien();
    bool InsertNhanVien(const string& ten_nv, const string& vai_tro,
                        const string& cccd_plaintext,
                        const string& sdt_plaintext,
                        const string& matkhau_plaintext,
                        const string& luong_plaintext);
    vector<nhanvien> GetAllNhanVienForClient();
    bool UpdateNhanVien(int id, const string& ten_nv, const string& vai_tro,
                        const string& cccd_plaintext, const string& sdt_plaintext,
                        const string& matkhau_plaintext, const string& luong_plaintext);
    bool DeleteNhanVienById(int id);
    int GetTotalNhanVien();
    nhanvien AuthenticateUser(const string& cccd_plaintext, const string& matkhau_plaintext);
    bool IsConnected();
    void Disconnect();
};

#endif
