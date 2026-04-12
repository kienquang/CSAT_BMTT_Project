#include "MedicalRecordDetailDialog.h"

#include <QDialog>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace medical_record_dialog {

// [GROUP: Medical Detail Popup]
void ShowMedicalRecordDetailDialog(QWidget* parent, const MedicalRecordListItem& detail) {
    QDialog dialog(parent);
    dialog.setWindowTitle("Chi tiet benh an");
    dialog.setModal(true);
    dialog.resize(980, 700);
    dialog.setMinimumSize(860, 620);
    dialog.setStyleSheet(
        "QDialog { background-color: #f4f7fb; }"
        "QLabel#TitleLabel { color: #0f172a; font-size: 22px; font-weight: 700; }"
        "QFrame#MetaCard { background: white; border: 1px solid #dbe4f0; border-radius: 12px; }"
        "QFrame#ContentCard { background: white; border: 1px solid #dbe4f0; border-radius: 12px; }"
        "QLabel.MetaKey { color: #64748b; font-size: 12px; font-weight: 600; text-transform: uppercase; }"
        "QLabel.MetaValue { color: #0f172a; font-size: 14px; font-weight: 600; }"
        "QLabel.ContentTitle { color: #0f172a; font-size: 15px; font-weight: 700; }"
        "QPlainTextEdit { border: 1px solid #cbd5e1; border-radius: 8px; background: #f8fafc; padding: 8px; color: #0f172a; }"
        "QPushButton#CloseButton { background-color: #0f766e; color: white; border-radius: 8px; padding: 8px 20px; font-weight: 700; }"
        "QPushButton#CloseButton:hover { background-color: #0d5f59; }"
        "QPushButton#CloseButton:pressed { background-color: #0a4e49; }");

    QVBoxLayout* rootLayout = new QVBoxLayout(&dialog);
    rootLayout->setContentsMargins(20, 20, 20, 20);
    rootLayout->setSpacing(14);

    QLabel* titleLabel = new QLabel("Medical Record Detail", &dialog);
    titleLabel->setObjectName("TitleLabel");
    rootLayout->addWidget(titleLabel);

    QFrame* metaCard = new QFrame(&dialog);
    metaCard->setObjectName("MetaCard");
    QGridLayout* metaGrid = new QGridLayout(metaCard);
    metaGrid->setContentsMargins(16, 14, 16, 14);
    metaGrid->setHorizontalSpacing(24);
    metaGrid->setVerticalSpacing(10);

    auto addMeta = [&](int row, int col, const QString& key, const QString& value) {
        QLabel* keyLabel = new QLabel(key, metaCard);
        keyLabel->setProperty("class", "MetaKey");
        keyLabel->setStyleSheet("color: #64748b; font-size: 12px; font-weight: 600;");

        QLabel* valueLabel = new QLabel(value, metaCard);
        valueLabel->setProperty("class", "MetaValue");
        valueLabel->setStyleSheet("color: #0f172a; font-size: 14px; font-weight: 600;");
        valueLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        valueLabel->setWordWrap(true);

        metaGrid->addWidget(keyLabel, row, col);
        metaGrid->addWidget(valueLabel, row, col + 1);
    };

    addMeta(0, 0, "Medical ID", QString::number(detail.recordId));
    addMeta(0, 2, "Visit Date", QString::fromStdString(detail.visitDate));
    addMeta(1, 0, "Patient ID", QString::number(detail.patientId));
    addMeta(1, 2, "Patient Name", QString::fromStdString(detail.patientName));
    addMeta(2, 0, "Doctor ID", QString::number(detail.doctorId));
    addMeta(2, 2, "Doctor Name", QString::fromStdString(detail.doctorName));
    addMeta(3, 0, "Department", QString::fromStdString(detail.department));

    rootLayout->addWidget(metaCard);

    QHBoxLayout* contentLayout = new QHBoxLayout;
    contentLayout->setSpacing(12);

    QFrame* diagnosisCard = new QFrame(&dialog);
    diagnosisCard->setObjectName("ContentCard");
    QVBoxLayout* diagnosisLayout = new QVBoxLayout(diagnosisCard);
    diagnosisLayout->setContentsMargins(14, 12, 14, 14);
    diagnosisLayout->setSpacing(8);
    QLabel* diagnosisTitle = new QLabel("Diagnosis", diagnosisCard);
    diagnosisTitle->setProperty("class", "ContentTitle");
    QPlainTextEdit* diagnosisText = new QPlainTextEdit(diagnosisCard);
    diagnosisText->setReadOnly(true);
    diagnosisText->setPlainText(QString::fromStdString(detail.diagnosis));
    diagnosisLayout->addWidget(diagnosisTitle);
    diagnosisLayout->addWidget(diagnosisText);

    QFrame* prescriptionCard = new QFrame(&dialog);
    prescriptionCard->setObjectName("ContentCard");
    QVBoxLayout* prescriptionLayout = new QVBoxLayout(prescriptionCard);
    prescriptionLayout->setContentsMargins(14, 12, 14, 14);
    prescriptionLayout->setSpacing(8);
    QLabel* prescriptionTitle = new QLabel("Prescription", prescriptionCard);
    prescriptionTitle->setProperty("class", "ContentTitle");
    QPlainTextEdit* prescriptionText = new QPlainTextEdit(prescriptionCard);
    prescriptionText->setReadOnly(true);
    prescriptionText->setPlainText(QString::fromStdString(detail.prescription));
    prescriptionLayout->addWidget(prescriptionTitle);
    prescriptionLayout->addWidget(prescriptionText);

    contentLayout->addWidget(diagnosisCard, 1);
    contentLayout->addWidget(prescriptionCard, 1);
    rootLayout->addLayout(contentLayout, 1);

    QHBoxLayout* actionsLayout = new QHBoxLayout;
    actionsLayout->addStretch();
    QPushButton* closeButton = new QPushButton("Dong", &dialog);
    closeButton->setObjectName("CloseButton");
    closeButton->setMinimumWidth(140);
    QObject::connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    actionsLayout->addWidget(closeButton);
    rootLayout->addLayout(actionsLayout);

    dialog.exec();
}

}  // namespace medical_record_dialog
