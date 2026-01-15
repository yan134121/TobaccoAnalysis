#ifndef TGSMALLDATAPROCESSDIALOG_H
#define TGSMALLDATAPROCESSDIALOG_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QTreeWidget>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QTextEdit>
#include <QProgressBar>
#include <QMap>
#include <QToolBar>
#include <QAction>
#include <QtCharts/QLineSeries>
#include <QtCharts/QChart>
#include <QMetaType>
#include "data_access/SampleDAO.h"
#include "data_access/NavigatorDAO.h"
#include "gui/views/ChartView.h"
#include "TgSmallParameterSettingsDialog.h"
#include "src/services/DataProcessingService.h"
#include "core/common.h"
#include "AppInitializer.h"
#include "core/singletons/SampleSelectionManager.h"

#include <QTabWidget>
#include <QListWidget>
#include <QSet>
#include <QHash>
#include <QVector>
#include <QPointF>

QT_CHARTS_USE_NAMESPACE

// 使用common.h中定义的NavigatorNodeInfo

// 前置声明
class DataNavigator;
class TgSmallParameterSettingsDialog;
class TgBigParameterSettingsDialog;

namespace Ui {
class TgSmallDataProcessDialog;
}

class TgSmallDataProcessDialog : public QWidget
{
    Q_OBJECT

public:
    explicit TgSmallDataProcessDialog(QWidget *parent = nullptr,
                                      AppInitializer* appInitializer = nullptr,
                                      DataNavigator *mainNavigator = nullptr,
                                      const QString& dataTypeName = QString("小热重"));
    ~TgSmallDataProcessDialog();
    
    // 选择指定的样本并显示其曲线
    void selectSample(int sampleId, const QString& sampleName);
    
    // 添加和移除样本曲线
    void addSampleCurve(int sampleId, const QString& sampleName);
    void removeSampleCurve(int sampleId);
    
    // 获取所有选中的样本ID和名称
    QMap<int, QString> getSelectedSamples() const;
    
    // 统计选中的样本数量
    int countSelectedSamples() const;
    
    // 显示选中样本的统计信息
    void showSelectedSamplesStatistics();

    void recalculateAndUpdatePlot();

    void updatePlot();
    void updateLegendPanel();

signals:
    // 当样本被选中时发出信号，通知主窗口设置导航树选择框状态
    void sampleSelected(int sampleId, bool selected);

    // void requestNewDifferenceWorkbench(const QVariantMap& referenceCurve,
    //                                    const QList<QVariantMap>& allDerivativeCurves);
    // void requestNewDifferenceWorkbench(QSharedPointer<Curve> referenceCurve,
    //                                    const QList<QSharedPointer<Curve>>& allDerivativeCurves);
    void requestNewTgSmallDifferenceWorkbench(int referenceSampleId,
                                       const BatchGroupData& allProcessedData,
                                       const ProcessingParameters& params);


private slots:
    void onCancelButtonClicked();
    void onParameterChanged();
    void onLeftItemClicked(QTreeWidgetItem *item, int column);
    void onSampleItemClicked(QTreeWidgetItem *item, int column);
    void onItemChanged(QTreeWidgetItem *item, int column);
    void onParameterSettingsClicked();

    void onParametersApplied(const ProcessingParameters &newParams);

    // 负责接收后台计算结果的槽函数
    void onCalculationFinished();

    void onProcessAndPlotButtonClicked();
    void onStartComparison();
    void onClearCurvesClicked(); // 清除曲线按钮槽函数
    
    // 处理左侧导航器的右键菜单
    void onLeftNavigatorContextMenu(const QPoint &pos);
    
    // 显示/隐藏导航树
    // void onToggleNavigatorClicked();
    void onDrawAllSelectedCurvesClicked(); // 绘制所有选中曲线
    void onUnselectAllSamplesClicked();    // 取消所有选中样本
    void onPickBestCurveClicked();         // 返回最优曲线

    // 显示/隐藏左侧标签页（导航 + 选中样本）
    void onToggleNavigatorClicked();

    // 选中样本列表的勾选框变化
    void onSelectedSamplesListItemChanged(QListWidgetItem* item);

    // 主导航样本选中状态变化的槽函数（带数据类型），用于动态同步小热重导航树
    void onMainNavigatorSampleSelectionChanged(int sampleId, const QString& sampleName, const QString& dataType, bool selected);
    // void onStringsUpdated();
    // void onSamplesUpdated(const QString& dataType);

public:
    // 批量选择批次下所有样本
    void onSelectAllSamplesInBatch(const QString& projectName, const QString& batchCode);
    
private:
    void setupUI();
    void setupConnections();
    void setupLeftNavigator();
    
    // 当前选中的批次节点
    QTreeWidgetItem* m_currentBatchItem = nullptr;
    void setupMiddlePanel();
    void setupRightPanel();
    void loadNavigatorData();
    void loadNavigatorDataFromDatabase();
    void loadNavigatorDataFromMainNavigator();
    void loadSampleCurve(int sampleId);
    
    // 在左侧导航树中查找样本节点（通过样本ID），若不存在返回nullptr
    QTreeWidgetItem* findSampleItemById(int sampleId) const;
    
    // 绘制所有选中的样本曲线
    void drawSelectedSampleCurves();
    
    // 参数重置
    void resetParameters();
    void updateRightPanel(QTreeWidgetItem *item);
    
    QString m_dataTypeName;
    
    // 存储样本曲线的映射表 <样本ID, 曲线对象>
    QMap<int, QLineSeries*> m_sampleCurves;

