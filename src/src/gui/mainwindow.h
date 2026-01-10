
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTextEdit>

#include <QTreeWidgetItem> // 为 QTreeWidgetItem* 参数提供完整定义
#include <QSharedPointer> // 需要为槽函数参数提供类型定义

#include "gui/IInteractiveView.h"
#include "gui/dialogs/AlgorithmSetting.h" // 包含算法设置对话框
#include "core/common.h"
#include "ChartView.h"
#include "dialogs/DifferenceTableDialog.h"
#include "AppInitializer.h"
#include "gui/views/workbenches/TgBigDifferenceWorkbench.h"
#include "views/workbenches/TgSmallDifferenceWorkbench.h"
#include "views/workbenches/ChromatographDifferenceWorkbench.h"
#include "views/workbenches/ProcessTgBigDifferenceWorkbench.h"

// 前置声明对于普通的成员变量指针是OK的
class QMdiArea;
class QDockWidget;
class DataNavigator;
class QAction;
class QMenu;
class QToolBar;
class QActionGroup;
class QMdiSubWindow;
class AlgorithmSetting;
class TgBigDataProcessDialog;
class TgSmallDataProcessDialog;
class ChromatographDataProcessDialog;
class ProcessTgBigDataProcessDialog;
class QLineEdit; // 搜索框前置声明（中文注释）
class QLabel;    // 标签前置声明（中文注释）


// QT_BEGIN_NAMESPACE ... (如果 Qt Creator 自动生成了)

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    // MainWindow(QWidget *parent = nullptr);
    MainWindow(AppInitializer* initializer, QWidget *parent = nullptr);
    ~MainWindow();

    IInteractiveView* activeInteractiveView() const;  // 🔹加上

    void logToOperationPanel(const QString &msg);
    void printWindowInfo();

private slots:
    void updateActions();
    void onZoomIn();
    void onZoomOut();
    void onZoomReset();
    // 现在 MOC 认识 QTreeWidgetItem 了，所以这个声明是合法的
    void onSampleDoubleClicked(QTreeWidgetItem *item, int column);
    void onActiveSubWindowChanged(QMdiSubWindow* window);
    
    // 处理打开样本窗口的请求
    void onOpenSampleViewWindow(int id,
        const QString &projectName, 
                            const QString &batchCode, const QString &shortCode,
                            int parallelNo, const QString &dataType);

    void onToolActionTriggered(QAction* action);      // 🔹加上
    
    void onSelectArrowClicked();
    
    // 算法设置菜单槽函数
    void onAlignmentActionTriggered();
    void onNormalizeActionTriggered();
    void onSmoothActionTriggered();
    void onBaseLineActionTriggered();
    void onPeakLineActionTriggered();
    void onDiffActionTriggered();
    void onAboutActionTriggered();
    
    // 新的按数据类型分类的算法设置槽函数
    void onBigThermalAlgorithmTriggered();
    void onSmallThermalAlgorithmTriggered();
    void onChromatographyAlgorithmTriggered();

    void onExportTable();
    void onExportPlot();
    void on_m_diffAction_triggered(); // 添加这一行
    void on_m_alignAction_triggered();

    void onTgBigDataProcessActionTriggered();
    void onTgSmallDataProcessActionTriggered();
    void onChromatographDataProcessActionTriggered();
    void onProcessTgBigDataProcessActionTriggered();
    // 打印所有选中样本（按数据类型分组）
    void onPrintSelectedSamplesGrouped();
    
    // 处理样本选择状态变化
    void onSampleSelectionChanged(int sampleId, const QString &sampleName, 
                                 const QString &dataType, bool selected);
    void onSelectAllSamplesInBatch(const NavigatorNodeInfo& batchInfo);

    void on_actionOpenSingleMaterialData_triggered();

public slots:
    // 用于响应创建新工作台请求的槽函数
    // void onCreateDifferenceWorkbench(QSharedPointer<Curve> referenceCurve,
    //                                  const QList<QSharedPointer<Curve>>& allCurves);
    // void onCreateDifferenceWorkbench(int referenceSampleId,
    //                                  const BatchMultiStageData& allProcessedData);
    // void onDifferenceWorkbenchClosed(TgBigDifferenceWorkbench* wb);

    void onCreateTgBigDifferenceWorkbench(int refId, const BatchGroupData& data, const ProcessingParameters& params);
    void onCreateTgSmallDifferenceWorkbench(int refId, const BatchGroupData& data, const ProcessingParameters& params);
    void onCreateChromatographDifferenceWorkbench(int referenceSampleId, const BatchGroupData& allProcessedData, const ProcessingParameters& params);
    void onCreateProcessTgBigDifferenceWorkbench(int referenceSampleId, const BatchGroupData& allProcessedData, const ProcessingParameters& params);

    void onTgBigDifferenceWorkbenchClosed(TgBigDifferenceWorkbench* wb);
    void onTgSmallDifferenceWorkbenchClosed(TgSmallDifferenceWorkbench * wb);
    void onChromatographDifferenceWorkbenchClosed(ChromatographDifferenceWorkbench* wb);
    void onProcessTgBigDifferenceWorkbenchClosed(ProcessTgBigDifferenceWorkbench* wb);

    

    void onSubWindowActivated(QMdiSubWindow *window);
    
    
    


