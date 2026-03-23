#ifndef PLAINRMSE_H
#define PLAINRMSE_H

#include "services/algorithm/comparison/IDifferenceStrategy.h"
#include <QObject>

// 未除以参考曲线 Y 范围的均方根误差：RMSE = sqrt(mean((y_cmp - y_ref)^2))
class PlainRmse : public QObject, public IDifferenceStrategy
{
    Q_OBJECT
public:
    explicit PlainRmse(QObject* parent = nullptr);

    QString algorithmId() const override;
    QString userVisibleName() const override;
    double calculateDifference(const Curve& curveA, const Curve& curveB, const QVariantMap& params) override;
};

#endif // PLAINRMSE_H
