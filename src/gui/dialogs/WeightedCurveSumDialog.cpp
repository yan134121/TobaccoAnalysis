#include "WeightedCurveSumDialog.h"

#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QVBoxLayout>

bool WeightedCurveSumDialog::pickWeights(QWidget* parent,
                                         const QList<QPair<QString, double>>& items,
                                         QVector<double>& weights)
{
    if (items.size() < 2) {
        return false;
    }

    QDialog dlg(parent);
    dlg.setWindowTitle(QObject::tr("曲线加权加和"));
    dlg.resize(520, 420);

    auto* root = new QVBoxLayout(&dlg);
    root->addWidget(new QLabel(QObject::tr("请为每条曲线设置权重（0~1）。\n系统会在计算时自动归一化。"), &dlg));

    auto* form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);

    QList<QDoubleSpinBox*> editors;
    editors.reserve(items.size());

    auto* sumLabel = new QLabel(QObject::tr("当前权重和：0.0000"), &dlg);

    auto refreshSum = [&]() {
        double s = 0.0;
        for (auto* sp : editors) {
            if (sp) s += sp->value();
        }
        sumLabel->setText(QObject::tr("当前权重和：%1").arg(QString::number(s, 'f', 4)));
    };

    for (const auto& item : items) {
        auto* spin = new QDoubleSpinBox(&dlg);
        spin->setRange(0.0, 1.0);
        spin->setDecimals(4);
        spin->setSingleStep(0.05);
        spin->setValue(item.second);
        editors.push_back(spin);
        QObject::connect(spin, qOverload<double>(&QDoubleSpinBox::valueChanged), &dlg, [refreshSum](double) {
            Q_UNUSED(refreshSum);
        });
        form->addRow(item.first, spin);
    }

    // 修正：为每个spin显式连接刷新逻辑
    for (auto* spin : editors) {
        QObject::connect(spin, qOverload<double>(&QDoubleSpinBox::valueChanged), &dlg, [refreshSum](double){
            refreshSum();
        });
    }

    root->addLayout(form);
    root->addWidget(sumLabel);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    root->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    refreshSum();

    if (dlg.exec() != QDialog::Accepted) {
        return false;
    }

    QVector<double> out;
    out.reserve(editors.size());
    double total = 0.0;
    for (auto* sp : editors) {
        const double v = sp ? sp->value() : 0.0;
        out.push_back(v);
        total += v;
    }

    if (total <= 0.0) {
        QMessageBox::warning(parent, QObject::tr("提示"), QObject::tr("权重和必须大于 0。"));
        return false;
    }

    weights = out;
    return true;
}
