#include "SampleComparisonService.h"
#include "core/entities/Curve.h"
#include "services/algorithm/comparison/IDifferenceStrategy.h"
// 包含所有具体的差异度算法策略
#include "services/algorithm/Nrmse.h"
#include "services/algorithm/Pearson.h"
#include "services/algorithm/Euclidean.h"
#include "services/algorithm/Loess.h"
#include "Logger.h"
#include <QtMath>
#include <algorithm>
#include <limits>

SampleComparisonService::SampleComparisonService(QObject *parent) : QObject(parent)
{
    registerStrategies();
}

SampleComparisonService::~SampleComparisonService()
{
    qDeleteAll(m_registeredStrategies);
}

void SampleComparisonService::registerStrategies()
{
    // 创建并注册所有可用的差异度计算策略
    IDifferenceStrategy* nrmse = new Nrmse();
    m_registeredStrategies[nrmse->algorithmId()] = nrmse;
    
    IDifferenceStrategy* pearson = new Pearson();
    m_registeredStrategies[pearson->algorithmId()] = pearson;
    
    IDifferenceStrategy* euclidean = new Euclidean();
    m_registeredStrategies[euclidean->algorithmId()] = euclidean;
}

QMap<QString, QString> SampleComparisonService::availableAlgorithms() const
{
    QMap<QString, QString> result;
    for (auto* strategy : m_registeredStrategies) {
        result[strategy->algorithmId()] = strategy->userVisibleName();
    }
    return result;
}

double SampleComparisonService::calculateRMSE(QSharedPointer<Curve> curve1, QSharedPointer<Curve> curve2)
{
    // 查找NRMSE策略并使用它计算
    IDifferenceStrategy* strategy = m_registeredStrategies.value("nrmse");
    if (strategy) {
        return strategy->calculateDifference(*curve1, *curve2, {});
    }
    return 0.0;
}

double SampleComparisonService::calculatePearsonCorrelation(QSharedPointer<Curve> curve1, QSharedPointer<Curve> curve2)
{
    // 查找Pearson策略并使用它计算
    IDifferenceStrategy* strategy = m_registeredStrategies.value("pearson");
    if (strategy) {
        return strategy->calculateDifference(*curve1, *curve2, {});
    }
    return 0.0;
}

double SampleComparisonService::calculateEuclideanDistance(QSharedPointer<Curve> curve1, QSharedPointer<Curve> curve2)
{
    // 查找Euclidean策略并使用它计算
    IDifferenceStrategy* strategy = m_registeredStrategies.value("euclidean");
    if (strategy) {
        return strategy->calculateDifference(*curve1, *curve2, {});
    }
    return 0.0;
}

QList<DifferenceResultRow> SampleComparisonService::calculateRankingFromCurves(
    QSharedPointer<Curve> referenceCurve, 
    const QList<QSharedPointer<Curve>> &allCurves)
{
    QList<DifferenceResultRow> finalResults;
    if (referenceCurve.isNull() || allCurves.isEmpty()) {
        return finalResults;
    }

    // for (QSharedPointer<Curve> compCurve : allCurves) {
    //     if (compCurve.isNull() || compCurve == referenceCurve) continue;

    //     DifferenceResultRow result;
    //     result.sampleId = compCurve->sampleId();
    //     result.sampleName = compCurve->name();
        
    //     // 遍历所有注册的差异度算法
    //     for (const QString& algId : m_registeredStrategies.keys()) {
    //         IDifferenceStrategy* strategy = m_registeredStrategies.value(algId);
    //         // 【关键】确保数据对齐，这是算法策略内部的职责
    //         double score = strategy->calculateDifference(*referenceCurve, *compCurve, {});
    //         result.scores[algId] = score;
    //     }
    //     finalResults.append(result);
    // }

    for (QSharedPointer<Curve> compCurve : allCurves) {
    if (compCurve.isNull()) continue;

    DifferenceResultRow result;
    result.sampleId = compCurve->sampleId();
    DEBUG_LOG << "sampleId ==" << result.sampleId;
    result.sampleName = compCurve->name();

    for (const QString& algId : m_registeredStrategies.keys()) {
        IDifferenceStrategy* strategy = m_registeredStrategies.value(algId);
        double score = (compCurve == referenceCurve) 
                        ? 1.0  // 自己和自己的比较直接设为 1.0
                        : strategy->calculateDifference(*referenceCurve, *compCurve, {});
        result.scores[algId] = score;
    }
    finalResults.append(result);
}

    
    return finalResults;
}

