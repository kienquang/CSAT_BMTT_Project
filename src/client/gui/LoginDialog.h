#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <memory>

class NetworkClient;

class LoginDialog : public QDialog {
    Q_OBJECT

public:
    struct LoginResult {
        bool success;
        QString userRole;    // "Admin" or "User"
        int userId;
        QString userName;
        std::shared_ptr<NetworkClient> networkClient;
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
    
    std::shared_ptr<NetworkClient> networkClient;
};

#endif // LOGINDIALOG_H
