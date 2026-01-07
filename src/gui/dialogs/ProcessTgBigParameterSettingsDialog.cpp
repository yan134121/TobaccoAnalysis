#include "ProcessTgBigParameterSettingsDialog.h"
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


// ProcessTgBigParameterSettingsDialog::ProcessTgBigParameterSettingsDialog(const ProcessingParameters& initialParams, QWidget *parent)
//     : QDialog(parent)
// {
// 主要构造函数的实现
ProcessTgBigParameterSettingsDialog::ProcessTgBigParameterSettingsDialog(const ProcessingParameters& initialParams, QWidget *parent)
    : QDialog(parent)
    , m_currentParams(initialParams)
{
    setupUi();
    setParameters(initialParams);

    // (可选) 设置为非模态，并提供 Apply 按钮
    // setModal(false); 
}

ProcessTgBigParameterSettingsDialog::~ProcessTgBigParameterSettingsDialog()
{
    // C++11 之后，如果控件有父对象，这里不需要手动 delete UI 成员
}

void ProcessTgBigParameterSettingsDialog::setupUi()
{
    setWindowTitle(tr("色谱处理参数设置"));
    setMinimumWidth(400);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    m_tabWidget = new QTabWidget(this);
    m_tabWidget->addTab(createRawDataTab(), tr("原始数据"));
    m_tabWidget->addTab(createProcessParamsTab(), tr("清理坏点"));
    m_tabWidget->addTab(createClippingTab(), tr("裁剪"));
    m_tabWidget->addTab(createNormalizationTab(), tr("归一化"));
    m_tabWidget->addTab(createSmoothingTab(), tr("平滑处理"));
    m_tabWidget->addTab(createDerivativeTab(), tr("DTG导数"));
    m_tabWidget->addTab(createDerivative2Tab(), tr("一阶导数"));
    // m_tabWidget->addTab(createBaselineTab(), tr("基线校正"));
    m_tabWidget->addTab(createDifferenceTab(), tr("差异度")); // createDifferenceTab();

    mainLayout->addWidget(m_tabWidget);

    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply, this);
    // 修改按钮文本
    m_buttonBox->button(QDialogButtonBox::Ok)->setText(tr("确认"));
    m_buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("取消"));
    m_buttonBox->button(QDialogButtonBox::Apply)->setText(tr("应用"));
    mainLayout->addWidget(m_buttonBox);

    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &ProcessTgBigParameterSettingsDialog::acceptChanges);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, &ProcessTgBigParameterSettingsDialog::applyChanges);
    connect(m_buttonBox, &QDialogButtonBox::clicked, this, &ProcessTgBigParameterSettingsDialog::onButtonClicked);

    // // === 信号连接：权重调整时自动校正 ===
    // connect(m_weightNRMSE, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ProcessTgBigParameterSettingsDialog::onWeightChanged);
    // connect(m_weightPearson, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ProcessTgBigParameterSettingsDialog::onWeightChanged);
    // connect(m_weightEuclidean, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ProcessTgBigParameterSettingsDialog::onWeightChanged);


    setLayout(mainLayout);
}

QWidget* ProcessTgBigParameterSettingsDialog::createClippingTab()
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

QWidget* ProcessTgBigParameterSettingsDialog::createNormalizationTab()
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


QWidget* ProcessTgBigParameterSettingsDialog::createSmoothingTab()
{
    QWidget* widget = new QWidget;
    QFormLayout* layout = new QFormLayout(widget);

    m_smoothEnabledCheck = new QCheckBox(tr("启用平滑"));
    m_smoothMethodCombo = new QComboBox;
    m_smoothMethodCombo->addItem(tr("Savitzky-Golay"), "savitzky_golay");
    m_smoothMethodCombo->addItem(tr("Loess 平滑"), "loess");
    
    m_smoothWindowSpinBox = new QSpinBox;
    m_smoothWindowSpinBox->setRange(3, 99);
    m_smoothWindowSpinBox->setSingleStep(2); // SG窗口通常是奇数
    
    m_smoothPolyOrderSpinBox = new QSpinBox;
    m_smoothPolyOrderSpinBox->setRange(2, 10);
    
    layout->addRow(m_smoothEnabledCheck);
    layout->addRow(tr("算法:"), m_smoothMethodCombo);
    layout->addRow(tr("窗口大小:"), m_smoothWindowSpinBox);
    layout->addRow(tr("多项式阶数:"), m_smoothPolyOrderSpinBox);
    layout->addRow(tr("Loess 窗口比例:"), (m_loessSpan = new QDoubleSpinBox));
    m_loessSpan->setRange(0.01, 0.99); m_loessSpan->setDecimals(2); m_loessSpan->setValue(0.10);
    
    return widget;
}


