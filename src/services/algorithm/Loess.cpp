
// #include "Loess.h"
// #include "core/entities/Curve.h"

// QString Loess::stepName() const {
//     return "smoothing_loess"; // 给 Loess 一个唯一的ID
// }
// QString Loess::userVisibleName() const {
//     return QObject::tr("Loess 平滑");
// }
// QVariantMap Loess::defaultParameters() const {
//     QVariantMap params;
//     params["fraction"] = 0.1; // Loess 算法常用的参数：窗口分数
//     return params;
// }

// ProcessingResult Loess::process(const QList<Curve*> &inputCurves, const QVariantMap &params, QString &error)
// {
//     ProcessingResult result;
//     double fraction = params.value("fraction", 0.1).toDouble();
    
//     for (Curve* inCurve : inputCurves) {
//         // --- 在这里实现 Loess 平滑的数学逻辑 ---
//         // 1. 获取输入数据 inCurve->data()
//         // 2. 根据 fraction 参数，对数据进行局部加权回归计算
//         // 3. 得到平滑后的数据点 smoothedPoints
        
//         // (以下为模拟代码)
//         QVector<QPointF> smoothedPoints = inCurve->data(); // 暂时返回原始数据
//         // --- 模拟代码结束 ---
        
//         Curve* smoothedCurve = new Curve(smoothedPoints, inCurve->name() + QObject::tr(" (Loess)"));
//         result.namedCurves["smoothed"].append(smoothedCurve);
//     }
    
//     return result;
// }



#include "Loess.h"
#include "core/entities/Curve.h"
#include <QtMath>

QString Loess::stepName() const {
    return "smoothing_loess";
}

QString Loess::userVisibleName() const {
    return QObject::tr("Loess 平滑");
}

QVariantMap Loess::defaultParameters() const {
    QVariantMap params;
    params["fraction"] = 0.1;
    return params;
}

// ------------------------
// Tri-cube 权重函数
// ------------------------
static double tricube(double x)
{
    x = qAbs(x);
    return (x >= 1.0) ? 0.0 : qPow(1 - x*x*x, 3);
}

// ------------------------
// Loess 平滑主函数（给 QVector<QPointF>）
// ------------------------
static QVector<QPointF> loessSmooth(const QVector<QPointF> &data, double fraction)
{
    int n = data.size();
    if (n < 3) return data;

    int window = qMax(3, int(n * fraction));
    QVector<QPointF> result;
    result.reserve(n);

    for (int i = 0; i < n; i++) {
        double xi = data[i].x();

        // --- 窗口范围 ---
        int left = qMax(0, i - window / 2);
        int right = qMin(n - 1, left + window - 1);
        if (right - left + 1 < window && left > 0)
            left = qMax(0, right - window + 1);

        double maxDist = qAbs(data[right].x() - data[left].x());
        if (maxDist == 0) maxDist = 1e-12;

        // --- 加权计算 ---
        double Sw = 0, Swx = 0, Swy = 0, Swxx = 0, Swxy = 0;

        for (int j = left; j <= right; j++) {
            double d = qAbs(data[j].x() - xi) / maxDist;
            double w = tricube(d);

            double x = data[j].x();
            double y = data[j].y();

            Sw   += w;
            Swx  += w * x;
            Swy  += w * y;
            Swxx += w * x * x;
            Swxy += w * x * y;
        }

        double denom = Sw * Swxx - Swx * Swx;
        double a = 0, b = 0;

        if (qAbs(denom) > 1e-12) {
            b = (Sw * Swxy - Swx * Swy) / denom;
            a = (Swy - b * Swx) / Sw;
        } else {
            a = Swy / Sw;
            b = 0;
        }

        result.append(QPointF(xi, a + b * xi));
    }

    return result;
}

// --------------------------------------------------
// IProcessingStep 覆盖接口：执行 Loess 平滑
// --------------------------------------------------
ProcessingResult Loess::process(const QList<Curve*> &inputCurves,
                                const QVariantMap &params,
                                QString &error)
{
    ProcessingResult result;

    double fraction = params.value("fraction", 0.1).toDouble();
    if (fraction <= 0 || fraction >= 1) {
        error = "fraction must be between 0 and 1.";
        return result;
    }

    for (Curve* inCurve : inputCurves) {
        if (!inCurve) continue;

        QVector<QPointF> source = inCurve->data();
        QVector<QPointF> smoothed = loessSmooth(source, fraction);

        Curve* smoothedCurve =
                new Curve(smoothed, inCurve->name() + QObject::tr(" (Loess)"));

        result.namedCurves["smoothed"].append(smoothedCurve);
    }

    return result;
}
