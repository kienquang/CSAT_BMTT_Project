#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "EmployeeDialog.h"
#include "../network/NetworkClient.h"
#include <algorithm>
#include <QMessageBox>
#include <QApplication>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <iostream>

MainWindow::MainWindow(std::shared_ptr<NetworkClient> networkClient, QWidget *parent, const QString& userRole, const QString& userName)
    : QMainWindow(parent),
      ui(new Ui::MainWindow),
      networkClient(std::move(networkClient)),
      currentUserRole(userRole),
      currentUserName(userName),
      selectedEmployeeId(-1)
{
    ui->setupUi(this);
    
    ui->roleDisplay->setText(QString("%1 (%2)").arg(currentUserName, currentUserRole));
    
    // Setup UI connections
    setupConnections();
    
    // Apply styles
    applyStyles();
    
    // Load initial data
    loadEmployeeData();
    updateStatistics();
    
    // Control button visibility based on role
    controlButtonVisibility();
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::setupConnections() {
    // Connect button signals to slots
    connect(ui->addBtn, &QPushButton::clicked, this, &MainWindow::onAddEmployee);
    connect(ui->editBtn, &QPushButton::clicked, this, &MainWindow::onEditEmployee);
    connect(ui->deleteBtn, &QPushButton::clicked, this, &MainWindow::onDeleteEmployee);
    connect(ui->logoutBtn, &QPushButton::clicked, this, &MainWindow::onLogout);
    
    // Connect search box
    connect(ui->searchBox, &QLineEdit::textChanged,
            this, &MainWindow::onSearch);
    
    // Connect table selection
    connect(ui->employeeTable, &QTableWidget::itemSelectionChanged,
            this, &MainWindow::onTableRowSelection);
}

void MainWindow::applyStyles() {
    // Apply stylesheet to main window
    setStyleSheet(
        "QMainWindow { background-color: #ecf0f1; }"
        "QPushButton { padding: 5px 10px; border-radius: 3px; font-weight: bold; }"
        "QPushButton:hover { opacity: 0.8; }"
        "QTableWidget { background-color: white; }"
        "QLineEdit { padding: 5px; border: 1px solid #bdc3c7; border-radius: 3px; }"
        "QComboBox { padding: 5px; border: 1px solid #bdc3c7; border-radius: 3px; }"
    );
}

void MainWindow::controlButtonVisibility() {
    bool isAdmin = (currentUserRole == "Admin");
    
    // Only Admin can add/edit/delete employees
    ui->addBtn->setEnabled(isAdmin);
    ui->editBtn->setEnabled(isAdmin);
    ui->deleteBtn->setEnabled(isAdmin);
    
    // Update title bar with role info
    setWindowTitle(QString("CSAT_BMTT - Role: %1").arg(currentUserRole));
    
    std::cout << "[UI] Role-based controls applied - User Role: " << currentUserRole.toStdString() << std::endl;
}

void MainWindow::loadEmployeeData() {
    if (!networkClient || !networkClient->IsConnected()) {
        QMessageBox::warning(this, "Warning", "Server is not connected!");
        return;
    }
    
    ui->employeeTable->setRowCount(0);
    
    try {
        std::string error;
        currentEmployees.clear();
        if (!networkClient->FetchAllEmployees(currentEmployees, error)) {
            throw std::runtime_error(error);
        }
        
        for (size_t i = 0; i < currentEmployees.size(); ++i) {
            ui->employeeTable->insertRow(i);
            ui->employeeTable->setItem(i, 0, 
                new QTableWidgetItem(QString::number(currentEmployees[i].id)));
            ui->employeeTable->setItem(i, 1,
                new QTableWidgetItem(QString::fromStdString(currentEmployees[i].ten_nv)));
            ui->employeeTable->setItem(i, 2,
                new QTableWidgetItem(QString::fromStdString(currentEmployees[i].vai_tro)));
            ui->employeeTable->setItem(i, 3,
                new QTableWidgetItem(QString::fromStdString(currentEmployees[i].cccd_cipher)));
            ui->employeeTable->setItem(i, 4,
                new QTableWidgetItem(QString::fromStdString(currentEmployees[i].sdt_cipher)));
            ui->employeeTable->setItem(i, 5,
                new QTableWidgetItem(QString::fromStdString(currentEmployees[i].luong_cipher)));
            ui->employeeTable->setItem(i, 6,
                new QTableWidgetItem(QString::fromStdString(currentEmployees[i].matkhau_cipher)));
        }

        ui->employeeTable->horizontalHeader()->stretchLastSection();
        
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Error", 
                              QString("Failed to load employees: %1").arg(e.what()));
        std::cerr << "[ERROR] " << e.what() << std::endl;
    }
}

