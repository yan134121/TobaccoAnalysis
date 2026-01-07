#include "Euclidean.h"
#include "core/entities/Curve.h" // 包含 Curve 定义
#include <QtMath> // 用于 qSqrt
#include <limits> // 用于 std::numeric_limits

Euclidean::Euclidean(QObject *parent) : QObject(parent)
{
    // 构造函数体，如果需要初始化任何成员，可以在这里进行
}

QString Euclidean::algorithmId() const
{
    return "euclidean"; // 【关键】返回唯一的算法ID
}

QString Euclidean::userVisibleName() const
{
    // 使用 QObject::tr() 以支持国际化
    // return tr("欧氏距离 (Euclidean)"); // 【关键】返回用户友好的名称
    return tr("欧氏距离"); // 【关键】返回用户友好的名称
}

double Euclidean::calculateDifference(const Curve &curveA, const Curve& curveB, const QVariantMap& params)
{
    Q_UNUSED(params); // 表明 params 参数未使用，避免编译器警告

    const auto& dataA = curveA.data();
    const auto& dataB = curveB.data();

    if (dataA.size() != dataB.size() || dataA.isEmpty()) {
        qWarning("Euclidean: Input curves must have the same non-empty size. Interpolation might be required.");
        return -1.0; // 返回一个表示错误的无效值
    }

    double sumOfSquares = 0.0;
    for (int i = 0; i < dataA.size(); ++i) {
        double diff = dataA[i].y() - dataB[i].y();
        sumOfSquares += diff * diff;
    }
    
    // 返回欧氏距离，值越小差异越小
    return qSqrt(sumOfSquares);
}