#include "LoginDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <iostream>
#include "../../core/EnvConfig.h"
#include "../network/NetworkClient.h"

LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent), 
      loginResult{false, "", -1, "", nullptr}
{
    setWindowTitle("CSAT_BMTT - Login");
    setModal(true);
    setMinimumWidth(400);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    
    setupUi();

    networkClient = std::make_shared<NetworkClient>();
    const std::string serverHost = EnvConfig::GetString("APP_SERVER_HOST", "127.0.0.1");
    const int serverPort = EnvConfig::GetInt("APP_SERVER_PORT", 8080);
    std::string error;
    if (!networkClient->Connect(serverHost, serverPort, error)) {
        QMessageBox::critical(this, "Server Error",
                              QString("Failed to connect to server %1:%2.\n%3")
                                  .arg(QString::fromStdString(serverHost))
                                  .arg(serverPort)
                                  .arg(QString::fromStdString(error)));
    }
}

LoginDialog::~LoginDialog() {}

void LoginDialog::setupUi() {
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    // Title
    QLabel *titleLabel = new QLabel("CSAT_BMTT Employee Management System");
    titleLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: #34495e;");
    mainLayout->addWidget(titleLabel);
    
    mainLayout->addSpacing(20);
    
    // CCCD
    mainLayout->addWidget(new QLabel("CCCD (12 digits):"));
    cccdEdit = new QLineEdit;
    cccdEdit->setPlaceholderText("Enter your CCCD");
    mainLayout->addWidget(cccdEdit);
    
    // Password
    mainLayout->addWidget(new QLabel("Password:"));
    passwordEdit = new QLineEdit;
    passwordEdit->setPlaceholderText("Enter your password");
    passwordEdit->setEchoMode(QLineEdit::Password);
    mainLayout->addWidget(passwordEdit);
    
    mainLayout->addSpacing(20);
    
    // Buttons
    QHBoxLayout *btnLayout = new QHBoxLayout;
    loginBtn = new QPushButton("Login");
    cancelBtn = new QPushButton("Exit");
    loginBtn->setMinimumHeight(35);
    cancelBtn->setMinimumHeight(35);
    btnLayout->addWidget(loginBtn);
    btnLayout->addWidget(cancelBtn);
    mainLayout->addLayout(btnLayout);
    
    setLayout(mainLayout);
    
    // Connect signals
    connect(loginBtn, &QPushButton::clicked, this, &LoginDialog::onLoginClicked);
    connect(cancelBtn, &QPushButton::clicked, this, &LoginDialog::onCancelClicked);
    
    // Allow Enter key to login
    connect(passwordEdit, &QLineEdit::returnPressed, this, &LoginDialog::onLoginClicked);
}

void LoginDialog::onLoginClicked() {
    QString cccd = cccdEdit->text().trimmed();
    QString password = passwordEdit->text();
    
    // Validate input
    if (cccd.isEmpty()) {
        QMessageBox::warning(this, "Validation Error", "Please enter your CCCD!");
        cccdEdit->setFocus();
        return;
    }
    
    if (cccd.length() != 12 || !cccd.toLongLong(nullptr, 10)) {
        QMessageBox::warning(this, "Validation Error", "CCCD must be 12 digits!");
        return;
    }
    
    if (password.isEmpty()) {
        QMessageBox::warning(this, "Validation Error", "Please enter your password!");
        passwordEdit->setFocus();
        return;
    }
    
    if (!networkClient || !networkClient->IsConnected()) {
        QMessageBox::critical(this, "Error", "Server is not connected!");
        return;
    }
    
    try {
        NetworkClient::LoginResult result;
        std::string error;
        if (!networkClient->Login(cccd.toStdString(), password.toStdString(), result, error)) {
            QMessageBox::critical(this, "Error",
                                  QString("Login request failed: %1").arg(QString::fromStdString(error)));
            return;
        }

        if (result.success) {
            loginResult.success = true;
            loginResult.userRole = QString::fromStdString(result.userRole);
            loginResult.userId = result.userId;
            loginResult.userName = QString::fromStdString(result.userName);
            loginResult.networkClient = networkClient;
            accept();
        } else {
            QMessageBox::warning(this, "Login Failed", 
                QString::fromStdString(result.message));
            cccdEdit->clear();
            passwordEdit->clear();
            cccdEdit->setFocus();
        }
        
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Error",
                              QString("Login error: %1").arg(e.what()));
    }
}

void LoginDialog::onCancelClicked() {
    loginResult = {false, "", -1, "", nullptr};
    reject();
}
