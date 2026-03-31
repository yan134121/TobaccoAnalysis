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