// QWidget* ProcessTgBigParameterSettingsDialog::createBaselineTab()
// {
//     QWidget* widget = new QWidget;
//     QFormLayout* layout = new QFormLayout(widget);

//     m_baselineEnabledCheck = new QCheckBox(tr("启用基线校正"), widget);

//     m_lambdaSpinBox = new QDoubleSpinBox(widget);
//     m_lambdaSpinBox->setDecimals(1);
//     m_lambdaSpinBox->setRange(1, 1e8);
//     m_lambdaSpinBox->setValue(1e5);
//     m_lambdaSpinBox->setSingleStep(1000);

//     m_orderSpinBox = new QSpinBox(widget);
//     m_orderSpinBox->setRange(1, 5);
//     m_orderSpinBox->setValue(2);

//     m_pSpinBox = new QDoubleSpinBox(widget);
//     m_pSpinBox->setRange(0.0, 1.0);
//     m_pSpinBox->setSingleStep(0.01);
//     m_pSpinBox->setDecimals(3);
//     m_pSpinBox->setValue(0.05);

//     m_itermaxSpinBox = new QSpinBox(widget);
//     m_itermaxSpinBox->setRange(1, 200);
//     m_itermaxSpinBox->setValue(50);

//     layout->addRow(m_baselineEnabledCheck);
//     layout->addRow(tr("λ (平滑系数):"), m_lambdaSpinBox);
//     layout->addRow(tr("差分阶数:"), m_orderSpinBox);
//     layout->addRow(tr("p (不对称参数):"), m_pSpinBox);
//     layout->addRow(tr("最大迭代次数:"), m_itermaxSpinBox);

//     return widget;
// }



// 差异度计算
QWidget* ProcessTgBigParameterSettingsDialog::createDifferenceTab()
{
    QWidget* widget = new QWidget;
    QVBoxLayout* mainLayout = new QVBoxLayout(widget);
    

    // === 启用开关 ===
    m_diffEnabledCheck = new QCheckBox(tr("启用差异度计算"));
    mainLayout->addWidget(m_diffEnabledCheck);
    m_diffEnabledCheck->setChecked(true);   //  默认启用
    // layout->addRow(m_diffEnabledCheck);



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

    // === 3. Euclidean 算法区域 ===
    QGroupBox* euclideanGroup = new QGroupBox(tr("Euclidean（欧氏距离，取平方根）"));
    QFormLayout* euclideanLayout = new QFormLayout(euclideanGroup);

    // m_euclideanRootCheck = new QCheckBox(tr("取平方根 (√ΣΔ²)"));
    // m_euclideanRootCheck->setChecked(true);
    // euclideanLayout->addRow(m_euclideanRootCheck);

    mainLayout->addWidget(euclideanGroup);

    // === 4. 权重设置 ===
    QGroupBox* weightGroup = new QGroupBox(tr("算法权重设置（总和必须为 1）"));
    QFormLayout* weightLayout = new QFormLayout(weightGroup);

    m_weightNRMSE = new QDoubleSpinBox;
    m_weightNRMSE->setRange(0.0, 1.0);
    m_weightNRMSE->setDecimals(3);
    m_weightNRMSE->setSingleStep(0.05);
    m_weightNRMSE->setValue(0.50);
    weightLayout->addRow(tr("NRMSE 权重:"), m_weightNRMSE);

    m_weightPearson = new QDoubleSpinBox;
    m_weightPearson->setRange(0.0, 1.0);
    m_weightPearson->setDecimals(3);
    m_weightPearson->setSingleStep(0.05);
    m_weightPearson->setValue(0.30);
    weightLayout->addRow(tr("Pearson 权重:"), m_weightPearson);

    m_weightEuclidean = new QDoubleSpinBox;
    m_weightEuclidean->setRange(0.0, 1.0);
    m_weightEuclidean->setDecimals(3);
    m_weightEuclidean->setSingleStep(0.05);
    m_weightEuclidean->setValue(0.20);
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
            totalLabel->setText(QString("<font color='red'>%1 ⚠️ (需等于1)</font>").arg(text));
        }
    };

    connect(m_weightNRMSE, QOverload<double>::of(&QDoubleSpinBox::valueChanged), widget, updateTotal);
    connect(m_weightPearson, QOverload<double>::of(&QDoubleSpinBox::valueChanged), widget, updateTotal);
    connect(m_weightEuclidean, QOverload<double>::of(&QDoubleSpinBox::valueChanged), widget, updateTotal);

    // 初始化显示一次
    updateTotal();

    return widget;
}


