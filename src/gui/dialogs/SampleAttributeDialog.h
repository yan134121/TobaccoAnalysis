#ifndef SAMPLEATTRIBUTEDIALOG_H
#define SAMPLEATTRIBUTEDIALOG_H

#include <QDialog>
#include <QJsonObject>
#include <QList>

class QLineEdit;
class QComboBox;
class QDateEdit;
class QDialogButtonBox;

class SampleAttributeDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SampleAttributeDialog(QWidget* parent = nullptr);

    void setAttributes(const QJsonObject& attributes);
    QJsonObject attributes() const;
    void setReadOnly(bool readOnly);

    static QJsonObject defaultAttributes();

private:
    void setupUi();
    void populateYearOptions();
    void updateDateEditFromText(QLineEdit* edit, QDateEdit* dateEdit) const;

    QLineEdit* m_codeEdit = nullptr;
    QComboBox* m_yearCombo = nullptr;
    QLineEdit* m_originEdit = nullptr;
    QComboBox* m_partCombo = nullptr;
    QComboBox* m_gradeCombo = nullptr;
    QComboBox* m_stemLeafCombo = nullptr;
    QLineEdit* m_detectDateEdit = nullptr;
    QComboBox* m_factoryCombo = nullptr;
    QDialogButtonBox* m_buttonBox = nullptr;
};

#endif // SAMPLEATTRIBUTEDIALOG_H
