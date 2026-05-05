#include "MainWindow.h"

#include "EmployeeDialog.h"
#include "MedicalRecordDetailDialog.h"
#include "ui_MainWindow.h"
#include "../network/NetworkClient.h"

#include <QApplication>
#include <QDate>
#include <QDialog>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QFont>

#include <iostream>
#include <stdexcept>

namespace {

// [GROUP: Validation Helpers]
bool IsDigitsOnly(const QString& value) {
    if (value.isEmpty()) {
        return false;
    }

    for (const QChar ch : value) {
        if (!ch.isDigit()) {
            return false;
        }
    }
    return true;
}

bool ValidateRecordData(const EmployeeDialog::RecordData& data, QString& error) {
    if (data.username.isEmpty()) {
        error = "Username cannot be empty.";
        return false;
    }

    if (data.name.isEmpty()) {
        error = "Full name cannot be empty.";
        return false;
    }

    if (data.password.length() < 8) {
        error = "Password must be at least 8 characters.";
        return false;
    }

    if (data.cccd.length() != 12 || !IsDigitsOnly(data.cccd)) {
        error = "CCCD must contain exactly 12 digits.";
        return false;
    }

    if (data.phone.length() < 10 || !IsDigitsOnly(data.phone)) {
        error = "Phone must contain at least 10 digits.";
        return false;
    }

    if (!data.email.contains('@') || !data.email.contains('.')) {
        error = "Email format is invalid.";
        return false;
    }

    return true;
}

// [GROUP: Text Formatting Helpers]
QString GenderToLabel(int gender) {
    return gender == 2 ? "Female" : "Male";
}

QString RoleToLabel(int role) {
    if (role == 2) {
        return "Doctor";
    }
    if (role == 3) {
        return "Admin";
    }
    return "User";
}

int LabelToRole(const QString& roleLabel) {
    if (roleLabel.compare("Admin", Qt::CaseInsensitive) == 0) {
        return 3;
    }
    if (roleLabel.compare("Doctor", Qt::CaseInsensitive) == 0) {
        return 2;
    }
    return 1;
}

bool IsAdminRole(int role) {
    return role == 3;
}

bool IsDoctorRole(int role) {
    return role == 2;
}

// [GROUP: Masking Helpers]
QString MaskKeepLast(const std::string& value, int keepLast) {
    const QString text = QString::fromStdString(value);
    if (text.isEmpty()) {
        return text;
    }

    if (keepLast <= 0 || text.size() <= keepLast) {
        return QString(text.size(), '*');
    }

    return QString(text.size() - keepLast, '*') + text.right(keepLast);
}

QString MaskEmail(const std::string& email) {
    const QString value = QString::fromStdString(email);
    const int atPos = value.indexOf('@');
    if (atPos <= 1) {
        return "***";
    }

    const QString local = value.left(atPos);
    const QString domain = value.mid(atPos);
    return local.left(1) + QString(local.size() - 1, '*') + domain;
}

}  // namespace

