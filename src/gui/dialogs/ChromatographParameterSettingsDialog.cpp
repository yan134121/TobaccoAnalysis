#include "ChromatographParameterSettingsDialog.h"
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


// ChromatographParameterSettingsDialog::ChromatographParameterSettingsDialog(const ProcessingParameters& initialParams, QWidget *parent)
//     : QDialog(parent)
// {
// 主要构造函数的实现
ChromatographParameterSettingsDialog::ChromatographParameterSettingsDialog(const ProcessingParameters& initialParams, QWidget *parent)
    : QDialog(parent)
{
    // 默认启用基线校正勾选
    ProcessingParameters init = initialParams;
    init.baselineEnabled = true;
    init.peakDetectionEnabled = true;
    m_currentParams = init;

    setupUi();
    setParameters(init);

    // (可选) 设置为非模态，并提供 Apply 按钮
    // setModal(false); 
}

ChromatographParameterSettingsDialog::~ChromatographParameterSettingsDialog()
{
    // C++11 之后，如果控件有父对象，这里不需要手动 delete UI 成员
}

void ChromatographParameterSettingsDialog::setupUi()
{
    setWindowTitle(tr("色谱处理参数设置"));
    setMinimumWidth(400);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    m_tabWidget = new QTabWidget(this);
    // m_tabWidget->addTab(createClippingTab(), tr("裁剪"));
    // m_tabWidget->addTab(createNormalizationTab(), tr("归一化"));
    // m_tabWidget->addTab(createSmoothingTab(), tr("平滑"));
    // m_tabWidget->addTab(createDerivativeTab(), tr("微分"));
    m_tabWidget->addTab(createBaselineTab(), tr("基线校正"));
    m_tabWidget->addTab(createPeakTab(), tr("峰检测"));
    m_tabWidget->addTab(createAlignmentTab(), tr("峰对齐"));
    m_tabWidget->addTab(createDifferenceTab(), tr("差异度")); // createDifferenceTab();

    mainLayout->addWidget(m_tabWidget);

    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply, this);
    // 修改按钮文本
    m_buttonBox->button(QDialogButtonBox::Ok)->setText(tr("确认"));
    m_buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("取消"));
    m_buttonBox->button(QDialogButtonBox::Apply)->setText(tr("应用"));
    mainLayout->addWidget(m_buttonBox);

    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &ChromatographParameterSettingsDialog::acceptChanges);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, &ChromatographParameterSettingsDialog::applyChanges);
    connect(m_buttonBox, &QDialogButtonBox::clicked, this, &ChromatographParameterSettingsDialog::onButtonClicked);

    // // === 信号连接：权重调整时自动校正 ===
    // connect(m_weightNRMSE, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ChromatographParameterSettingsDialog::onWeightChanged);
    // connect(m_weightPearson, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ChromatographParameterSettingsDialog::onWeightChanged);
    // connect(m_weightEuclidean, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ChromatographParameterSettingsDialog::onWeightChanged);


    setLayout(mainLayout);
}

// QWidget* ChromatographParameterSettingsDialog::createClippingTab()
// {
//     QWidget* widget = new QWidget;
//     QFormLayout* layout = new QFormLayout(widget);
    
//     m_clipEnabledCheck = new QCheckBox(tr("启用裁剪"));
//     m_clipMinSpinBox = new QDoubleSpinBox;
//     m_clipMaxSpinBox = new QDoubleSpinBox;

//     m_clipMinSpinBox->setRange(0, 10000);
//     m_clipMaxSpinBox->setRange(0, 10000);

//     layout->addRow(m_clipEnabledCheck);
//     layout->addRow(tr("X轴最小值:"), m_clipMinSpinBox);
//     layout->addRow(tr("X轴最大值:"), m_clipMaxSpinBox);
    
//     return widget;
// }

// QWidget* ChromatographParameterSettingsDialog::createNormalizationTab()
// {
//     QWidget* widget = new QWidget;
//     QFormLayout* layout = new QFormLayout(widget);

