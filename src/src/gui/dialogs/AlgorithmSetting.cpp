#include "AlgorithmSetting.h"
#include "ui_AlgorithmSetting.h"
#include <QMessageBox>

AlgorithmSetting::AlgorithmSetting(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AlgorithmSetting)
{
    ui->setupUi(this);
    setupConnections();
    
    // 设置窗口标题
    setWindowTitle("算法设置");
    
    // 初始化界面状态
    initializeUI();
}

AlgorithmSetting::~AlgorithmSetting()
{
    delete ui;
}

void AlgorithmSetting::setupConnections()
{
    // 连接主要按钮
    connect(ui->applyButton, &QPushButton::clicked, this, &AlgorithmSetting::onApplyButtonClicked);
    connect(ui->okButton, &QPushButton::clicked, this, &AlgorithmSetting::onOkButtonClicked);
    connect(ui->cancelButton, &QPushButton::clicked, this, &AlgorithmSetting::onCancelButtonClicked);
    
    // 连接QTabWidget
    connect(ui->dataTypeTabWidget, &QTabWidget::currentChanged, this, &AlgorithmSetting::onTabChanged);
    
    // 连接各个tab页面的处理方式列表
    connect(ui->bigThermalMethodsList, &QListWidget::currentRowChanged, this, &AlgorithmSetting::onBigThermalMethodChanged);
    connect(ui->smallThermalMethodsList, &QListWidget::currentRowChanged, this, &AlgorithmSetting::onSmallThermalMethodChanged);
    connect(ui->chromatographyMethodsList, &QListWidget::currentRowChanged, this, &AlgorithmSetting::onChromatographyMethodChanged);
    
    // 连接算法下拉框
    connect(ui->bigThermalNormMethodComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &AlgorithmSetting::onBigThermalNormMethodChanged);
    connect(ui->bigThermalSmoothMethodComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &AlgorithmSetting::onBigThermalSmoothMethodChanged);
    connect(ui->smallThermalNormMethodComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &AlgorithmSetting::onSmallThermalNormMethodChanged);
    connect(ui->smallThermalSmoothMethodComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &AlgorithmSetting::onSmallThermalSmoothMethodChanged);
    connect(ui->chromatographyBaselineMethodComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &AlgorithmSetting::onChromatographyBaselineMethodChanged);
}

void AlgorithmSetting::initializeUI()
{
    // 设置默认显示第一个tab页面（大热重数据）
    ui->dataTypeTabWidget->setCurrentIndex(0);
    
    // 初始化各个tab页面
    initializeBigThermalTab();
    initializeSmallThermalTab();
    initializeChromatographyTab();
    
    // 默认隐藏所有参数设置的QStackedWidget组件
    ui->bigThermalParametersStackedWidget->setVisible(false);
    ui->smallThermalParametersStackedWidget->setVisible(false);
    ui->chromatographyParametersStackedWidget->setVisible(false);
}

void AlgorithmSetting::showAlignmentPage()
{
    // 由于新UI没有标签页，这个方法现在显示整个对话框
    show();
}

void AlgorithmSetting::showNormalizationPage()
{
    // 显示对话框并滚动到归一化相关区域
    show();
}

void AlgorithmSetting::showSmoothPage()
{
    // 显示对话框并滚动到平滑相关区域
    show();
}

void AlgorithmSetting::showBaselinePage()
{
    // 显示对话框并滚动到基线校正相关区域
    show();
}

void AlgorithmSetting::showPeakAlignPage()
{
    // 显示对话框并滚动到峰对齐相关区域
    show();
}

void AlgorithmSetting::showDifferencePage()
{
    // 显示对话框并滚动到差异度相关区域
    show();
}

void AlgorithmSetting::onApplyButtonClicked()
{
    // 应用当前设置但不关闭对话框
    applySettings();
}

void AlgorithmSetting::onOkButtonClicked()
{
    // 应用设置并关闭对话框
    if (applySettings()) {
        accept();
    }
}

void AlgorithmSetting::onCancelButtonClicked()
{
    // 取消设置并关闭对话框
    reject();
}

bool AlgorithmSetting::applySettings()
{
    // 验证和应用算法设置
    try {
        // 这里可以添加设置验证和应用逻辑
        QMessageBox::information(this, "提示", "算法设置已应用");
        return true;
    } catch (const std::exception& e) {
        QMessageBox::warning(this, "错误", QString("应用设置时发生错误: %1").arg(e.what()));
        return false;
    }
}

