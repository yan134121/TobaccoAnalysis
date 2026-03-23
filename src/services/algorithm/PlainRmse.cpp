#include "PlainRmse.h"
#include "core/entities/Curve.h"
#include <QtMath>
#include <limits>

PlainRmse::PlainRmse(QObject* parent) : QObject(parent) {}

QString PlainRmse::algorithmId() const
{
    return QStringLiteral("plain_rmse");
}

QString PlainRmse::userVisibleName() const
{
    return tr("均方根误差(RMSE)");
}

double PlainRmse::calculateDifference(const Curve& curveA, const Curve& curveB, const QVariantMap& params)
{
    Q_UNUSED(params);
    const auto& dataA = curveA.data();
    const auto& dataB = curveB.data();

    if (dataA.size() != dataB.size() || dataA.isEmpty()) {
        qWarning("PlainRmse: Input curves must have the same non-empty size.");
        return -1.0;
    }

    double mse = 0.0;
    const int n = dataA.size();
    for (int i = 0; i < n; ++i) {
        const double diff = dataB[i].y() - dataA[i].y();
        mse += diff * diff;
    }
    mse /= static_cast<double>(n);
    return qSqrt(mse);
}
