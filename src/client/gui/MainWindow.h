#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QLineEdit>
#include <memory>
#include <vector>
#include "../../core/DatabaseHelper.h"

class NetworkClient;

// Forward declaration for auto-generated UI class
namespace Ui {
    class MainWindow;
}

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(std::shared_ptr<NetworkClient> networkClient,
               QWidget *parent = nullptr,
               const QString& userRole = "User",
               const QString& userName = "");
    ~MainWindow();

private slots:
    // Button slots
    void onAddEmployee();
    void onEditEmployee();
    void onDeleteEmployee();
    void onSearch(const QString& searchText);
    void onLogout();
    void onTableRowSelection();

private:
    void setupConnections();
    void loadEmployeeData();
    void applyStyles();
    void updateStatistics();
    void controlButtonVisibility();

    // UI Components - managed by Ui::MainWindow
    Ui::MainWindow *ui;
    
    // Network client
    std::shared_ptr<NetworkClient> networkClient;
    
    // State management
    QString currentUserRole;
    QString currentUserName;
    int selectedEmployeeId;
    std::vector<nhanvien> currentEmployees;
};

#endif // MAINWINDOW_H
