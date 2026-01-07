#include "Clipping.h"
#include "core/entities/Curve.h"

QString Clipping::stepName() const {
    return "clipping";
}
QString Clipping::userVisibleName() const {
    return QObject::tr("数据裁剪");
}
QVariantMap Clipping::defaultParameters() const {
    QVariantMap params;
    params["min_x"] = 0.0;
    params["max_x"] = 1000.0;
    return params;
}

ProcessingResult Clipping::process(const QList<Curve*> &inputCurves, const QVariantMap &params, QString &error)
{
    ProcessingResult result;
    bool okMin, okMax;
    double minX = params.value("min_x", 0.0).toDouble(&okMin);
    double maxX = params.value("max_x", 1000.0).toDouble(&okMax);

    if (!okMin || !okMax || minX >= maxX) {
        error = "裁剪参数无效：X轴范围不正确。";
        return result;
    }

    for (Curve* inCurve : inputCurves) {
        QVector<QPointF> originalData = inCurve->data();
        QVector<QPointF> clippedData;
        clippedData.reserve(originalData.size());

        // 遍历所有数据点，只保留在范围内的点
        for (const QPointF& point : originalData) {
            if (point.x() >= minX && point.x() <= maxX) {
                clippedData.append(point);
            }
        }
        
        // 创建一个新的 Curve 对象来存储裁剪后的结果
        Curve* clippedCurve = new Curve(clippedData, inCurve->name() + QObject::tr(" (裁剪后)"));
        clippedCurve->setSampleId(inCurve->sampleId());
        
        // 将新曲线添加到结果中
        result.namedCurves["clipped"].append(clippedCurve);
    }
    
    return result;
}