MainWindow::MainWindow(std::shared_ptr<NetworkClient> networkClient,
                       QWidget* parent,
                       const QString& username,
                       int role)
    : QMainWindow(parent),
      ui(new Ui::MainWindow),
      networkClient(std::move(networkClient)),
      currentUsername(username),
      currentRole(role),
      selectedRecordId(-1),
            selectedUserId(-1),
            selectedUserRole(1),
      hasLoadedRecord(false) {
    ui->setupUi(this);
    ui->roleDisplay->setText(RoleToLabel(currentRole));

    setupConnections();
    applyStyles();
    configureUiForRole();
    loadProfileData();
    updateStatistics();
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::setupConnections() {
    connect(ui->addBtn, &QPushButton::clicked, this, &MainWindow::onRefreshProfile);
    connect(ui->editBtn, &QPushButton::clicked, this, &MainWindow::onEditProfile);
    connect(ui->deleteBtn, &QPushButton::clicked, this, &MainWindow::onDeleteAccount);
    connect(ui->viewMyInfoBtn, &QPushButton::clicked, this, &MainWindow::onViewMyInfo);
    connect(ui->logoutBtn, &QPushButton::clicked, this, &MainWindow::onLogout);
    connect(ui->searchBox, &QLineEdit::textChanged, this, &MainWindow::onSearch);
    connect(ui->employeeTable, &QTableWidget::itemSelectionChanged, this, &MainWindow::onTableRowSelection);
    connect(ui->adminTabWidget, &QTabWidget::currentChanged, this, &MainWindow::onAdminTabChanged);
    connect(ui->createMedicalRecordBtn, &QPushButton::clicked, this, &MainWindow::onCreateMedicalRecord);
}

bool MainWindow::isAdminMode() const {
    return IsAdminRole(currentRole);
}

bool MainWindow::isDoctorMode() const {
    return IsDoctorRole(currentRole);
}

bool MainWindow::isUserMode() const {
    return currentRole == 1;
}

bool MainWindow::isAdminUsersTabActive() const {
    return isAdminMode() && ui->adminTabWidget->currentIndex() == 0;
}

bool MainWindow::isDoctorListTabActive() const {
    return isDoctorMode() && ui->adminTabWidget->currentIndex() == 0;
}

bool MainWindow::isDoctorCreateTabActive() const {
    return isDoctorMode() && ui->adminTabWidget->currentIndex() == 1;
}

void MainWindow::configureUiForRole() {
    if (isAdminMode()) {
        ui->titleLabel->setText("Admin - Encrypted User Directory");
        ui->addBtn->setText("Refresh Users");
        ui->addBtn->setVisible(true);
        ui->editBtn->setVisible(true);
        ui->deleteBtn->setVisible(true);
        ui->editBtn->setText("Phân quyền");
        ui->deleteBtn->setText("Xóa tài khoản");
        ui->editBtn->setEnabled(false);
        ui->deleteBtn->setEnabled(false);
        ui->viewMyInfoBtn->setVisible(true);
        ui->viewMyInfoBtn->setEnabled(true);
        ui->adminTabWidget->setVisible(true);
        ui->adminTabWidget->setEnabled(true);
        ui->adminTabWidget->setTabText(0, "Danh sách user");
        ui->adminTabWidget->setTabText(1, "Danh sách bệnh án");
        ui->doctorCreateGroup->setVisible(false);
        ui->employeeTable->setVisible(true);
        ui->searchBox->setEnabled(true);
        ui->searchBox->setPlaceholderText("Search encrypted user list...");
        return;
    }

    if (isDoctorMode()) {
        ui->titleLabel->setText("Doctor - Medical Record Workspace");
        ui->addBtn->setText("Refresh Records");
        ui->addBtn->setVisible(true);
        ui->editBtn->setVisible(false);
        ui->deleteBtn->setVisible(false);
        ui->viewMyInfoBtn->setVisible(true);
        ui->viewMyInfoBtn->setEnabled(true);
        ui->adminTabWidget->setVisible(true);
        ui->adminTabWidget->setEnabled(true);
        ui->adminTabWidget->setTabText(0, "Bệnh án đã lập");
        ui->adminTabWidget->setTabText(1, "Lập bệnh án");
        ui->doctorCreateGroup->setVisible(false);
        ui->employeeTable->setVisible(true);
        ui->searchBox->setEnabled(true);
        ui->searchBox->setPlaceholderText("Search your medical records...");
        resetDoctorCreateForm();
        return;
    }

    ui->titleLabel->setText("User - My Medical Records");
    ui->addBtn->setVisible(true);
    ui->addBtn->setText("Refresh Records");
    ui->addBtn->setEnabled(true);
    ui->editBtn->setVisible(false);
    ui->deleteBtn->setVisible(false);
    ui->editBtn->setEnabled(false);
    ui->deleteBtn->setEnabled(false);
    ui->viewMyInfoBtn->setVisible(true);
    ui->viewMyInfoBtn->setEnabled(true);
    ui->adminTabWidget->setVisible(false);
    ui->adminTabWidget->setEnabled(false);
    ui->doctorCreateGroup->setVisible(false);
    ui->employeeTable->setVisible(true);
    ui->searchBox->setEnabled(true);
    ui->searchBox->setPlaceholderText("Search your medical records...");
}

void MainWindow::loadAdminUserList() {
    ui->employeeTable->setRowCount(0);
    ui->employeeTable->setColumnCount(8);
    ui->employeeTable->setHorizontalHeaderLabels(QStringList{
        "Record ID",
        "User ID",
        "Username",
        "Role",
        "Gender",
        "CCCD",
        "Phone",
        "Email"
    });

    std::string error;
    std::vector<PersonalRecord> records;
    if (!networkClient->FetchEncryptedUserList(records, error)) {
        throw std::runtime_error(error);
    }

    for (int i = 0; i < static_cast<int>(records.size()); ++i) {
        const PersonalRecord& record = records[static_cast<size_t>(i)];
        ui->employeeTable->insertRow(i);
        ui->employeeTable->setItem(i, 0, new QTableWidgetItem(QString::number(record.recordId)));
        ui->employeeTable->setItem(i, 1, new QTableWidgetItem(QString::number(record.userId)));
        ui->employeeTable->setItem(i, 2, new QTableWidgetItem(QString::fromStdString(record.username)));
        ui->employeeTable->setItem(i, 3, new QTableWidgetItem(RoleToLabel(record.role)));
        ui->employeeTable->setItem(i, 4, new QTableWidgetItem(GenderToLabel(record.gender)));
        ui->employeeTable->setItem(i, 5, new QTableWidgetItem(QString::fromStdString(record.cccd)));
        ui->employeeTable->setItem(i, 6, new QTableWidgetItem(QString::fromStdString(record.phone)));
        ui->employeeTable->setItem(i, 7, new QTableWidgetItem(QString::fromStdString(record.email)));
    }

    if (!records.empty()) {
        ui->employeeTable->selectRow(0);
        hasLoadedRecord = true;
        selectedRecordId = records.front().recordId;
        selectedUserId = records.front().userId;
        selectedUserRole = records.front().role;
    } else {
        selectedRecordId = -1;
        selectedUserId = -1;
        selectedUserRole = 1;
        hasLoadedRecord = false;
        ui->editBtn->setEnabled(false);
        ui->deleteBtn->setEnabled(false);
    }
}

void MainWindow::loadAdminMedicalRecordList() {
    std::string error;
    std::vector<MedicalRecordListItem> records;
    if (!networkClient->FetchMedicalRecordList(records, error)) {
        throw std::runtime_error(error);
    }

    populateMedicalRecordTable(records);
}

void MainWindow::loadDoctorMedicalRecordList() {
    std::string error;
    std::vector<MedicalRecordListItem> records;
    if (!networkClient->FetchMyMedicalRecordList(records, error)) {
        throw std::runtime_error(error);
    }

    populateMedicalRecordTable(records);
}

void MainWindow::loadUserMedicalRecordList() {
    std::string error;
    std::vector<MedicalRecordListItem> records;
    if (!networkClient->FetchMyMedicalRecordList(records, error)) {
        throw std::runtime_error(error);
    }

    populateMedicalRecordTable(records);
}

void MainWindow::populateMedicalRecordTable(const std::vector<MedicalRecordListItem>& records) {
    ui->employeeTable->setRowCount(0);
    if (isUserMode()) {
        ui->employeeTable->setColumnCount(9);
        ui->employeeTable->setHorizontalHeaderLabels(QStringList{
            "Medical ID",
            "Patient ID",
            "Patient Name",
            "Doctor ID",
            "Doctor Name",
            "Visit Date",
            "Department",
            "Diagnosis",
            "Prescription"
        });
    } else {
        ui->employeeTable->setColumnCount(8);
        ui->employeeTable->setHorizontalHeaderLabels(QStringList{
            "Medical ID",
            "Patient ID",
            "Patient Name",
            "Doctor ID",
            "Visit Date",
            "Department",
            "Diagnosis",
            "Prescription"
        });
    }

    for (int i = 0; i < static_cast<int>(records.size()); ++i) {
        const MedicalRecordListItem& record = records[static_cast<size_t>(i)];
        ui->employeeTable->insertRow(i);
        ui->employeeTable->setItem(i, 0, new QTableWidgetItem(QString::number(record.recordId)));
        ui->employeeTable->setItem(i, 1, new QTableWidgetItem(QString::number(record.patientId)));
        ui->employeeTable->setItem(i, 2, new QTableWidgetItem(QString::fromStdString(record.patientName)));
        ui->employeeTable->setItem(i, 3, new QTableWidgetItem(QString::number(record.doctorId)));
        if (isUserMode()) {
            ui->employeeTable->setItem(i, 4, new QTableWidgetItem(QString::fromStdString(record.doctorName)));
            ui->employeeTable->setItem(i, 5, new QTableWidgetItem(QString::fromStdString(record.visitDate)));
            ui->employeeTable->setItem(i, 6, new QTableWidgetItem(QString::fromStdString(record.department)));
            ui->employeeTable->setItem(i, 7, new QTableWidgetItem(QString::fromStdString(record.diagnosis)));
            ui->employeeTable->setItem(i, 8, new QTableWidgetItem(QString::fromStdString(record.prescription)));
        } else {
            ui->employeeTable->setItem(i, 4, new QTableWidgetItem(QString::fromStdString(record.visitDate)));
            ui->employeeTable->setItem(i, 5, new QTableWidgetItem(QString::fromStdString(record.department)));
            ui->employeeTable->setItem(i, 6, new QTableWidgetItem(QString::fromStdString(record.diagnosis)));
            ui->employeeTable->setItem(i, 7, new QTableWidgetItem(QString::fromStdString(record.prescription)));
        }
    }

    hasLoadedRecord = false;
    selectedRecordId = -1;
    selectedUserId = -1;
    selectedUserRole = 1;
    ui->editBtn->setEnabled(false);
    ui->deleteBtn->setEnabled(false);
}

void MainWindow::loadNonAdminPlaceholder() {
    ui->employeeTable->setRowCount(0);
    ui->employeeTable->setColumnCount(1);
    ui->employeeTable->setHorizontalHeaderLabels(QStringList{"Notice"});
    ui->employeeTable->insertRow(0);
    ui->employeeTable->setItem(0, 0, new QTableWidgetItem("Non-admin interface is temporarily empty."));
    hasLoadedRecord = false;
    selectedRecordId = -1;
    selectedUserId = -1;
    selectedUserRole = 1;
}

void MainWindow::resetDoctorCreateForm() {
    ui->patientIdInput->clear();
    ui->visitDateInput->setDate(QDate::currentDate());
    ui->departmentInput->clear();
    ui->diagnosisInput->clear();
    ui->prescriptionInput->clear();
}

bool MainWindow::promptPasswordForSensitiveAction(const QString& title,
                                                  const QString& prompt,
                                                  QString& passwordOut) const {
    bool ok = false;
    const QString password = QInputDialog::getText(
        const_cast<MainWindow*>(this),
        title,
        prompt,
        QLineEdit::Password,
        "",
        &ok);

    if (!ok) {
        return false;
    }

    if (password.isEmpty()) {
        QMessageBox::warning(const_cast<MainWindow*>(this), "Validation Error", "Mat khau khong duoc de trong.");
        return false;
    }

    passwordOut = password;
    return true;
}

void MainWindow::applyStyles() {
    setFont(QFont("Segoe UI", 10));

    setStyleSheet(
        "QMainWindow { background-color: #eaf4f5; color: #163239; }"
        "QWidget { color: #163239; }"

        "QLabel#titleLabel { font-size: 22px; font-weight: 700; color: #0c3d45; }"
        "QLabel#userLabel { color: #3b5560; font-weight: 600; }"

        "QLineEdit, QTextEdit, QDateEdit, QPlainTextEdit {"
        "  background-color: #ffffff;"
        "  border: 1px solid #b8d4d8;"
        "  border-radius: 8px;"
        "  padding: 7px 9px;"
        "  selection-background-color: #1f8b99;"
        "}"
        "QLineEdit:focus, QTextEdit:focus, QDateEdit:focus, QPlainTextEdit:focus {"
        "  border: 1px solid #1f8b99;"
        "}"

        "QPushButton {"
        "  border-radius: 9px;"
        "  border: none;"
        "  padding: 8px 14px;"
        "  font-weight: 700;"
        "  background-color: #2a9cab;"
        "  color: #ffffff;"
        "}"
        "QPushButton:hover { background-color: #228595; }"
        "QPushButton:pressed { background-color: #1c6f7d; }"
        "QPushButton:disabled { background-color: #b7c7cc; color: #edf3f4; }"

        "QPushButton#viewMyInfoBtn { background-color: #2f8f95; }"
        "QPushButton#viewMyInfoBtn:hover { background-color: #28797e; }"
        "QPushButton#logoutBtn { background-color: #cf5a52; }"
        "QPushButton#logoutBtn:hover { background-color: #b84e47; }"
        "QPushButton#editBtn { background-color: #c58a2e; }"
        "QPushButton#editBtn:hover { background-color: #a97827; }"
        "QPushButton#deleteBtn { background-color: #bf4d48; }"
        "QPushButton#createMedicalRecordBtn { background-color: #1e8d72; }"
        "QPushButton#createMedicalRecordBtn:hover { background-color: #19755f; }"

        "QGroupBox {"
        "  border: 1px solid #c8dde0;"
        "  border-radius: 10px;"
        "  margin-top: 10px;"
        "  background-color: #f7fbfc;"
        "  font-weight: 700;"
        "}"
        "QGroupBox::title {"
        "  subcontrol-origin: margin;"
        "  left: 12px;"
        "  padding: 0 6px;"
        "  color: #1f5861;"
        "}"

        "QTabWidget::pane {"
        "  border: 1px solid #c8dde0;"
        "  border-radius: 10px;"
        "  background-color: #f7fbfc;"
        "  top: -1px;"
        "}"
        "QTabBar::tab {"
        "  background-color: #dfeff1;"
        "  color: #365961;"
        "  border: 1px solid #c2d9dd;"
        "  border-bottom: none;"
        "  border-top-left-radius: 8px;"
        "  border-top-right-radius: 8px;"
        "  padding: 8px 14px;"
        "  min-width: 120px;"
        "  font-weight: 600;"
        "}"
        "QTabBar::tab:selected {"
        "  background-color: #f7fbfc;"
        "  color: #0f3f47;"
        "}"

        "QTableWidget {"
        "  background-color: #ffffff;"
        "  border: 1px solid #c8dde0;"
        "  border-radius: 10px;"
        "  gridline-color: #e6eff1;"
        "  alternate-background-color: #f5fafb;"
        "}"
        "QTableWidget::item { padding: 8px 6px; }"
        "QTableWidget::item:selected { background-color: #d8eef2; color: #0d3138; }"
        "QHeaderView::section {"
        "  background-color: #2a7280;"
        "  color: #ffffff;"
        "  font-weight: 700;"
        "  padding: 8px 6px;"
        "  border: none;"
        "  border-right: 1px solid #3c8592;"
        "}"

        "QLabel#totalValue { color: #197c89; font-size: 17px; font-weight: 800; }"
        "QLineEdit#roleDisplay { background-color: #f4fbfc; border: 1px solid #b8d4d8; font-weight: 600; }"
    );

    ui->employeeTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->employeeTable->horizontalHeader()->setStretchLastSection(true);
    ui->employeeTable->verticalHeader()->setVisible(false);

    setWindowTitle("CSAT_BMTT - Clinical Record Center");
}

void MainWindow::loadProfileData() {
    if (!networkClient || !networkClient->IsConnected()) {
        QMessageBox::warning(this, "Warning", "Server is not connected.");
        return;
    }

    ui->employeeTable->setRowCount(0);
    hasLoadedRecord = false;
    selectedRecordId = -1;
    selectedUserId = -1;
    selectedUserRole = 1;

    try {
        if (isAdminMode()) {
            ui->employeeTable->setVisible(true);
            ui->doctorCreateGroup->setVisible(false);
            if (isAdminUsersTabActive()) {
                loadAdminUserList();
            } else {
                loadAdminMedicalRecordList();
            }
        } else if (isDoctorMode()) {
            if (isDoctorListTabActive()) {
                ui->employeeTable->setVisible(true);
                ui->doctorCreateGroup->setVisible(false);
                loadDoctorMedicalRecordList();
            } else {
                ui->employeeTable->setVisible(false);
                ui->doctorCreateGroup->setVisible(true);
                resetDoctorCreateForm();
            }
        } else if (isUserMode()) {
            ui->employeeTable->setVisible(true);
            ui->doctorCreateGroup->setVisible(false);
            loadUserMedicalRecordList();
        } else {
            ui->employeeTable->setVisible(true);
            ui->doctorCreateGroup->setVisible(false);
            loadNonAdminPlaceholder();
        }

        ui->employeeTable->horizontalHeader()->stretchLastSection();
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Error",
                              QString("Failed to load data: %1").arg(e.what()));
        std::cerr << "[ERROR] " << e.what() << std::endl;
    }
}

