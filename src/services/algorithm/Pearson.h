#ifndef PEARSON_H
#define PEARSON_H

#include "services/algorithm/comparison/IDifferenceStrategy.h"
#include <QObject>

class Pearson : public QObject, public IDifferenceStrategy
{
    Q_OBJECT
public:
    explicit Pearson(QObject *parent = nullptr);

    QString algorithmId() const override;
    QString userVisibleName() const override;
    double calculateDifference(const Curve& curveA, const Curve& curveB, const QVariantMap& params) override;
};

#endif // PEARSON_H