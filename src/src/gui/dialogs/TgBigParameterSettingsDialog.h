#ifndef TGBIGPARAMETERSETTINGSDIALOG_H
#define TGBIGPARAMETERSETTINGSDIALOG_H

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
// ... (其他需要的控件)




class TgBigParameterSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    // // explicit TgBigParameterSettingsDialog(const ProcessingParameters& initialParams, QWidget *parent = nullptr);
    // // explicit TgBigParameterSettingsDialog(QWidget *parent = nullptr)
    // // : TgBigParameterSettingsDialog(ProcessingParameters(), parent) {}
    // explicit TgBigParameterSettingsDialog(QWidget *parent = nullptr)
    // : TgBigParameterSettingsDialog(ProcessingParameters(), parent) {}

    // 主要构造函数
    explicit TgBigParameterSettingsDialog(const ProcessingParameters& initialParams = ProcessingParameters(), QWidget *parent = nullptr);
    
    // 简化构造函数（委托给主要构造函数）
    explicit TgBigParameterSettingsDialog(QWidget *parent = nullptr)
        : TgBigParameterSettingsDialog(ProcessingParameters(), parent) {}

    ~TgBigParameterSettingsDialog();

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
    QWidget* createDerivativeTab();
    QWidget* createDifferenceTab();
    QWidget* createBadPointTab();
    
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

    // 差异度计算tab
    QCheckBox* m_diffEnabledCheck = nullptr;
    // QComboBox* m_diffMethodCombo = nullptr;

    // 权重
    QDoubleSpinBox* m_weightNRMSE = nullptr;
    // QDoubleSpinBox* m_weightRMSE = nullptr; // 新增
    QDoubleSpinBox* m_weightPearson = nullptr;
    QDoubleSpinBox* m_weightEuclidean = nullptr;

    // 参数
    QComboBox* m_nrmseNormalizeCombo = nullptr;
    // QCheckBox* m_pearsonAbsCheck = nullptr;
    // QCheckBox* m_euclideanRootCheck = nullptr;

    // ROI 设置
    QDoubleSpinBox* m_roiStartSpinBox = nullptr;
    QDoubleSpinBox* m_roiEndSpinBox = nullptr;

    // 坏点修复Tab（对齐 Copy_of_V2.2）
    QCheckBox* m_outlierEnabledCheck = nullptr;       // 是否启用坏点修复
    QDoubleSpinBox* m_invalidTokenFraction = nullptr; // 非数值比例阈值（导入阶段校验）
    QSpinBox* m_outlierWindow = nullptr;              // Hampel窗口（奇数）
    QDoubleSpinBox* m_outlierNSigma = nullptr;        // Hampel阈值倍数
    QDoubleSpinBox* m_jumpDiffThreshold = nullptr;    // 跳变差分阈值
    QDoubleSpinBox* m_globalNSigma = nullptr;         // 全局MAD阈值倍数
    QSpinBox* m_badPointsToShow = nullptr;            // UI显示坏点TopN
    QComboBox* m_fitTypeCombo = nullptr;              // 拟合类型：linear/quad
    QDoubleSpinBox* m_gammaSpin = nullptr;            // 修复段混合权重
    QSpinBox* m_anchorWindow = nullptr;               // 锚点窗口大小
    QSpinBox* m_monoStart = nullptr;                  // 单调区间起点
    QSpinBox* m_monoEnd = nullptr;                    // 单调区间终点
    QDoubleSpinBox* m_edgeBlend = nullptr;            // 边缘平滑强度
    QDoubleSpinBox* m_epsScale = nullptr;             // 平台严格递减斜率尺度
    QDoubleSpinBox* m_slopeThreshold = nullptr;       // 斜率阈值
    QComboBox* m_interpMethodCombo = nullptr;         // 插值方法：pchip/linear


    ProcessingParameters m_currentParams; // 添加这个成员变量来存储当前参数
};

#endif // TGBIGPARAMETERSETTINGSDIALOG_H