void MainWindow::updateStatistics() {
    if (!networkClient || !networkClient->IsConnected()) {
        return;
    }
    
    try {
        std::string error;
        int total = 0;
        if (!networkClient->GetTotalEmployees(total, error)) {
            throw std::runtime_error(error);
        }
        ui->totalValue->setText(QString::number(total));
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Failed to update statistics: " << e.what() << std::endl;
    }
}

// ===== SLOT IMPLEMENTATIONS =====

void MainWindow::onAddEmployee() {
    if (!networkClient || !networkClient->IsConnected()) {
        QMessageBox::warning(this, "Warning", "Cannot add employee: Server not connected!");
        return;
    }
    
    // Create and show add employee dialog
    EmployeeDialog dialog(EmployeeDialog::AddMode, this);
    
    if (dialog.exec() == QDialog::Accepted) {
        EmployeeDialog::EmployeeData data = dialog.getEmployeeData();
        
        // Validate input
        if (data.name.isEmpty()) {
            QMessageBox::warning(this, "Validation Error", "Name cannot be empty!");
            return;
        }
        
        if (data.cccd.length() != 12 || !data.cccd.toLongLong(nullptr, 10)) {
            QMessageBox::warning(this, "Validation Error", 
                QString("CCCD must be 12 digits! (Current length: %1)").arg(data.cccd.length()));
            return;
        }
        
        if (data.phone.length() != 10 || !data.phone.toLongLong(nullptr, 10)) {
            QMessageBox::warning(this, "Validation Error", 
                QString("Phone must be 10 digits! (Current length: %1)").arg(data.phone.length()));
            return;
        }
        
        if (data.password.isEmpty()) {
            QMessageBox::warning(this, "Validation Error", "Password cannot be empty!");
            return;
        }
        
        if (data.salary.isEmpty() || data.salary.toDouble() < 0) {
            QMessageBox::warning(this, "Validation Error", "Salary must be a valid positive number!");
            return;
        }
        
        try {
            nhanvien employee;
            employee.id = -1;
            employee.ten_nv = data.name.toStdString();
            employee.vai_tro = data.role.toStdString();
            employee.cccd_cipher = data.cccd.toStdString();
            employee.sdt_cipher = data.phone.toStdString();
            employee.luong_cipher = data.salary.toStdString();

            std::string error;
            if (networkClient->AddEmployee(employee, data.password.toStdString(), error)) {
                QMessageBox::information(this, "Success", "Employee added successfully!");
                loadEmployeeData();
                updateStatistics();
            } else {
                QMessageBox::warning(this, "Error",
                                     QString("Failed to add employee: %1").arg(QString::fromStdString(error)));
            }
        } catch (const std::exception& e) {
            QMessageBox::critical(this, "Error",
                                  QString("Add employee operation failed: %1").arg(e.what()));
        }
    }
}

