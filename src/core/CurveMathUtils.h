#ifndef CURVEMATHUTILS_H
#define CURVEMATHUTILS_H

#include <QPointF>
#include <QVector>

namespace CurveMathUtils {

// 在按 X 升序排列的折线上对 x 做线性插值/外推（端点外推用首段/末段斜率）
double interpolateYAtX(const QVector<QPointF>& sortedByX, double x);

// 将两条曲线在所有 X 并集上对齐，对 Y 相加（X 来自两条曲线全部采样点的并集，排序去重）
QVector<QPointF> sumCurvesYByUnionX(const QVector<QPointF>& curveA, const QVector<QPointF>& curveB);

} // namespace CurveMathUtils

#endif
