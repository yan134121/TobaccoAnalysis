#ifndef CHROMATOGRAPHPARAMETERSETTINGSDIALOG_H
#define CHROMATOGRAPHPARAMETERSETTINGSDIALOG_H

#include <QDialog>
#include <QVariantMap>
#include <QCheckBox>
#include <QSpinBox>
#include <QDebug>

#include "core/common.h"

// 前置声明，避免在头文件中包含不必要的重量级头文件
class QTabWidget;
class QDialogButtonBox;
class QSpinBox;
class QDoubleSpinBox;
class QComboBox;
// ... (其他需要的控件)



// inline QDebug operator<<(QDebug dbg, const ProcessingParameters &p)
// {
//     QDebugStateSaver saver(dbg); // 保留原有格式
//     dbg.nospace() << "ProcessingParameters("
//                   << "outlierRemovalEnabled=" << p.outlierRemovalEnabled << ", "
//                   << "outlierThreshold=" << p.outlierThreshold << ", "
//                   << "clippingEnabled=" << p.clippingEnabled << ", "
//                   << "clipMinX=" << p.clipMinX << ", "
//                   << "clipMaxX=" << p.clipMaxX << ", "
//                   << "normalizationEnabled=" << p.normalizationEnabled << ", "
//                   << "normalizationMethod=" << p.normalizationMethod << ", "
//                   << "smoothingEnabled=" << p.smoothingEnabled << ", "
//                   << "smoothingMethod=" << p.smoothingMethod << ", "
//                   << "sgWindowSize=" << p.sgWindowSize << ", "
//                   << "sgPolyOrder=" << p.sgPolyOrder << ", "
//                   << "derivativeEnabled=" << p.derivativeEnabled << ", "
//                   << "derivativeMethod=" << p.derivativeMethod << ", "
//                   << "derivSgWindowSize=" << p.derivSgWindowSize << ", "
//                   << "derivSgPolyOrder=" << p.derivSgPolyOrder
//                   << ")";
//     return dbg;
// }



class ChromatographParameterSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    // // explicit ChromatographParameterSettingsDialog(const ProcessingParameters& initialParams, QWidget *parent = nullptr);
    // // explicit ChromatographParameterSettingsDialog(QWidget *parent = nullptr)
    // // : ChromatographParameterSettingsDialog(ProcessingParameters(), parent) {}
    // explicit ChromatographParameterSettingsDialog(QWidget *parent = nullptr)
    // : ChromatographParameterSettingsDialog(ProcessingParameters(), parent) {}

    // 主要构造函数
    explicit ChromatographParameterSettingsDialog(const ProcessingParameters& initialParams = ProcessingParameters(), QWidget *parent = nullptr);
    
    // 简化构造函数（委托给主要构造函数）
    explicit ChromatographParameterSettingsDialog(QWidget *parent = nullptr)
        : ChromatographParameterSettingsDialog(ProcessingParameters(), parent) {}

    ~ChromatographParameterSettingsDialog();

    // 公共接口，用于获取用户最终设置的参数
    ProcessingParameters getParameters() const;

// 【关键】增加一个新的信号
signals:
    // 当用户点击“应用”或“确定”时，发出此信号，并携带最新的参数
    void parametersApplied(const ProcessingParameters& newParams);

private slots:
    // 响应“应用”和“确定”按钮
    void applyChanges();
    void acceptChanges();

    void onButtonClicked(QAbstractButton *button);

private:
    void setupUi();
    // QWidget* createClippingTab();
    // QWidget* createNormalizationTab();
    // QWidget* createSmoothingTab();
    // QWidget* createDerivativeTab();
    QWidget* createDifferenceTab();
    QWidget* createBaselineTab();
    QWidget* createPeakTab();
    QWidget* createAlignmentTab();
    
    // 从传入的参数初始化UI
    void setParameters(const ProcessingParameters& params);

    // --- UI 控件成员变量 ---
    QTabWidget* m_tabWidget;
    QDialogButtonBox* m_buttonBox;
    
    // 裁剪Tab
    QCheckBox* m_clipEnabledCheck;
    QDoubleSpinBox* m_clipMinSpinBox;
    QDoubleSpinBox* m_clipMaxSpinBox;
    
    // 归一化Tab
    QCheckBox* m_normEnabledCheck;
    QComboBox* m_normMethodCombo;
    
    // 平滑Tab
    QCheckBox* m_smoothEnabledCheck = nullptr;   // 未启用页签时置空，防止野指针
    QComboBox* m_smoothMethodCombo = nullptr;    // 未启用页签时置空，防止野指针
    QSpinBox* m_smoothWindowSpinBox = nullptr;   // 未启用页签时置空，防止野指针
    QSpinBox* m_smoothPolyOrderSpinBox = nullptr;// 未启用页签时置空，防止野指针
    
    // 微分Tab
    QCheckBox* m_derivativeEnabledCheck = nullptr; // 未启用页签时置空，防止野指针
    QComboBox* m_derivMethodCombo = nullptr;       // 未启用页签时置空，防止野指针
    QSpinBox* m_derivWindowSpinBox = nullptr;      // 未启用页签时置空，防止野指针
    QSpinBox* m_derivPolyOrderSpinBox = nullptr;   // 未启用页签时置空，防止野指针

    //基线校正
    // === 基线校正参数控件 ===
    QCheckBox* m_baselineEnabledCheck = nullptr;
    QDoubleSpinBox* m_lambdaSpinBox = nullptr;
    QSpinBox* m_orderSpinBox = nullptr;
    QDoubleSpinBox* m_pSpinBox = nullptr;
    QDoubleSpinBox* m_wepSpinBox = nullptr;
    QSpinBox* m_itermaxSpinBox = nullptr;
    QCheckBox* m_baselineOverlayRawCheck = nullptr; // 叠加显示原始数据开关
    QCheckBox* m_baselineDisplayCheck = nullptr;     // 是否绘制基线（原点样式）

    // 峰检测
    QCheckBox* m_peakEnabledCheck = nullptr;
    QDoubleSpinBox* m_peakMinHeightSpin = nullptr;
    QDoubleSpinBox* m_peakMinProminenceSpin = nullptr;
    QSpinBox* m_peakMinDistanceSpin = nullptr;
    QDoubleSpinBox* m_peakSnrSpin = nullptr;

    // 峰对齐
    QCheckBox* m_alignEnabledCheck = nullptr;
    QSpinBox* m_referenceSampleIdSpin = nullptr;
    QSpinBox* m_cowWindowSizeSpin = nullptr;
    QSpinBox* m_cowMaxWarpSpin = nullptr;
    QSpinBox* m_cowSegmentCountSpin = nullptr;
    QDoubleSpinBox* m_cowResampleStepSpin = nullptr;
    // 差异度计算tab
    QCheckBox* m_diffEnabledCheck = nullptr;
    // QComboBox* m_diffMethodCombo = nullptr;

    // 权重
    QDoubleSpinBox* m_weightNRMSE = nullptr;
    QDoubleSpinBox* m_weightPearson = nullptr;
    QDoubleSpinBox* m_weightEuclidean = nullptr;

    // 参数
    QComboBox* m_nrmseNormalizeCombo = nullptr;
    // QCheckBox* m_pearsonAbsCheck = nullptr;
    // QCheckBox* m_euclideanRootCheck = nullptr;


    ProcessingParameters m_currentParams; // 添加这个成员变量来存储当前参数
};

#endif // CHROMATOGRAPHPARAMETERSETTINGSDIALOG_H