// 保留旧的槽函数以保持兼容性，但实现为空或重定向到新的处理方法
void AlgorithmSetting::onAlignmentOkButtonClicked()
{
    onOkButtonClicked();
}

void AlgorithmSetting::onAlignmentCancelButtonClicked()
{
    onCancelButtonClicked();
}

void AlgorithmSetting::onNormalizationOkButtonClicked()
{
    onOkButtonClicked();
}

void AlgorithmSetting::onNormalizationCancelButtonClicked()
{
    onCancelButtonClicked();
}

void AlgorithmSetting::onSmoothOkButtonClicked()
{
    onOkButtonClicked();
}

void AlgorithmSetting::onSmoothCancelButtonClicked()
{
    onCancelButtonClicked();
}

void AlgorithmSetting::onBaselineOkButtonClicked()
{
    onOkButtonClicked();
}

void AlgorithmSetting::onBaselineCancelButtonClicked()
{
    onCancelButtonClicked();
}

void AlgorithmSetting::onPeakAlignOkButtonClicked()
{
    onOkButtonClicked();
}

void AlgorithmSetting::onPeakAlignCancelButtonClicked()
{
    onCancelButtonClicked();
}

void AlgorithmSetting::onDifferenceOkButtonClicked()
{
    onOkButtonClicked();
}

void AlgorithmSetting::onDifferenceCancelButtonClicked()
{
    onCancelButtonClicked();
}


void AlgorithmSetting::onTabChanged(int index)
{
    // 当切换tab时，隐藏所有参数设置的QStackedWidget组件
    ui->bigThermalParametersStackedWidget->setVisible(false);
    ui->smallThermalParametersStackedWidget->setVisible(false);
    ui->chromatographyParametersStackedWidget->setVisible(false);
}

void AlgorithmSetting::onBigThermalMethodChanged()
{
    if (ui->bigThermalMethodsList->currentItem()) {
        QString method = ui->bigThermalMethodsList->currentItem()->text();
        updateAlgorithmStackForMethod(method, 0);
    }
}

void AlgorithmSetting::onSmallThermalMethodChanged()
{
    if (ui->smallThermalMethodsList->currentItem()) {
        QString method = ui->smallThermalMethodsList->currentItem()->text();
        updateAlgorithmStackForMethod(method, 1);
    }
}

void AlgorithmSetting::onChromatographyMethodChanged()
{
    if (ui->chromatographyMethodsList->currentItem()) {
        QString method = ui->chromatographyMethodsList->currentItem()->text();
        updateAlgorithmStackForMethod(method, 2);
    }
}

// QTabWidget界面管理方法
void AlgorithmSetting::initializeBigThermalTab()
{
    // 初始化大热重数据处理方式列表
    ui->bigThermalMethodsList->clear();
    ui->bigThermalMethodsList->addItem("归一化");
    ui->bigThermalMethodsList->addItem("平滑");
    ui->bigThermalMethodsList->addItem("差异度");
    
    // 默认选择第一项
    if (ui->bigThermalMethodsList->count() > 0) {
        ui->bigThermalMethodsList->setCurrentRow(0);
    }
}

void AlgorithmSetting::initializeSmallThermalTab()
{
    // 初始化小热重数据处理方式列表
    ui->smallThermalMethodsList->clear();
    ui->smallThermalMethodsList->addItem("归一化");
    ui->smallThermalMethodsList->addItem("平滑");
    ui->smallThermalMethodsList->addItem("差异度");
    
    // 默认选择第一项
    if (ui->smallThermalMethodsList->count() > 0) {
        ui->smallThermalMethodsList->setCurrentRow(0);
    }
}

void AlgorithmSetting::initializeChromatographyTab()
{
    // 初始化色谱数据处理方式列表
    ui->chromatographyMethodsList->clear();
    ui->chromatographyMethodsList->addItem("基线校正");
    ui->chromatographyMethodsList->addItem("峰识别");
    ui->chromatographyMethodsList->addItem("峰对齐");
    ui->chromatographyMethodsList->addItem("归一化");
    ui->chromatographyMethodsList->addItem("差异度");
    
    // 默认选择第一项
    if (ui->chromatographyMethodsList->count() > 0) {
        ui->chromatographyMethodsList->setCurrentRow(0);
    }
}

