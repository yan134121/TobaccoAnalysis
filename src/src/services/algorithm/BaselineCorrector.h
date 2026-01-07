#pragma once

#include "core/entities/Curve.h"
// #include "core/ProcessingResult.h"
#include "services/algorithm/processing/IProcessingStep.h"
#include <QObject>
#include <QVariantMap>
#include <QVector>

/**
 * @brief BaselineCorrector
 * 基线校正处理模块，基于 airPLS 算法实现。
 * 
 * 参数：
 *   - lambda: 平滑强度（越大越平滑）
 *   - order: 差分阶数（一般为 2）
 *   - p: 不对称参数 (0.01~0.1)
 *   - itermax: 最大迭代次数
 */
class BaselineCorrector : public IProcessingStep
{
public:
    QString stepName() const;
    QString userVisibleName() const;
    QVariantMap defaultParameters() const;
    ProcessingResult process(const QList<Curve*>& inputCurves, const QVariantMap& params, QString& error);

private:
    QVector<double> airPLS(const QVector<double>& y, double lambda, int order, double wep, double p, int itermax);
    QVector<double> diffMatrixApply(const QVector<double>& y, int order);
};

