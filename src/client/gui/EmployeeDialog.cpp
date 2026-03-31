#include "EmployeeDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QMessageBox>

EmployeeDialog::EmployeeDialog(Mode mode, QWidget *parent)
    : QDialog(parent), mode(mode)
{
    setWindowTitle(mode == AddMode ? "Add New Employee" : "Edit Employee");
    setModal(true);
    setMinimumWidth(400);
    
    setupUi();
    connectSignals();
}

EmployeeDialog::~EmployeeDialog() {}

void EmployeeDialog::setupUi() {
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    // Name
    mainLayout->addWidget(new QLabel("Name:"));
    nameEdit = new QLineEdit;
    mainLayout->addWidget(nameEdit);
    
    // Role
    mainLayout->addWidget(new QLabel("Role:"));
    roleCombo = new QComboBox;
    roleCombo->addItem("Admin");
    roleCombo->addItem("User");
    mainLayout->addWidget(roleCombo);
    
    // CCCD
    mainLayout->addWidget(new QLabel("CCCD (12 digits):"));
    cccdEdit = new QLineEdit;
    mainLayout->addWidget(cccdEdit);
    
    // Phone
    mainLayout->addWidget(new QLabel("Phone (10 digits):"));
    phoneEdit = new QLineEdit;
    mainLayout->addWidget(phoneEdit);
    
    // Password
    mainLayout->addWidget(new QLabel("Password:"));
    passwordEdit = new QLineEdit;
    passwordEdit->setEchoMode(QLineEdit::Password);
    mainLayout->addWidget(passwordEdit);
    
    // Salary
    mainLayout->addWidget(new QLabel("Salary:"));
    salaryEdit = new QLineEdit;
    mainLayout->addWidget(salaryEdit);
    
    // Buttons
    QHBoxLayout *btnLayout = new QHBoxLayout;
    saveBtn = new QPushButton("Save");
    cancelBtn = new QPushButton("Cancel");
    btnLayout->addWidget(saveBtn);
    btnLayout->addWidget(cancelBtn);
    mainLayout->addLayout(btnLayout);
    
    setLayout(mainLayout);
}

void EmployeeDialog::connectSignals() {
    connect(saveBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

EmployeeDialog::EmployeeData EmployeeDialog::getEmployeeData() const {
    return {
        nameEdit->text(),
        roleCombo->currentText(),
        cccdEdit->text(),
        phoneEdit->text(),
        passwordEdit->text(),
        salaryEdit->text()
    };
}

void EmployeeDialog::setEmployeeData(const EmployeeData& data) {
    nameEdit->setText(data.name);
    roleCombo->setCurrentText(data.role);
    cccdEdit->setText(data.cccd);
    phoneEdit->setText(data.phone);
    passwordEdit->setText(data.password);
    salaryEdit->setText(data.salary);
}
