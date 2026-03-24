#include <iostream>
#include "Blowfish.h" // Nối với file Header bạn vừa tách

using namespace std;

int main() {
    cout << "=== TEST BLOWFISH SAU KHI TACH FILE ===\n\n";

    // 1. Khởi tạo thuật toán
    string secretKey = "MatMaHoc123";
    Blowfish cipher(secretKey);

    // 2. Chạy thử dữ liệu
    string dataThuc = "001202037855";
    cout << "[+] Du lieu goc: " << dataThuc << endl;

    string dataHex = cipher.EncryptString(dataThuc);
    cout << "[+] Du lieu HEX (Luu DB): " << dataHex << endl;

    string recoveredData = cipher.DecryptString(dataHex);
    cout << "[+] Du lieu giai ma : " << recoveredData << endl;

    if (dataThuc == recoveredData) {
        cout << "\n=> THANH CONG! File .h va .cpp da lien ket hoan hao." << endl;
    } else {
        cout << "\n=> THAT BAI! Kiem tra lai." << endl;
    }

    return 0;
}