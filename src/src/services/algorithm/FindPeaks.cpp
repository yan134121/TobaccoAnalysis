#include "FindPeaks.h"
#include "core/entities/Curve.h"
#include <QtMath>

QString FindPeaks::stepName() const {
    return "peak_detection";
}

QString FindPeaks::userVisibleName() const {
    return QObject::tr("峰检测");
}

QVariantMap FindPeaks::defaultParameters() const {
    QVariantMap params;
    params["min_height"] = 0.0;        // 最小峰高度
    params["min_prominence"] = 0.0;    // 最小峰显著性
    params["min_distance"] = 5;        // 峰间最小点距
    params["snr_threshold"] = 0.0;     // 最小信噪比
    params["window"] = 20;             // 用于显著性与噪声估计的局部窗口
    return params;
}

ProcessingResult FindPeaks::process(const QList<Curve*>& inputCurves,
                                    const QVariantMap& params,
                                    QString& error)
{
    ProcessingResult result;
    if (inputCurves.isEmpty() || !inputCurves.first()) {
        error = QObject::tr("峰检测输入曲线为空");
        return result;
    }

    const Curve* in = inputCurves.first();
    QVector<QPointF> data = in->data();
    if (data.size() < 3) {
        error = QObject::tr("数据点过少，无法进行峰检测");
        return result;
    }

    double minHeight = params.value("min_height", 0.0).toDouble();
    double minProm   = params.value("min_prominence", 0.0).toDouble();
    int    minDist   = params.value("min_distance", 5).toInt();
    double snrThr    = params.value("snr_threshold", 0.0).toDouble();
    int    win       = params.value("window", 20).toInt();

    QVector<double> xs, ys;
    xs.reserve(data.size());
    ys.reserve(data.size());
    for (const auto& p : data) { xs.append(p.x()); ys.append(p.y()); }

    QVector<QPointF> peaks;
    int lastPeakIdx = -minDist - 1;
    for (int i = 1; i < ys.size() - 1; ++i) {
        if (!isLocalMax(ys, i)) continue;
        if (ys[i] < minHeight) continue;
        double prom = localProminence(ys, i, win);
        if (prom < minProm) continue;
        double noise = localNoiseStd(ys, i, win);
        double snr = (noise > 0.0) ? (ys[i] / noise) : ys[i];
        if (snr < snrThr) continue;
        if (i - lastPeakIdx < minDist) continue;
        peaks.append(QPointF(xs[i], ys[i]));
        lastPeakIdx = i;
    }

    Curve* peakCurve = new Curve(peaks, in->name() + QObject::tr(" (峰标记)"));
    peakCurve->setSampleId(in->sampleId());
    result.namedCurves["peaks"].append(peakCurve);
    return result;
}

bool FindPeaks::isLocalMax(const QVector<double>& y, int i) const
{
    return (y[i] > y[i-1]) && (y[i] >= y[i+1]);
}

double FindPeaks::localProminence(const QVector<double>& y, int i, int win) const
{
    int n = y.size();
    int l0 = qMax(0, i - win);
    int r0 = qMin(n - 1, i + win);
    double leftMin = y[i], rightMin = y[i];
    for (int k = l0; k <= i; ++k) leftMin = qMin(leftMin, y[k]);
    for (int k = i; k <= r0; ++k) rightMin = qMin(rightMin, y[k]);
    double base = qMax(leftMin, rightMin);
    return y[i] - base;
}

double FindPeaks::localNoiseStd(const QVector<double>& y, int i, int win) const
{
    int n = y.size();
    int l0 = qMax(0, i - win);
    int r0 = qMin(n - 1, i + win);
    int len = r0 - l0 + 1;
    if (len <= 1) return 0.0;
    double sum = 0.0, sum2 = 0.0;
    for (int k = l0; k <= r0; ++k) {
        sum += y[k];
        sum2 += y[k] * y[k];
    }
    double mean = sum / len;
    double var = (sum2 / len) - mean * mean;
    return (var > 0.0) ? qSqrt(var) : 0.0;
}

