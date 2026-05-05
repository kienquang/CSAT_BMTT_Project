#include "EmployeeDialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QFont>

EmployeeDialog::EmployeeDialog(Mode mode, QWidget* parent, bool showPasswordField)
        : QDialog(parent),
            mode(mode),
    showPasswordField(showPasswordField),
    changePasswordBtn(nullptr),
    changePasswordRequested(false) {
    setWindowTitle(mode == RegisterMode ? "Register New Account" : "Edit Personal Profile");
    setModal(true);
    setMinimumWidth(420);
    setFont(QFont("Segoe UI", 10));

    setupUi();
    connectSignals();
}

EmployeeDialog::~EmployeeDialog() {
}

void EmployeeDialog::setupUi() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(18, 18, 18, 18);
    mainLayout->setSpacing(8);

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

        changePasswordBtn = new QPushButton("Change Password...");
        changePasswordBtn->setStyleSheet(
            "QPushButton { background-color: #1e88e5; color: white; font-weight: bold; padding: 6px 10px; border-radius: 4px; }"
            "QPushButton:hover { background-color: #1565c0; }"
            "QPushButton:pressed { background-color: #0d47a1; }");
        mainLayout->addWidget(changePasswordBtn);
    }

    QHBoxLayout* buttonLayout = new QHBoxLayout;
    saveBtn = new QPushButton(mode == RegisterMode ? "Register" : "Save");
    cancelBtn = new QPushButton("Cancel");
    buttonLayout->addWidget(saveBtn);
    buttonLayout->addWidget(cancelBtn);
    mainLayout->addLayout(buttonLayout);

    setLayout(mainLayout);

    setStyleSheet(
        "QDialog { background-color: #f0f8f9; color: #17363e; }"
        "QLabel { color: #2d4a53; font-weight: 600; }"
        "QLineEdit, QComboBox { background-color: #ffffff; border: 1px solid #bad6db; border-radius: 8px; padding: 7px 9px; }"
        "QLineEdit:focus, QComboBox:focus { border: 1px solid #1f8b99; }"
        "QPushButton { border: none; border-radius: 8px; padding: 8px 12px; color: #ffffff; font-weight: 700; }"
        "QPushButton:hover { opacity: 0.95; }");

    saveBtn->setStyleSheet("QPushButton { background-color: #1f8b99; }");
    cancelBtn->setStyleSheet("QPushButton { background-color: #607d8b; }");
}

void EmployeeDialog::connectSignals() {
    connect(saveBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    if (changePasswordBtn != nullptr) {
        connect(changePasswordBtn, &QPushButton::clicked, this, &EmployeeDialog::onChangePasswordClicked);
    }
}

void EmployeeDialog::onChangePasswordClicked() {
    QDialog passwordDialog(this);
    passwordDialog.setWindowTitle("Change Password");
    passwordDialog.setModal(true);

    QVBoxLayout* rootLayout = new QVBoxLayout(&passwordDialog);
    QFormLayout* formLayout = new QFormLayout;

    passwordDialog.setStyleSheet(
        "QDialog { background-color: #f0f8f9; color: #17363e; }"
        "QLabel { color: #2d4a53; font-weight: 600; }"
        "QLineEdit { background-color: #ffffff; border: 1px solid #bad6db; border-radius: 8px; padding: 7px 9px; }"
        "QLineEdit:focus { border: 1px solid #1f8b99; }"
        "QPushButton { border: none; border-radius: 8px; padding: 7px 12px; font-weight: 700; }"
        "QPushButton:hover { opacity: 0.95; }");

    QLineEdit* currentPasswordEdit = new QLineEdit;
    currentPasswordEdit->setEchoMode(QLineEdit::Password);
    currentPasswordEdit->setPlaceholderText("Enter current password");

    QLineEdit* newPasswordEdit = new QLineEdit;
    newPasswordEdit->setEchoMode(QLineEdit::Password);
    newPasswordEdit->setPlaceholderText("Enter new password");

    QLineEdit* confirmPasswordEdit = new QLineEdit;
    confirmPasswordEdit->setEchoMode(QLineEdit::Password);
    confirmPasswordEdit->setPlaceholderText("Confirm new password");

    formLayout->addRow("Current password:", currentPasswordEdit);
    formLayout->addRow("New password:", newPasswordEdit);
    formLayout->addRow("Confirm password:", confirmPasswordEdit);
    rootLayout->addLayout(formLayout);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    if (buttonBox->button(QDialogButtonBox::Ok) != nullptr) {
        buttonBox->button(QDialogButtonBox::Ok)->setStyleSheet("QPushButton { background-color: #1f8b99; color: white; }");
    }
    if (buttonBox->button(QDialogButtonBox::Cancel) != nullptr) {
        buttonBox->button(QDialogButtonBox::Cancel)->setStyleSheet("QPushButton { background-color: #607d8b; color: white; }");
    }
    rootLayout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::rejected, &passwordDialog, &QDialog::reject);
    connect(buttonBox, &QDialogButtonBox::accepted, &passwordDialog, [&]() {
        const QString currentPassword = currentPasswordEdit->text();
        const QString newPassword = newPasswordEdit->text();
        const QString confirmPassword = confirmPasswordEdit->text();

        if (currentPassword.isEmpty() || newPassword.isEmpty() || confirmPassword.isEmpty()) {
            QMessageBox::warning(&passwordDialog, "Validation Error", "Please fill all password fields.");
            return;
        }

        if (newPassword.length() < 8) {
            QMessageBox::warning(&passwordDialog, "Validation Error", "New password must be at least 8 characters.");
            return;
        }

        if (newPassword != confirmPassword) {
            QMessageBox::warning(&passwordDialog, "Validation Error", "New password confirmation does not match.");
            return;
        }

        pendingCurrentPassword = currentPassword;
        pendingNewPassword = newPassword;
        changePasswordRequested = true;
        passwordDialog.accept();
    });

    if (passwordDialog.exec() == QDialog::Accepted && changePasswordBtn != nullptr) {
        changePasswordBtn->setText("Change Password... (ready)");
        changePasswordBtn->setStyleSheet(
            "QPushButton { background-color: #2e7d32; color: white; font-weight: bold; padding: 6px 10px; border-radius: 4px; }"
            "QPushButton:hover { background-color: #1b5e20; }"
            "QPushButton:pressed { background-color: #0f3d14; }");
    }
}

EmployeeDialog::RecordData EmployeeDialog::getRecordData() const {
    return {
        usernameEdit->text().trimmed(),
        nameEdit->text().trimmed(),
        genderCombo->currentData().toInt(),
        cccdEdit->text().trimmed(),
        phoneEdit->text().trimmed(),
        emailEdit->text().trimmed(),
        passwordEdit->text(),
        pendingCurrentPassword,
        pendingNewPassword,
        changePasswordRequested
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
    pendingCurrentPassword.clear();
    pendingNewPassword.clear();
    changePasswordRequested = false;

    if (changePasswordBtn != nullptr) {
        changePasswordBtn->setText("Change Password...");
        changePasswordBtn->setStyleSheet(
            "QPushButton { background-color: #1e88e5; color: white; font-weight: bold; padding: 6px 10px; border-radius: 4px; }"
            "QPushButton:hover { background-color: #1565c0; }"
            "QPushButton:pressed { background-color: #0d47a1; }");
    }
}
