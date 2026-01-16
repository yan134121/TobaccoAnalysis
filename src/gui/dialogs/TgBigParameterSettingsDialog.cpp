#include "TgBigParameterSettingsDialog.h"
#include "Logger.h"

// 包含所有需要的 Qt Widget 头文件
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QTabWidget>
#include <QFormLayout>
#include <QMessageBox>


// TgBigParameterSettingsDialog::TgBigParameterSettingsDialog(const ProcessingParameters& initialParams, QWidget *parent)
//     : QDialog(parent)
// {
// 主要构造函数的实现
TgBigParameterSettingsDialog::TgBigParameterSettingsDialog(const ProcessingParameters& initialParams, 
                                                           const QString& dataTypeForClipping,
                                                           QWidget *parent)
    : QDialog(parent)
    , m_currentParams(initialParams)
    , m_dataTypeForClipping(dataTypeForClipping)
{
    setupUi();
    setParameters(initialParams);

    // (可选) 设置为非模态，并提供 Apply 按钮
    // setModal(false); 
}

TgBigParameterSettingsDialog::~TgBigParameterSettingsDialog()
{
    // C++11 之后，如果控件有父对象，这里不需要手动 delete UI 成员
}

void TgBigParameterSettingsDialog::setupUi()
{
    setWindowTitle(tr("大热重处理参数设置"));
    setMinimumWidth(400);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    m_tabWidget = new QTabWidget(this);
    m_tabWidget->addTab(createBadPointTab(), tr("坏点修复"));
    m_tabWidget->addTab(createClippingTab(), tr("裁剪"));
    
    m_tabWidget->addTab(createNormalizationTab(), tr("归一化"));
    m_tabWidget->addTab(createSmoothingTab(), tr("平滑"));
    m_tabWidget->addTab(createDerivativeTab(), tr("微分"));
    m_tabWidget->addTab(createDifferenceTab(), tr("差异度")); // createDifferenceTab();

    mainLayout->addWidget(m_tabWidget);

    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply, this);
    // 修改按钮文本
    m_buttonBox->button(QDialogButtonBox::Ok)->setText(tr("确认"));
    m_buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("取消"));
    m_buttonBox->button(QDialogButtonBox::Apply)->setText(tr("应用"));
    mainLayout->addWidget(m_buttonBox);

    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &TgBigParameterSettingsDialog::acceptChanges);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, &TgBigParameterSettingsDialog::applyChanges);
    connect(m_buttonBox, &QDialogButtonBox::clicked, this, &TgBigParameterSettingsDialog::onButtonClicked);

    // // === 信号连接：权重调整时自动校正 ===
    // connect(m_weightNRMSE, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &TgBigParameterSettingsDialog::onWeightChanged);
    // connect(m_weightPearson, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &TgBigParameterSettingsDialog::onWeightChanged);
    // connect(m_weightEuclidean, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &TgBigParameterSettingsDialog::onWeightChanged);


    setLayout(mainLayout);
}

QWidget* TgBigParameterSettingsDialog::createClippingTab()
{
    QWidget* widget = new QWidget;
    QFormLayout* layout = new QFormLayout(widget);
    
    m_clipEnabledCheck = new QCheckBox(tr("启用裁剪"));
    m_clipMinSpinBox = new QDoubleSpinBox;
    m_clipMaxSpinBox = new QDoubleSpinBox;

    m_clipMinSpinBox->setRange(0, 10000);
    m_clipMaxSpinBox->setRange(0, 10000);

    layout->addRow(m_clipEnabledCheck);
    layout->addRow(tr("X轴最小值:"), m_clipMinSpinBox);
    layout->addRow(tr("X轴最大值:"), m_clipMaxSpinBox);
    
    return widget;
}

QWidget* TgBigParameterSettingsDialog::createNormalizationTab()
{
    QWidget* widget = new QWidget;
    QFormLayout* layout = new QFormLayout(widget);

    m_normEnabledCheck = new QCheckBox(tr("启用归一化"));
    m_normMethodCombo = new QComboBox;
    // 【更新】新增“绝对最大值归一化 (absmax)”以匹配 Normalization.cpp 的实现
    m_normMethodCombo->addItem(tr("按绝对最大值归一化"), "absmax");
    // 保留旧项（不再生效，仅为兼容界面）
    m_normMethodCombo->addItem(tr("最大-最小值归一化"), "max_min");
    m_normMethodCombo->addItem(tr("Z-score 标准化"), "z_score");
    
    layout->addRow(m_normEnabledCheck);
    layout->addRow(tr("方法:"), m_normMethodCombo);
    
    return widget;
}