private:
    void createActions();
    void createMenus();
    void createToolBars();
    void createStatusBar();
    void setupUiLayout(); // 将这个函数声明也加上，保持一致性

    // --- 成员变量 ---
    QMdiArea* m_mdiArea = nullptr;
    QDockWidget* m_navigatorDock;
    QDockWidget* m_propertiesDock;
    QDockWidget* m_analysisDock;
    QDockWidget* m_operationLogDock; // 日志面板
    QTextEdit* m_operationLogWidget;

    DataNavigator* m_navigator;
    DataNavigator* m_dataNavigator;

    // ChartView* m_chartView;
    ChartView* m_chartView = nullptr;

    // --- Actions ---
    QAction* m_importAction;
    QAction* m_exportAction;
    QAction* m_exitAction;
    QAction* m_zoomInAction;
    QAction* m_zoomOutAction;
    QAction* m_zoomResetAction;
    QAction* m_aboutAction;

    // (模式工具 Actions)
    QAction* m_selectAction;
    QAction* m_panAction;
    QAction* m_zoomRectAction;

    // 算法工具 Actions
    QAction* m_analysisAction;
    
    // 算法设置工具 Actions
    QAction* m_alignActionTriggered;
    QAction* m_normalizeActionTriggered;
    QAction* m_smoothActionTriggered;
    QAction* m_baseLineActionTriggered;
    QAction* m_peakLineActionTriggered;
    QAction* m_diffActionTriggered;
    
    // 算法设置对话框
    AlgorithmSetting* m_algorithmSetting;
    
    // 数据处理对话框
    TgBigDataProcessDialog* tgBigDataProcessDialog;
    TgSmallDataProcessDialog* tgSmallDataProcessDialog;
    ChromatographDataProcessDialog* chromatographDataProcessDialog;
    ProcessTgBigDataProcessDialog* processTgBigDataProcessDialog;
    TgBigDifferenceWorkbench* m_tgBigDifferenceWorkbench = nullptr;
    TgSmallDifferenceWorkbench* m_tgSmallDifferenceWorkbench = nullptr;
    ChromatographDifferenceWorkbench* m_chromatographDifferenceWorkbench = nullptr;
    ProcessTgBigDifferenceWorkbench* m_processTgBigDifferenceWorkbench = nullptr;


    // // 动作
    // QAction *newAct;
    // QAction *openAct;
    // QAction *saveAct;
    // QAction *saveAsAct;
    // QAction *printAct;
    // QAction *exitAct;
    
    // QAction *cutAct;
    // QAction *copyAct;
    // QAction *pasteAct;
    // QAction *deleteAct;
    // QAction *undoAct;
    // QAction *redoAct;
    
    // QAction *zoomInAct;
    // QAction *zoomOutAct;
    // QAction *fitToViewAct;
    
    // QAction *settingsAct;
    
    QAction* m_helpAction;
    // QAction* m_aboutAction;

    // 文件菜单
    QAction* m_newAction;
    QAction* m_openAction;
    QAction* m_saveAction;
    QAction* m_saveAsAction;
    QAction* m_printAction;
    // QAction* m_exitAction;
    QAction *m_exportPlotAction, *m_exportDataAction, *m_exportTableAction;



    // 编辑菜单
    QAction* m_undoAction;
    QAction* m_redoAction;
    QAction* m_cutAction;
    QAction* m_copyAction;
    QAction* m_pasteAction;
    QAction* m_deleteAction;
    // 视图菜单 (除了缩放，还可以控制Dock的显示/隐藏)
    QAction* m_navigatorDockAction;
    QAction* m_propertiesDockAction;
    QAction* m_operationLogDockAction, *m_analysisDockAction;

    //  算法设置菜单
    //对齐、归一化、平滑处理、底对齐、峰对齐、差异度
    QAction* m_alignAction;
    QAction* m_normalizeAction;
    QAction* m_smoothAction; 
    QAction* m_baseLineAction;
    QAction* m_peakLineAction;
    QAction* m_diffAction;
    // 打印选中样本（按类型分组）的动作
    QAction* m_printSelectedSamplesGroupedAction = nullptr;
    
    // 新的按数据类型分类的算法设置Action
    QAction* m_bigThermalAlgorithmAction;
    QAction* m_smallThermalAlgorithmAction;
    QAction* m_chromatographyAlgorithmAction;

    // 数据处理菜单
    // 数据处理菜单
    QAction *tgBigDataProcessAction, *tgSmallDataProcessAction, *chromatographDataProcessAction;
    QAction *processTgBigDataProcessAction;//大热重工序数据处理
    

    QToolBar *fileToolBar;
    QToolBar *editToolBar;
    QToolBar *viewToolBar;
    QToolBar* commonToolBar;
    QToolBar *algorithmToolBar, *dataProcessToolBar;

    


    QActionGroup* m_toolsActionGroup; // 用于管理工具的互斥性

    QActionGroup *dataTools;

    // // 保存算法处理结果到数据库
    // void saveAlgorithmResults(const QVector<QPair<QVector<double>, QVector<double>>>& processedData, 
    //                          const QString& algoName, int runNo, 
    //                          const QString& rawType = "small", int rawId = 1);

    AppInitializer* m_appInitializer = nullptr;  // 如果你有 AppInitializer
    SingleTobaccoSampleService* m_singleTobaccoSampleService = nullptr;


    QToolBar* m_searchToolBar = nullptr;   // 搜索工具栏
    QLineEdit* m_searchEdit = nullptr;     // 搜索输入框
    QAction* m_clearSearchAction = nullptr; // 清除搜索条件
    QAction* m_searchHelpAction = nullptr;  // 搜索帮助（显示层级规则说明）
    QAction* m_doSearchAction = nullptr;    // 执行搜索动作（按钮）
    QLabel* m_searchLabel = nullptr;        // “搜索”字样标签
};


#endif // MAINWINDOW_H