//     m_normEnabledCheck = new QCheckBox(tr("启用归一化"));
//     m_normMethodCombo = new QComboBox;
//     m_normMethodCombo->addItem(tr("最大-最小值归一化"), "max_min");
//     m_normMethodCombo->addItem(tr("Z-score 标准化"), "z_score");
    
//     layout->addRow(m_normEnabledCheck);
//     layout->addRow(tr("方法:"), m_normMethodCombo);
    
//     return widget;
// }


// QWidget* ChromatographParameterSettingsDialog::createSmoothingTab()
// {
//     QWidget* widget = new QWidget;
//     QFormLayout* layout = new QFormLayout(widget);

//     m_smoothEnabledCheck = new QCheckBox(tr("启用平滑"));
//     m_smoothMethodCombo = new QComboBox;
//     m_smoothMethodCombo->addItem(tr("Savitzky-Golay"), "savitzky_golay");
    
//     m_smoothWindowSpinBox = new QSpinBox;
//     m_smoothWindowSpinBox->setRange(3, 99);
//     m_smoothWindowSpinBox->setSingleStep(2); // SG窗口通常是奇数
    
//     m_smoothPolyOrderSpinBox = new QSpinBox;
//     m_smoothPolyOrderSpinBox->setRange(2, 10);
    
//     layout->addRow(m_smoothEnabledCheck);
//     layout->addRow(tr("算法:"), m_smoothMethodCombo);
//     layout->addRow(tr("窗口大小:"), m_smoothWindowSpinBox);
//     layout->addRow(tr("多项式阶数:"), m_smoothPolyOrderSpinBox);
    
//     return widget;
// }


QWidget* ChromatographParameterSettingsDialog::createBaselineTab()
{
    QWidget* widget = new QWidget;
    QFormLayout* layout = new QFormLayout(widget);

    m_baselineEnabledCheck = new QCheckBox(tr("启用基线校正"), widget);
    m_baselineOverlayRawCheck = new QCheckBox(tr("叠加显示原始数据"), widget); // 新增选项，控制在基线图表叠加显示原始曲线
    m_baselineDisplayCheck = new QCheckBox(tr("绘制基线（原点样式）"), widget); // 新增选项，控制是否绘制基线

    m_lambdaSpinBox = new QDoubleSpinBox(widget);
    m_lambdaSpinBox->setDecimals(1);
    m_lambdaSpinBox->setRange(1, 1e8);
    m_lambdaSpinBox->setValue(1e5);
    m_lambdaSpinBox->setSingleStep(1000);

    m_orderSpinBox = new QSpinBox(widget);
    m_orderSpinBox->setRange(1, 5);
    m_orderSpinBox->setValue(2);

    m_pSpinBox = new QDoubleSpinBox(widget);
    m_pSpinBox->setRange(0.0, 1.0);
    m_pSpinBox->setSingleStep(0.01);
    m_pSpinBox->setDecimals(3);
    m_pSpinBox->setValue(0.05);

    m_wepSpinBox = new QDoubleSpinBox(widget);
    m_wepSpinBox->setRange(0.0, 0.5);
    m_wepSpinBox->setSingleStep(0.01);
    m_wepSpinBox->setDecimals(3);
    m_wepSpinBox->setValue(0.0);

    m_itermaxSpinBox = new QSpinBox(widget);
    m_itermaxSpinBox->setRange(1, 200);
    m_itermaxSpinBox->setValue(50);

    layout->addRow(m_baselineEnabledCheck);
    layout->addRow(m_baselineOverlayRawCheck);
    layout->addRow(m_baselineDisplayCheck);
    layout->addRow(tr("λ (平滑系数):"), m_lambdaSpinBox);
    layout->addRow(tr("差分阶数:"), m_orderSpinBox);
    layout->addRow(tr("p (不对称参数):"), m_pSpinBox);
    layout->addRow(tr("wep (端点保护比例):"), m_wepSpinBox);
    layout->addRow(tr("最大迭代次数:"), m_itermaxSpinBox);

    return widget;
}