// 坏点修复参数设置页，对齐 Copy_of_V2.2 的 getColumnC 参数
QWidget* TgBigParameterSettingsDialog::createBadPointTab()
{
    QWidget* widget = new QWidget;
    QFormLayout* layout = new QFormLayout(widget);

    // 是否启用坏点修复
    m_outlierEnabledCheck = new QCheckBox(tr("启用坏点修复"));
    m_outlierEnabledCheck->setChecked(true);
    layout->addRow(m_outlierEnabledCheck);

    // 非数值比例阈值（导入阶段校验，0..1）
    m_invalidTokenFraction = new QDoubleSpinBox;
    m_invalidTokenFraction->setRange(0.0, 1.0);
    m_invalidTokenFraction->setSingleStep(0.05);
    layout->addRow(tr("最大非数值比例:"), m_invalidTokenFraction);

    // Hampel局部异常窗口（奇数）
    m_outlierWindow = new QSpinBox;
    m_outlierWindow->setRange(3, 999);
    m_outlierWindow->setSingleStep(2); // 仅奇数步进
    layout->addRow(tr("异常检测窗口(奇数):"), m_outlierWindow);

    // Hampel阈值倍数
    m_outlierNSigma = new QDoubleSpinBox;
    m_outlierNSigma->setRange(0.1, 20.0);
    m_outlierNSigma->setSingleStep(0.1);
    layout->addRow(tr("窗口内NSigma阈值:"), m_outlierNSigma);

    // 跳变异常差分阈值
    m_jumpDiffThreshold = new QDoubleSpinBox;
    m_jumpDiffThreshold->setRange(0.0, 1000.0);
    m_jumpDiffThreshold->setSingleStep(0.1);
    layout->addRow(tr("跳变差分阈值:"), m_jumpDiffThreshold);

    // 全局MAD阈值
    m_globalNSigma = new QDoubleSpinBox;
    m_globalNSigma->setRange(0.1, 20.0);
    m_globalNSigma->setSingleStep(0.1);
    layout->addRow(tr("全局MAD阈值倍数:"), m_globalNSigma);

    // 显示坏点TopN（0=全部）
    m_badPointsToShow = new QSpinBox;
    m_badPointsToShow->setRange(0, 100000);
    layout->addRow(tr("坏点显示TopN:"), m_badPointsToShow);

    // 拟合类型
    m_fitTypeCombo = new QComboBox;
    m_fitTypeCombo->addItem(tr("线性拟合"), "linear");
    m_fitTypeCombo->addItem(tr("二次拟合"), "quad");
    layout->addRow(tr("坏段拟合类型:"), m_fitTypeCombo);

    // 修复混合权重
    m_gammaSpin = new QDoubleSpinBox;
    m_gammaSpin->setRange(0.0, 1.0);
    m_gammaSpin->setSingleStep(0.05);
    layout->addRow(tr("修复混合权重(0..1):"), m_gammaSpin);

    // 锚点窗口
    m_anchorWindow = new QSpinBox;
    m_anchorWindow->setRange(1, 9999);
    layout->addRow(tr("锚点窗口大小:"), m_anchorWindow);

    // 单调区间
    m_monoStart = new QSpinBox;
    m_monoStart->setRange(0, 100000);
    m_monoEnd = new QSpinBox;
    m_monoEnd->setRange(0, 100000);
    layout->addRow(tr("单调区间起点(索引):"), m_monoStart);
    layout->addRow(tr("单调区间终点(索引):"), m_monoEnd);

    // 边缘平滑强度
    m_edgeBlend = new QDoubleSpinBox;
    m_edgeBlend->setRange(0.0, 1.0);
    m_edgeBlend->setSingleStep(0.05);
    layout->addRow(tr("边缘平滑强度(0..1):"), m_edgeBlend);

    // 平台严格递减斜率尺度
    m_epsScale = new QDoubleSpinBox;
    m_epsScale->setRange(1e-12, 1.0);
    m_epsScale->setDecimals(12);
    m_epsScale->setSingleStep(1e-9);
    layout->addRow(tr("严格递减斜率尺度(eps):"), m_epsScale);

    // 斜率阈值
    m_slopeThreshold = new QDoubleSpinBox;
    m_slopeThreshold->setRange(1e-12, 1.0);
    m_slopeThreshold->setDecimals(12);
    m_slopeThreshold->setSingleStep(1e-9);
    layout->addRow(tr("斜率阈值:"), m_slopeThreshold);

    // 插值方法
    m_interpMethodCombo = new QComboBox;
    m_interpMethodCombo->addItem(tr("PCHIP 保形三次插值"), "pchip");
    m_interpMethodCombo->addItem(tr("线性插值"), "linear");
    layout->addRow(tr("插值方法:"), m_interpMethodCombo);

    return widget;
}

