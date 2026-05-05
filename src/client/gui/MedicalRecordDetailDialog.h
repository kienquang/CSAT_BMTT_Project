#ifndef MEDICAL_RECORD_DETAIL_DIALOG_H
#define MEDICAL_RECORD_DETAIL_DIALOG_H

#include <QWidget>

#include "../network/NetworkClient.h"

namespace medical_record_dialog {

void ShowMedicalRecordDetailDialog(QWidget* parent, const MedicalRecordListItem& detail);

}  // namespace medical_record_dialog

#endif  // MEDICAL_RECORD_DETAIL_DIALOG_H