void AlgorithmSetting::updateAlgorithmStackForMethod(const QString& method, int tabIndex)
{
    // 根据处理方式和tab索引切换到对应的算法设置页面
    if (method == "归一化") {
        if (tabIndex == 0) { // 大热重
            ui->bigThermalParametersStackedWidget->setCurrentWidget(ui->bigThermalNormalizationPage);
        } else if (tabIndex == 1) { // 小热重
            ui->smallThermalParametersStackedWidget->setCurrentWidget(ui->smallThermalNormalizationPage);
        } else if (tabIndex == 2) { // 色谱
            ui->chromatographyParametersStackedWidget->setCurrentWidget(ui->chromatographyNormalizationPage);
        }
    } else if (method == "平滑") {
        if (tabIndex == 0) { // 大热重
            ui->bigThermalParametersStackedWidget->setCurrentWidget(ui->bigThermalSmoothPage);
        } else if (tabIndex == 1) { // 小热重
            ui->smallThermalParametersStackedWidget->setCurrentWidget(ui->smallThermalSmoothPage);
        }
    } else if (method == "差异度") {
        if (tabIndex == 0) { // 大热重
            ui->bigThermalParametersStackedWidget->setCurrentWidget(ui->bigThermalDifferencePage);
        } else if (tabIndex == 1) { // 小热重
            ui->smallThermalParametersStackedWidget->setCurrentWidget(ui->smallThermalDifferencePage);
        } else if (tabIndex == 2) { // 色谱
            ui->chromatographyParametersStackedWidget->setCurrentWidget(ui->chromatographyDifferencePage);
        }
    } else if (method == "基线校正") {
        if (tabIndex == 2) { // 色谱
            ui->chromatographyParametersStackedWidget->setCurrentWidget(ui->chromatographyBaselinePage);
        }
    } else if (method == "峰识别") {
        if (tabIndex == 2) { // 色谱
            ui->chromatographyParametersStackedWidget->setCurrentWidget(ui->chromatographyPeakDetectionPage);
        }
    } else if (method == "峰对齐") {
        if (tabIndex == 2) { // 色谱
            ui->chromatographyParametersStackedWidget->setCurrentWidget(ui->chromatographyPeakAlignmentPage);
        }
    }
}

void AlgorithmSetting::loadParametersForAlgorithm(const QString& method, const QString& algorithm)
{
    // 根据处理方式和具体算法加载对应的参数设置
    // 这里可以根据算法名称设置不同的默认参数值
    // 具体实现可以根据实际需求进行扩展
}

// 算法下拉框槽函数实现
void AlgorithmSetting::onBigThermalNormMethodChanged()
{
    QString algorithm = ui->bigThermalNormMethodComboBox->currentText();
    
    // 显示参数设置区域
    ui->bigThermalParametersStackedWidget->setVisible(true);
    
    // 根据选择的归一化算法切换参数设置页面
    if (algorithm == "最大值归一化") {
        ui->bigThermalNormAlgorithmStack->setCurrentWidget(ui->bigThermalMaxNormPage);
    } else if (algorithm == "面积归一化") {
        ui->bigThermalNormAlgorithmStack->setCurrentWidget(ui->bigThermalAreaNormPage);
    } else if (algorithm == "Z-score标准化") {
        ui->bigThermalNormAlgorithmStack->setCurrentWidget(ui->bigThermalZScoreNormPage);
    } else if (algorithm == "Min-Max归一化") {
        ui->bigThermalNormAlgorithmStack->setCurrentWidget(ui->bigThermalMinMaxNormPage);
    } else if (algorithm == "单位向量归一化") {
        ui->bigThermalNormAlgorithmStack->setCurrentWidget(ui->bigThermalUnitVectorNormPage);
    }
}

