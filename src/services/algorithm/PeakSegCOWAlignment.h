#pragma once

#include "services/algorithm/processing/IProcessingStep.h"
#include <QObject>
#include <QVariantMap>

/**
 * @brief PeakSegCOWAlignment
 * 将 MATLAB `my_cow_align_peakseg_V5_auto.m` 的核心逻辑移植到 C++：
 * - 参考信号平滑（movmean）
 * - 多区间峰检测（按 MinPeakProminence）
 * - 峰聚类 -> 以谷底分段
 * - 分段 DP 回溯（COW 思路）+ 线性重采样拼接
 *
 * 输入：两条曲线（参考 + 目标）
 * 输出：key="aligned" 的曲线（目标对齐到参考的 x 坐标）
 */
class PeakSegCOWAlignment : public IProcessingStep
{
public:
    QString stepName() const override;
    QString userVisibleName() const override;
    QVariantMap defaultParameters() const override;
    ProcessingResult process(const QList<Curve*>& inputCurves,
                             const QVariantMap& params,
                             QString& error) override;
};

