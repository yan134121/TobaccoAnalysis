#include "TgSmallParameterSettingsDialog.h"
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


// TgSmallParameterSettingsDialog::TgSmallParameterSettingsDialog(const ProcessingParameters& initialParams, QWidget *parent)
//     : QDialog(parent)
// {
// 主要构造函数的实现
TgSmallParameterSettingsDialog::TgSmallParameterSettingsDialog(const ProcessingParameters& initialParams, QWidget *parent)
    : QDialog(parent)
    , m_currentParams(initialParams)
{
    setupUi();
    setParameters(initialParams);

    // (可选) 设置为非模态，并提供 Apply 按钮
    // setModal(false); 
}

TgSmallParameterSettingsDialog::~TgSmallParameterSettingsDialog()
{
    // C++11 之后，如果控件有父对象，这里不需要手动 delete UI 成员
}

void TgSmallParameterSettingsDialog::setupUi()
{
    setWindowTitle(tr("小热重处理参数设置"));
    setMinimumWidth(400);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    m_tabWidget = new QTabWidget(this);
    m_tabWidget->addTab(createClippingTab(), tr("裁剪"));
    // m_tabWidget->addTab(createNormalizationTab(), tr("归一化"));
    // m_tabWidget->addTab(createSmoothingTab(), tr("平滑"));
    // m_tabWidget->addTab(createDerivativeTab(), tr("微分"));
    m_tabWidget->addTab(createDifferenceTab(), tr("差异度")); // createDifferenceTab();

    mainLayout->addWidget(m_tabWidget);

    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply, this);
    // 修改按钮文本
    m_buttonBox->button(QDialogButtonBox::Ok)->setText(tr("确认"));
    m_buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("取消"));
    m_buttonBox->button(QDialogButtonBox::Apply)->setText(tr("应用"));
    mainLayout->addWidget(m_buttonBox);

    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &TgSmallParameterSettingsDialog::acceptChanges);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, &TgSmallParameterSettingsDialog::applyChanges);
    connect(m_buttonBox, &QDialogButtonBox::clicked, this, &TgSmallParameterSettingsDialog::onButtonClicked);

    // // === 信号连接：权重调整时自动校正 ===
    // connect(m_weightNRMSE, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &TgSmallParameterSettingsDialog::onWeightChanged);
    // connect(m_weightPearson, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &TgSmallParameterSettingsDialog::onWeightChanged);
    // connect(m_weightEuclidean, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &TgSmallParameterSettingsDialog::onWeightChanged);


    setLayout(mainLayout);
}

// 裁剪参数设置页
QWidget* TgSmallParameterSettingsDialog::createClippingTab()
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

// 差异度计算
QWidget* TgSmallParameterSettingsDialog::createDifferenceTab()
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


// QWidget* TgSmallParameterSettingsDialog::createDerivativeTab()
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


void TgSmallParameterSettingsDialog::setParameters(const ProcessingParameters &params)
{
    // 裁剪
    // 使用"小热重（原始数据）"独立裁剪参数
    if (m_clipEnabledCheck) {
        m_clipEnabledCheck->setChecked(params.clippingEnabled_TgSmallRaw);
    }
    if (m_clipMinSpinBox) {
        m_clipMinSpinBox->setValue(params.clipMinX_TgSmallRaw);
    }
    if (m_clipMaxSpinBox) {
        m_clipMaxSpinBox->setValue(params.clipMaxX_TgSmallRaw);
    }

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

    // 权重
    m_weightNRMSE->setValue(params.weightNRMSE);
    m_weightPearson->setValue(params.weightPearson);
    m_weightEuclidean->setValue(params.weightEuclidean);
}

ProcessingParameters TgSmallParameterSettingsDialog::getParameters() const
{
    ProcessingParameters params;

    // 从UI控件收集数据并填充结构体
    // 写入"小热重（原始数据）"独立裁剪参数
    if (m_clipEnabledCheck) {
        params.clippingEnabled_TgSmallRaw = m_clipEnabledCheck->isChecked();
    }
    if (m_clipMinSpinBox) {
        params.clipMinX_TgSmallRaw = m_clipMinSpinBox->value();
    }
    if (m_clipMaxSpinBox) {
        params.clipMaxX_TgSmallRaw = m_clipMaxSpinBox->value();
    }
    
    // params.normalizationEnabled = m_normEnabledCheck->isChecked();
    // params.normalizationMethod = m_normMethodCombo->currentData().toString();
    
    // params.smoothingEnabled = m_smoothEnabledCheck->isChecked();
    // params.smoothingMethod = m_smoothMethodCombo->currentData().toString();
    // params.sgWindowSize = m_smoothWindowSpinBox->value();
    // params.sgPolyOrder = m_smoothPolyOrderSpinBox->value();
    
    // params.derivativeEnabled = m_derivativeEnabledCheck->isChecked();
    // params.derivativeMethod = m_derivMethodCombo->currentData().toString();
    // params.derivSgWindowSize = m_derivWindowSpinBox->value();
    // params.derivSgPolyOrder = m_derivPolyOrderSpinBox->value();

    params.totalWeight = m_weightNRMSE->value() + m_weightPearson->value() + m_weightEuclidean->value();
    params.weightNRMSE = m_weightNRMSE->value();
    params.weightPearson = m_weightPearson->value();
    params.weightEuclidean = m_weightEuclidean->value();

    return params;
}

void TgSmallParameterSettingsDialog::applyChanges()
{
    // 如果设计为非模态实时预览，这个槽函数会发出信号
    // emit parametersChanged(getParameters());
    
    // 对于模态对话框，"Apply"通常不是必需的，但可以保留
    // 这里我们先不做任何事
    DEBUG_LOG << "Apply button clicked. In a non-modal dialog, this would emit a signal.";
}

void TgSmallParameterSettingsDialog::acceptChanges()
{
    // // 在调用 accept() 之前，可以进行参数的最终校验
    // if (m_smoothWindowSpinBox->value() <= m_smoothPolyOrderSpinBox->value()) {
    //     QMessageBox::warning(this, tr("参数错误"), tr("平滑的窗口大小必须大于多项式阶数。"));
    //     return; // 阻止对话框关闭
    // }
    
    // 调用基类的 accept() 方法，它会关闭对话框并返回 QDialog::Accepted
    QDialog::accept();
}

// 【关键】实现统一的按钮点击处理槽函数
void TgSmallParameterSettingsDialog::onButtonClicked(QAbstractButton *button)
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