QWidget* ChromatographParameterSettingsDialog::createPeakTab()
{
    QWidget* widget = new QWidget;
    QFormLayout* layout = new QFormLayout(widget);

    m_peakEnabledCheck = new QCheckBox(tr("启用峰检测"), widget);
    m_peakMinHeightSpin = new QDoubleSpinBox(widget);
    m_peakMinProminenceSpin = new QDoubleSpinBox(widget);
    m_peakMinDistanceSpin = new QSpinBox(widget);
    m_peakSnrSpin = new QDoubleSpinBox(widget);

    m_peakMinHeightSpin->setRange(0.0, 1e9);
    m_peakMinProminenceSpin->setRange(0.0, 1e9);
    m_peakMinProminenceSpin->setDecimals(6);
    m_peakMinDistanceSpin->setRange(1, 100000);
    m_peakSnrSpin->setRange(0.0, 1e9);
    m_peakSnrSpin->setDecimals(6);

    layout->addRow(m_peakEnabledCheck);
    layout->addRow(tr("最小峰高度:"), m_peakMinHeightSpin);
    layout->addRow(tr("最小显著性:"), m_peakMinProminenceSpin);
    layout->addRow(tr("最小峰间距(点):"), m_peakMinDistanceSpin);
    layout->addRow(tr("最小SNR:"), m_peakSnrSpin);

    return widget;
}

QWidget* ChromatographParameterSettingsDialog::createAlignmentTab()
{
    QWidget* widget = new QWidget;
    QFormLayout* layout = new QFormLayout(widget);

    m_alignEnabledCheck = new QCheckBox(tr("启用峰对齐"), widget);
    m_referenceSampleIdSpin = new QSpinBox(widget);
    m_cowWindowSizeSpin = new QSpinBox(widget);
    m_cowMaxWarpSpin = new QSpinBox(widget);
    m_cowSegmentCountSpin = new QSpinBox(widget);
    m_cowResampleStepSpin = new QDoubleSpinBox(widget);

    m_referenceSampleIdSpin->setRange(1, 100000000);
    m_cowWindowSizeSpin->setRange(1, 100000);
    m_cowMaxWarpSpin->setRange(0, 100000);
    m_cowSegmentCountSpin->setRange(1, 10000);
    m_cowResampleStepSpin->setRange(0.0, 1e9);
    m_cowResampleStepSpin->setDecimals(6);

    layout->addRow(m_alignEnabledCheck);
    layout->addRow(tr("参考样本ID:"), m_referenceSampleIdSpin);
    layout->addRow(tr("窗口大小:"), m_cowWindowSizeSpin);
    layout->addRow(tr("最大滞后(点):"), m_cowMaxWarpSpin);
    layout->addRow(tr("分段数:"), m_cowSegmentCountSpin);
    layout->addRow(tr("重采样步长:"), m_cowResampleStepSpin);

    return widget;
}



// 差异度计算
QWidget* ChromatographParameterSettingsDialog::createDifferenceTab()
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


// QWidget* ChromatographParameterSettingsDialog::createDerivativeTab()
// {
//     // 为微分创建控件 (与平滑类似)
//     QWidget* widget = new QWidget;
//     QFormLayout* layout = new QFormLayout(widget);

//     m_derivativeEnabledCheck = new QCheckBox(tr("启用微分"));
//     m_derivMethodCombo = new QComboBox;
//     // m_derivMethodCombo->addItem(tr("Savitzky-Golay"), "savitzky_golay");
//     m_derivMethodCombo->addItem(tr("derivative_sg"), "derivative_sg");
    
//     m_derivWindowSpinBox = new QSpinBox;
//     m_derivPolyOrderSpinBox = new QSpinBox;
    
