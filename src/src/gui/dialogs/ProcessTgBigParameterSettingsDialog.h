#ifndef PROCESSTGBIGPARAMETERSETTINGSDIALOG_H
#define PROCESSTGBIGPARAMETERSETTINGSDIALOG_H

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
class QComboBox;
class QDoubleSpinBox;
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



class ProcessTgBigParameterSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    // // explicit ProcessTgBigParameterSettingsDialog(const ProcessingParameters& initialParams, QWidget *parent = nullptr);
    // // explicit ProcessTgBigParameterSettingsDialog(QWidget *parent = nullptr)
    // // : ProcessTgBigParameterSettingsDialog(ProcessingParameters(), parent) {}
    // explicit ProcessTgBigParameterSettingsDialog(QWidget *parent = nullptr)
    // : ProcessTgBigParameterSettingsDialog(ProcessingParameters(), parent) {}

    // 主要构造函数
    explicit ProcessTgBigParameterSettingsDialog(const ProcessingParameters& initialParams = ProcessingParameters(), QWidget *parent = nullptr);
    
    // 简化构造函数（委托给主要构造函数）
    explicit ProcessTgBigParameterSettingsDialog(QWidget *parent = nullptr)
        : ProcessTgBigParameterSettingsDialog(ProcessingParameters(), parent) {}

    ~ProcessTgBigParameterSettingsDialog();

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
    QWidget* createClippingTab();
    QWidget* createNormalizationTab();
    QWidget* createSmoothingTab();
    QWidget* createDerivativeTab();    // DTG导数
    QWidget* createDerivative2Tab();   // 一阶导数
    QWidget* createDifferenceTab();
    QWidget* createProcessParamsTab(); // 清理坏点参数
    QWidget* createRawDataTab();       // 原始数据参数
    // QWidget* createBaselineTab();
    
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
    QCheckBox* m_smoothEnabledCheck;
    QComboBox* m_smoothMethodCombo;
    QSpinBox* m_smoothWindowSpinBox;
    QSpinBox* m_smoothPolyOrderSpinBox;
    
    // 微分Tab
    QCheckBox* m_derivativeEnabledCheck;
    QComboBox* m_derivMethodCombo;
    QSpinBox* m_derivWindowSpinBox;
    QSpinBox* m_derivPolyOrderSpinBox;
    QComboBox* m_deriv2MethodCombo = nullptr;
    QSpinBox* m_deriv2WindowSpinBox = nullptr;
    QSpinBox* m_deriv2PolyOrderSpinBox = nullptr;

    // 工序参数Tab（坏点修复/检测等）
    QDoubleSpinBox* m_invalidTokenFraction = nullptr;
    QSpinBox* m_outlierWindow = nullptr;
    QDoubleSpinBox* m_outlierNSigma = nullptr;
    QDoubleSpinBox* m_jumpDiffThreshold = nullptr;
    QSpinBox* m_badPointsToShow = nullptr;
    QComboBox* m_fitTypeCombo = nullptr;
    QDoubleSpinBox* m_gammaSpin = nullptr;
    QSpinBox* m_anchorWindow = nullptr;
    QSpinBox* m_monoStart = nullptr;
    QSpinBox* m_monoEnd = nullptr;
    QDoubleSpinBox* m_edgeBlend = nullptr;
    QDoubleSpinBox* m_epsScale = nullptr;
    QDoubleSpinBox* m_slopeThreshold = nullptr;
    QComboBox* m_interpMethodCombo = nullptr;
    QDoubleSpinBox* m_loessSpan = nullptr;
    QComboBox* m_defaultSmoothDisplayCombo = nullptr;

    // 新增：绘图相关选项（均值曲线、绘图时插值、原始叠加、显示坏点）
    QCheckBox* m_showMeanCurveCheck = nullptr;
    QCheckBox* m_plotInterpolationCheck = nullptr;
    QCheckBox* m_showRawOverlayCheck = nullptr;
    QCheckBox* m_showBadPointsCheck = nullptr;

    //基线校正
    // === 基线校正参数控件 ===
    QCheckBox* m_baselineEnabledCheck = nullptr;
    QDoubleSpinBox* m_lambdaSpinBox = nullptr;
    QSpinBox* m_orderSpinBox = nullptr;
    QDoubleSpinBox* m_pSpinBox = nullptr;
    QSpinBox* m_itermaxSpinBox = nullptr;

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

#endif // PROCESSTGBIGPARAMETERSETTINGSDIALOG_H
