#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <memory>
#include "../core/DatabaseHelper.h"

class LoginDialog : public QDialog {
    Q_OBJECT

public:
    struct LoginResult {
        bool success;
        QString userRole;    // "Admin" or "User"
        int userId;
    };

    LoginDialog(QWidget *parent = nullptr);
    ~LoginDialog();

    LoginResult getLoginResult() const { return loginResult; }

private slots:
    void onLoginClicked();
    void onCancelClicked();

private:
    void setupUi();
    LoginResult loginResult;
    
    QLineEdit *cccdEdit;
    QLineEdit *passwordEdit;
    QPushButton *loginBtn;
    QPushButton *cancelBtn;
    
    std::unique_ptr<DatabaseHelper> dbHelper;
};

#endif // LOGINDIALOG_H
