#include "NavigatorAttributeFilterDialog.h"

#include <QDate>
#include <QJsonObject>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

namespace {

void fillYearCombo(QComboBox* combo)
{
    combo->addItem(QString(), QVariant());
    const int currentYear = QDate::currentDate().year();
    for (int y = currentYear; y >= currentYear - 19; --y) {
        combo->addItem(QString::number(y), y);
    }
}

} // namespace

NavigatorAttributeFilterDialog::NavigatorAttributeFilterDialog(const QString& dataTypeTitle, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("属性分类 — %1").arg(dataTypeTitle));
    setupUi();
}

void NavigatorAttributeFilterDialog::setupUi()
{
    auto* main = new QVBoxLayout(this);
    main->addWidget(new QLabel(tr("仅填写需要作为条件的属性，留空的项不参与筛选。")));

    auto* form = new QFormLayout();
    m_yearCombo = new QComboBox(this);
    fillYearCombo(m_yearCombo);
    m_originEdit = new QLineEdit(this);
    m_partCombo = new QComboBox(this);
    m_partCombo->addItem(QString());
    m_partCombo->addItems({QStringLiteral("上部"), QStringLiteral("中部"), QStringLiteral("下部")});
    m_gradeCombo = new QComboBox(this);
    m_gradeCombo->addItem(QString());
    m_gradeCombo->addItems({QStringLiteral("高"), QStringLiteral("中"), QStringLiteral("低")});
    m_typeEdit = new QLineEdit(this);
    m_projectEdit = new QLineEdit(this);
    m_batchCodeEdit = new QLineEdit(this);

    form->addRow(tr("年份"), m_yearCombo);
    form->addRow(tr("产地"), m_originEdit);
    form->addRow(tr("部位"), m_partCombo);
    form->addRow(tr("等级"), m_gradeCombo);
    form->addRow(tr("类型"), m_typeEdit);
    form->addRow(tr("项目名"), m_projectEdit);
    form->addRow(tr("批次号"), m_batchCodeEdit);

    main->addLayout(form);

    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    main->addWidget(m_buttonBox);
}

void NavigatorAttributeFilterDialog::setFilter(const QJsonObject& filter)
{
    const QString year = filter.value(QStringLiteral("year")).toString().trimmed();
    if (year.isEmpty()) {
        m_yearCombo->setCurrentIndex(0);
    } else {
        const int idx = m_yearCombo->findText(year);
        m_yearCombo->setCurrentIndex(idx >= 0 ? idx : 0);
    }

    m_originEdit->setText(filter.value(QStringLiteral("origin")).toString());
    {
        const QString part = filter.value(QStringLiteral("part")).toString().trimmed();
        const int pi = m_partCombo->findText(part);
        m_partCombo->setCurrentIndex(pi >= 0 ? pi : 0);
    }
    {
        const QString grade = filter.value(QStringLiteral("grade")).toString().trimmed();
        const int gi = m_gradeCombo->findText(grade);
        m_gradeCombo->setCurrentIndex(gi >= 0 ? gi : 0);
    }
    m_typeEdit->setText(filter.value(QStringLiteral("type")).toString());
    m_projectEdit->setText(filter.value(QStringLiteral("project_name")).toString());
    m_batchCodeEdit->setText(filter.value(QStringLiteral("batch_code")).toString());
}

QJsonObject NavigatorAttributeFilterDialog::filter() const
{
    QJsonObject o;

    if (m_yearCombo->currentIndex() > 0) {
        const QString y = m_yearCombo->currentText().trimmed();
        if (!y.isEmpty()) {
            o.insert(QStringLiteral("year"), y);
        }
    }

    auto ins = [&](const QString& key, const QString& value) {
        const QString t = value.trimmed();
        if (!t.isEmpty()) {
            o.insert(key, t);
        }
    };

    ins(QStringLiteral("origin"), m_originEdit->text());
    if (m_partCombo->currentIndex() > 0) {
        ins(QStringLiteral("part"), m_partCombo->currentText());
    }
    if (m_gradeCombo->currentIndex() > 0) {
        ins(QStringLiteral("grade"), m_gradeCombo->currentText());
    }
    ins(QStringLiteral("type"), m_typeEdit->text());
    ins(QStringLiteral("project_name"), m_projectEdit->text());
    ins(QStringLiteral("batch_code"), m_batchCodeEdit->text());

    return o;
}