QWidget* TgBigParameterSettingsDialog::createSmoothingTab()
{
    QWidget* widget = new QWidget;
    QFormLayout* layout = new QFormLayout(widget);

    m_smoothEnabledCheck = new QCheckBox(tr("启用平滑"));
    m_smoothMethodCombo = new QComboBox;
    m_smoothMethodCombo->addItem(tr("Loess 平滑"), "loess"); // 新增 Loess 平滑选项
    m_smoothMethodCombo->addItem(tr("Savitzky-Golay"), "savitzky_golay");
    
    m_smoothWindowSpinBox = new QSpinBox;
    m_smoothWindowSpinBox->setRange(3, 99);
    m_smoothWindowSpinBox->setSingleStep(2); // SG窗口通常是奇数
    
    m_smoothPolyOrderSpinBox = new QSpinBox;
    m_smoothPolyOrderSpinBox->setRange(2, 10);
    
    layout->addRow(m_smoothEnabledCheck);
    layout->addRow(tr("算法:"), m_smoothMethodCombo);
    layout->addRow(tr("窗口大小:"), m_smoothWindowSpinBox);
    layout->addRow(tr("多项式阶数:"), m_smoothPolyOrderSpinBox);
    
    return widget;
}



// 差异度计算
QWidget* TgBigParameterSettingsDialog::createDifferenceTab()
{
    QWidget* widget = new QWidget;
    QVBoxLayout* mainLayout = new QVBoxLayout(widget);
    

    // === 启用开关 ===
    m_diffEnabledCheck = new QCheckBox(tr("启用差异度计算"));
    mainLayout->addWidget(m_diffEnabledCheck);
    m_diffEnabledCheck->setChecked(true);   //  默认启用
    // layout->addRow(m_diffEnabledCheck);

    // // === 1. NRMSE 算法区域 ===
    // QGroupBox* nrmseGroup = new QGroupBox(tr("NRMSE（标准化均方根误差）"));
    // QFormLayout* nrmseLayout = new QFormLayout(nrmseGroup);

    // m_nrmseNormalizeCombo = new QComboBox;
    // m_nrmseNormalizeCombo->addItem(tr("按最大值归一化"), "max");
    // m_nrmseNormalizeCombo->addItem(tr("按标准差归一化"), "std");
    // nrmseLayout->addRow(tr("归一化方式:"), m_nrmseNormalizeCombo);

    // mainLayout->addWidget(nrmseGroup);

    // // === 2. Pearson 算法区域 ===
    // QGroupBox* pearsonGroup = new QGroupBox(tr("Pearson（皮尔逊相关系数）"));
    // QFormLayout* pearsonLayout = new QFormLayout(pearsonGroup);

    // m_pearsonAbsCheck = new QCheckBox(tr("取绝对值 (|r|)"));
    // m_pearsonAbsCheck->setChecked(true);
    // pearsonLayout->addRow(m_pearsonAbsCheck);

    // mainLayout->addWidget(pearsonGroup);

    // // === 3. Euclidean 算法区域 ===
    // QGroupBox* euclideanGroup = new QGroupBox(tr("Euclidean（欧氏距离）"));
    // QFormLayout* euclideanLayout = new QFormLayout(euclideanGroup);

    // m_euclideanRootCheck = new QCheckBox(tr("取平方根 (√ΣΔ²)"));
    // m_euclideanRootCheck->setChecked(true);
    // euclideanLayout->addRow(m_euclideanRootCheck);

    // mainLayout->addWidget(euclideanGroup);

    // === 1. NRMSE 算法区域 ===
    QGroupBox* nrmseGroup = new QGroupBox(tr("NRMSE（标准化均方根误差，最大-最小值归一化）"));
    QVBoxLayout* nrmseLayout = new QVBoxLayout(nrmseGroup);
    // QLabel* nrmseInfo = new QLabel(tr("无参数设置，默认使用最大值-最小值映射"));
    // nrmseLayout->addWidget(nrmseInfo);
    mainLayout->addWidget(nrmseGroup);

    // === 2. Pearson 算法区域 ===
    QGroupBox* pearsonGroup = new QGroupBox(tr("Pearson（皮尔逊相关系数）"));
    QFormLayout* pearsonLayout = new QFormLayout(pearsonGroup);

    // m_pearsonAbsCheck = new QCheckBox(tr("取绝对值 (|r|)"));
    // m_pearsonAbsCheck->setChecked(true);
    // pearsonLayout->addRow(m_pearsonAbsCheck);

    mainLayout->addWidget(pearsonGroup);

    // // === 3. RMSE 算法区域 ===
    // QGroupBox* rmseGroup = new QGroupBox(tr("RMSE（均方根误差）"));
    // QVBoxLayout* rmseLayout = new QVBoxLayout(rmseGroup);
    // // QLabel* rmseInfo = new QLabel(tr("无参数设置"));
    // // rmseLayout->addWidget(rmseInfo);
    // mainLayout->addWidget(rmseGroup);

    // === 4. Euclidean 算法区域 ===
    QGroupBox* euclideanGroup = new QGroupBox(tr("Euclidean（欧氏距离，取平方根）"));
    QFormLayout* euclideanLayout = new QFormLayout(euclideanGroup);

    // m_euclideanRootCheck = new QCheckBox(tr("取平方根 (√ΣΔ²)"));
    // m_euclideanRootCheck->setChecked(true);
    // euclideanLayout->addRow(m_euclideanRootCheck);

    mainLayout->addWidget(euclideanGroup);

    // === 5. ROI 设置 ===
    QGroupBox* roiGroup = new QGroupBox(tr("对比区域 (ROI) 设置"));
    QFormLayout* roiLayout = new QFormLayout(roiGroup);
    
    m_roiStartSpinBox = new QDoubleSpinBox;
    m_roiStartSpinBox->setRange(-1.0, 10000.0);
    m_roiStartSpinBox->setSingleStep(10.0);
    m_roiStartSpinBox->setSpecialValueText(tr("全范围")); // -1 表示全范围
    m_roiStartSpinBox->setValue(-1.0);
    
    m_roiEndSpinBox = new QDoubleSpinBox;
    m_roiEndSpinBox->setRange(-1.0, 10000.0);
    m_roiEndSpinBox->setSingleStep(10.0);
    m_roiEndSpinBox->setSpecialValueText(tr("全范围")); // -1 表示全范围
    m_roiEndSpinBox->setValue(-1.0);

    roiLayout->addRow(tr("起始温度/时间:"), m_roiStartSpinBox);
    roiLayout->addRow(tr("结束温度/时间:"), m_roiEndSpinBox);
    mainLayout->addWidget(roiGroup);

    // === 6. 权重设置 ===
    QGroupBox* weightGroup = new QGroupBox(tr("算法权重设置（总和为 1）"));
    QFormLayout* weightLayout = new QFormLayout(weightGroup);

    m_weightNRMSE = new QDoubleSpinBox;
    m_weightNRMSE->setRange(0.0, 1.0);
    m_weightNRMSE->setDecimals(3);
    m_weightNRMSE->setSingleStep(0.05);
    m_weightNRMSE->setValue(0.333);
    weightLayout->addRow(tr("NRMSE 权重:"), m_weightNRMSE);

    m_weightPearson = new QDoubleSpinBox;
    m_weightPearson->setRange(0.0, 1.0);
    m_weightPearson->setDecimals(3);
    m_weightPearson->setSingleStep(0.05);
    m_weightPearson->setValue(0.333);
    weightLayout->addRow(tr("Pearson 权重:"), m_weightPearson);

    // m_weightRMSE = new QDoubleSpinBox;
    // m_weightRMSE->setRange(0.0, 1.0);
    // m_weightRMSE->setDecimals(3);
    // m_weightRMSE->setSingleStep(0.05);
    // m_weightRMSE->setValue(0.0);
    // weightLayout->addRow(tr("RMSE 权重:"), m_weightRMSE);

    m_weightEuclidean = new QDoubleSpinBox;
    m_weightEuclidean->setRange(0.0, 1.0);
    m_weightEuclidean->setDecimals(3);
    m_weightEuclidean->setSingleStep(0.05);
    m_weightEuclidean->setValue(0.333);
    weightLayout->addRow(tr("Euclidean 权重:"), m_weightEuclidean);

    // 显示权重总和提示标签
    QLabel* totalLabel = new QLabel;
    totalLabel->setAlignment(Qt::AlignRight);
    weightLayout->addRow(tr("总权重:"), totalLabel);

    mainLayout->addWidget(weightGroup);
    mainLayout->addStretch();

    // === 信号槽连接 ===
    auto updateTotal = [=]() {
        double sum = m_weightNRMSE->value() + m_weightPearson->value() + m_weightEuclidean->value();
        QString text = QString::number(sum, 'f', 3);
        if (qFuzzyCompare(sum + 1.0, 2.0)) { // sum == 1.0
            totalLabel->setText(QString("<font color='green'>%1 </font>").arg(text));
        } else {
            // totalLabel->setText(QString("<font color='red'>%1 ⚠️ (需等于1)</font>").arg(text));
            totalLabel->setText(QString("<font color='red'>%1 ").arg(text));
        }
    };

    connect(m_weightNRMSE, QOverload<double>::of(&QDoubleSpinBox::valueChanged), widget, updateTotal);
    connect(m_weightPearson, QOverload<double>::of(&QDoubleSpinBox::valueChanged), widget, updateTotal);
    // connect(m_weightRMSE, QOverload<double>::of(&QDoubleSpinBox::valueChanged), widget, updateTotal);
    connect(m_weightEuclidean, QOverload<double>::of(&QDoubleSpinBox::valueChanged), widget, updateTotal);

    // 初始化显示一次
    updateTotal();

    return widget;
}


