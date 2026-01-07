#ifndef IPROCESSINGSTEP_H
#define IPROCESSINGSTEP_H

#include <QVariantMap>
#include <QList>

// 前置声明我们核心的 Curve 类
class Curve;

/**
 * @brief ProcessingResult 结构体
 * 
 * 用于从一个处理步骤中返回一个或多个命名后的结果曲线。
 * 这对于像 Savitzky-Golay 这样能同时产出“平滑”和“求导”结果的算法非常有用。
 */
struct ProcessingResult
{
    // Key: 结果的名称 (e.g., "smoothed", "derivative1")
    // Value: 该名称对应的曲线列表 (通常只有一条)
    QMap<QString, QList<Curve*>> namedCurves;
};


/**
 * @brief IProcessingStep 接口 (纯虚基类)
 * 
 * 定义了所有数据处理步骤（如平滑、归一化、求导）必须遵守的“契约”。
 * DataProcessingService 将通过这个接口来调用任何具体的算法，而无需知道其内部实现。
 */
class IProcessingStep
{
public:
    virtual ~IProcessingStep() {}

    /**
     * @brief 返回该步骤在代码中使用的唯一标识符 (ID)。
     * @return 简短、小写的字符串, e.g., "smoothing_sg", "normalization_max_min"。
     */
    virtual QString stepName() const = 0;

    /**
     * @brief 返回该步骤在用户界面上显示的友好名称。
     * @return 可被翻译的字符串, e.g., tr("Savitzky-Golay 平滑")。
     */
    virtual QString userVisibleName() const = 0;

    /**
     * @brief 返回该步骤所有可用参数及其“出厂”默认值。
     * @return 一个 QVariantMap，用于在参数设置界面中初始化控件。
     */
    virtual QVariantMap defaultParameters() const = 0;

    /**
     * @brief 执行核心的处理/计算逻辑。
     * @param inputCurves 输入的一组曲线。
     * @param params 传递给该算法的具体参数。
     * @param error 如果发生错误，通过此引用参数传出错误信息。
     * @return 一个 ProcessingResult 结构体，包含了所有处理后的结果曲线。
     */
    virtual ProcessingResult process(const QList<Curve*>& inputCurves, 
                                     const QVariantMap& params, 
                                     QString& error) = 0;
};

#endif // IPROCESSINGSTEP_H