#ifndef PARALLELSAMPLEANALYSISSERVICE_H
#define PARALLELSAMPLEANALYSISSERVICE_H

#include <QObject>
#include <QMap>
#include <QSharedPointer>
#include "core/common.h"

class AppInitializer;
class SampleComparisonService;
class Curve;

/**
 * ParallelSampleAnalysisService
 * 负责在同组平行样本中选择“代表样”（优选）。
 * 基于 SampleComparisonService 的差异度算法，并结合 ProcessingParameters 的权重进行综合评分。
 */
class ParallelSampleAnalysisService : public QObject
{
    Q_OBJECT
public:
    explicit ParallelSampleAnalysisService(AppInitializer* appInit, QObject* parent = nullptr);

    /**
     * 处理单个组（Replicate Group），选择组内“代表样”，并设置 bestInGroup 标记。
     * 该方法对应于 Copy_of_V2.2 中的 processReplicateGroup 思路，
     * 通过两两比较曲线、归一化差异度、按权重合成综合相似度，最终选择平均综合分最高的样本。
     * @param group 单个平行样组（输入/输出）
     * @param params 处理参数（包含 NRMSE / Pearson / Euclidean 权重）
     * @param stage 采用的比较阶段（如 RawData 或 Derivative）
     * @return 是否成功选择代表样（true 表示至少有一个曲线可用）
     */
    bool processReplicateGroup(SampleGroup& group,
                               const ProcessingParameters& params,
                               StageName stage) const;

    /**
     * 在批量数据的每个组内选择代表样，并用 bestInGroup 标记。
     * @param batchData 输入/输出的批量组数据
     * @param params 处理参数（使用其中的权重）
     * @param stage 参与对比的阶段（不同数据类型可选 RawData/Derivative 等）
     */
    void selectRepresentativesInBatch(BatchGroupData& batchData,
                                      const ProcessingParameters& params,
                                      StageName stage) const;

private:
    // 根据阶段名称获取曲线指针（复用各工作台的本地逻辑，这里统一封装）
    static QSharedPointer<Curve> getCurveFromStage(const SampleDataFlexible& sample, StageName stage);

    AppInitializer* m_appInitializer = nullptr;
};

#endif // PARALLELSAMPLEANALYSISSERVICE_H