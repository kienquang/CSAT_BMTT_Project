#ifndef EMPLOYEEDIALOG_H
#define EMPLOYEEDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>

class EmployeeDialog : public QDialog {
    Q_OBJECT

public:
    struct EmployeeData {
        QString name;
        QString role;
        QString cccd;
        QString phone;
        QString password;
        QString salary;
    };

    enum Mode {
        AddMode,
        EditMode
    };

    EmployeeDialog(Mode mode, QWidget *parent = nullptr);
    ~EmployeeDialog();

    EmployeeData getEmployeeData() const;
    void setEmployeeData(const EmployeeData& data);

private:
    void setupUi();
    void connectSignals();

    Mode mode;
    
    QLineEdit *nameEdit;
    QComboBox *roleCombo;
    QLineEdit *cccdEdit;
    QLineEdit *phoneEdit;
    QLineEdit *passwordEdit;
    QLineEdit *salaryEdit;
    
    QPushButton *saveBtn;
    QPushButton *cancelBtn;
};

#endif // EMPLOYEEDIALOG_H
