#include "MainWindow.h"

#include "EmployeeDialog.h"
#include "ui_MainWindow.h"
#include "../network/NetworkClient.h"

#include <QApplication>
#include <QHeaderView>
#include <QMessageBox>
#include <QTableWidgetItem>

#include <iostream>
#include <stdexcept>

namespace {

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

QString GenderToLabel(int gender) {
    return gender == 2 ? "Female" : "Male";
}

QString RoleToLabel(int role) {
    return role == 2 ? "Admin" : "User";
}

bool IsAdminRole(int role) {
    return role == 2;
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
    connect(ui->logoutBtn, &QPushButton::clicked, this, &MainWindow::onLogout);
    connect(ui->searchBox, &QLineEdit::textChanged, this, &MainWindow::onSearch);
    connect(ui->employeeTable, &QTableWidget::itemSelectionChanged, this, &MainWindow::onTableRowSelection);
}

bool MainWindow::isAdminMode() const {
    return IsAdminRole(currentRole);
}

void MainWindow::configureUiForRole() {
    if (isAdminMode()) {
        ui->titleLabel->setText("Admin - Encrypted User Directory");
        ui->addBtn->setText("Refresh Users");
        ui->editBtn->setEnabled(false);
        ui->deleteBtn->setEnabled(false);
        ui->searchBox->setEnabled(true);
        ui->searchBox->setPlaceholderText("Search encrypted user list...");
        return;
    }

    ui->titleLabel->setText("User Dashboard (Coming Soon)");
    ui->addBtn->setEnabled(false);
    ui->editBtn->setEnabled(false);
    ui->deleteBtn->setEnabled(false);
    ui->searchBox->setEnabled(false);
    ui->searchBox->setPlaceholderText("This interface will be available soon.");
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
    }
}

void MainWindow::loadNonAdminPlaceholder() {
    ui->employeeTable->setRowCount(0);
    ui->employeeTable->setColumnCount(1);
    ui->employeeTable->setHorizontalHeaderLabels(QStringList{"Notice"});
    ui->employeeTable->insertRow(0);
    ui->employeeTable->setItem(0, 0, new QTableWidgetItem("Non-admin interface is temporarily empty."));
    hasLoadedRecord = false;
    selectedRecordId = -1;
}

void MainWindow::applyStyles() {
    setStyleSheet(
        "QMainWindow { background-color: #ecf0f1; }"
        "QPushButton { padding: 5px 10px; border-radius: 3px; font-weight: bold; }"
        "QPushButton:hover { opacity: 0.8; }"
        "QTableWidget { background-color: white; }"
        "QLineEdit { padding: 5px; border: 1px solid #bdc3c7; border-radius: 3px; }"
    );

    setWindowTitle("CSAT_BMTT - Personal Record Vault");
}

void MainWindow::loadProfileData() {
    if (!networkClient || !networkClient->IsConnected()) {
        QMessageBox::warning(this, "Warning", "Server is not connected.");
        return;
    }

    ui->employeeTable->setRowCount(0);
    hasLoadedRecord = false;
    selectedRecordId = -1;

    try {
        if (isAdminMode()) {
            loadAdminUserList();
        } else {
            loadNonAdminPlaceholder();
        }

        ui->employeeTable->horizontalHeader()->stretchLastSection();
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Error",
                              QString("Failed to load data: %1").arg(e.what()));
        std::cerr << "[ERROR] " << e.what() << std::endl;
    }
}

void MainWindow::updateStatistics() {
    if (!isAdminMode()) {
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
        QMessageBox::information(this, "Read-only", "Admin view currently supports encrypted user listing only.");
        return;
    }

    if (!hasLoadedRecord || selectedRecordId == -1) {
        QMessageBox::warning(this, "Warning", "No profile is loaded.");
        return;
    }

    EmployeeDialog dialog(EmployeeDialog::EditMode, this);
    EmployeeDialog::RecordData data;
    data.username = QString::fromStdString(currentRecord.username);
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
    record.gender = updated.gender;
    record.cccd = updated.cccd.toStdString();
    record.phone = updated.phone.toStdString();
    record.email = updated.email.toStdString();

    std::string error;
    if (!networkClient->UpdateProfile(record, updated.password.toStdString(), error)) {
        QMessageBox::critical(this, "Update Failed", QString::fromStdString(error));
        return;
    }

    QMessageBox::information(this, "Success", "Profile updated successfully.");
    loadProfileData();
}

void MainWindow::onDeleteAccount() {
    if (isAdminMode()) {
        QMessageBox::information(this, "Read-only", "Admin view currently supports encrypted user listing only.");
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
        QTableWidgetItem* idItem = ui->employeeTable->item(selected.first()->row(), 0);
        if (idItem) {
            selectedRecordId = idItem->text().toInt();
        }
    }
}
