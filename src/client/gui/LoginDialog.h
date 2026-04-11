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
        bool success = false;
        int userId = -1;
        int role = 1;
        QString username;
        std::shared_ptr<NetworkClient> networkClient;
    };

    LoginDialog(QWidget* parent = nullptr);
    ~LoginDialog();

    LoginResult getLoginResult() const { return loginResult; }

private slots:
    void onLoginClicked();
    void onRegisterClicked();
    void onCancelClicked();

private:
    void setupUi();

    LoginResult loginResult;

    QLineEdit* usernameEdit;
    QLineEdit* passwordEdit;
    QPushButton* loginBtn;
    QPushButton* registerBtn;
    QPushButton* cancelBtn;

    std::shared_ptr<NetworkClient> networkClient;
};

#endif  // LOGINDIALOG_H
