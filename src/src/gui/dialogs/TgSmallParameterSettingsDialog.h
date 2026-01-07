#ifndef TGSMALLPARAMETERSETTINGSDIALOG_H
#define TGSMALLPARAMETERSETTINGSDIALOG_H

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





class TgSmallParameterSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    // // explicit TgSmallParameterSettingsDialog(const ProcessingParameters& initialParams, QWidget *parent = nullptr);
    // // explicit TgSmallParameterSettingsDialog(QWidget *parent = nullptr)
    // // : TgSmallParameterSettingsDialog(ProcessingParameters(), parent) {}
    // explicit TgSmallParameterSettingsDialog(QWidget *parent = nullptr)
    // : TgSmallParameterSettingsDialog(ProcessingParameters(), parent) {}

    // 主要构造函数
    explicit TgSmallParameterSettingsDialog(const ProcessingParameters& initialParams = ProcessingParameters(), QWidget *parent = nullptr);
    
    // 简化构造函数（委托给主要构造函数）
    explicit TgSmallParameterSettingsDialog(QWidget *parent = nullptr)
        : TgSmallParameterSettingsDialog(ProcessingParameters(), parent) {}

    ~TgSmallParameterSettingsDialog();

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
    
    // 从传入的参数初始化UI
    void setParameters(const ProcessingParameters& params);

    // --- UI 控件成员变量 ---
    QTabWidget* m_tabWidget;
    QDialogButtonBox* m_buttonBox;
    
    // // 裁剪Tab
    // QCheckBox* m_clipEnabledCheck;
    // QDoubleSpinBox* m_clipMinSpinBox;
    // QDoubleSpinBox* m_clipMaxSpinBox;
    
    // // 归一化Tab
    // QCheckBox* m_normEnabledCheck;
    // QComboBox* m_normMethodCombo;
    
    // // 平滑Tab
    // QCheckBox* m_smoothEnabledCheck;
    // QComboBox* m_smoothMethodCombo;
    // QSpinBox* m_smoothWindowSpinBox;
    // QSpinBox* m_smoothPolyOrderSpinBox;
    
    // // 微分Tab
    // QCheckBox* m_derivativeEnabledCheck;
    // QComboBox* m_derivMethodCombo;
    // QSpinBox* m_derivWindowSpinBox;
    // QSpinBox* m_derivPolyOrderSpinBox;

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

#endif // TGSMALLPARAMETERSETTINGSDIALOG_H
