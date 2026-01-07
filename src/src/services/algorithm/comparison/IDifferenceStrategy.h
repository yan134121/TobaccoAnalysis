#ifndef IDIFFERENCESTRATEGY_H
#define IDIFFERENCESTRATEGY_H

#include <QString>
#include <QVariantMap>

// 前置声明 Curve 类，以避免在头文件中包含重量级的 Curve.h
// 这是一种减少编译依赖的好习惯
class Curve;

/**
 * @brief IDifferenceStrategy 接口 (纯虚基类)
 * 
 * 定义了所有“差异度/相似度计算算法”必须遵守的“契约”。
 * SampleComparisonService 将通过这个接口来调用任何具体的算法，
 * 而无需知道其内部是如何实现的。
 */
class IDifferenceStrategy
{
public:
    virtual ~IDifferenceStrategy() {}

    /**
     * @brief 返回该算法在代码中使用的唯一标识符 (ID)。
     * @return 简短、小写的字符串, e.g., "nrmse", "pearson"。
     */
    virtual QString algorithmId() const = 0;

    /**
     * @brief 返回该算法在用户界面上显示的友好名称。
     * @return 可被翻译的字符串, e.g., tr("标准化均方根误差 (NRMSE)")。
     */
    virtual QString userVisibleName() const = 0;

    /**
     * @brief 核心计算方法：计算两条曲线之间的差异度或相似度。
     * @param curveA 第一条曲线。
     * @param curveB 第二条曲线。
     * @param params (可选) 传递给算法的额外参数。
     * @return 一个 double 值。约定：对于“差异度”，值越小越相似；对于“相似度”，值越大越相似。
     */
    virtual double calculateDifference(const Curve& curveA, const Curve& curveB, const QVariantMap& params) = 0;
};

#endif // IDIFFERENCESTRATEGY_H