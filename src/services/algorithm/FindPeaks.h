#pragma once

#include "services/algorithm/processing/IProcessingStep.h"
#include "core/entities/Curve.h"
#include <QObject>
#include <QVariantMap>
#include <QVector>

/**
 * @brief FindPeaks
 * 峰检测算法的轻量实现，支持最小高度、最小显著性、最小间距与SNR阈值。
 * 返回：
 *   - key="peaks" 的曲线，点集为检测到的峰位置（用于叠加显示峰标记）。
 */
class FindPeaks : public IProcessingStep
{
public:
    QString stepName() const override;
    QString userVisibleName() const override;
    QVariantMap defaultParameters() const override;
    ProcessingResult process(const QList<Curve*>& inputCurves,
                             const QVariantMap& params,
                             QString& error) override;

private:
    bool isLocalMax(const QVector<double>& y, int i) const;
    double localProminence(const QVector<double>& y, int i, int win) const;
    double localNoiseStd(const QVector<double>& y, int i, int win) const;
};