void MainWindow::onAdminTabChanged(int) {
    if (!isAdminMode() && !isDoctorMode()) {
        return;
    }

    if (isAdminMode() && isAdminUsersTabActive()) {
        ui->searchBox->setPlaceholderText("Search encrypted user list...");
        ui->searchBox->setEnabled(true);
        ui->employeeTable->setVisible(true);
        ui->doctorCreateGroup->setVisible(false);
    } else if (isAdminMode()) {
        ui->searchBox->setPlaceholderText("Search medical records...");
        ui->searchBox->setEnabled(true);
        ui->employeeTable->setVisible(true);
        ui->doctorCreateGroup->setVisible(false);
        ui->editBtn->setEnabled(false);
        ui->deleteBtn->setEnabled(false);
    } else if (isDoctorListTabActive()) {
        ui->searchBox->setPlaceholderText("Search your medical records...");
        ui->searchBox->setEnabled(true);
        ui->employeeTable->setVisible(true);
        ui->doctorCreateGroup->setVisible(false);
    } else {
        ui->searchBox->clear();
        ui->searchBox->setPlaceholderText("Search is disabled in create tab.");
        ui->searchBox->setEnabled(false);
        ui->employeeTable->setVisible(false);
        ui->doctorCreateGroup->setVisible(true);
        resetDoctorCreateForm();
    }

    loadProfileData();
}

