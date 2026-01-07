#include "Pearson.h"
#include "core/entities/Curve.h"
#include "Logger.h"
#include <QtMath>

Pearson::Pearson(QObject *parent) : QObject(parent) {}

QString Pearson::algorithmId() const
{
    return "pearson";
}

QString Pearson::userVisibleName() const
{
    // return tr("皮尔逊相关系数 (Pearson)");
    return tr("皮尔逊相关系数");
}

double Pearson::calculateDifference(const Curve &curveA, const Curve &curveB, const QVariantMap &params)
{
    // DEBUG_LOG << "打印样本曲线数据点" << curveB.printCurveData(20);  // 打印前20点
    // DEBUG_LOG << "打印参考曲线数据点" << curveA.printCurveData(20);
    
    const auto& dataA = curveA.data();
    const auto& dataB = curveB.data();

    if (dataA.size() != dataB.size() || dataA.size() < 2) {
        qWarning("Pearson : Input curves must have the same size (at least 2 points).");
        return -1.0;
    }

    int n = dataA.size();
    double sumA = 0.0, sumB = 0.0, sumSqA = 0.0, sumSqB = 0.0, sumProd = 0.0;

    for (int i = 0; i < n; ++i) {
        double yA = dataA[i].y();
        double yB = dataB[i].y();
        sumA += yA;
        sumB += yB;
        sumSqA += yA * yA;
        sumSqB += yB * yB;
        sumProd += yA * yB;
    }

    double numerator = sumProd - (sumA * sumB / n);
    double denominator = qSqrt((sumSqA - sumA * sumA / n) * (sumSqB - sumB * sumB / n));

    if (qAbs(denominator) < 1e-9) {
        return 0.0; // 如果一条线是平的，则认为不相关
    }
    
    // double correlation = numerator / denominator;
    
    // // Pearson 系数是相似度 (-1 to 1), 我们需要将其转换为差异度 (0 to 2)
    // // 差异度越小越相似
    // return 1.0 - correlation;

    double correlation = numerator / denominator;

    // // 打印日志帮助验证
    // DEBUG_LOG << "[Pearson] 相关系数 r =" << correlation;

    return correlation;  // ← 直接返回相关系数

}
