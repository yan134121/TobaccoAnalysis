/**
 * 对齐后分段边界微调 —— 对应仓库内 色谱处理算法/04_对齐后微调边界/finetuned_bounds_results.m
 * - 硬约束：第 s 段起点 ∈ (上一段已微调起点, 下一段「模板」起点)，与 MATLAB 中 seg_starts(s±1) 一致
 * - 用户范围：old_start ± adjustRangeHalfWidth（MATLAB adjust_range_default = [-5,5] 时即 ±5）
 * - 在 [L,R] 内取 chrom 最小值点为新的段起点；再按 trapz(x时间, y强度) 重算各段面积
 * 若需与 MATLAB「Excel→seg_<ref>.mat」完全一致的分段模板，请使用参数 chromExternalSegmentStartsFile（每行 1-based 起点）
 */
#include "ChromatogramSegmentFinetune.h"

#include <QtMath>
#include <algorithm>

namespace {

/** iLast0 为本段最后一个点的 0-based 索引；梯形积分使用 iLast0 之前的相邻点对 */
static double trapzSlice(const QVector<double>& x, const QVector<double>& y, int iStart0, int iLast0)
{
    if (y.isEmpty() || x.size() != y.size()) return 0.0;
    if (iStart0 < 0 || iLast0 >= y.size() || iStart0 > iLast0) return 0.0;
    double s = 0.0;
    for (int i = iStart0; i < iLast0; ++i) {
        const double dx = x[i + 1] - x[i];
        s += 0.5 * (y[i] + y[i + 1]) * dx;
    }
    return s;
}

static QVector<double> areasFromStarts1Based(const QVector<double>& x,
                                            const QVector<double>& y,
                                            const QVector<int>& starts1)
{
    const int n = y.size();
    QVector<double> areas;
    if (starts1.isEmpty() || n < 1) return areas;
    const int m = starts1.size();
    areas.resize(m);
    for (int s = 0; s < m; ++s) {
        const int start1 = starts1[s];
        const int start0 = qMax(0, start1 - 1);
        int iLast0 = n - 1;
        if (s + 1 < m)
            iLast0 = qMin(n - 1, qMax(start0, starts1[s + 1] - 2));
        if (start0 > iLast0) {
            areas[s] = 0.0;
            continue;
        }
        areas[s] = trapzSlice(x, y, start0, iLast0);
    }
    return areas;
}

} // namespace

ChromatogramFinetuneResult finetuneSegmentBoundaries(const QVector<double>& x,
                                                     const QVector<double>& yAligned,
                                                     const QVector<int>& segStartsTemplate1Based,
                                                     int adjustRangeHalfWidth)
{
    ChromatogramFinetuneResult out;
    const int n = yAligned.size();
    if (x.size() != n || n < 1 || segStartsTemplate1Based.isEmpty()) return out;

    QVector<int> startsOrig = segStartsTemplate1Based;
    for (int& v : startsOrig) v = qBound(1, v, n);
    QVector<int> starts = startsOrig;

    out.segmentAreasOrig = areasFromStarts1Based(x, yAligned, startsOrig);

    const int m = starts.size();
    for (int s = 0; s < m; ++s) {
        const int oldStart = starts[s];
        int hard_L = 1;
        int hard_R = n;
        if (s > 0) hard_L = starts[s - 1] + 1;
        if (s < m - 1) hard_R = startsOrig[s + 1] - 1;
        const int user_L = oldStart - adjustRangeHalfWidth;
        const int user_R = oldStart + adjustRangeHalfWidth;
        int L = qMax(hard_L, user_L);
        L = qMax(1, L);
        int R = qMin(hard_R, user_R);
        R = qMin(n, R);
        if (L >= R) continue;

        int best = L;
        double bestVal = yAligned[best - 1];
        for (int j = L; j <= R; ++j) {
            const double v = yAligned[j - 1];
            if (v < bestVal) {
                bestVal = v;
                best = j;
            }
        }
        starts[s] = best;
    }

    out.segStartsFinetuned1Based = starts;
    out.segmentAreasFinetuned = areasFromStarts1Based(x, yAligned, starts);
    return out;
}
