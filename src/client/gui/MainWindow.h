#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <memory>

#include "../../core/DatabaseHelper.h"

class NetworkClient;

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(std::shared_ptr<NetworkClient> networkClient,
               QWidget* parent = nullptr,
               const QString& username = "",
               int role = 1);
    ~MainWindow();

private slots:
    void onRefreshProfile();
    void onEditProfile();
    void onDeleteAccount();
    void onSearch(const QString& searchText);
    void onLogout();
    void onTableRowSelection();

private:
    bool isAdminMode() const;
    void configureUiForRole();
    void loadAdminUserList();
    void loadNonAdminPlaceholder();
    void setupConnections();
    void loadProfileData();
    void applyStyles();
    void updateStatistics();

    Ui::MainWindow* ui;
    std::shared_ptr<NetworkClient> networkClient;

    QString currentUsername;
    int currentRole;
    int selectedRecordId;
    PersonalRecord currentRecord;
    bool hasLoadedRecord;
};

#endif  // MAINWINDOW_H
