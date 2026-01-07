// #include "Normalization.h"
// #include "core/entities/Curve.h"
// #include <limits>
// #include <QtMath>   // qSqrt、qPow 等 Qt 数学函数


// QString Normalization::stepName() const {
//     return "normalization";
// }
// QString Normalization::userVisibleName() const {
//     return QObject::tr("归一化");
// }
// QVariantMap Normalization::defaultParameters() const {
//     QVariantMap params;
//     params["method"] = "max_min"; // "max_min" 或 "z_score"
//     return params;
// }

// ProcessingResult Normalization::process(const QList<Curve*> &inputCurves, const QVariantMap &params, QString &error)
// {
//     ProcessingResult result;
//     QString method = params.value("method", "max_min").toString();

//     for (Curve* inCurve : inputCurves) {
//         const QVector<QPointF>& originalData = inCurve->data();
//         if (originalData.isEmpty()) continue;

//         QVector<QPointF> normalizedData;
//         normalizedData.reserve(originalData.size());

//         if (method == "max_min") {
//             // --- 最大-最小值归一化 (到 [0, 1] 区间) ---
//             double minY = std::numeric_limits<double>::max();
//             double maxY = std::numeric_limits<double>::lowest();
//             for (const QPointF& point : originalData) {
//                 if (point.y() < minY) minY = point.y();
//                 if (point.y() > maxY) maxY = point.y();
//             }
//             double range = maxY - minY;
//             if (range < 1e-9) { // 避免除以零
//                 for (const QPointF& point : originalData) {
//                     normalizedData.append(QPointF(point.x(), 0.0));
//                 }
//             } else {
//                 for (const QPointF& point : originalData) {
//                     normalizedData.append(QPointF(point.x(), (point.y() - minY) / range));
//                 }
//             }
//         } 
//         else if (method == "z_score") {
//             // --- Z-score 标准化 (均值为0，标准差为1) ---
//             double sum = 0.0;
//             for (const QPointF& point : originalData) {
//                 sum += point.y();
//             }
//             double mean = sum / originalData.size();
            
//             double sqSum = 0.0;
//             for (const QPointF& point : originalData) {
//                 sqSum += (point.y() - mean) * (point.y() - mean);
//             }
//             double stdDev = qSqrt(sqSum / originalData.size());
            
//             if (stdDev < 1e-9) {
//                 for (const QPointF& point : originalData) {
//                     normalizedData.append(QPointF(point.x(), 0.0));
//                 }
//             } else {
//                 for (const QPointF& point : originalData) {
//                     normalizedData.append(QPointF(point.x(), (point.y() - mean) / stdDev));
//                 }
//             }
//         } 
//         else {
//             error = QString("未知的归一化方法: %1").arg(method);
//             return result;
//         }

//         Curve* normalizedCurve = new Curve(normalizedData, inCurve->name() + QObject::tr(" (归一化)"));
//         normalizedCurve->setSampleId(inCurve->sampleId());
//         result.namedCurves["normalized"].append(normalizedCurve);
//     }
    
//     return result;
// }


#include "Normalization.h"
#include "core/entities/Curve.h"
#include <QtMath>
#include <limits>

QString Normalization::stepName() const {
    return "normalization";
}

QString Normalization::userVisibleName() const {
    return QObject::tr("归一化");
}

QVariantMap Normalization::defaultParameters() const {
    QVariantMap params;
    params["method"] = "absmax";       // 改为 MATLAB absMaxNormalize 风格
    params["rangeMin"] = 0.0;          // 输出范围 a
    params["rangeMax"] = 1.0;          // 输出范围 b
    return params;
}

ProcessingResult Normalization::process(const QList<Curve*> &inputCurves, const QVariantMap &params, QString &error)
{
    ProcessingResult result;

    QString method = params.value("method", "absmax").toString();
    double a = params.value("rangeMin", 0.0).toDouble();
    double b = params.value("rangeMax", 1.0).toDouble();
    if (a >= b) {
        error = QString("rangeMin 必须小于 rangeMax");
        return result;
    }

    for (Curve* inCurve : inputCurves) {
        const QVector<QPointF>& originalData = inCurve->data();
        if (originalData.isEmpty()) continue;

        QVector<QPointF> normalizedData;
        normalizedData.reserve(originalData.size());

        if (method == "absmax") {
            // --- MATLAB absMaxNormalize 实现 ---
            double posMax = -std::numeric_limits<double>::max();
            double absMax = 0.0;

            for (const QPointF& p : originalData) {
                if (p.y() > posMax) posMax = p.y();
                if (qAbs(p.y()) > absMax) absMax = qAbs(p.y());
            }

            if (posMax > 1e-12) {
                for (const QPointF& p : originalData)
                    normalizedData.append(QPointF(p.x(), (p.y() / posMax) * (b - a) + a));
            } else if (absMax > 1e-12) {
                for (const QPointF& p : originalData)
                    normalizedData.append(QPointF(p.x(), (p.y() / absMax) * (b - a) + a));
            } else {
                // 全零向量
                for (const QPointF& p : originalData)
                    normalizedData.append(QPointF(p.x(), a));
            }
        } else {
            error = QString("未知的归一化方法: %1").arg(method);
            return result;
        }

        // 生成归一化 Curve
        Curve* normalizedCurve = new Curve(normalizedData, inCurve->name() + QObject::tr(" (归一化)"));
        normalizedCurve->setSampleId(inCurve->sampleId());
        result.namedCurves["normalized"].append(normalizedCurve);
    }

    return result;
}


