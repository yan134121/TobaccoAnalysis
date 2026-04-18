#include "SampleAttributeDialog.h"

#include <QComboBox>
#include <QDate>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QVBoxLayout>

SampleAttributeDialog::SampleAttributeDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUi();
    setWindowTitle(tr("导入样本属性"));
    resize(520, 320);
}

void SampleAttributeDialog::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    auto* formLayout = new QFormLayout();

    m_codeEdit = new QLineEdit(this);
    m_yearCombo = new QComboBox(this);
    m_originEdit = new QLineEdit(this);
    m_partCombo = new QComboBox(this);
    m_gradeCombo = new QComboBox(this);
    m_stemLeafCombo = new QComboBox(this);
    m_detectDateEdit = new QLineEdit(this);
    m_factoryCombo = new QComboBox(this);

    populateYearOptions();
    m_partCombo->addItems({QStringLiteral("上部"), QStringLiteral("中部"), QStringLiteral("下部")});
    m_gradeCombo->addItems({QStringLiteral("高"), QStringLiteral("中"), QStringLiteral("低")});
    m_stemLeafCombo->addItems({QStringLiteral("单打"), QStringLiteral("混打")});
    m_factoryCombo->addItems({QStringLiteral("广州"), QStringLiteral("韶关"), QStringLiteral("梅州"), QStringLiteral("湛江")});

    m_detectDateEdit->setPlaceholderText(tr("YYYY-MM-DD"));

    formLayout->addRow(tr("编码"), m_codeEdit);
    formLayout->addRow(tr("年份"), m_yearCombo);
    formLayout->addRow(tr("产地"), m_originEdit);
    formLayout->addRow(tr("部位"), m_partCombo);
    formLayout->addRow(tr("等级"), m_gradeCombo);
    formLayout->addRow(tr("叶梗分离方式"), m_stemLeafCombo);
    formLayout->addRow(tr("检测日期"), m_detectDateEdit);
    formLayout->addRow(tr("分厂"), m_factoryCombo);

    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    mainLayout->addLayout(formLayout);
    mainLayout->addWidget(m_buttonBox);
}

void SampleAttributeDialog::populateYearOptions()
{
    const int currentYear = QDate::currentDate().year();
    for (int y = currentYear; y >= currentYear - 19; --y) {
        m_yearCombo->addItem(QString::number(y), y);
    }
}

QJsonObject SampleAttributeDialog::defaultAttributes()
{
    QJsonObject obj;
    obj["code"] = QString();
    obj["year"] = QDate::currentDate().year();
    obj["origin"] = QString();
    obj["part"] = QStringLiteral("上部");
    obj["grade"] = QStringLiteral("高");
    obj["stemLeafMode"] = QStringLiteral("单打");
    obj["detectDate"] = QDate::currentDate().toString(Qt::ISODate);
    obj["factory"] = QStringLiteral("广州");
    return obj;
}

void SampleAttributeDialog::setAttributes(const QJsonObject& attributes)
{
    const QJsonObject obj = attributes.isEmpty() ? defaultAttributes() : attributes;
    m_codeEdit->setText(obj.value("code").toString());
    m_yearCombo->setCurrentText(obj.value("year").toVariant().toString());
    m_originEdit->setText(obj.value("origin").toString());
    {
        const QString part = obj.value("part").toString();
        const int partIdx = m_partCombo->findText(part);
        m_partCombo->setCurrentIndex(partIdx >= 0 ? partIdx : 0);
    }
    {
        const QString grade = obj.value("grade").toString();
        const int gradeIdx = m_gradeCombo->findText(grade);
        m_gradeCombo->setCurrentIndex(gradeIdx >= 0 ? gradeIdx : 0);
    }
    m_stemLeafCombo->setCurrentText(obj.value("stemLeafMode").toString(QStringLiteral("单打")));
    m_detectDateEdit->setText(obj.value("detectDate").toString(QDate::currentDate().toString(Qt::ISODate)));
    m_factoryCombo->setCurrentText(obj.value("factory").toString(QStringLiteral("广州")));
}

QJsonObject SampleAttributeDialog::attributes() const
{
    QJsonObject obj;
    obj["code"] = m_codeEdit->text().trimmed();
    obj["year"] = m_yearCombo->currentText().toInt();
    obj["origin"] = m_originEdit->text().trimmed();
    obj["part"] = m_partCombo->currentText();
    obj["grade"] = m_gradeCombo->currentText();
    obj["stemLeafMode"] = m_stemLeafCombo->currentText();
    obj["detectDate"] = m_detectDateEdit->text().trimmed();
    obj["factory"] = m_factoryCombo->currentText();
    return obj;
}

void SampleAttributeDialog::setReadOnly(bool readOnly)
{
    m_codeEdit->setReadOnly(readOnly);
    m_yearCombo->setEnabled(!readOnly);
    m_originEdit->setReadOnly(readOnly);
    m_partCombo->setEnabled(!readOnly);
    m_gradeCombo->setEnabled(!readOnly);
    m_stemLeafCombo->setEnabled(!readOnly);
    m_detectDateEdit->setReadOnly(readOnly);
    m_factoryCombo->setEnabled(!readOnly);
    if (readOnly && m_buttonBox) {
        m_buttonBox->setStandardButtons(QDialogButtonBox::Close);
    }
}