//     layout->addRow(m_derivativeEnabledCheck);
//     layout->addRow(tr("算法:"), m_derivMethodCombo);
//     layout->addRow(tr("窗口大小:"), m_derivWindowSpinBox);
//     layout->addRow(tr("多项式阶数:"), m_derivPolyOrderSpinBox);
    
//     return widget;
// }


void ChromatographParameterSettingsDialog::setParameters(const ProcessingParameters &params)
{
    // // 裁剪
    // m_clipEnabledCheck->setChecked(params.clippingEnabled);
    // m_clipMinSpinBox->setValue(params.clipMinX);
    // m_clipMaxSpinBox->setValue(params.clipMaxX);

    // // 归一化
    // m_normEnabledCheck->setChecked(params.normalizationEnabled);
    // int normIndex = m_normMethodCombo->findData(params.normalizationMethod);
    // if (normIndex != -1) m_normMethodCombo->setCurrentIndex(normIndex);

    // // 平滑
    // m_smoothEnabledCheck->setChecked(params.smoothingEnabled);
    // m_smoothWindowSpinBox->setValue(params.sgWindowSize);
    // m_smoothPolyOrderSpinBox->setValue(params.sgPolyOrder);

    // // 微分
    // m_derivativeEnabledCheck->setChecked(params.derivativeEnabled);
    // m_derivWindowSpinBox->setValue(params.derivSgWindowSize);
    // m_derivPolyOrderSpinBox->setValue(params.derivSgPolyOrder);

    // === 基线校正 ===
    m_baselineEnabledCheck->setChecked(params.baselineEnabled);
    m_lambdaSpinBox->setValue(params.lambda);
    m_orderSpinBox->setValue(params.order);
    m_pSpinBox->setValue(params.p);
    if (m_wepSpinBox) m_wepSpinBox->setValue(params.wep);
    m_itermaxSpinBox->setValue(params.itermax);
    if (m_baselineOverlayRawCheck) m_baselineOverlayRawCheck->setChecked(params.baselineOverlayRaw);
    if (m_baselineDisplayCheck) m_baselineDisplayCheck->setChecked(params.baselineDisplayEnabled);

    // 权重
    m_weightNRMSE->setValue(params.weightNRMSE);
    m_weightPearson->setValue(params.weightPearson);
    m_weightEuclidean->setValue(params.weightEuclidean);

    if (m_peakEnabledCheck) m_peakEnabledCheck->setChecked(params.peakDetectionEnabled);
    if (m_peakMinHeightSpin) m_peakMinHeightSpin->setValue(params.peakMinHeight);
    if (m_peakMinProminenceSpin) m_peakMinProminenceSpin->setValue(params.peakMinProminence);
    if (m_peakMinDistanceSpin) m_peakMinDistanceSpin->setValue(params.peakMinDistance);
    if (m_peakSnrSpin) m_peakSnrSpin->setValue(params.peakSnrThreshold);

    if (m_alignEnabledCheck) m_alignEnabledCheck->setChecked(params.alignmentEnabled);
    if (m_referenceSampleIdSpin) m_referenceSampleIdSpin->setValue(params.referenceSampleId > 0 ? params.referenceSampleId : 1);
    if (m_cowWindowSizeSpin) m_cowWindowSizeSpin->setValue(params.cowWindowSize);
    if (m_cowMaxWarpSpin) m_cowMaxWarpSpin->setValue(params.cowMaxWarp);
    if (m_cowSegmentCountSpin) m_cowSegmentCountSpin->setValue(params.cowSegmentCount);
    if (m_cowResampleStepSpin) m_cowResampleStepSpin->setValue(params.cowResampleStep);
}

