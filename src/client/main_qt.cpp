#include <QApplication>

#include <iostream>

#include "gui/LoginDialog.h"
#include "gui/MainWindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    std::cout << "========================================" << std::endl;
    std::cout << "  CSAT_BMTT - Personal Record Vault" << std::endl;
    std::cout << "  Version: 2.0 (Qt6.x)" << std::endl;
    std::cout << "  Argon2id + Password-Derived KEK" << std::endl;
    std::cout << "========================================" << std::endl;

    LoginDialog loginDialog;
    if (loginDialog.exec() != QDialog::Accepted) {
        std::cout << "[INFO] User cancelled login" << std::endl;
        return 0;
    }

    LoginDialog::LoginResult loginResult = loginDialog.getLoginResult();
    if (!loginResult.success) {
        std::cout << "[ERROR] Login validation failed" << std::endl;
        return 1;
    }

    MainWindow window(loginResult.networkClient, nullptr, loginResult.username, loginResult.role);
    window.show();

    std::cout << "[INFO] Application started for user "
              << loginResult.username.toStdString() << std::endl;

    return app.exec();
}