QWidget* ProcessTgBigParameterSettingsDialog::createDerivativeTab()
{
    // 为微分创建控件 (与平滑类似)
    QWidget* widget = new QWidget;
    QFormLayout* layout = new QFormLayout(widget);

    m_derivativeEnabledCheck = new QCheckBox(tr("启用微分"));
    m_derivMethodCombo = new QComboBox;
    // m_derivMethodCombo->addItem(tr("Savitzky-Golay"), "savitzky_golay");
    m_derivMethodCombo->addItem(tr("SG 导数"), "derivative_sg");
    m_derivMethodCombo->addItem(tr("前向差分"), "first_diff");
    
    m_derivWindowSpinBox = new QSpinBox;
    m_derivPolyOrderSpinBox = new QSpinBox;
    
    layout->addRow(m_derivativeEnabledCheck);
    layout->addRow(tr("算法:"), m_derivMethodCombo);
    layout->addRow(tr("窗口大小:"), m_derivWindowSpinBox);
    layout->addRow(tr("多项式阶数:"), m_derivPolyOrderSpinBox);
    
    return widget;
}


void ProcessTgBigParameterSettingsDialog::setParameters(const ProcessingParameters &params)
{
    // 裁剪
    // 使用“工序大热重”独立裁剪参数
    m_clipEnabledCheck->setChecked(params.clippingEnabled_ProcessTgBig);
    m_clipMinSpinBox->setValue(params.clipMinX_ProcessTgBig);
    m_clipMaxSpinBox->setValue(params.clipMaxX_ProcessTgBig);

    // 归一化
    m_normEnabledCheck->setChecked(params.normalizationEnabled);
    int normIndex = m_normMethodCombo->findData(params.normalizationMethod);
    if (normIndex != -1) m_normMethodCombo->setCurrentIndex(normIndex);

    // 平滑
    m_smoothEnabledCheck->setChecked(params.smoothingEnabled);
    m_smoothWindowSpinBox->setValue(params.sgWindowSize);
    m_smoothPolyOrderSpinBox->setValue(params.sgPolyOrder);
    int smIndex = m_smoothMethodCombo->findData(params.smoothingMethod);
    if (smIndex != -1) m_smoothMethodCombo->setCurrentIndex(smIndex);

    // DTG导数
    m_derivativeEnabledCheck->setChecked(params.derivativeEnabled);
    m_derivWindowSpinBox->setValue(params.derivSgWindowSize);
    m_derivPolyOrderSpinBox->setValue(params.derivSgPolyOrder);
    int dIndex = m_derivMethodCombo->findData(params.derivativeMethod);
    if (dIndex != -1) m_derivMethodCombo->setCurrentIndex(dIndex);
    // 一阶导数
    if (m_deriv2MethodCombo) {
        int d2Index = m_deriv2MethodCombo->findData(params.derivative2Method);
        if (d2Index != -1) m_deriv2MethodCombo->setCurrentIndex(d2Index);
    }
    if (m_deriv2WindowSpinBox) m_deriv2WindowSpinBox->setValue(params.deriv2SgWindowSize);
    if (m_deriv2PolyOrderSpinBox) m_deriv2PolyOrderSpinBox->setValue(params.deriv2SgPolyOrder);

    // // === 基线校正 ===
    // m_baselineEnabledCheck->setChecked(params.baselineEnabled);
    // m_lambdaSpinBox->setValue(params.lambda);
    // m_orderSpinBox->setValue(params.order);
    // m_pSpinBox->setValue(params.p);
    // m_itermaxSpinBox->setValue(params.itermax);

    // 权重
    m_weightNRMSE->setValue(params.weightNRMSE);
    m_weightPearson->setValue(params.weightPearson);
    m_weightEuclidean->setValue(params.weightEuclidean);

    // 清理坏点参数
    if (m_invalidTokenFraction) m_invalidTokenFraction->setValue(params.invalidTokenFraction);
    if (m_outlierWindow) m_outlierWindow->setValue(params.outlierWindow);
    if (m_outlierNSigma) m_outlierNSigma->setValue(params.outlierNSigma);
    if (m_jumpDiffThreshold) m_jumpDiffThreshold->setValue(params.jumpDiffThreshold);
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
    if (m_loessSpan) m_loessSpan->setValue(params.loessSpan);
    if (m_defaultSmoothDisplayCombo) {
        int idx = m_defaultSmoothDisplayCombo->findData(params.defaultSmoothDisplay);
        if (idx != -1) m_defaultSmoothDisplayCombo->setCurrentIndex(idx);
    }

    // 绘图相关勾选
    if (m_showMeanCurveCheck) m_showMeanCurveCheck->setChecked(params.showMeanCurve);
    if (m_plotInterpolationCheck) m_plotInterpolationCheck->setChecked(params.plotInterpolation);
    if (m_showRawOverlayCheck) m_showRawOverlayCheck->setChecked(params.showRawOverlay);
    if (m_showBadPointsCheck) m_showBadPointsCheck->setChecked(params.showBadPoints);
}