ProcessingParameters ChromatographParameterSettingsDialog::getParameters() const
{
    // 基于当前缓存参数，更新来自UI的字段并返回
    ProcessingParameters params = m_currentParams;
    // 基线校正
    if (m_baselineEnabledCheck) params.baselineEnabled = m_baselineEnabledCheck->isChecked();
    if (m_lambdaSpinBox)        params.lambda = m_lambdaSpinBox->value();
    if (m_orderSpinBox)         params.order = m_orderSpinBox->value();
    if (m_pSpinBox)             params.p = m_pSpinBox->value();
    if (m_wepSpinBox)           params.wep = m_wepSpinBox->value();
    if (m_itermaxSpinBox)       params.itermax = m_itermaxSpinBox->value();
    if (m_baselineOverlayRawCheck) params.baselineOverlayRaw = m_baselineOverlayRawCheck->isChecked();
    if (m_baselineDisplayCheck) params.baselineDisplayEnabled = m_baselineDisplayCheck->isChecked();
    if (m_peakEnabledCheck)     params.peakDetectionEnabled = m_peakEnabledCheck->isChecked();
    if (m_peakMinHeightSpin)    params.peakMinHeight = m_peakMinHeightSpin->value();
    if (m_peakMinProminenceSpin) params.peakMinProminence = m_peakMinProminenceSpin->value();
    if (m_peakMinDistanceSpin)  params.peakMinDistance = m_peakMinDistanceSpin->value();
    if (m_peakSnrSpin)          params.peakSnrThreshold = m_peakSnrSpin->value();
    if (m_alignEnabledCheck)    params.alignmentEnabled = m_alignEnabledCheck->isChecked();
    if (m_referenceSampleIdSpin) params.referenceSampleId = m_referenceSampleIdSpin->value();
    if (m_cowWindowSizeSpin)    params.cowWindowSize = m_cowWindowSizeSpin->value();
    if (m_cowMaxWarpSpin)       params.cowMaxWarp = m_cowMaxWarpSpin->value();
    if (m_cowSegmentCountSpin)  params.cowSegmentCount = m_cowSegmentCountSpin->value();
    if (m_cowResampleStepSpin)  params.cowResampleStep = m_cowResampleStepSpin->value();
    // 权重
    params.totalWeight = m_weightNRMSE->value() + m_weightPearson->value() + m_weightEuclidean->value();
    params.weightNRMSE = m_weightNRMSE->value();
    params.weightPearson = m_weightPearson->value();
    params.weightEuclidean = m_weightEuclidean->value();
    return params;
}

void ChromatographParameterSettingsDialog::applyChanges()
{
    // 如果设计为非模态实时预览，这个槽函数会发出信号
    // emit parametersChanged(getParameters());
    
    // 对于模态对话框，"Apply"通常不是必需的，但可以保留
    // 这里我们先不做任何事
    DEBUG_LOG << "Apply button clicked. In a non-modal dialog, this would emit a signal.";
}

void ChromatographParameterSettingsDialog::acceptChanges()
{
    // 在调用 accept() 之前，可以进行参数的最终校验
    // 当前未启用“平滑”页签，相关控件可能为空指针，需判空
    if (m_smoothWindowSpinBox && m_smoothPolyOrderSpinBox) {
        if (m_smoothWindowSpinBox->value() <= m_smoothPolyOrderSpinBox->value()) {
            QMessageBox::warning(this, tr("参数错误"), tr("平滑的窗口大小必须大于多项式阶数。"));
            return; // 阻止对话框关闭
        }
    }
    // 统一在此处发出参数应用信号，避免在 onButtonClicked 中重复触发
    emit parametersApplied(getParameters());
    
    // 调用基类的 accept() 方法，它会关闭对话框并返回 QDialog::Accepted
    QDialog::accept();
}

// 【关键】实现统一的按钮点击处理槽函数
void ChromatographParameterSettingsDialog::onButtonClicked(QAbstractButton *button)
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
        // 不在此处关闭对话框，交由 accepted -> acceptChanges 统一处理，避免重复关闭导致崩溃
        
    } else if (role == QDialogButtonBox::RejectRole) {
        // --- Cancel (取消) 按钮 ---
        // 直接调用 reject() 关闭对话框，不发出任何信号
        QDialog::reject();
    }
}