QWidget* TgBigParameterSettingsDialog::createDerivativeTab()
{
    // 为微分创建控件 (与平滑类似)
    QWidget* widget = new QWidget;
    QFormLayout* layout = new QFormLayout(widget);

    m_derivativeEnabledCheck = new QCheckBox(tr("启用微分"));
    m_derivMethodCombo = new QComboBox;
    // m_derivMethodCombo->addItem(tr("Savitzky-Golay"), "savitzky_golay");
    m_derivMethodCombo->addItem(tr("derivative_sg"), "derivative_sg");
    
    m_derivWindowSpinBox = new QSpinBox;
    m_derivPolyOrderSpinBox = new QSpinBox;
    
    layout->addRow(m_derivativeEnabledCheck);
    layout->addRow(tr("算法:"), m_derivMethodCombo);
    layout->addRow(tr("窗口大小:"), m_derivWindowSpinBox);
    layout->addRow(tr("多项式阶数:"), m_derivPolyOrderSpinBox);
    
    return widget;
}


void TgBigParameterSettingsDialog::setParameters(const ProcessingParameters &params)
{
    // 裁剪
    // 使用“大热重”独立裁剪参数
    // 根据数据类型使用对应的独立裁剪参数
    if (m_dataTypeForClipping == QStringLiteral("TG_SMALL_RAW")) {
        // 使用"小热重（原始数据）"独立裁剪参数
        m_clipEnabledCheck->setChecked(params.clippingEnabled_TgSmallRaw);
        m_clipMinSpinBox->setValue(params.clipMinX_TgSmallRaw);
        m_clipMaxSpinBox->setValue(params.clipMaxX_TgSmallRaw);
    } else {
        // 默认使用"大热重"独立裁剪参数
        m_clipEnabledCheck->setChecked(params.clippingEnabled_TgBig);
        m_clipMinSpinBox->setValue(params.clipMinX_TgBig);
        m_clipMaxSpinBox->setValue(params.clipMaxX_TgBig);
    }

    // 归一化
    m_normEnabledCheck->setChecked(params.normalizationEnabled);
    int normIndex = m_normMethodCombo->findData(params.normalizationMethod);
    if (normIndex != -1) m_normMethodCombo->setCurrentIndex(normIndex);

    // 平滑
    m_smoothEnabledCheck->setChecked(params.smoothingEnabled);
    // 根据参数选择默认平滑算法（默认 loess）
    int smIndex = m_smoothMethodCombo->findData(params.smoothingMethod);
    if (smIndex != -1) m_smoothMethodCombo->setCurrentIndex(smIndex);
    m_smoothWindowSpinBox->setValue(params.sgWindowSize);
    m_smoothPolyOrderSpinBox->setValue(params.sgPolyOrder);

    // 微分
    m_derivativeEnabledCheck->setChecked(params.derivativeEnabled);
    m_derivWindowSpinBox->setValue(params.derivSgWindowSize);
    m_derivPolyOrderSpinBox->setValue(params.derivSgPolyOrder);

    // 权重
    m_weightNRMSE->setValue(params.weightNRMSE);
    m_weightPearson->setValue(params.weightPearson);
    m_weightEuclidean->setValue(params.weightEuclidean);
    // 当前 ProcessingParameters 未包含 RMSE 权重字段，保持控件显示但不从结构体回填

    // 当前 ProcessingParameters 未包含比较 ROI 字段，保持控件默认值

    // 坏点修复
    if (m_outlierEnabledCheck) m_outlierEnabledCheck->setChecked(params.outlierRemovalEnabled);
    if (m_invalidTokenFraction) m_invalidTokenFraction->setValue(params.invalidTokenFraction);
    if (m_outlierWindow) m_outlierWindow->setValue(params.outlierWindow);
    if (m_outlierNSigma) m_outlierNSigma->setValue(params.outlierNSigma);
    if (m_jumpDiffThreshold) m_jumpDiffThreshold->setValue(params.jumpDiffThreshold);
    if (m_globalNSigma) m_globalNSigma->setValue(params.globalNSigma);
    if (m_badPointsToShow) m_badPointsToShow->setValue(params.badPointsToShow);
    if (m_fitTypeCombo) {
        int idx = m_fitTypeCombo->findData(params.fitType);
        if (idx != -1) m_fitTypeCombo->setCurrentIndex(idx);
    }
    if (m_gammaSpin) m_gammaSpin->setValue(params.gamma);
    if (m_anchorWindow) m_anchorWindow->setValue(params.anchorWindow);
    if (m_monoStart) m_monoStart->setValue(params.monoStart);
    if (m_monoEnd) m_monoEnd->setValue(params.monoEnd);
    if (m_edgeBlend) m_edgeBlend->setValue(params.edgeBlend);
    if (m_epsScale) m_epsScale->setValue(params.epsScale);
    if (m_slopeThreshold) m_slopeThreshold->setValue(params.slopeThreshold);
    if (m_interpMethodCombo) {
        int idx = m_interpMethodCombo->findData(params.interpMethod);
        if (idx != -1) m_interpMethodCombo->setCurrentIndex(idx);
    }
}

