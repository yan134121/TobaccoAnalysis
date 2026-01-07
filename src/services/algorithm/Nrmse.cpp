#include "Nrmse.h"
#include "core/entities/Curve.h"
#include <QtMath> // 用于 qSqrt
#include <limits> // 用于 std::numeric_limits

Nrmse::Nrmse(QObject *parent) : QObject(parent) {}

QString Nrmse::algorithmId() const
{
    return "nrmse";
}

QString Nrmse::userVisibleName() const
{
    // return tr("标准化均方根误差 (NRMSE)");
    return tr("归一化均方根");
}

// double Nrmse::calculateDifference(const Curve &curveA, const Curve &curveB, const QVariantMap &params)
// {
//     // 【注意】这是一个简化的实现，它假设两条曲线的X轴已经对齐且数据点数量相同。
//     // 在一个生产级的实现中，这里必须先进行“插值(interpolation)”预处理。
//     const auto& dataA = curveA.data();
//     const auto& dataB = curveB.data();

//     if (dataA.size() != dataB.size() || dataA.isEmpty()) {
//         qWarning("NRMSE : Input curves must have the same non-empty size. Interpolation is required.");
//         return -1.0; // 返回一个表示错误的无效值
//     }

//     double mse = 0.0;
//     double maxVal = -std::numeric_limits<double>::infinity();
//     double minVal = std::numeric_limits<double>::infinity();

//     for (int i = 0; i < dataA.size(); ++i) {
//         double diff = dataA[i].y() - dataB[i].y();
//         mse += diff * diff;

//         // 在两条曲线的所有Y值中找到最大和最小值，用于归一化
//         maxVal = std::max({maxVal, dataA[i].y(), dataB[i].y()});
//         minVal = std::min({minVal, dataA[i].y(), dataB[i].y()});
//     }
    
//     mse /= dataA.size();
//     double rmse = qSqrt(mse);
//     double range = maxVal - minVal;

//     // 避免除以零
//     if (qAbs(range) < std::numeric_limits<double>::epsilon()) {
//         return 0.0;
//     }

//     return rmse / range;
// }


double Nrmse::calculateDifference(const Curve &curveA, const Curve &curveB, const QVariantMap &params)
{
    const auto& dataA = curveA.data(); // 参考曲线
    const auto& dataB = curveB.data();

    if (dataA.size() != dataB.size() || dataA.isEmpty()) {
        qWarning("NRMSE : Input curves must have the same non-empty size.");
        return -1.0;
    }

    double mse = 0.0;
    double maxRef = -std::numeric_limits<double>::infinity();
    double minRef = std::numeric_limits<double>::infinity();

    for (int i = 0; i < dataA.size(); ++i) {
        double diff = dataB[i].y() - dataA[i].y(); // 样本 - 参考
        mse += diff * diff;

        // 只使用参考曲线求范围
        maxRef = std::max(maxRef, dataA[i].y());
        minRef = std::min(minRef, dataA[i].y());
    }

    mse /= dataA.size();
    double rmse = qSqrt(mse);
    double range = maxRef - minRef;

    if (qAbs(range) < std::numeric_limits<double>::epsilon()) {
        return 0.0;
    }

    // return (rmse / range) * 100.0; // 返回百分比
    return (rmse / range); // 返回百分比
}