void MainWindow::updateStatistics() {
    if (!isAdminMode() && !isDoctorMode()) {
        ui->totalValue->setText("N/A");
        return;
    }

    if (isDoctorMode()) {
        ui->totalValue->setText("N/A");
        return;
    }

    if (!networkClient || !networkClient->IsConnected()) {
        return;
    }

    try {
        std::string error;
        int totalUsers = 0;
        if (!networkClient->GetTotalUsers(totalUsers, error)) {
            throw std::runtime_error(error);
        }
        ui->totalValue->setText(QString::number(totalUsers));
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Failed to update statistics: " << e.what() << std::endl;
    }
}

void MainWindow::onRefreshProfile() {
    loadProfileData();
    updateStatistics();
}

void MainWindow::onEditProfile() {
    if (isAdminMode()) {
        if (!isAdminUsersTabActive()) {
            QMessageBox::information(this, "Chi doc", "Tab benh an hien tai chi ho tro xem danh sach.");
            return;
        }

        const int selectedRow = ui->employeeTable->currentRow();
        const QString selectedUsername =
            (selectedRow >= 0 && ui->employeeTable->item(selectedRow, 2) != nullptr)
                ? ui->employeeTable->item(selectedRow, 2)->text()
                : QString();

        if (selectedUsername.compare(currentUsername, Qt::CaseInsensitive) == 0) {
            QMessageBox::warning(this, "Khong hop le", "Admin khong duoc tu phan quyen cho chinh minh.");
            return;
        }

        if (selectedUserId <= 0) {
            QMessageBox::warning(this, "Warning", "Vui long chon nguoi dung de phan quyen.");
            return;
        }

        const QStringList roleOptions{"User (1)", "Doctor (2)", "Admin (3)"};
        int currentIndex = 0;
        if (selectedUserRole == 2) {
            currentIndex = 1;
        } else if (selectedUserRole == 3) {
            currentIndex = 2;
        }

        bool ok = false;
        const QString selectedRoleLabel = QInputDialog::getItem(
            this,
            "Phan quyen nguoi dung",
            QString("Chon role moi cho User ID %1:").arg(selectedUserId),
            roleOptions,
            currentIndex,
            false,
            &ok);

        if (!ok || selectedRoleLabel.isEmpty()) {
            return;
        }

        int newRole = 1;
        if (selectedRoleLabel.contains("(2)")) {
            newRole = 2;
        } else if (selectedRoleLabel.contains("(3)")) {
            newRole = 3;
        }

        std::string error;
        if (!networkClient->AdminUpdateUserRole(selectedUserId, newRole, error)) {
            QMessageBox::critical(this, "Phan quyen that bai", QString::fromStdString(error));
            return;
        }

        QMessageBox::information(this, "Thanh cong", "Da cap nhat role nguoi dung.");
        loadProfileData();
        updateStatistics();
        return;
    }

    if (isDoctorMode()) {
        QMessageBox::information(this, "Notice", "Doctor interface does not use Edit button.");
        return;
    }

    if (!hasLoadedRecord || selectedRecordId == -1) {
        QMessageBox::warning(this, "Warning", "No profile is loaded.");
        return;
    }

    EmployeeDialog dialog(EmployeeDialog::EditMode, this);
    EmployeeDialog::RecordData data;
    data.username = QString::fromStdString(currentRecord.username);
    data.name = QString::fromStdString(currentRecord.name);
    data.gender = currentRecord.gender;
    data.cccd = QString::fromStdString(currentRecord.cccd);
    data.phone = QString::fromStdString(currentRecord.phone);
    data.email = QString::fromStdString(currentRecord.email);
    dialog.setRecordData(data);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const EmployeeDialog::RecordData updated = dialog.getRecordData();

    QString validationError;
    if (!ValidateRecordData(updated, validationError)) {
        QMessageBox::warning(this, "Validation Error", validationError);
        return;
    }

    PersonalRecord record = currentRecord;
    record.username = updated.username.toStdString();
    record.name = updated.name.toStdString();
    record.gender = updated.gender;
    record.cccd = updated.cccd.toStdString();
    record.phone = updated.phone.toStdString();
    record.email = updated.email.toStdString();

    std::string error;
    if (!networkClient->UpdateProfile(record, updated.password.toStdString(), error)) {
        QMessageBox::critical(this, "Update Failed", QString::fromStdString(error));
        return;
    }

    currentUsername = QString::fromStdString(record.username);
    QMessageBox::information(this, "Success", "Profile updated successfully.");
    loadProfileData();
}

