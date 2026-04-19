#pragma once

#include <QVector>

/**
 * 对齐后分段边界微调（对应 色谱处理算法/04_对齐后微调边界/finetuned_bounds_results.m）：
 * 在每段起点 ±adjustRangeHalfWidth 且满足不跨段硬约束的范围内，将起点移到信号最小值点，
 * 并用梯形积分重算每段面积。
 */
struct ChromatogramFinetuneResult {
    QVector<int> segStartsFinetuned1Based;
    QVector<double> segmentAreasOrig;
    QVector<double> segmentAreasFinetuned;
};

ChromatogramFinetuneResult finetuneSegmentBoundaries(const QVector<double>& x,
                                                     const QVector<double>& yAligned,
                                                     const QVector<int>& segStartsTemplate1Based,
                                                     int adjustRangeHalfWidth);
