#include "TwoCurvePickDialog.h"
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QMessageBox>
#include <QVBoxLayout>

bool TwoCurvePickDialog::pickTwo(QWidget* parent, const QList<QPair<int, QString>>& items, int& id1, int& id2)
{
    if (items.size() < 2)
        return false;

    QDialog dlg(parent);
    dlg.setWindowTitle(QObject::tr("选择两条曲线"));
    auto* vbox = new QVBoxLayout(&dlg);
    vbox->addWidget(new QLabel(QObject::tr("请选择要叠加显示的两条曲线（将显示原曲线及纵坐标加和）:")));

    auto* combo1 = new QComboBox(&dlg);
    auto* combo2 = new QComboBox(&dlg);
    for (const auto& it : items) {
        combo1->addItem(it.second, it.first);
        combo2->addItem(it.second, it.first);
    }
    if (combo2->count() > 1)
        combo2->setCurrentIndex(1);

    auto* form = new QFormLayout();
    form->addRow(QObject::tr("曲线 1:"), combo1);
    form->addRow(QObject::tr("曲线 2:"), combo2);
    vbox->addLayout(form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    vbox->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted)
        return false;

    id1 = combo1->currentData().toInt();
    id2 = combo2->currentData().toInt();
    if (id1 == id2) {
        QMessageBox::warning(parent, QObject::tr("提示"), QObject::tr("请选择两条不同的曲线。"));
        return false;
    }
    return true;
}
