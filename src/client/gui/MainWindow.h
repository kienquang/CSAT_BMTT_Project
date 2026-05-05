#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <memory>
#include <vector>

#include "../../core/DatabaseHelper.h"

class NetworkClient;
struct MedicalRecordListItem;

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
    void onViewMyInfo();
    void onAdminTabChanged(int index);
    void onCreateMedicalRecord();
    void onSearch(const QString& searchText);
    void onLogout();
    void onTableRowSelection();

private:
    bool isAdminMode() const;
    bool isDoctorMode() const;
    bool isUserMode() const;
    bool isAdminUsersTabActive() const;
    bool isDoctorListTabActive() const;
    bool isDoctorCreateTabActive() const;
    void configureUiForRole();
    void loadAdminUserList();
    void loadAdminMedicalRecordList();
    void loadDoctorMedicalRecordList();
    void loadUserMedicalRecordList();
    void populateMedicalRecordTable(const std::vector<MedicalRecordListItem>& records);
    void loadNonAdminPlaceholder();
    void resetDoctorCreateForm();
    void openDetailedMyInfoFlow();
    bool promptPasswordForSensitiveAction(const QString& title,
                                          const QString& prompt,
                                          QString& passwordOut) const;
    void setupConnections();
    void loadProfileData();
    void applyStyles();
    void updateStatistics();

    Ui::MainWindow* ui;
    std::shared_ptr<NetworkClient> networkClient;

    QString currentUsername;
    int currentRole;
    int selectedRecordId;
    int selectedUserId;
    int selectedUserRole;
    PersonalRecord currentRecord;
    bool hasLoadedRecord;
};

#endif  // MAINWINDOW_H
