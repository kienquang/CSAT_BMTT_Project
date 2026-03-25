#include "LoginDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <iostream>

LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent), 
      loginResult{false, "", -1}
{
    setWindowTitle("CSAT_BMTT - Login");
    setModal(true);
    setMinimumWidth(400);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    
    setupUi();
    
    // Initialize database
    dbHelper = std::make_unique<DatabaseHelper>(
        "localhost",
        3306,
        "root",
        "",  // No password for root
        "csatbmtt"
    );
    
    if (!dbHelper->Connect()) {
        QMessageBox::critical(this, "Database Error",
                              "Failed to connect to database.\n"
                              "Please check your database configuration.");
        return;
    }
    
    // Initialize database tables and views
    std::cout << "[LOGIN] Creating database tables..." << std::endl;
    if (!dbHelper->CreateTableNhanVien()) {
        QMessageBox::critical(this, "Database Error",
                              "Failed to create database tables.\n"
                              "Please check your database permissions.");
        return;
    }
    
    if (!dbHelper->CreateRoleBasedViews()) {
        QMessageBox::warning(this, "Database Warning",
                             "Failed to create database views.\n"
                             "Some features may not work correctly.");
    }
    
    std::cout << "[LOGIN] Database initialization completed" << std::endl;
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
    
    // Check database connection
    if (!dbHelper || !dbHelper->IsConnected()) {
        QMessageBox::critical(this, "Error", "Database is not connected!");
        return;
    }
    
    try {
        // Authenticate user
        nhanvien user = dbHelper->AuthenticateUser(cccd.toStdString(), password.toStdString());
        
        if (user.id != -1) {
            // Login successful
            loginResult.success = true;
            loginResult.userRole = QString::fromStdString(user.vai_tro);
            loginResult.userId = user.id;
            
            accept();
        } else {
            // Login failed
            QMessageBox::warning(this, "Login Failed", 
                "Invalid CCCD or password!\nPlease try again.");
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
    loginResult = {false, "", -1};
    reject();
}
