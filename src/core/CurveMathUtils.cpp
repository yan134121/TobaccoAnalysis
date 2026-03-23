#include "CurveMathUtils.h"
#include <algorithm>
#include <cmath>

namespace CurveMathUtils {

double interpolateYAtX(const QVector<QPointF>& pts, double x)
{
    if (pts.isEmpty())
        return 0.0;
    if (pts.size() == 1)
        return pts[0].y();

    if (x <= pts.front().x()) {
        const QPointF& p0 = pts[0];
        const QPointF& p1 = pts[1];
        const double dx = p1.x() - p0.x();
        if (std::abs(dx) < 1e-15)
            return p0.y();
        const double t = (x - p0.x()) / dx;
        return p0.y() + t * (p1.y() - p0.y());
    }
    if (x >= pts.back().x()) {
        const QPointF& p0 = pts[pts.size() - 2];
        const QPointF& p1 = pts[pts.size() - 1];
        const double dx = p1.x() - p0.x();
        if (std::abs(dx) < 1e-15)
            return p1.y();
        const double t = (x - p0.x()) / dx;
        return p0.y() + t * (p1.y() - p0.y());
    }

    int lo = 0;
    int hi = static_cast<int>(pts.size()) - 1;
    while (hi - lo > 1) {
        const int mid = (lo + hi) / 2;
        if (pts[mid].x() <= x)
            lo = mid;
        else
            hi = mid;
    }
    const QPointF& p0 = pts[lo];
    const QPointF& p1 = pts[hi];
    const double dx = p1.x() - p0.x();
    if (std::abs(dx) < 1e-15)
        return 0.5 * (p0.y() + p1.y());
    const double t = (x - p0.x()) / dx;
    return p0.y() + t * (p1.y() - p0.y());
}

QVector<QPointF> sumCurvesYByUnionX(const QVector<QPointF>& curveA, const QVector<QPointF>& curveB)
{
    QVector<QPointF> out;
    if (curveA.isEmpty() || curveB.isEmpty())
        return out;

    QVector<double> xs;
    xs.reserve(curveA.size() + curveB.size());
    for (const QPointF& p : curveA)
        xs.append(p.x());
    for (const QPointF& p : curveB)
        xs.append(p.x());
    std::sort(xs.begin(), xs.end());
    xs.erase(std::unique(xs.begin(), xs.end()), xs.end());

    out.reserve(xs.size());
    for (double x : xs) {
        const double y = interpolateYAtX(curveA, x) + interpolateYAtX(curveB, x);
        out.append(QPointF(x, y));
    }
    return out;
}

} // namespace CurveMathUtils