void MainWindow::onDeleteAccount() {
    if (isAdminMode()) {
        if (!isAdminUsersTabActive()) {
            QMessageBox::information(this, "Chi doc", "Tab benh an hien tai chi ho tro xem danh sach.");
            return;
        }

        const int selectedRow = ui->employeeTable->currentRow();
        const QString selectedUsername =
            (selectedRow >= 0 && ui->employeeTable->item(selectedRow, 2) != nullptr)
                ? ui->employeeTable->item(selectedRow, 2)->text()
                : QString();

        if (selectedUsername.compare(currentUsername, Qt::CaseInsensitive) == 0) {
            QMessageBox::warning(this, "Khong hop le", "Admin khong duoc xoa chinh minh o che do quan tri.");
            return;
        }

        if (selectedUserId <= 0) {
            QMessageBox::warning(this, "Warning", "Vui long chon nguoi dung de xoa.");
            return;
        }

        const QString targetUsername = ui->employeeTable->item(ui->employeeTable->currentRow(), 2)
                                           ? ui->employeeTable->item(ui->employeeTable->currentRow(), 2)->text()
                                           : QString();

        const QMessageBox::StandardButton reply =
            QMessageBox::question(this,
                                  "Xoa tai khoan nguoi dung",
                                  QString("Xoa tai khoan User ID %1 (%2)?").arg(selectedUserId).arg(targetUsername),
                                  QMessageBox::Yes | QMessageBox::No);

        if (reply != QMessageBox::Yes) {
            return;
        }

        std::string error;
        if (!networkClient->AdminDeleteUser(selectedUserId, error)) {
            QMessageBox::critical(this, "Xoa that bai", QString::fromStdString(error));
            return;
        }

        QMessageBox::information(this, "Thanh cong", "Da xoa tai khoan nguoi dung.");
        loadProfileData();
        updateStatistics();
        return;
    }

    if (isDoctorMode()) {
        QMessageBox::information(this, "Notice", "Doctor interface does not use Delete button.");
        return;
    }

    if (!hasLoadedRecord) {
        QMessageBox::warning(this, "Warning", "No account is loaded.");
        return;
    }

    const QMessageBox::StandardButton reply =
        QMessageBox::question(this, "Delete Account",
                              "Delete this account and its encrypted personal record?",
                              QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) {
        return;
    }

    std::string error;
    if (!networkClient->DeleteAccount(error)) {
        QMessageBox::critical(this, "Delete Failed", QString::fromStdString(error));
        return;
    }

    QMessageBox::information(this, "Account Deleted", "Your account has been deleted.");
    QApplication::quit();
}