void MainWindow::onEditEmployee() {
    if (selectedEmployeeId == -1) {
        QMessageBox::warning(this, "Warning", "Please select an employee to edit!");
        return;
    }
    
    if (!networkClient || !networkClient->IsConnected()) {
        QMessageBox::warning(this, "Warning", "Cannot edit employee: Server not connected!");
        return;
    }
    
    try {
        auto it = std::find_if(currentEmployees.begin(), currentEmployees.end(),
                               [this](const nhanvien& employee) { return employee.id == selectedEmployeeId; });
        if (it == currentEmployees.end()) {
            QMessageBox::warning(this, "Error", "Selected employee is not available.");
            return;
        }
        
        QMessageBox::information(this, "Security Notice",
                                 "Sensitive fields are only sent masked from the server.\n"
                                 "Please re-enter CCCD, phone, password, and salary to update this employee.");

        EmployeeDialog dialog(EmployeeDialog::EditMode, this);
        
        EmployeeDialog::EmployeeData currentData;
        currentData.name = QString::fromStdString(it->ten_nv);
        currentData.role = QString::fromStdString(it->vai_tro);
        currentData.cccd = "";
        currentData.phone = "";
        currentData.password = "";
        currentData.salary = "";
        
        dialog.setEmployeeData(currentData);
        
        if (dialog.exec() == QDialog::Accepted) {
            EmployeeDialog::EmployeeData newData = dialog.getEmployeeData();
            
            // Validate input
            if (newData.name.isEmpty()) {
                QMessageBox::warning(this, "Validation Error", "Name cannot be empty!");
                return;
            }
            
            if (newData.cccd.length() != 12 || !newData.cccd.toLongLong(nullptr, 10)) {
                QMessageBox::warning(this, "Validation Error", 
                    QString("CCCD must be 12 digits! (Current length: %1)").arg(newData.cccd.length()));
                return;
            }
            
            if (newData.phone.length() != 10 || !newData.phone.toLongLong(nullptr, 10)) {
                QMessageBox::warning(this, "Validation Error", 
                    QString("Phone must be 10 digits! (Current length: %1)").arg(newData.phone.length()));
                return;
            }
            
            if (newData.password.isEmpty()) {
                QMessageBox::warning(this, "Validation Error", "Password cannot be empty!");
                return;
            }
            
            if (newData.salary.isEmpty() || newData.salary.toDouble() < 0) {
                QMessageBox::warning(this, "Validation Error", "Salary must be a valid positive number!");
                return;
            }
            
            nhanvien employee;
            employee.id = selectedEmployeeId;
            employee.ten_nv = newData.name.toStdString();
            employee.vai_tro = newData.role.toStdString();
            employee.cccd_cipher = newData.cccd.toStdString();
            employee.sdt_cipher = newData.phone.toStdString();
            employee.luong_cipher = newData.salary.toStdString();

            std::string error;
            if (networkClient->UpdateEmployee(employee, newData.password.toStdString(), error)) {
                QMessageBox::information(this, "Success", "Employee updated successfully!");
                loadEmployeeData();
                updateStatistics();
                selectedEmployeeId = -1;
            } else {
                QMessageBox::warning(this, "Error",
                                     QString("Failed to update employee: %1").arg(QString::fromStdString(error)));
            }
        }
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Error",
                              QString("Edit operation failed: %1").arg(e.what()));
    }
}

void MainWindow::onDeleteEmployee() {
    if (selectedEmployeeId == -1) {
        QMessageBox::warning(this, "Warning", "Please select an employee to delete!");
        return;
    }
    
    if (!networkClient || !networkClient->IsConnected()) {
        QMessageBox::warning(this, "Warning", "Cannot delete employee: Server not connected!");
        return;
    }
    
    // Confirm deletion
    QMessageBox::StandardButton reply = QMessageBox::question(this, "Confirm Delete",
                                                               QString("Delete employee with ID %1?").arg(selectedEmployeeId),
                                                               QMessageBox::Yes | QMessageBox::No);
    
    if (reply == QMessageBox::No) {
        return;
    }
    
    try {
        std::string error;
        if (networkClient->DeleteEmployee(selectedEmployeeId, error)) {
            QMessageBox::information(this, "Success", "Employee deleted successfully!");
            loadEmployeeData();
            updateStatistics();
            selectedEmployeeId = -1;
        } else {
            QMessageBox::warning(this, "Error",
                                 QString("Failed to delete employee: %1").arg(QString::fromStdString(error)));
        }
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Error",
                              QString("Delete operation failed: %1").arg(e.what()));
    }
}

void MainWindow::onSearch(const QString& searchText) {
    // Hide/show rows based on search text
    for (int i = 0; i < ui->employeeTable->rowCount(); ++i) {
        bool found = false;
        
        // Search in all columns
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
    QMessageBox::StandardButton reply = QMessageBox::question(this, "Logout",
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
    // Get selected row
    QList<QTableWidgetItem*> selected = ui->employeeTable->selectedItems();
    
    if (!selected.isEmpty()) {
        int row = selected.first()->row();
        QTableWidgetItem* idItem = ui->employeeTable->item(row, 0);
        
        if (idItem) {
            selectedEmployeeId = idItem->text().toInt();
        }
    }
}
