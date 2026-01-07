#pragma once

#include "services/algorithm/processing/IProcessingStep.h"
#include "core/entities/Curve.h"
#include <QObject>
#include <QVariantMap>
#include <QVector>

/**
 * @brief COWAlignment
 * COW（Correlation Optimized Warping，相关性优化形变）峰对齐算法的简化实现。
 * 说明：
 *   - 该实现采用“全局滞后搜索”的简化策略，快速估计最佳整体位移以提升参考谱与目标谱的相关性，
 *     并在此基础上进行线性重采样。相比经典分段COW，此版本更轻量、适合先集成后逐步增强。
 * 使用：
 *   - 输入曲线列表至少包含两条曲线：第1条为参考曲线，第2条为待对齐曲线。
 *   - 返回结果包含 key="aligned" 的单条曲线（目标谱对齐到参考谱的坐标）。
 * 参数（默认值可通过 defaultParameters 获取）：
 *   - window_size(int)：相关性计算的窗口大小（用于稳健性，当前实现为全局滞后，窗口仅用于噪声鲁棒处理）
 *   - max_warp(int)：最大滞后搜索范围（以点为单位）
 *   - segment_count(int)：分段数（预留，当前实现未使用）
 *   - resample_step(double)：重采样步长（预留，当前实现按参考x插值）
 */
class COWAlignment : public IProcessingStep
{
public:
    QString stepName() const override;
    QString userVisibleName() const override;
    QVariantMap defaultParameters() const override;
    ProcessingResult process(const QList<Curve*>& inputCurves,
                             const QVariantMap& params,
                             QString& error) override;

private:
    // 计算在给定滞后范围内的最佳整体位移（通过最大皮尔逊相关系数选择）
    int estimateBestLag(const QVector<double>& refY,
                        const QVector<double>& tgtY,
                        int maxLag) const;

    // 将目标y序列按照最佳滞后进行对齐，并线性插值到参考x坐标
    QVector<QPointF> alignByLagAndResample(const QVector<double>& refX,
                                           const QVector<double>& refY,
                                           const QVector<double>& tgtX,
                                           const QVector<double>& tgtY,
                                           int bestLag) const;
};