void MainWindow::onSearch(const QString& searchText) {
    if (isDoctorCreateTabActive()) {
        return;
    }

    for (int i = 0; i < ui->employeeTable->rowCount(); ++i) {
        bool found = false;
        for (int j = 0; j < ui->employeeTable->columnCount(); ++j) {
            QTableWidgetItem* item = ui->employeeTable->item(i, j);
            if (item && item->text().contains(searchText, Qt::CaseInsensitive)) {
                found = true;
                break;
            }
        }
        ui->employeeTable->setRowHidden(i, !found);
    }
}

void MainWindow::onViewMyInfo() {
    if (!isAdminMode() && !isDoctorMode() && !isUserMode()) {
        return;
    }

    PersonalRecord previewRecord;
    std::string error;
    if (!networkClient->FetchProfile(previewRecord, error)) {
        QMessageBox::critical(this, "Error", QString::fromStdString(error));
        return;
    }

    const QString maskedInfo =
        QString("Username: %1\n"
                "Name: %2\n"
                "Gender: %3\n"
                "CCCD: %4\n"
                "Phone: %5\n"
                "Email: %6")
            .arg(QString::fromStdString(previewRecord.username))
            .arg(QString::fromStdString(previewRecord.name))
            .arg(GenderToLabel(previewRecord.gender))
            .arg(MaskKeepLast(previewRecord.cccd, 4))
            .arg(MaskKeepLast(previewRecord.phone, 3))
            .arg(MaskEmail(previewRecord.email));

    QDialog previewDialog(this);
    previewDialog.setWindowTitle("Thông tin cá nhân");
    previewDialog.setModal(true);
    previewDialog.setMinimumWidth(460);

    QVBoxLayout* rootLayout = new QVBoxLayout(&previewDialog);

    QLabel* maskedInfoLabel = new QLabel(maskedInfo, &previewDialog);
    maskedInfoLabel->setWordWrap(true);
    maskedInfoLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    maskedInfoLabel->setStyleSheet(
        "QLabel { border: 1px solid #cfd8dc; border-radius: 6px; padding: 10px; background-color: #f8fafc; color: #263238; }");
    rootLayout->addWidget(maskedInfoLabel);

    QHBoxLayout* buttonLayout = new QHBoxLayout;
    QPushButton* detailBtn = new QPushButton("Xem chi tiết", &previewDialog);
    QPushButton* closeBtn = new QPushButton("Đóng", &previewDialog);
    detailBtn->setMinimumWidth(150);
    closeBtn->setMinimumWidth(150);
    detailBtn->setStyleSheet(
        "QPushButton { background-color: #00897b; color: white; font-weight: bold; padding: 6px 10px; border-radius: 4px; }"
        "QPushButton:hover { background-color: #00695c; }"
        "QPushButton:pressed { background-color: #004d40; }");
    closeBtn->setStyleSheet(
        "QPushButton { background-color: #546e7a; color: white; font-weight: bold; padding: 6px 10px; border-radius: 4px; }"
        "QPushButton:hover { background-color: #455a64; }"
        "QPushButton:pressed { background-color: #37474f; }");

    buttonLayout->addWidget(detailBtn);
    buttonLayout->addStretch();
    buttonLayout->addWidget(closeBtn);
    rootLayout->addLayout(buttonLayout);

    connect(detailBtn, &QPushButton::clicked, &previewDialog, &QDialog::accept);
    connect(closeBtn, &QPushButton::clicked, &previewDialog, &QDialog::reject);

    if (previewDialog.exec() != QDialog::Accepted) {
        return;
    }

    openDetailedMyInfoFlow();
}

