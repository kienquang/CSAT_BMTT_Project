#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QLineEdit>
#include <memory>
#include "../core/DatabaseHelper.h"

// Forward declaration for auto-generated UI class
namespace Ui {
    class MainWindow;
}

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr, const QString& userRole = "User");
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
    void initializeDatabase();
    void loadEmployeeData();
    void applyStyles();
    void updateStatistics();
    void controlButtonVisibility();

    // UI Components - managed by Ui::MainWindow
    Ui::MainWindow *ui;
    
    // Database helper
    std::unique_ptr<DatabaseHelper> dbHelper;
    
    // State management
    QString currentUserRole;
    int selectedEmployeeId;
};

#endif // MAINWINDOW_H
