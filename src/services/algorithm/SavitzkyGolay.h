#ifndef SAVITZKYGOLAY_H
#define SAVITZKYGOLAY_H

#include "services/algorithm/processing/IProcessingStep.h"

/**
 * @brief The SavitzkyGolay class
 * 
 * 实现了 Savitzky-Golay 滤波算法的策略类。
 * 
 * 这个类是多功能的，通过 "derivative_order" 参数，
 * 它可以同时用于执行“平滑”(order=0) 和“求导”(order>0)。
 * 
 * 它实现了 IProcessing 接口，可以被 DataProcessingService 动态调用。
 */
class SavitzkyGolay : public IProcessingStep
{
public:
    /**
     * @brief 返回该步骤在代码中使用的唯一标识符 (ID)。
     * 注意：我们用一个类实现多种功能，但在 Service 中可以用不同 ID 注册它。
     * 这个 stepName() 只是它的“本名”。
     */
    QString stepName() const override;

    /**
     * @brief 返回该步骤在用户界面上显示的通用友好名称。
     */
    QString userVisibleName() const override;

    /**
     * @brief 返回该算法所有可用参数及其“出厂”默认值。
     */
    QVariantMap defaultParameters() const override;

    /**
     * @brief 执行 Savitzky-Golay 滤波的核心计算逻辑。
     * @param inputCurves 输入的一组曲线。
     * @param params 传递给算法的具体参数，必须包含 "window_size", "poly_order", "derivative_order"。
     * @param error 如果发生错误，通过此引用参数传出错误信息。
     * @return 一个 ProcessingResult 结构体，其 namedCurves 中包含了处理后的结果曲线。
     *         结果曲线的 key 将是 "smoothed", "derivative1", "derivative2" 等。
     */
    ProcessingResult process(const QList<Curve*>& inputCurves, 
                             const QVariantMap& params, 
                             QString& error) override;
};

#endif // SAVITZKYGOLAY_H