void MainWindow::openDetailedMyInfoFlow() {

    QString password;
    if (!promptPasswordForSensitiveAction("Xem thong tin cua toi", "Nhap lai mat khau de xac thuc:", password)) {
        return;
    }

    PersonalRecord myInfo;
    std::string error;
    if (!networkClient->FetchMyInfoForAdmin(password.toStdString(), myInfo, error)) {
        QMessageBox::critical(this, "Error", QString::fromStdString(error));
        return;
    }

    EmployeeDialog dialog(EmployeeDialog::EditMode, this, false);
    EmployeeDialog::RecordData data;
    data.username = QString::fromStdString(myInfo.username);
    data.name = QString::fromStdString(myInfo.name);
    data.gender = myInfo.gender;
    data.cccd = QString::fromStdString(myInfo.cccd);
    data.phone = QString::fromStdString(myInfo.phone);
    data.email = QString::fromStdString(myInfo.email);
    dialog.setRecordData(data);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const EmployeeDialog::RecordData updated = dialog.getRecordData();

    QString currentPasswordForUpdate = password;
    QString newPasswordForUpdate = password;
    if (updated.changePassword) {
        currentPasswordForUpdate = updated.currentPassword;
        newPasswordForUpdate = updated.newPassword;
    }

    EmployeeDialog::RecordData validated = updated;
    validated.password = newPasswordForUpdate;

    QString validationError;
    if (!ValidateRecordData(validated, validationError)) {
        QMessageBox::warning(this, "Validation Error", validationError);
        return;
    }

    PersonalRecord updatedRecord = myInfo;
    updatedRecord.username = updated.username.toStdString();
    updatedRecord.name = updated.name.toStdString();
    updatedRecord.gender = updated.gender;
    updatedRecord.cccd = updated.cccd.toStdString();
    updatedRecord.phone = updated.phone.toStdString();
    updatedRecord.email = updated.email.toStdString();

    const QMessageBox::StandardButton confirm = QMessageBox::question(
        this,
        "Xac nhan cap nhat",
        "Ban co chac chan muon luu thay doi thong tin ca nhan?",
        QMessageBox::Yes | QMessageBox::No);
    if (confirm != QMessageBox::Yes) {
        return;
    }

    if (!networkClient->UpdateProfile(updatedRecord,
                                      newPasswordForUpdate.toStdString(),
                                      error,
                                      currentPasswordForUpdate.toStdString())) {
        QMessageBox::critical(this, "Update Failed", QString::fromStdString(error));
        return;
    }

    currentUsername = QString::fromStdString(updatedRecord.username);
    QMessageBox::information(this, "Success", "Profile updated successfully.");
    loadProfileData();
}