ProcessingParameters ProcessTgBigParameterSettingsDialog::getParameters() const
{
    ProcessingParameters params;

    // 从UI控件收集数据并填充结构体
    // 写入“工序大热重”独立裁剪参数
    params.clippingEnabled_ProcessTgBig = m_clipEnabledCheck->isChecked();
    params.clipMinX_ProcessTgBig = m_clipMinSpinBox->value();
    params.clipMaxX_ProcessTgBig = m_clipMaxSpinBox->value();
    
    params.normalizationEnabled = m_normEnabledCheck->isChecked();
    params.normalizationMethod = m_normMethodCombo->currentData().toString();
    
    params.smoothingEnabled = m_smoothEnabledCheck->isChecked();
    params.smoothingMethod = m_smoothMethodCombo->currentData().toString();
    params.sgWindowSize = m_smoothWindowSpinBox->value();
    params.sgPolyOrder = m_smoothPolyOrderSpinBox->value();
    if (m_loessSpan) params.loessSpan = m_loessSpan->value();
    
    params.derivativeEnabled = m_derivativeEnabledCheck->isChecked();
    params.derivativeMethod = m_derivMethodCombo->currentData().toString();
    params.derivSgWindowSize = m_derivWindowSpinBox->value();
    params.derivSgPolyOrder = m_derivPolyOrderSpinBox->value();
    if (m_deriv2MethodCombo) params.derivative2Method = m_deriv2MethodCombo->currentData().toString();
    if (m_deriv2WindowSpinBox) params.deriv2SgWindowSize = m_deriv2WindowSpinBox->value();
    if (m_deriv2PolyOrderSpinBox) params.deriv2SgPolyOrder = m_deriv2PolyOrderSpinBox->value();

    // // 基线校正
    // params.baselineEnabled = m_baselineEnabledCheck->isChecked();
    // params.lambda = m_lambdaSpinBox->value();
    // params.order = m_orderSpinBox->value();
    // params.p = m_pSpinBox->value();
    // params.itermax = m_itermaxSpinBox->value();

    //权重设置
    params.totalWeight = m_weightNRMSE->value() + m_weightPearson->value() + m_weightEuclidean->value();
    params.weightNRMSE = m_weightNRMSE->value();
    params.weightPearson = m_weightPearson->value();
    params.weightEuclidean = m_weightEuclidean->value();

    // 清理坏点参数
    if (m_invalidTokenFraction) params.invalidTokenFraction = m_invalidTokenFraction->value();
    if (m_outlierWindow) params.outlierWindow = m_outlierWindow->value();
    if (m_outlierNSigma) params.outlierNSigma = m_outlierNSigma->value();
    if (m_jumpDiffThreshold) params.jumpDiffThreshold = m_jumpDiffThreshold->value();
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
    if (m_defaultSmoothDisplayCombo) params.defaultSmoothDisplay = m_defaultSmoothDisplayCombo->currentData().toString();

    // 绘图相关勾选
    if (m_showMeanCurveCheck) params.showMeanCurve = m_showMeanCurveCheck->isChecked();
    if (m_plotInterpolationCheck) params.plotInterpolation = m_plotInterpolationCheck->isChecked();
    if (m_showRawOverlayCheck) params.showRawOverlay = m_showRawOverlayCheck->isChecked();
    if (m_showBadPointsCheck) params.showBadPoints = m_showBadPointsCheck->isChecked();

    return params;
}

