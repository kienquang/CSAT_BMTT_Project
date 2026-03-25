#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "EmployeeDialog.h"
#include <QMessageBox>
#include <QApplication>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <iostream>

MainWindow::MainWindow(QWidget *parent, const QString& userRole)
    : QMainWindow(parent),
      ui(new Ui::MainWindow),
      currentUserRole(userRole),
      selectedEmployeeId(-1)
{
    ui->setupUi(this);
    
    // Set user role display (read-only)
    ui->roleDisplay->setText(userRole);
    
    // Initialize database
    initializeDatabase();
    
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

void MainWindow::initializeDatabase() {
    // Create DatabaseHelper instance
    // Note: Update credentials as needed from config
    dbHelper = std::make_unique<DatabaseHelper>(
        "localhost",
        3306,
        "root",
        "",  // Change to actual password
        "csatbmtt"
    );
    
    if (!dbHelper->Connect()) {
        QMessageBox::critical(this, "Database Error",
                              "Failed to connect to database.\n"
                              "Please check your database configuration.");
        std::cerr << "[ERROR] Database connection failed!" << std::endl;
    } else {
        // Create tables if they don't exist
        dbHelper->CreateTableNhanVien();
        dbHelper->CreateRoleBasedViews();
        std::cout << "[SUCCESS] Database initialized" << std::endl;
    }
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
    if (!dbHelper || !dbHelper->IsConnected()) {
        QMessageBox::warning(this, "Warning", "Database is not connected!");
        return;
    }
    
    // Clear existing rows
    ui->employeeTable->setRowCount(0);
    
    try {
        // Get employees with role-based masking
        auto employees = dbHelper->GetAllNhanVienWithRole(currentUserRole.toStdString());
        
        // Populate table
        for (size_t i = 0; i < employees.size(); ++i) {
            ui->employeeTable->insertRow(i);
            
            // ID
            ui->employeeTable->setItem(i, 0, 
                new QTableWidgetItem(QString::number(employees[i].id)));
            
            // Name
            ui->employeeTable->setItem(i, 1,
                new QTableWidgetItem(QString::fromStdString(employees[i].ten_nv)));
            
            // Role
            ui->employeeTable->setItem(i, 2,
                new QTableWidgetItem(QString::fromStdString(employees[i].vai_tro)));
            
            // CCCD (with masking applied by DatabaseHelper)
            ui->employeeTable->setItem(i, 3,
                new QTableWidgetItem(QString::fromStdString(employees[i].cccd_cipher)));
            
            // Phone (with masking applied by DatabaseHelper)
            ui->employeeTable->setItem(i, 4,
                new QTableWidgetItem(QString::fromStdString(employees[i].sdt_cipher)));
            
            // Salary (with masking applied by DatabaseHelper)
            ui->employeeTable->setItem(i, 5,
                new QTableWidgetItem(QString::fromStdString(employees[i].luong_cipher)));
            
            // Password (with masking applied by DatabaseHelper)
            ui->employeeTable->setItem(i, 6,
                new QTableWidgetItem(QString::fromStdString(employees[i].matkhau_cipher)));
        }
        
        // Resize columns to content
        ui->employeeTable->horizontalHeader()->stretchLastSection();
        
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Error", 
                              QString("Failed to load employees: %1").arg(e.what()));
        std::cerr << "[ERROR] " << e.what() << std::endl;
    }
}

void MainWindow::updateStatistics() {
    if (!dbHelper || !dbHelper->IsConnected()) {
        return;
    }
    
    try {
        int total = dbHelper->GetTotalNhanVien();
        ui->totalValue->setText(QString::number(total));
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Failed to update statistics: " << e.what() << std::endl;
    }
}

// ===== SLOT IMPLEMENTATIONS =====

void MainWindow::onAddEmployee() {
    if (!dbHelper || !dbHelper->IsConnected()) {
        QMessageBox::warning(this, "Warning", "Cannot add employee: Database not connected!");
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
            // Insert new employee
            if (dbHelper->InsertNhanVien(data.name.toStdString(),
                                        data.role.toStdString(),
                                        data.cccd.toStdString(),
                                        data.phone.toStdString(),
                                        data.password.toStdString(),
                                        data.salary.toStdString())) {
                QMessageBox::information(this, "Success", "Employee added successfully!");
                loadEmployeeData();
                updateStatistics();
            } else {
                QMessageBox::warning(this, "Error", "Failed to add employee. Check if CCCD already exists.");
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
    
    if (!dbHelper || !dbHelper->IsConnected()) {
        QMessageBox::warning(this, "Warning", "Cannot edit employee: Database not connected!");
        return;
    }
    
    try {
        // Get current employee data
        struct nhanvien emp;
        if (!dbHelper->GetNhanVienByIdWithRole(selectedEmployeeId, emp, currentUserRole.toStdString())) {
            QMessageBox::warning(this, "Error", "Failed to load employee data!");
            return;
        }
        
        // Create and show edit employee dialog with pre-filled data
        EmployeeDialog dialog(EmployeeDialog::EditMode, this);
        
        // Pre-fill dialog with current data
        EmployeeDialog::EmployeeData currentData;
        currentData.name = QString::fromStdString(emp.ten_nv);
        currentData.role = QString::fromStdString(emp.vai_tro);
        currentData.cccd = QString::fromStdString(emp.cccd_cipher);  // Note: these are decrypted by GetNhanVienByIdWithRole
        currentData.phone = QString::fromStdString(emp.sdt_cipher);
        currentData.password = QString::fromStdString(emp.matkhau_cipher);
        currentData.salary = QString::fromStdString(emp.luong_cipher);
        
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
            
            // Update employee
            if (dbHelper->UpdateNhanVien(selectedEmployeeId,
                                        newData.name.toStdString(),
                                        newData.role.toStdString(),
                                        newData.cccd.toStdString(),
                                        newData.phone.toStdString(),
                                        newData.password.toStdString(),
                                        newData.salary.toStdString())) {
                QMessageBox::information(this, "Success", "Employee updated successfully!");
                loadEmployeeData();
                updateStatistics();
                selectedEmployeeId = -1;
            } else {
                QMessageBox::warning(this, "Error", "Failed to update employee!");
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
    
    if (!dbHelper || !dbHelper->IsConnected()) {
        QMessageBox::warning(this, "Warning", "Cannot delete employee: Database not connected!");
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
        if (dbHelper->DeleteNhanVienById(selectedEmployeeId)) {
            QMessageBox::information(this, "Success", "Employee deleted successfully!");
            loadEmployeeData();
            updateStatistics();
            selectedEmployeeId = -1;
        } else {
            QMessageBox::warning(this, "Error", "Failed to delete employee!");
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
        // Close application
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