ProcessingParameters TgBigParameterSettingsDialog::getParameters() const
{
    ProcessingParameters params;

    // 从UI控件收集数据并填充结构体
    // 写入“大热重”独立裁剪参数
    // 根据数据类型写入对应的独立裁剪参数
    if (m_dataTypeForClipping == QStringLiteral("TG_SMALL_RAW")) {
        // 写入"小热重（原始数据）"独立裁剪参数
        params.clippingEnabled_TgSmallRaw = m_clipEnabledCheck->isChecked();
        params.clipMinX_TgSmallRaw = m_clipMinSpinBox->value();
        params.clipMaxX_TgSmallRaw = m_clipMaxSpinBox->value();
    } else {
        // 默认写入"大热重"独立裁剪参数
        params.clippingEnabled_TgBig = m_clipEnabledCheck->isChecked();
        params.clipMinX_TgBig = m_clipMinSpinBox->value();
        params.clipMaxX_TgBig = m_clipMaxSpinBox->value();
    }
    
    params.normalizationEnabled = m_normEnabledCheck->isChecked();
    params.normalizationMethod = m_normMethodCombo->currentData().toString();

    params.smoothingEnabled = m_smoothEnabledCheck->isChecked();
    params.smoothingMethod = m_smoothMethodCombo->currentData().toString();
    params.sgWindowSize = m_smoothWindowSpinBox->value();
    params.sgPolyOrder = m_smoothPolyOrderSpinBox->value();

    params.derivativeEnabled = m_derivativeEnabledCheck->isChecked();
    params.derivativeMethod = m_derivMethodCombo->currentData().toString();
    params.derivSgWindowSize = m_derivWindowSpinBox->value();
    params.derivSgPolyOrder = m_derivPolyOrderSpinBox->value();

    params.weightNRMSE = m_weightNRMSE->value();
    params.weightPearson = m_weightPearson->value();
    params.weightEuclidean = m_weightEuclidean->value();
    // 当前 ProcessingParameters 未包含 RMSE 权重字段，忽略写回

    // 当前 ProcessingParameters 未包含比较 ROI 字段，忽略写回

    // 坏点修复
    if (m_outlierEnabledCheck) params.outlierRemovalEnabled = m_outlierEnabledCheck->isChecked();
    if (m_invalidTokenFraction) params.invalidTokenFraction = m_invalidTokenFraction->value();
    if (m_outlierWindow) params.outlierWindow = m_outlierWindow->value();
    if (m_outlierNSigma) params.outlierNSigma = m_outlierNSigma->value();
    if (m_jumpDiffThreshold) params.jumpDiffThreshold = m_jumpDiffThreshold->value();
    if (m_globalNSigma) params.globalNSigma = m_globalNSigma->value();
    if (m_badPointsToShow) params.badPointsToShow = m_badPointsToShow->value();
    if (m_fitTypeCombo) params.fitType = m_fitTypeCombo->currentData().toString();
    if (m_gammaSpin) params.gamma = m_gammaSpin->value();
    if (m_anchorWindow) params.anchorWindow = m_anchorWindow->value();
    if (m_monoStart) params.monoStart = m_monoStart->value();
    if (m_monoEnd) params.monoEnd = m_monoEnd->value();
    if (m_edgeBlend) params.edgeBlend = m_edgeBlend->value();
    if (m_epsScale) params.epsScale = m_epsScale->value();
    if (m_slopeThreshold) params.slopeThreshold = m_slopeThreshold->value();
    if (m_interpMethodCombo) params.interpMethod = m_interpMethodCombo->currentData().toString();

    return params;
}