void ProcessTgBigParameterSettingsDialog::applyChanges()
{
    // 如果设计为非模态实时预览，这个槽函数会发出信号
    // emit parametersChanged(getParameters());
    
    // 对于模态对话框，"Apply"通常不是必需的，但可以保留
    // 这里我们先不做任何事
    DEBUG_LOG << "Apply button clicked. In a non-modal dialog, this would emit a signal.";
}

void ProcessTgBigParameterSettingsDialog::acceptChanges()
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
void ProcessTgBigParameterSettingsDialog::onButtonClicked(QAbstractButton *button)
{
    DEBUG_LOG << "Button clicked.";
    // 获取被点击按钮在 buttonBox 中的角色
    QDialogButtonBox::ButtonRole role = m_buttonBox->buttonRole(button);
    
    // a. 首先，进行参数校验
    if (role == QDialogButtonBox::AcceptRole || role == QDialogButtonBox::ApplyRole) {
        // if (m_smoothWindowSpinBox->value() <= m_smoothPolyOrderSpinBox->value()) {
        //     QMessageBox::warning(this, tr("参数错误"), tr("平滑的窗口大小必须大于多项式阶数。"));
        //     return; // 校验失败，中断操作，对话框不关闭
        // }
    }
    
    DEBUG_LOG << "Button clicked.";
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

QWidget* ProcessTgBigParameterSettingsDialog::createProcessParamsTab()
{
    QWidget* widget = new QWidget;
    QFormLayout* layout = new QFormLayout(widget);

    // 工序参数（坏点修复与检测等）
    m_invalidTokenFraction = new QDoubleSpinBox(widget);
    m_invalidTokenFraction->setRange(0.0, 1.0);
    m_invalidTokenFraction->setDecimals(3);

    m_outlierWindow = new QSpinBox(widget);
    m_outlierWindow->setRange(3, 199); m_outlierWindow->setSingleStep(2);
    m_outlierNSigma = new QDoubleSpinBox(widget);
    m_outlierNSigma->setRange(0.1, 20.0); m_outlierNSigma->setDecimals(2);

    m_jumpDiffThreshold = new QDoubleSpinBox(widget);
    m_jumpDiffThreshold->setRange(0.0, 1e6); m_jumpDiffThreshold->setDecimals(3);

    m_badPointsToShow = new QSpinBox(widget);
    m_badPointsToShow->setRange(0, 100000);

    m_fitTypeCombo = new QComboBox(widget);
    m_fitTypeCombo->addItem(tr("线性拟合"), "linear");
    m_fitTypeCombo->addItem(tr("二次拟合"), "quad");

    m_gammaSpin = new QDoubleSpinBox(widget);
    m_gammaSpin->setRange(0.0, 1.0); m_gammaSpin->setDecimals(3);

    m_anchorWindow = new QSpinBox(widget);
    m_anchorWindow->setRange(1, 1000);

    m_monoStart = new QSpinBox(widget);
    m_monoEnd = new QSpinBox(widget);
    m_monoStart->setRange(0, 1000000);
    m_monoEnd->setRange(0, 1000000);

    m_edgeBlend = new QDoubleSpinBox(widget);
    m_edgeBlend->setRange(0.0, 1.0); m_edgeBlend->setDecimals(3);

    m_epsScale = new QDoubleSpinBox(widget);
    m_epsScale->setRange(1e-12, 1.0); m_epsScale->setDecimals(12);

    m_slopeThreshold = new QDoubleSpinBox(widget);
    m_slopeThreshold->setRange(1e-12, 1.0); m_slopeThreshold->setDecimals(12);

    m_interpMethodCombo = new QComboBox(widget);
    m_interpMethodCombo->addItem(tr("PCHIP 保形三次"), "pchip");
    m_interpMethodCombo->addItem(tr("线性插值"), "linear");

    m_loessSpan = new QDoubleSpinBox(widget);
    m_loessSpan->setRange(0.01, 0.99); m_loessSpan->setDecimals(2);

    // 布局添加
    layout->addRow(tr("INVALID TOKEN FRACTION"), m_invalidTokenFraction);
    layout->addRow(tr("OUTLIER_WINDOW"), m_outlierWindow);
    layout->addRow(tr("OUTLIER_NSIGMA"), m_outlierNSigma);
    layout->addRow(tr("JUMP DIFF THRESHOLD"), m_jumpDiffThreshold);
    layout->addRow(tr("Bad points to show"), m_badPointsToShow);
    layout->addRow(tr("Fit Type"), m_fitTypeCombo);
    layout->addRow(tr("Gamma"), m_gammaSpin);
    layout->addRow(tr("Anchor window"), m_anchorWindow);
    layout->addRow(tr("Mono Start"), m_monoStart);
    layout->addRow(tr("Mono End"), m_monoEnd);
    layout->addRow(tr("Edge Blend"), m_edgeBlend);
    layout->addRow(tr("EPS Scale"), m_epsScale);
    layout->addRow(tr("Slope Threshold"), m_slopeThreshold);
    layout->addRow(tr("插值方法"), m_interpMethodCombo);
    layout->addRow(tr("LOESS Span"), m_loessSpan);

    // 新增：绘图显示与插值相关选项
    m_showMeanCurveCheck = new QCheckBox(tr("显示均值曲线"), widget);
    m_plotInterpolationCheck = new QCheckBox(tr("绘图时插值"), widget);
    m_showRawOverlayCheck = new QCheckBox(tr("显示原始（预修复）叠加"), widget);
    m_showBadPointsCheck = new QCheckBox(tr("显示坏点"), widget);
    layout->addRow(m_showMeanCurveCheck);
    layout->addRow(m_plotInterpolationCheck);
    layout->addRow(m_showRawOverlayCheck);
    layout->addRow(m_showBadPointsCheck);

    return widget;
}

QWidget* ProcessTgBigParameterSettingsDialog::createRawDataTab()
{
    QWidget* widget = new QWidget;
    QFormLayout* layout = new QFormLayout(widget);

    // 原始数据参数
    m_invalidTokenFraction = new QDoubleSpinBox(widget);
    m_invalidTokenFraction->setRange(0.0, 1.0); m_invalidTokenFraction->setDecimals(3);

    m_defaultSmoothDisplayCombo = new QComboBox(widget);
    m_defaultSmoothDisplayCombo->addItem(tr("平滑曲线"), "smoothed");

    layout->addRow(tr("INVALID TOKEN FRACTION"), m_invalidTokenFraction);
    layout->addRow(tr("Default Smooth Display"), m_defaultSmoothDisplayCombo);
    return widget;
}

QWidget* ProcessTgBigParameterSettingsDialog::createDerivative2Tab()
{
    QWidget* widget = new QWidget;
    QFormLayout* layout = new QFormLayout(widget);

    m_deriv2MethodCombo = new QComboBox(widget);
    m_deriv2MethodCombo->addItem(tr("SG 导数"), "derivative_sg");
    m_deriv2MethodCombo->addItem(tr("前向差分"), "first_diff");

    m_deriv2WindowSpinBox = new QSpinBox(widget);
    m_deriv2PolyOrderSpinBox = new QSpinBox(widget);

    layout->addRow(tr("算法:"), m_deriv2MethodCombo);
    layout->addRow(tr("窗口大小:"), m_deriv2WindowSpinBox);
    layout->addRow(tr("多项式阶数:"), m_deriv2PolyOrderSpinBox);

    return widget;
}
