#ifndef NRMSE_H
#define NRMSE_H

#include "services/algorithm/comparison/IDifferenceStrategy.h"
#include <QObject> // 包含 QObject 以便使用 tr()

class Nrmse : public QObject, public IDifferenceStrategy
{
    Q_OBJECT // QObject 子类需要这个宏
public:
    explicit Nrmse(QObject *parent = nullptr);

    // --- 实现 IDifferenceStrategy 接口 ---
    QString algorithmId() const override;
    QString userVisibleName() const override;
    double calculateDifference(const Curve& curveA, const Curve& curveB, const QVariantMap& params) override;
};

#endif // NRMSE_H