    // 主布局组件
    QVBoxLayout* m_mainLayout;
    QSplitter* m_mainSplitter;
    
    // 左侧导航树面板
    QWidget* m_leftPanel;
    QVBoxLayout* m_leftLayout;
    QTabWidget* m_leftTabWidget;     // 左侧标签页容器（样本导航 / 选中样本）
    QTreeWidget* m_leftNavigator;    // 样本导航树
    QListWidget* m_selectedSamplesList; // 选中样本名称列表
    QLabel* m_selectedStatsLabel;    // 显示“已选中/绘图样本数”的统计标签
    QMap<int, QListWidgetItem*> m_selectedItemMap; // 列表项复用，按样本ID索引
    
    
    // 中间绘图面板
    QWidget* m_middlePanel;
    // QVBoxLayout* m_middleLayout;
    QGridLayout* m_middleLayout;
    // ChartView* m_chartView;
    QChart* m_chart;

    QWidget* m_legendPanel = nullptr;
    QVBoxLayout* m_legendLayout = nullptr;

    ChartView* m_chartView1 = nullptr;
    ChartView* m_chartView2 = nullptr;
    ChartView* m_chartView3 = nullptr;
    ChartView* m_chartView4 = nullptr;
    ChartView* m_chartView5 = nullptr;

    
    // 右侧控制面板
    QWidget* m_rightPanel;
    QVBoxLayout* m_rightLayout;
    // QGroupBox* m_methodGroup;
    // QComboBox* m_methodCombo;
    // QGroupBox* m_paramGroup;
    QLabel* m_tempRangeLabel;
    QDoubleSpinBox* m_tempMinSpin;
    QDoubleSpinBox* m_tempMaxSpin;
    QLabel* m_smoothingLabel;
    QSpinBox* m_smoothingSpin;
    QCheckBox* m_baselineCorrectionCheck;
    QCheckBox* m_peakDetectionCheck;
    // QTextEdit* m_resultText;
    // QProgressBar* m_progressBar;
    
    // 底部按钮
    QHBoxLayout* m_buttonLayout;
    // QPushButton* m_processButton;
    // QPushButton* m_resetButton;
    // QPushButton* m_cancelButton;
    QPushButton* m_parameterButton;
    QPushButton* m_processAndPlotButton;
    QPushButton* m_startComparisonButton;
    QPushButton* m_toggleNavigatorButton; // 添加导航树显示/隐藏按钮
    QPushButton* m_clearCurvesButton; // 清除曲线按钮
    QPushButton* m_drawAllButton; // 绘制所有选中曲线按钮
    QPushButton* m_unselectAllButton; // 取消所有选中样本按钮
    QPushButton* m_pickBestCurveButton; // 返回最优曲线按钮
    
    // 数据访问
    SampleDAO m_sampleDao;
    NavigatorDAO m_navigatorDao;
    QList<QVariantMap> m_smallTgSamples;
    
    // 存储选中的大热重样本，键为样本ID，值为样本名称
    QMap<int, QString> m_selectedSamples;
    // 当前需要显示曲线的样本ID集合（列表勾选控制）
    QSet<int> m_visibleSamples;
    // 批次选择绘图去抖标记
    bool m_drawScheduled = false;

    // 曲线数据与图例名称缓存，降低重复数据库访问与字符串拼接
    QHash<int, QVector<QPointF>> m_curveCache;   // <样本ID, 曲线点缓存>
    QMap<int, QString> m_legendNameCache;        // <样本ID, 图例名称缓存>
    
    // 主界面导航树引用
    DataNavigator *m_mainNavigator;
    
    // 图表工具栏和操作
    QToolBar* m_chartToolBar;
    QAction* m_zoomInAction;
    QAction* m_zoomOutAction;
    QAction* m_zoomResetAction;

    QTabWidget* tabWidget;
    QWidget* tab1Widget;
QVBoxLayout* tab1Layout;
// QWidget* tab2Widget;
// QWidget* tab3Widget;

ProcessingParameters m_currentParams; 
DataProcessingService* m_processingService = nullptr; // 指向后台服务

// int m_sampleId;                         // 当前正在分析的样本ID
// MultiStageData m_stageDataCache;

// 【升级】缓存也变成了一个 Map
    BatchGroupData m_stageDataCache;

    AppInitializer* m_appInitializer = nullptr; // <-- 新增成员变量

    TgSmallParameterSettingsDialog *m_smallParamDialog = nullptr;
    TgBigParameterSettingsDialog *m_bigParamDialog = nullptr;
    QMap<int, SampleIdentifier> sampleIdMap;

    // 根据样本ID构造统一显示名称 short_code(parallel_no)-timestamp
    QString buildSampleDisplayName(int sampleId);

    // 用于抑制因程序化设置复选框状态而触发的 itemChanged 递归
    bool m_suppressItemChanged = false;

    // 刷新左侧“选中样本”列表显示
    void updateSelectedSamplesList();
    // 刷新顶部统计信息（已选中样本总数 / 绘图样本数）
    void updateSelectedStatsInfo();
    // 调度一次异步重绘与图例刷新，避免频繁调用造成阻塞
    void scheduleRedraw();
    // 确保列表中存在对应样本的条目
    void ensureSelectedListItem(int sampleId, const QString& displayName);
    // 从列表与索引移除条目
    void removeSelectedListItem(int sampleId);
    // 如无缓存则异步预取曲线数据并缓存
    void prefetchCurveIfNeeded(int sampleId);

    QDialog* activeParameterDialog() const;
    ProcessingParameters activeDialogParameters() const;




};

#endif // TGSMALLDATAPROCESSDIALOG_H
