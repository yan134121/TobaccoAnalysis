#include "COWAlignment.h"
#include "core/entities/Curve.h"
#include <QtMath>
#include <algorithm>

QString COWAlignment::stepName() const {
    return "cow_alignment";
}

QString COWAlignment::userVisibleName() const {
    return QObject::tr("COW峰对齐");
}

QVariantMap COWAlignment::defaultParameters() const {
    QVariantMap params;
    params["window_size"] = 100;    // 相关性窗口大小（简化实现中仅用于鲁棒估计）
    params["max_warp"] = 50;        // 最大滞后搜索范围（单位：点）
    params["segment_count"] = 1;    // 分段数（预留）
    params["resample_step"] = 0.0;  // 重采样步长（预留，0表示按参考x直接插值）
    return params;
}

ProcessingResult COWAlignment::process(const QList<Curve*>& inputCurves,
                                       const QVariantMap& params,
                                       QString& error)
{
    ProcessingResult result;
    if (inputCurves.size() < 2) {
        error = QObject::tr("COW峰对齐需要至少两条输入曲线（参考 + 待对齐）");
        return result;
    }

    const Curve* ref = inputCurves[0];
    const Curve* tgt = inputCurves[1];
    if (!ref || !tgt) {
        error = QObject::tr("输入曲线为空，无法执行对齐");
        return result;
    }

    // 提取参考与目标曲线数据
    QVector<QPointF> refData = ref->data();
    QVector<QPointF> tgtData = tgt->data();
    if (refData.isEmpty() || tgtData.isEmpty()) {
        error = QObject::tr("参考或目标曲线数据为空");
        return result;
    }

    QVector<double> refX, refY, tgtX, tgtY;
    refX.reserve(refData.size());
    refY.reserve(refData.size());
    for (const auto& p : refData) { refX.append(p.x()); refY.append(p.y()); }
    tgtX.reserve(tgtData.size());
    tgtY.reserve(tgtData.size());
    for (const auto& p : tgtData) { tgtX.append(p.x()); tgtY.append(p.y()); }

    int maxWarp = params.value("max_warp", 50).toInt();
    if (maxWarp < 0) maxWarp = 0;

    // 估计最佳整体滞后
    int bestLag = estimateBestLag(refY, tgtY, maxWarp);

    // 按最佳滞后进行重采样到参考x坐标
    QVector<QPointF> alignedPoints = alignByLagAndResample(refX, refY, tgtX, tgtY, bestLag);

    // 构造对齐后的曲线对象
    Curve* alignedCurve = new Curve(alignedPoints, tgt->name() + QObject::tr(" (对齐到参考)"));
    alignedCurve->setSampleId(tgt->sampleId());
    result.namedCurves["aligned"].append(alignedCurve);
    return result;
}

int COWAlignment::estimateBestLag(const QVector<double>& refY,
                                  const QVector<double>& tgtY,
                                  int maxLag) const
{
    // 为简化实现，这里使用全局皮尔逊相关在 [-maxLag, maxLag] 范围内搜索最佳滞后
    if (refY.isEmpty() || tgtY.isEmpty()) return 0;
    int nRef = refY.size();
    int nTgt = tgtY.size();
    int n = qMin(nRef, nTgt);

    auto pearson = [&](int lag)->double {
        // lag > 0 表示目标向右移（滞后），负值表示向左移（超前）
        // 对齐长度受限于公共重叠部分
        int startRef = qMax(0, lag);
        int startTgt = qMax(0, -lag);
        int len = n - qAbs(lag);
        if (len <= 5) return -1.0; // 长度过短，忽略

        double sumX = 0, sumY = 0, sumXX = 0, sumYY = 0, sumXY = 0;
        for (int i = 0; i < len; ++i) {
            double x = refY[startRef + i];
            double y = tgtY[startTgt + i];
            sumX += x; sumY += y;
            sumXX += x * x; sumYY += y * y;
            sumXY += x * y;
        }
        double meanX = sumX / len;
        double meanY = sumY / len;
        double covXY = (sumXY / len) - meanX * meanY;
        double varX = (sumXX / len) - meanX * meanX;
        double varY = (sumYY / len) - meanY * meanY;
        double denom = qSqrt(varX) * qSqrt(varY);
        if (denom == 0.0) return -1.0;
        return covXY / denom;
    };

    double bestCorr = -1.0;
    int bestLag = 0;
    for (int lag = -maxLag; lag <= maxLag; ++lag) {
        double r = pearson(lag);
        if (r > bestCorr) {
            bestCorr = r;
            bestLag = lag;
        }
    }
    return bestLag;
}

static double linearInterp(const QVector<double>& xs, const QVector<double>& ys, double x)
{
    // 一维线性插值（边界外采用就近值）
    int n = xs.size();
    if (n == 0) return 0.0;
    if (n == 1) return ys[0];
    if (x <= xs[0]) return ys[0];
    if (x >= xs[n-1]) return ys[n-1];
    // 二分查找定位区间
    int lo = 0, hi = n - 1;
    while (hi - lo > 1) {
        int mid = (lo + hi) / 2;
        if (xs[mid] <= x) lo = mid;
        else hi = mid;
    }
    double x0 = xs[lo], x1 = xs[hi];
    double y0 = ys[lo], y1 = ys[hi];
    double t = (x - x0) / (x1 - x0);
    return y0 + t * (y1 - y0);
}

QVector<QPointF> COWAlignment::alignByLagAndResample(const QVector<double>& refX,
                                                     const QVector<double>& /*refY*/,
                                                     const QVector<double>& tgtX,
                                                     const QVector<double>& tgtY,
                                                     int bestLag) const
{
    // 将目标序列索引整体偏移 bestLag，并在参考x上进行线性插值
    // 这里为了简化，我们直接在目标的 x/y 上插值，不做复杂分段形变
    // 如果 bestLag > 0，表示目标相对参考滞后，等价于目标索引右移；<0 表示目标超前。
    // 为近似对齐，我们不直接移动索引，而是按参考x插值目标y，插值前将目标x进行等效平移。
    QVector<double> shiftedTgtX = tgtX;
    if (!tgtX.isEmpty()) {
        // 估计平均采样间隔，用于将“点级滞后”转换为“x轴平移”
        double avgStep = 0.0;
        for (int i = 1; i < tgtX.size(); ++i) avgStep += (tgtX[i] - tgtX[i-1]);
        avgStep = (tgtX.size() > 1) ? (avgStep / (tgtX.size() - 1)) : 0.0;
        double shift = bestLag * avgStep;
        for (int i = 0; i < shiftedTgtX.size(); ++i) shiftedTgtX[i] += shift;
    }

    QVector<QPointF> out;
    out.reserve(refX.size());
    for (double x : refX) {
        double y = linearInterp(shiftedTgtX, tgtY, x);
        out.append(QPointF(x, y));
    }
    return out;
}