void MainWindow::onCreateMedicalRecord() {
    if (!isDoctorMode() || !isDoctorCreateTabActive()) {
        return;
    }

    bool patientIdOk = false;
    const int patientId = ui->patientIdInput->text().trimmed().toInt(&patientIdOk);
    if (!patientIdOk || patientId <= 0) {
        QMessageBox::warning(this, "Validation Error", "Patient ID khong hop le.");
        return;
    }

    const QString visitDate = ui->visitDateInput->date().toString("yyyy-MM-dd");
    const QString department = ui->departmentInput->text().trimmed();
    const QString diagnosis = ui->diagnosisInput->toPlainText().trimmed();
    const QString prescription = ui->prescriptionInput->toPlainText().trimmed();

    if (diagnosis.isEmpty() || prescription.isEmpty()) {
        QMessageBox::warning(this, "Validation Error", "Diagnosis va Prescription khong duoc de trong.");
        return;
    }

    std::string error;
    if (!networkClient->CreateMedicalRecord(
            patientId,
            visitDate.toStdString(),
            department.toStdString(),
            diagnosis.toStdString(),
            prescription.toStdString(),
            error)) {
        QMessageBox::critical(this, "Create Failed", QString::fromStdString(error));
        return;
    }

    QMessageBox::information(this, "Success", "Da tao benh an moi.");
    resetDoctorCreateForm();
}

void MainWindow::onLogout() {
    const QMessageBox::StandardButton reply =
        QMessageBox::question(this, "Logout",
                              "Are you sure you want to logout?",
                              QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        if (networkClient && networkClient->IsConnected()) {
            std::string error;
            networkClient->Logout(error);
        }
        QApplication::quit();
    }
}

void MainWindow::onTableRowSelection() {
    const QList<QTableWidgetItem*> selected = ui->employeeTable->selectedItems();
    if (!selected.isEmpty()) {
        const int selectedRow = selected.first()->row();

        QTableWidgetItem* idItem = ui->employeeTable->item(selectedRow, 0);
        if (idItem != nullptr) {
            selectedRecordId = idItem->text().toInt();
        }

        if (isAdminMode()) {
            if (!isAdminUsersTabActive()) {
                selectedUserId = -1;
                selectedUserRole = 1;
                ui->editBtn->setEnabled(false);
                ui->deleteBtn->setEnabled(false);
                return;
            }

            QTableWidgetItem* userIdItem = ui->employeeTable->item(selectedRow, 1);
            QTableWidgetItem* roleItem = ui->employeeTable->item(selectedRow, 3);
            QTableWidgetItem* usernameItem = ui->employeeTable->item(selectedRow, 2);

            selectedUserId = userIdItem != nullptr ? userIdItem->text().toInt() : -1;
            selectedUserRole = roleItem != nullptr ? LabelToRole(roleItem->text()) : 1;
            const QString selectedUsername = usernameItem != nullptr ? usernameItem->text() : QString();

            const bool hasValidSelection =
                selectedUserId > 0 && selectedUsername.compare(currentUsername, Qt::CaseInsensitive) != 0;
            ui->editBtn->setEnabled(hasValidSelection);
            ui->deleteBtn->setEnabled(hasValidSelection);
        } else if (isDoctorMode() || isUserMode()) {
            ui->editBtn->setEnabled(false);
            ui->deleteBtn->setEnabled(false);

            const bool canOpenMedicalDetail =
                selectedRecordId > 0 && ((isDoctorMode() && isDoctorListTabActive()) || isUserMode());

            if (canOpenMedicalDetail) {
                QString password;
                if (!promptPasswordForSensitiveAction("Xem chi tiet benh an", "Nhap mat khau de giai ma benh an:", password)) {
                    return;
                }

                MedicalRecordListItem detail;
                std::string error;
                if (!networkClient->FetchMedicalRecordDetailForDoctor(selectedRecordId, password.toStdString(), detail, error)) {
                    QMessageBox::critical(this, "Error", QString::fromStdString(error));
                    return;
                }

                medical_record_dialog::ShowMedicalRecordDetailDialog(this, detail);
            }
        }
    } else if (isAdminMode()) {
        selectedUserId = -1;
        selectedUserRole = 1;
        ui->editBtn->setEnabled(false);
        ui->deleteBtn->setEnabled(false);
    }
}
