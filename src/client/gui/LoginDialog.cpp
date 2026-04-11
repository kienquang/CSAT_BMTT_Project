#include "LoginDialog.h"

#include "EmployeeDialog.h"
#include "../network/NetworkClient.h"
#include "../../core/EnvConfig.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QVBoxLayout>

namespace {

bool IsDigitsOnly(const QString& value) {
    if (value.isEmpty()) {
        return false;
    }

    for (const QChar ch : value) {
        if (!ch.isDigit()) {
            return false;
        }
    }
    return true;
}

bool ValidateRecordData(const EmployeeDialog::RecordData& data, QString& error) {
    if (data.username.isEmpty()) {
        error = "Username cannot be empty.";
        return false;
    }

    if (data.password.length() < 8) {
        error = "Password must be at least 8 characters.";
        return false;
    }

    if (data.cccd.length() != 12 || !IsDigitsOnly(data.cccd)) {
        error = "CCCD must contain exactly 12 digits.";
        return false;
    }

    if (data.phone.length() < 10 || !IsDigitsOnly(data.phone)) {
        error = "Phone must contain at least 10 digits.";
        return false;
    }

    if (!data.email.contains('@') || !data.email.contains('.')) {
        error = "Email format is invalid.";
        return false;
    }

    return true;
}

}  // namespace

LoginDialog::LoginDialog(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle("CSAT_BMTT - Login");
    setModal(true);
    setMinimumWidth(420);
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

LoginDialog::~LoginDialog() {
}

void LoginDialog::setupUi() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    QLabel* titleLabel = new QLabel("CSAT_BMTT Personal Record Vault");
    titleLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: #34495e;");
    mainLayout->addWidget(titleLabel);
    mainLayout->addSpacing(20);

    mainLayout->addWidget(new QLabel("Username:"));
    usernameEdit = new QLineEdit;
    usernameEdit->setPlaceholderText("Enter your username");
    mainLayout->addWidget(usernameEdit);

    mainLayout->addWidget(new QLabel("Password:"));
    passwordEdit = new QLineEdit;
    passwordEdit->setPlaceholderText("Enter your password");
    passwordEdit->setEchoMode(QLineEdit::Password);
    mainLayout->addWidget(passwordEdit);

    mainLayout->addSpacing(20);

    QHBoxLayout* buttonLayout = new QHBoxLayout;
    loginBtn = new QPushButton("Login");
    registerBtn = new QPushButton("Register");
    cancelBtn = new QPushButton("Exit");
    loginBtn->setMinimumHeight(35);
    registerBtn->setMinimumHeight(35);
    cancelBtn->setMinimumHeight(35);
    buttonLayout->addWidget(loginBtn);
    buttonLayout->addWidget(registerBtn);
    buttonLayout->addWidget(cancelBtn);
    mainLayout->addLayout(buttonLayout);

    setLayout(mainLayout);

    connect(loginBtn, &QPushButton::clicked, this, &LoginDialog::onLoginClicked);
    connect(registerBtn, &QPushButton::clicked, this, &LoginDialog::onRegisterClicked);
    connect(cancelBtn, &QPushButton::clicked, this, &LoginDialog::onCancelClicked);
    connect(passwordEdit, &QLineEdit::returnPressed, this, &LoginDialog::onLoginClicked);
}

void LoginDialog::onLoginClicked() {
    const QString username = usernameEdit->text().trimmed();
    const QString password = passwordEdit->text();

    if (username.isEmpty()) {
        QMessageBox::warning(this, "Validation Error", "Please enter your username.");
        usernameEdit->setFocus();
        return;
    }

    if (password.isEmpty()) {
        QMessageBox::warning(this, "Validation Error", "Please enter your password.");
        passwordEdit->setFocus();
        return;
    }

    if (!networkClient || !networkClient->IsConnected()) {
        QMessageBox::critical(this, "Error", "Server is not connected.");
        return;
    }

    NetworkClient::LoginResult result;
    std::string error;
    if (!networkClient->Login(username.toStdString(), password.toStdString(), result, error)) {
        QMessageBox::critical(this, "Error",
                              QString("Login request failed: %1").arg(QString::fromStdString(error)));
        return;
    }

    if (!result.success) {
        QMessageBox::warning(this, "Login Failed", QString::fromStdString(result.message));
        passwordEdit->clear();
        passwordEdit->setFocus();
        return;
    }

    loginResult.success = true;
    loginResult.userId = result.userId;
    loginResult.role = result.role;
    loginResult.username = QString::fromStdString(result.username);
    loginResult.networkClient = networkClient;
    accept();
}

void LoginDialog::onRegisterClicked() {
    if (!networkClient || !networkClient->IsConnected()) {
        QMessageBox::critical(this, "Error", "Server is not connected.");
        return;
    }

    EmployeeDialog dialog(EmployeeDialog::RegisterMode, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const EmployeeDialog::RecordData data = dialog.getRecordData();
    QString validationError;
    if (!ValidateRecordData(data, validationError)) {
        QMessageBox::warning(this, "Validation Error", validationError);
        return;
    }

    PersonalRecord record;
    record.username = data.username.toStdString();
    record.gender = data.gender;
    record.cccd = data.cccd.toStdString();
    record.phone = data.phone.toStdString();
    record.email = data.email.toStdString();

    std::string error;
    if (!networkClient->Register(record, data.password.toStdString(), error)) {
        QMessageBox::critical(this, "Registration Failed",
                              QString::fromStdString(error));
        return;
    }

    QMessageBox::information(this, "Registration Successful",
                             "Your account has been created. You can login now.");
    usernameEdit->setText(data.username);
    passwordEdit->setText(data.password);
}

void LoginDialog::onCancelClicked() {
    loginResult = LoginResult{};
    reject();
}
