#pragma once

#include <QString>
#include <QVector>
#include <QVariantMap>

/**
 * 分段面积向量差异度（对应 calculate_all_difference.m 中的 cosine / meanSPC / new；标定 Excel 为二期）。
 */
namespace ChromatogramDifferenceMetrics {

double cosineSimilarity(const QVector<double>& a, const QVector<double>& b);
/** 1 - cosineSimilarity */
double cosineDifference(const QVector<double>& a, const QVector<double>& b);
/** mean(|a-b|/(a+b))，分母为 0 时用 eps */
double meanSpc(const QVector<double>& a, const QVector<double>& b);
/** sqrt(mean((1 - a/b)^2))，要求 b 无零元素 */
double newMetric(const QVector<double>& a, const QVector<double>& b);

/** meanSPC * (1 - gain * calibCosine)；无标定时 calibCosine=0 */
double meanSpcCorrected(double meanSpc, double calibCosine, double gain);

/**
 * 对多样本分段面积向量做两两比较，返回行列表（便于写入 metrics）。
 * calibCosinePair(i,j) 可选，长度与样本数一致时用于 meanSPC 修正；否则修正项为 0。
 */
QVariantList pairwiseDifferenceTable(const QVector<int>& sampleIds,
                                    const QVector<QVector<double>>& areaRowsFinetuned,
                                    double gain,
                                    const QVector<double>& calibCosinePerPair);

} // namespace ChromatogramDifferenceMetrics
