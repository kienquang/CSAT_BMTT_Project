#include <QApplication>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QStringList>

#include <iostream>

#include "gui/LoginDialog.h"
#include "gui/MainWindow.h"

namespace {

QString ResolveVietnameseFontFamily() {
    const QStringList fontFileCandidates{
        ":/fonts/NotoSans-Regular.ttf",
        ":/fonts/DejaVuSans.ttf",
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/tahoma.ttf"
    };

    for (const QString& fontPath : fontFileCandidates) {
        if (!fontPath.startsWith(":/") && !QFileInfo::exists(fontPath)) {
            continue;
        }

        const int fontId = QFontDatabase::addApplicationFont(fontPath);
        if (fontId < 0) {
            continue;
        }

        const QStringList families = QFontDatabase::applicationFontFamilies(fontId);
        if (!families.isEmpty()) {
            return families.first();
        }
    }

    QFontDatabase fontDb;
    const QStringList fallbackFamilies{
        "Segoe UI",
        "Tahoma",
        "Arial",
        "Noto Sans",
        "DejaVu Sans",
        "Microsoft Sans Serif"
    };

    const QStringList availableFamilies = fontDb.families();
    for (const QString& candidate : fallbackFamilies) {
        for (const QString& installed : availableFamilies) {
            if (installed.compare(candidate, Qt::CaseInsensitive) == 0) {
                return installed;
            }
        }
    }

    return QString();
}

void ConfigureUiFont() {
    const QString preferredFamily = ResolveVietnameseFontFamily();
    if (preferredFamily.isEmpty()) {
        return;
    }

    QFont appFont = QApplication::font();
    appFont.setFamilies(QStringList{preferredFamily, "Segoe UI", "Tahoma", "Arial"});
    QApplication::setFont(appFont);

    QFont::insertSubstitution("MS Shell Dlg 2", preferredFamily);
    QFont::insertSubstitution("Sans Serif", preferredFamily);
}

}  // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    ConfigureUiFont();

    std::cout << "========================================" << std::endl;
    std::cout << "  CSAT_BMTT - Personal Record Vault" << std::endl;
    std::cout << "  Version: 2.0 (Qt6.x)" << std::endl;
    std::cout << "  Argon2id + Password-Derived KEK" << std::endl;
    std::cout << "========================================" << std::endl;

    LoginDialog loginDialog;
    if (loginDialog.exec() != QDialog::Accepted) {
        std::cout << "[INFO] User cancelled login" << std::endl;
        return 0;
    }

    LoginDialog::LoginResult loginResult = loginDialog.getLoginResult();
    if (!loginResult.success) {
        std::cout << "[ERROR] Login validation failed" << std::endl;
        return 1;
    }

    MainWindow window(loginResult.networkClient, nullptr, loginResult.username, loginResult.role);
    window.show();

    std::cout << "[INFO] Application started for user "
              << loginResult.username.toStdString() << std::endl;

    return app.exec();
}