void TgBigParameterSettingsDialog::applyChanges()
{
    // 如果设计为非模态实时预览，这个槽函数会发出信号
    // emit parametersChanged(getParameters());
    
    // 对于模态对话框，"Apply"通常不是必需的，但可以保留
    // 这里我们先不做任何事
    DEBUG_LOG << "Apply button clicked. In a non-modal dialog, this would emit a signal.";
}

void TgBigParameterSettingsDialog::acceptChanges()
{
    // 在调用 accept() 之前，可以进行参数的最终校验
    if (m_smoothWindowSpinBox->value() <= m_smoothPolyOrderSpinBox->value()) {
        QMessageBox::warning(this, tr("参数错误"), tr("平滑的窗口大小必须大于多项式阶数。"));
        return; // 阻止对话框关闭
    }
    
    // 调用基类的 accept() 方法，它会关闭对话框并返回 QDialog::Accepted
    QDialog::accept();
}

// 【关键】实现统一的按钮点击处理槽函数
void TgBigParameterSettingsDialog::onButtonClicked(QAbstractButton *button)
{
    // 获取被点击按钮在 buttonBox 中的角色
    QDialogButtonBox::ButtonRole role = m_buttonBox->buttonRole(button);
    
    // a. 首先，进行参数校验
    if (role == QDialogButtonBox::AcceptRole || role == QDialogButtonBox::ApplyRole) {
        if (m_smoothWindowSpinBox->value() <= m_smoothPolyOrderSpinBox->value()) {
            QMessageBox::warning(this, tr("参数错误"), tr("平滑的窗口大小必须大于多项式阶数。"));
            return; // 校验失败，中断操作，对话框不关闭
        }
    }
    
    // b. 根据按钮角色执行不同操作
    if (role == QDialogButtonBox::ApplyRole) {
        // --- Apply 按钮 ---
        // 1. 发出信号，通知外部世界应用新参数
        DEBUG_LOG << "Apply button clicked.";
        emit parametersApplied(getParameters());
        DEBUG_LOG << "Apply button clicked.";
        // 2. 对话框不关闭
        
    } else if (role == QDialogButtonBox::AcceptRole) {
        // --- OK (确定) 按钮 ---
        // 1. 发出信号，通知外部世界应用新参数
        emit parametersApplied(getParameters());
        // 2. 调用 accept() 关闭对话框
        QDialog::accept();
        
    } else if (role == QDialogButtonBox::RejectRole) {
        // --- Cancel (取消) 按钮 ---
        // 直接调用 reject() 关闭对话框，不发出任何信号
        QDialog::reject();
    }
}