void AlgorithmSetting::onBigThermalSmoothMethodChanged()
{
    QString algorithm = ui->bigThermalSmoothMethodComboBox->currentText();
    
    // 显示参数设置区域
    ui->bigThermalParametersStackedWidget->setVisible(true);
    
    // 根据选择的平滑算法切换参数设置页面
    if (algorithm == "移动平均") {
        ui->bigThermalSmoothAlgorithmStack->setCurrentWidget(ui->bigThermalMovingAvgPage);
    } else if (algorithm == "Savitzky-Golay") {
        ui->bigThermalSmoothAlgorithmStack->setCurrentWidget(ui->bigThermalSavGolPage);
    } else if (algorithm == "高斯滤波") {
        ui->bigThermalSmoothAlgorithmStack->setCurrentWidget(ui->bigThermalGaussianPage);
    } else if (algorithm == "中值滤波") {
        ui->bigThermalSmoothAlgorithmStack->setCurrentWidget(ui->bigThermalMedianPage);
    } else if (algorithm == "Butterworth滤波") {
        ui->bigThermalSmoothAlgorithmStack->setCurrentWidget(ui->bigThermalButterworthPage);
    }
}

void AlgorithmSetting::onSmallThermalNormMethodChanged()
{
    QString algorithm = ui->smallThermalNormMethodComboBox->currentText();
    
    // 显示参数设置区域
    ui->smallThermalParametersStackedWidget->setVisible(true);
    
    // 小热重数据归一化算法切换逻辑
    // 目前UI结构较简单，主要处理参数可见性和默认值
    if (algorithm == "最大值归一化") {
        // 设置归一化范围的默认值
        ui->smallThermalNormMinSpinBox->setValue(0.0);
        ui->smallThermalNormMaxSpinBox->setValue(1.0);
    } else if (algorithm == "面积归一化") {
        // 面积归一化通常不需要范围设置
        ui->smallThermalNormMinSpinBox->setValue(0.0);
        ui->smallThermalNormMaxSpinBox->setValue(1.0);
    } else if (algorithm == "Z-score标准化") {
        // Z-score标准化的典型范围
        ui->smallThermalNormMinSpinBox->setValue(-3.0);
        ui->smallThermalNormMaxSpinBox->setValue(3.0);
    }
}

void AlgorithmSetting::onSmallThermalSmoothMethodChanged()
{
    QString algorithm = ui->smallThermalSmoothMethodComboBox->currentText();
    
    // 显示参数设置区域
    ui->smallThermalParametersStackedWidget->setVisible(true);
    
    // 小热重数据平滑算法切换逻辑
    // 根据不同算法设置合适的窗口大小默认值
    if (algorithm == "移动平均") {
        ui->smallThermalSmoothWindowSpinBox->setValue(5);
        ui->smallThermalSmoothWindowSpinBox->setMinimum(3);
        ui->smallThermalSmoothWindowSpinBox->setMaximum(101);
    } else if (algorithm == "Savitzky-Golay") {
        ui->smallThermalSmoothWindowSpinBox->setValue(7);
        ui->smallThermalSmoothWindowSpinBox->setMinimum(5);
        ui->smallThermalSmoothWindowSpinBox->setMaximum(51);
    } else if (algorithm == "高斯滤波") {
        ui->smallThermalSmoothWindowSpinBox->setValue(5);
        ui->smallThermalSmoothWindowSpinBox->setMinimum(3);
        ui->smallThermalSmoothWindowSpinBox->setMaximum(21);
    }
}

void AlgorithmSetting::onChromatographyBaselineMethodChanged()
{
    QString algorithm = ui->chromatographyBaselineMethodComboBox->currentText();
    
    // 显示参数设置区域
    ui->chromatographyParametersStackedWidget->setVisible(true);
    
    // 根据选择的基线校正算法切换参数设置页面
    if (algorithm == "线性基线") {
        ui->chromatographyBaselineAlgorithmStack->setCurrentWidget(ui->chromatographyLinearBaselinePage);
    } else if (algorithm == "多项式基线") {
        ui->chromatographyBaselineAlgorithmStack->setCurrentWidget(ui->chromatographyPolynomialBaselinePage);
    } else if (algorithm == "自适应基线") {
        ui->chromatographyBaselineAlgorithmStack->setCurrentWidget(ui->chromatographyAdaptiveBaselinePage);
    } else if (algorithm == "AIRPLS基线") {
        ui->chromatographyBaselineAlgorithmStack->setCurrentWidget(ui->chromatographyAIRPLSBaselinePage);
    } else if (algorithm == "AsLS基线") {
        ui->chromatographyBaselineAlgorithmStack->setCurrentWidget(ui->chromatographyAsLSBaselinePage);
    }
}