// 辅助函数：Loess平滑（复用Loess.cpp中的逻辑）
static QVector<QPointF> loessSmoothLocal(const QVector<QPointF>& data, double fraction)
{
    int n = data.size();
    if (n < 3) return data;

    int window = qMax(3, int(n * fraction));
    QVector<QPointF> result;
    result.reserve(n);

    auto tricube = [](double x) {
        x = qAbs(x);
        return (x >= 1.0) ? 0.0 : qPow(1 - x*x*x, 3);
    };

    for (int i = 0; i < n; i++) {
        double xi = data[i].x();
        int left = qMax(0, i - window / 2);
        int right = qMin(n - 1, left + window - 1);
        if (right - left + 1 < window && left > 0)
            left = qMax(0, right - window + 1);

        double maxDist = qAbs(data[right].x() - data[left].x());
        if (maxDist == 0) maxDist = 1e-12;

        double Sw = 0, Swx = 0, Swy = 0, Swxx = 0, Swxy = 0;
        for (int j = left; j <= right; j++) {
            double d = qAbs(data[j].x() - xi) / maxDist;
            double w = tricube(d);
            double x = data[j].x();
            double y = data[j].y();
            Sw   += w;
            Swx  += w * x;
            Swy  += w * y;
            Swxx += w * x * x;
            Swxy += w * x * y;
        }

        double denom = Sw * Swxx - Swx * Swx;
        double a = 0, b = 0;
        if (qAbs(denom) > 1e-12) {
            b = (Sw * Swxy - Swx * Swy) / denom;
            a = (Swy - b * Swx) / Sw;
        } else {
            a = Swy / Sw;
            b = 0;
        }
        result.append(QPointF(xi, a + b * xi));
    }
    return result;
}

// 辅助函数：安全计算相关系数
static double safeCorr(const QVector<double>& x, const QVector<double>& y)
{
    if (x.size() != y.size() || x.size() < 2) return 0.0;
    double sumX = 0, sumY = 0, sumXY = 0, sumX2 = 0, sumY2 = 0;
    int n = x.size();
    for (int i = 0; i < n; ++i) {
        if (!qIsFinite(x[i]) || !qIsFinite(y[i])) continue;
        sumX += x[i];
        sumY += y[i];
        sumXY += x[i] * y[i];
        sumX2 += x[i] * x[i];
        sumY2 += y[i] * y[i];
    }
    double stdX = qSqrt((sumX2 - sumX*sumX/n) / (n-1));
    double stdY = qSqrt((sumY2 - sumY*sumY/n) / (n-1));
    if (stdX < 1e-12 || stdY < 1e-12) return 0.0;
    double cov = (sumXY - sumX*sumY/n) / (n-1);
    return cov / (stdX * stdY);
}

