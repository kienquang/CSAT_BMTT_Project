#include <QApplication>
#include "gui/MainWindow.h"
#include "gui/LoginDialog.h"
#include <iostream>

int main(int argc, char *argv[]) {
    // Create Qt Application
    QApplication app(argc, argv);
    
    std::cout << "========================================" << std::endl;
    std::cout << "  CSAT_BMTT - Employee Management System" << std::endl;
    std::cout << "  Version: 1.0 (Qt6.x)" << std::endl;
    std::cout << "  Role-Based Data Masking Enabled" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // Show login dialog
    LoginDialog loginDialog;
    if (loginDialog.exec() != QDialog::Accepted) {
        std::cout << "[INFO] User cancelled login" << std::endl;
        return 0;  // Exit if login cancelled
    }
    
    // Get login result
    LoginDialog::LoginResult loginResult = loginDialog.getLoginResult();
    
    if (!loginResult.success) {
        std::cout << "[ERROR] Login validation failed" << std::endl;
        return 1;
    }
    
    std::cout << "[INFO] User logged in - Role: " << loginResult.userRole.toStdString() << std::endl;
    
    // Create main window with connected GUI client
    MainWindow window(loginResult.networkClient, nullptr, loginResult.userRole, loginResult.userName);
    window.show();
    
    std::cout << "[INFO] Application started" << std::endl;
    
    // Run event loop
    return app.exec();
}
