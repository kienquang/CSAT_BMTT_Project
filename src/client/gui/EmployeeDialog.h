#ifndef EMPLOYEEDIALOG_H
#define EMPLOYEEDIALOG_H

#include <QComboBox>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

class EmployeeDialog : public QDialog {
    Q_OBJECT

public:
    struct RecordData {
        QString username;
        QString name;
        int gender = 1;
        QString cccd;
        QString phone;
        QString email;
        QString password;
        QString currentPassword;
        QString newPassword;
        bool changePassword = false;
    };

    enum Mode {
        RegisterMode,
        EditMode
    };

    EmployeeDialog(Mode mode, QWidget* parent = nullptr, bool showPasswordField = true);
    ~EmployeeDialog();

    RecordData getRecordData() const;
    void setRecordData(const RecordData& data);

private:
    void setupUi();
    void connectSignals();
    void onChangePasswordClicked();

    Mode mode;
    bool showPasswordField;

    QLineEdit* usernameEdit;
    QLineEdit* nameEdit;
    QComboBox* genderCombo;
    QLineEdit* cccdEdit;
    QLineEdit* phoneEdit;
    QLineEdit* emailEdit;
    QLabel* passwordLabel;
    QLineEdit* passwordEdit;
    QPushButton* changePasswordBtn;

    QString pendingCurrentPassword;
    QString pendingNewPassword;
    bool changePasswordRequested;

    QPushButton* saveBtn;
    QPushButton* cancelBtn;
};

#endif  // EMPLOYEEDIALOG_H
