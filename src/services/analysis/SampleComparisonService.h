#ifndef SAMPLECOMPARISONSERVICE_H
#define SAMPLECOMPARISONSERVICE_H

#include <QObject>
#include <QList>
#include <QMap>
#include <QSharedPointer>
#include "core/common.h" // 包含 DifferenceResultRow

// 前置声明
class Curve;
class IDifferenceStrategy;

class SampleComparisonService : public QObject
{
    Q_OBJECT
public:
    explicit SampleComparisonService(QObject *parent = nullptr);
    ~SampleComparisonService();

    // 返回所有可用的差异度计算算法的名称
    QMap<QString, QString> availableAlgorithms() const;
    
    // 计算均方根误差
    double calculateRMSE(QSharedPointer<Curve> curve1, QSharedPointer<Curve> curve2);
    
    // 计算皮尔逊相关系数
    double calculatePearsonCorrelation(QSharedPointer<Curve> curve1, QSharedPointer<Curve> curve2);
    
    // 计算欧氏距离
    double calculateEuclideanDistance(QSharedPointer<Curve> curve1, QSharedPointer<Curve> curve2);

public slots:
    /**
     * @brief 对【已经加载好】的曲线进行差异度排名计算 (同步方法，适合在后台线程调用)
     * @param referenceCurve 作为基准的曲线
     * @param comparisonCurves 需要对比的曲线列表 (包含参考曲线自身)
     * @return 包含所有差异度得分的结果列表
     */
    QList<DifferenceResultRow> calculateRankingFromCurves(
        QSharedPointer<Curve> referenceCurve,
        const QList<QSharedPointer<Curve>>& allCurves
    );

private:
    void registerStrategies();
    // 内部持有一个已注册的差异度算法策略列表
    QMap<QString, IDifferenceStrategy*> m_registeredStrategies;
};

#endif // SAMPLECOMPARISONSERVICE_H