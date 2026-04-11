#ifndef EMPLOYEEDIALOG_H
#define EMPLOYEEDIALOG_H

#include <QComboBox>
#include <QDialog>
#include <QLineEdit>
#include <QPushButton>

class EmployeeDialog : public QDialog {
    Q_OBJECT

public:
    struct RecordData {
        QString username;
        int gender = 1;
        QString cccd;
        QString phone;
        QString email;
        QString password;
    };

    enum Mode {
        RegisterMode,
        EditMode
    };

    EmployeeDialog(Mode mode, QWidget* parent = nullptr);
    ~EmployeeDialog();

    RecordData getRecordData() const;
    void setRecordData(const RecordData& data);

private:
    void setupUi();
    void connectSignals();

    Mode mode;

    QLineEdit* usernameEdit;
    QComboBox* genderCombo;
    QLineEdit* cccdEdit;
    QLineEdit* phoneEdit;
    QLineEdit* emailEdit;
    QLineEdit* passwordEdit;

    QPushButton* saveBtn;
    QPushButton* cancelBtn;
};

#endif  // EMPLOYEEDIALOG_H