SampleComparisonService::PickBestResult SampleComparisonService::pickBestOfTwo(
    QSharedPointer<Curve> curve1, QSharedPointer<Curve> curve2, double loessSpan)
{
    PickBestResult result;
    result.bestIndex = 0;
    result.quality1 = result.quality2 = std::numeric_limits<double>::max();
    result.residualRms1 = result.residualRms2 = 0.0;
    result.stability1 = result.stability2 = 0.0;
    result.nrmse = result.pearson = result.euclid = 0.0;

    if (curve1.isNull() || curve2.isNull()) {
        WARNING_LOG << "pickBestOfTwo: 输入曲线为空";
        return result;
    }

    QVector<QPointF> d1 = curve1->data();
    QVector<QPointF> d2 = curve2->data();

    if (d1.isEmpty() || d2.isEmpty()) {
        WARNING_LOG << "pickBestOfTwo: 曲线数据为空";
        return result;
    }

    // 对齐长度（取最短）
    int n = qMin(d1.size(), d2.size());
    d1 = d1.mid(0, n);
    d2 = d2.mid(0, n);

    // 提取Y值用于计算
    QVector<double> y1, y2;
    y1.reserve(n);
    y2.reserve(n);
    for (int i = 0; i < n; ++i) {
        y1.append(d1[i].y());
        y2.append(d2[i].y());
    }

    // 配对一致性指标
    result.nrmse = calculateRMSE(curve1, curve2);
    result.pearson = calculatePearsonCorrelation(curve1, curve2);
    result.euclid = calculateEuclideanDistance(curve1, curve2);

    // 各自质量度量：LOESS残差RMS和稳定性
    int spanPts = qMax(5, int(loessSpan * n));
    QVector<QPointF> s1_loess = loessSmoothLocal(d1, loessSpan);
    QVector<QPointF> s2_loess = loessSmoothLocal(d2, loessSpan);

    // 残差RMS
    double sumSq1 = 0, sumSq2 = 0;
    for (int i = 0; i < n; ++i) {
        double diff1 = y1[i] - s1_loess[i].y();
        double diff2 = y2[i] - s2_loess[i].y();
        sumSq1 += diff1 * diff1;
        sumSq2 += diff2 * diff2;
    }
    result.residualRms1 = qSqrt(sumSq1 / n);
    result.residualRms2 = qSqrt(sumSq2 / n);

    // 稳定性：改变平滑跨度±20%后的差异
    int spanPtsLow = qMax(5, int(0.8 * spanPts));
    int spanPtsHigh = qMax(5, int(1.2 * spanPts));
    double fractionLow = double(spanPtsLow) / n;
    double fractionHigh = double(spanPtsHigh) / n;

    QVector<QPointF> s1_low = loessSmoothLocal(d1, fractionLow);
    QVector<QPointF> s1_high = loessSmoothLocal(d1, fractionHigh);
    QVector<QPointF> s2_low = loessSmoothLocal(d2, fractionLow);
    QVector<QPointF> s2_high = loessSmoothLocal(d2, fractionHigh);

    double diff1 = 0, diff2 = 0;
    for (int i = 0; i < n; ++i) {
        diff1 += qPow(s1_low[i].y() - s1_high[i].y(), 2);
        diff2 += qPow(s2_low[i].y() - s2_high[i].y(), 2);
    }
    result.stability1 = qSqrt(diff1) / n;
    result.stability2 = qSqrt(diff2) / n;

    // 组合质量分数（越小越好）
    const double w_noise = 0.7;
    const double w_stab = 0.3;
    result.quality1 = w_noise * result.residualRms1 + w_stab * result.stability1;
    result.quality2 = w_noise * result.residualRms2 + w_stab * result.stability2;

    // 选择最优样本
    const double epsTie = 1e-12;
    if (result.quality1 < result.quality2 - epsTie) {
        result.bestIndex = 0;
    } else if (result.quality2 < result.quality1 - epsTie) {
        result.bestIndex = 1;
    } else {
        // 极端平局：按主峰一致性决策（局部相关性）
        int p1 = 0, p2 = 0;
        double min1 = y1[0], min2 = y2[0];
        for (int i = 1; i < n; ++i) {
            if (y1[i] < min1) { min1 = y1[i]; p1 = i; }
            if (y2[i] < min2) { min2 = y2[i]; p2 = i; }
        }
        int p = (p1 + p2) / 2;
        int halfWin = qMax(5, int(0.05 * n));
        int i1 = qMax(0, p - halfWin);
        int i2 = qMin(n - 1, p + halfWin);

        QVector<double> localY1, localY2, localS1, localS2;
        for (int i = i1; i <= i2; ++i) {
            localY1.append(y1[i]);
            localY2.append(y2[i]);
            localS1.append(s1_loess[i].y());
            localS2.append(s2_loess[i].y());
        }

        double r1 = safeCorr(localY1, localS1);
        double r2 = safeCorr(localY2, localS2);
        result.bestIndex = (r1 >= r2) ? 0 : 1;
    }

    return result;
}
