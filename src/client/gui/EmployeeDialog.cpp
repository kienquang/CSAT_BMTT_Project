#include "EmployeeDialog.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

EmployeeDialog::EmployeeDialog(Mode mode, QWidget* parent, bool showPasswordField)
        : QDialog(parent),
            mode(mode),
            showPasswordField(showPasswordField) {
    setWindowTitle(mode == RegisterMode ? "Register New Account" : "Edit Personal Profile");
    setModal(true);
    setMinimumWidth(420);

    setupUi();
    connectSignals();
}

EmployeeDialog::~EmployeeDialog() {
}

void EmployeeDialog::setupUi() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    mainLayout->addWidget(new QLabel("Username:"));
    usernameEdit = new QLineEdit;
    usernameEdit->setPlaceholderText("Enter login username");
    mainLayout->addWidget(usernameEdit);

    mainLayout->addWidget(new QLabel("Full name:"));
    nameEdit = new QLineEdit;
    nameEdit->setPlaceholderText("Enter your full name");
    mainLayout->addWidget(nameEdit);

    mainLayout->addWidget(new QLabel("Gender:"));
    genderCombo = new QComboBox;
    genderCombo->addItem("Male", 1);
    genderCombo->addItem("Female", 2);
    mainLayout->addWidget(genderCombo);

    mainLayout->addWidget(new QLabel("CCCD (12 digits):"));
    cccdEdit = new QLineEdit;
    cccdEdit->setPlaceholderText("Enter CCCD");
    mainLayout->addWidget(cccdEdit);

    mainLayout->addWidget(new QLabel("Phone (>=10 digits):"));
    phoneEdit = new QLineEdit;
    phoneEdit->setPlaceholderText("Enter phone number");
    mainLayout->addWidget(phoneEdit);

    mainLayout->addWidget(new QLabel("Email:"));
    emailEdit = new QLineEdit;
    emailEdit->setPlaceholderText("Enter email address");
    mainLayout->addWidget(emailEdit);

    passwordLabel = new QLabel(mode == RegisterMode
                                   ? "Password:"
                                   : "Password (enter again to re-wrap DEK):");
    mainLayout->addWidget(passwordLabel);
    passwordEdit = new QLineEdit;
    passwordEdit->setEchoMode(QLineEdit::Password);
    passwordEdit->setPlaceholderText(mode == RegisterMode
                                         ? "Enter password"
                                         : "Enter your current or new password");
    mainLayout->addWidget(passwordEdit);

    if (!showPasswordField) {
        passwordLabel->hide();
        passwordEdit->hide();
    }

    QHBoxLayout* buttonLayout = new QHBoxLayout;
    saveBtn = new QPushButton(mode == RegisterMode ? "Register" : "Save");
    cancelBtn = new QPushButton("Cancel");
    buttonLayout->addWidget(saveBtn);
    buttonLayout->addWidget(cancelBtn);
    mainLayout->addLayout(buttonLayout);

    setLayout(mainLayout);
}

void EmployeeDialog::connectSignals() {
    connect(saveBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

EmployeeDialog::RecordData EmployeeDialog::getRecordData() const {
    return {
        usernameEdit->text().trimmed(),
        nameEdit->text().trimmed(),
        genderCombo->currentData().toInt(),
        cccdEdit->text().trimmed(),
        phoneEdit->text().trimmed(),
        emailEdit->text().trimmed(),
        passwordEdit->text()
    };
}

void EmployeeDialog::setRecordData(const RecordData& data) {
    usernameEdit->setText(data.username);
    nameEdit->setText(data.name);
    const int genderIndex = genderCombo->findData(data.gender);
    genderCombo->setCurrentIndex(genderIndex >= 0 ? genderIndex : 0);
    cccdEdit->setText(data.cccd);
    phoneEdit->setText(data.phone);
    emailEdit->setText(data.email);
    passwordEdit->clear();
}
