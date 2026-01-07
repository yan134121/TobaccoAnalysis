#include "SampleComparisonService.h"
#include "core/entities/Curve.h"
#include "services/algorithm/comparison/IDifferenceStrategy.h"
// 包含所有具体的差异度算法策略
#include "services/algorithm/Nrmse.h"
#include "services/algorithm/Pearson.h"
#include "services/algorithm/Euclidean.h"
#include "Logger.h"

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
