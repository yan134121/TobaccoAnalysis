#ifndef ALGORITHMSETTING_H
#define ALGORITHMSETTING_H

#include <QWidget>
#include <QDialog>

namespace Ui {
class AlgorithmSetting;
}

class AlgorithmSetting : public QDialog
{
    Q_OBJECT

public:
    explicit AlgorithmSetting(QWidget *parent = nullptr);
    ~AlgorithmSetting();

    // 显示特定的设置页面
    void showAlignmentPage();
    void showNormalizationPage();
    void showSmoothPage();
    void showBaselinePage();
    void showPeakAlignPage();
    void showDifferencePage();

private slots:
    // 主要按钮槽函数
    void onApplyButtonClicked();
    void onOkButtonClicked();
    void onCancelButtonClicked();

    // QTabWidget交互槽函数
    void onTabChanged(int index);
    void onBigThermalMethodChanged();
    void onSmallThermalMethodChanged();
    void onChromatographyMethodChanged();
    
    // 算法下拉框槽函数
    void onBigThermalNormMethodChanged();
    void onBigThermalSmoothMethodChanged();
    void onSmallThermalNormMethodChanged();
    void onSmallThermalSmoothMethodChanged();
    void onChromatographyBaselineMethodChanged();

    // 保留旧的槽函数以保持兼容性
    void onAlignmentOkButtonClicked();
    void onAlignmentCancelButtonClicked();
    void onNormalizationOkButtonClicked();
    void onNormalizationCancelButtonClicked();
    void onSmoothOkButtonClicked();
    void onSmoothCancelButtonClicked();
    void onBaselineOkButtonClicked();
    void onBaselineCancelButtonClicked();
    void onPeakAlignOkButtonClicked();
    void onPeakAlignCancelButtonClicked();
    void onDifferenceOkButtonClicked();
    void onDifferenceCancelButtonClicked();

private:
    Ui::AlgorithmSetting *ui;
    void setupConnections();
    void initializeUI();
    bool applySettings();
    
    // QTabWidget界面管理方法
    void initializeBigThermalTab();
    void initializeSmallThermalTab();
    void initializeChromatographyTab();
    void updateAlgorithmStackForMethod(const QString& method, int tabIndex);
    void loadParametersForAlgorithm(const QString& method, const QString& algorithm);
};

#endif // ALGORITHMSETTING_H