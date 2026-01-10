#include "Logger.h" 
#include "MainWindow.h"
#include "core/common.h"                  // 为 NavigatorNodeInfo 提供定义
#include "core/singletons/StringManager.h"
#include "gui/views/SampleViewWindow.h"
#include "gui/views/DataNavigator.h"
#include "gui/views/ChartView.h"          // 为 ChartView 提供定义
#include "core/singletons/SampleSelectionManager.h" // 引入样本选中集中管理器
#include "data_access/SingleTobaccoSampleDAO.h" // 用于根据样本ID获取标识符构造图例名
#include "gui/IInteractiveView.h" // 包含接口头文件
#include "gui/dialogs/AlgorithmSetting.h" // 
#include "gui/dialogs/TgBigDataProcessDialog.h"
#include "gui/dialogs/TgSmallDataProcessDialog.h"
#include "gui/dialogs/ChromatographDataProcessDialog.h"
#include "gui/dialogs/ProcessTgBigDataProcessDialog.h"
// #include "WindowManager.h" // 未使用
#include "DifferenceTableDialog.h"
#include "gui/views/workbenches/TgBigDifferenceWorkbench.h"
#include "views/workbenches/TgSmallDifferenceWorkbench.h"

#include "src/gui/views/SingleMaterialDataWidget.h"

// 包含所有在 .cpp 中使用到的 Qt Widget 类的头文件
#include <QAction>
#include <QDockWidget>
#include <QIcon>
#include <QKeySequence>
#include <QLabel>                      
#include <QMdiArea>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QToolBar>
#include <QDebug>
#include <QActionGroup>
#include <QPixmap>


MainWindow::MainWindow(AppInitializer* initializer, QWidget *parent)
    : QMainWindow(parent)
    , m_appInitializer(initializer)
{
// MainWindow::MainWindow(QWidget *parent)
//     : QMainWindow(parent)
// {
    // setWindowTitle(STR("app.title"));
    setWindowTitle("烟草数据分析系统 - v1.3");
    setWindowState(Qt::WindowMaximized);
    setupUiLayout();
    DEBUG_LOG;
    createActions();
    DEBUG_LOG;
    createMenus();
    DEBUG_LOG;
    createToolBars();
    DEBUG_LOG;
    createStatusBar();
    DEBUG_LOG;

    // 初始化服务
    if (m_appInitializer) {
        m_singleTobaccoSampleService = m_appInitializer->getSingleTobaccoSampleService();
        if (!m_singleTobaccoSampleService) {
            FATAL_LOG << "SingleTobaccoSampleService 未初始化";
        }
    }
    
    // 创建算法设置对话框
    m_algorithmSetting = new AlgorithmSetting(this);
    
    // 初始化数据处理对话框指针
    tgBigDataProcessDialog = nullptr;
    tgSmallDataProcessDialog = nullptr;
    tgSmallRawDataProcessDialog = nullptr;
    chromatographDataProcessDialog = nullptr;
    processTgBigDataProcessDialog = nullptr;

    // m_chartView = new ChartView(this);
    // m_navigator->refreshNavigator();
    // m_navigator->refreshDataSource(); // 调用新方法
    // 在构造函数中
DEBUG_LOG << "m_navigator" << m_navigator;
    if (m_navigator) {
        m_navigator->refreshDataSource(); // 调用新方法
    } else {
        FATAL_LOG << "Failed to create DataNavigator widget!";
    }
    // 建立信号槽连接
    // connect(m_navigator, &QTreeWidget::itemDoubleClicked, this, &MainWindow::onSampleDoubleClicked);

    connect(m_navigator, &DataNavigator::requestOpenSampleViewWindow,
        this, &MainWindow::onOpenSampleViewWindow);

        
    connect(m_mdiArea, &QMdiArea::subWindowActivated, this, &MainWindow::updateActions);
    // 连接导航树的双击事件
    // connect(m_navigator, &QTreeWidget::itemDoubleClicked, this, &MainWindow::onSampleDoubleClicked);
    // 【新】连接 MDI 区域的信号，以更新导航树中的"打开的视图"列表
    connect(m_mdiArea, &QMdiArea::subWindowActivated, this, [=](QMdiSubWindow* window){
        m_navigator->setActiveView(window);
    });
    
    // 连接DataNavigator的requestOpenSampleViewWindow信号到MainWindow的onOpenSampleViewWindow槽
    connect(m_navigator, &DataNavigator::requestOpenSampleViewWindow, 
            this, &MainWindow::onOpenSampleViewWindow);
    
    // 连接样本选择状态变化信号
    connect(m_navigator, &DataNavigator::sampleSelectionChanged,
            this, &MainWindow::onSampleSelectionChanged);

    // 订阅 SampleSelectionManager 的选中变化，统一同步到主导航树
    connect(SampleSelectionManager::instance(), &SampleSelectionManager::selectionChanged,
            this, [this](int sampleId, bool selected, const QString& origin){
                Q_UNUSED(origin);
                if (m_navigator) {
                    m_navigator->setSampleCheckState(sampleId, selected);
                }
            });

    // 订阅 SampleSelectionManager 的按数据类型选中变化，用于驱动各数据处理界面的绘图逻辑（不再依赖 UI 广播）
    connect(SampleSelectionManager::instance(), &SampleSelectionManager::selectionChangedByType,
            this, [this](int sampleId, const QString& dataType, bool selected, const QString& origin){
                Q_UNUSED(origin);
                // 规范化数据类型（兼容英文代号与中文名称），统一路由判断
                auto normalizeType = [](const QString& t) -> QString {
                    QString s = t.trimmed();
                    if (s.compare("TG_BIG", Qt::CaseInsensitive) == 0 || s == QStringLiteral("大热重")) return QStringLiteral("大热重");
                    if (s.compare("TG_SMALL", Qt::CaseInsensitive) == 0 || s == QStringLiteral("小热重")) return QStringLiteral("小热重");
                    if (s.compare("TG_SMALL_RAW", Qt::CaseInsensitive) == 0 || s == QStringLiteral("小热重（原始数据）")) return QStringLiteral("小热重（原始数据）");
                    if (s.compare("CHROMATOGRAM", Qt::CaseInsensitive) == 0 || s == QStringLiteral("色谱")) return QStringLiteral("色谱");
                    if (s.compare("PROCESS_TG_BIG", Qt::CaseInsensitive) == 0 || s == QStringLiteral("工序大热重")) return QStringLiteral("工序大热重");
                    return s; // 未知类型原样返回
                };
                const QString typeKey = normalizeType(dataType);
                // 构造图例名（project-batch-short-parallel），避免依赖 DataNavigator 传入的 sampleName
                QString legendName;
                try {
                    SingleTobaccoSampleDAO sampleDao;
                    SampleIdentifier sid = sampleDao.getSampleIdentifierById(sampleId);
                    legendName = QString("%1-%2-%3-%4")
                                    .arg(sid.projectName)
                                    .arg(sid.batchCode)
                                    .arg(sid.shortCode)
                                    .arg(sid.parallelNo);
                } catch (...) {
                    // 兜底，若查询失败则用ID占位
                    legendName = QString("样本 %1").arg(sampleId);
                }

                // 根据数据类型路由到对应数据处理界面，调用 add/remove 曲线
                if (typeKey == QStringLiteral("大热重") && tgBigDataProcessDialog) {

                    // DEBUG_LOG << "MainWindow::onSampleSelectionChangedByType: " << sampleId << ", " << typeKey << ", " << selected;
                    if (selected) {
                        DEBUG_LOG << "MainWindow::onSampleSelectionChangedByType: " << sampleId << ", " << typeKey << ", " << selected;
                        tgBigDataProcessDialog->addSampleCurve(sampleId, legendName);
                    } else {

                        DEBUG_LOG << "MainWindow::onSampleSelectionChangedByType: else" << sampleId << ", " << typeKey << ", " << selected;
                        tgBigDataProcessDialog->removeSampleCurve(sampleId);
                    }
                } else if (typeKey == QStringLiteral("小热重") && tgSmallDataProcessDialog) {
                    if (selected) tgSmallDataProcessDialog->addSampleCurve(sampleId, legendName);
                    else tgSmallDataProcessDialog->removeSampleCurve(sampleId);
                } else if (typeKey == QStringLiteral("小热重（原始数据）") && tgSmallRawDataProcessDialog) {
                    if (selected) tgSmallRawDataProcessDialog->addSampleCurve(sampleId, legendName);
                    else tgSmallRawDataProcessDialog->removeSampleCurve(sampleId);
                } else if (typeKey == QStringLiteral("色谱") && chromatographDataProcessDialog) {
                    if (selected) chromatographDataProcessDialog->addSampleCurve(sampleId, legendName);
                    else chromatographDataProcessDialog->removeSampleCurve(sampleId);
                } else if (typeKey == QStringLiteral("工序大热重") && processTgBigDataProcessDialog) {
                    if (selected) processTgBigDataProcessDialog->addSampleCurve(sampleId, legendName);
                    else processTgBigDataProcessDialog->removeSampleCurve(sampleId);
                }
            });
            
    // 连接批次中选择所有样本的信号
    connect(m_navigator, &DataNavigator::requestSelectAllSamplesInBatch,
            this, &MainWindow::onSelectAllSamplesInBatch);

    connect(m_mdiArea, &QMdiArea::subWindowActivated,
        this, &MainWindow::onSubWindowActivated);


    DEBUG_LOG;
    
    updateActions(); // 初始化时调用一次

    // 设置最小宽度 800，高度 600
    this->setMinimumSize(800, 600);

    DEBUG_LOG;

}

// MainWindow::MainWindow(QWidget *parent)
//     : QMainWindow(parent)
// {
//     // 1. 基本窗口设置
//     // setWindowTitle(STR("app.title")); // 如果 StringManager 正常工作，请取消这行注释
//     setWindowTitle("烟草数据分析系统 - v1.3"); // 临时使用硬编码
//     setWindowState(Qt::WindowMaximized);

//     // 2. 初始化UI和Actions (严格按照此顺序)
//     setupUiLayout();      // 创建所有 Widget 实例 (m_navigator, m_mdiArea 等)
//     createActions();      // 创建所有 Action 实例
//     createMenus();        // 使用 Action 创建菜单
//     createToolBars();     // 使用 Action 创建工具栏
//     createStatusBar();    // 创建状态栏

//     // 3. 建立核心的信号槽连接
//     //    将导航树的双击事件连接到处理函数
//     connect(m_navigator, &QTreeWidget::itemDoubleClicked, this, &MainWindow::onSampleDoubleClicked);
    
//     //    当 MDI 区域的激活窗口变化时，执行统一的更新逻辑
//     connect(m_mdiArea, &QMdiArea::subWindowActivated, this, &MainWindow::onActiveSubWindowChanged);
    
//     // 4. 初始化状态和数据
//     //    更新 Action 的初始可用状态 (此时没有任何激活窗口)
//     updateActions(); 
    
//     //    加载导航树的初始数据
//     m_navigator->refreshDataSource();
// }

MainWindow::~MainWindow()
{
}

void MainWindow::setupUiLayout()
{
    DEBUG_LOG;
    m_mdiArea = new QMdiArea(this);
    setCentralWidget(m_mdiArea);

    m_navigatorDock = new QDockWidget(STR("main.dock.navigator"), this);
    m_navigator = new DataNavigator(m_navigatorDock);
    m_navigatorDock->setWidget(m_navigator);
    addDockWidget(Qt::LeftDockWidgetArea, m_navigatorDock);

    DEBUG_LOG;

    m_propertiesDock = new QDockWidget(STR("main.dock.properties"), this);
    // 现在编译器认识 QLabel 了
    m_propertiesDock->setWidget(new QLabel("属性详情面板", m_propertiesDock));
    addDockWidget(Qt::RightDockWidgetArea, m_propertiesDock);

    m_analysisDock = new QDockWidget(STR("main.dock.analysis"), this);
    m_analysisDock->setWidget(new QLabel("功能面板", m_analysisDock));
    addDockWidget(Qt::BottomDockWidgetArea, m_analysisDock);

    m_operationLogWidget = new QTextEdit(this);
    m_operationLogWidget->setReadOnly(true);  // 只显示日志，不允许编辑
    m_operationLogDock = new QDockWidget(STR("操作日志"), this);
    // m_operationLogDock->setWidget(new QLabel("操作日志面板", m_operationLogDock));
    m_operationLogDock->setWidget(m_operationLogWidget);
    
    addDockWidget(Qt::BottomDockWidgetArea, m_operationLogDock);

    // 设置操作日志文本框的最小尺寸，并调整底部停靠区域的初始高度
    // m_operationLogWidget->setMinimumSize(500, 180);
    QList<QDockWidget*> docks{ m_operationLogDock };
    QList<int> sizes{ 40 }; // 初始高度 220 像素
    resizeDocks(docks, sizes, Qt::Vertical);

    
    tabifyDockWidget(m_propertiesDock, m_analysisDock);

    DEBUG_LOG;
}


void MainWindow::printWindowInfo()
{
    DEBUG_LOG << "=== MDI 区域窗口信息 ===";
    
    // 获取所有子窗口
    QList<QMdiSubWindow*> windows = m_mdiArea->subWindowList();
    DEBUG_LOG << "总窗口数量:" << windows.count();
    
    // 获取活动窗口
    QMdiSubWindow* activeWindow = m_mdiArea->activeSubWindow();
    
    // 遍历所有窗口
    for (int i = 0; i < windows.count(); ++i) {
        QMdiSubWindow* window = windows[i];
        SampleViewWindow* sampleWindow = qobject_cast<SampleViewWindow*>(window->widget());
        
        DEBUG_LOG << "窗口" << i << ":";
        DEBUG_LOG << "  标题:" << window->windowTitle();
        DEBUG_LOG << "  指针:" << window;
        DEBUG_LOG << "  是否活动:" << (window == activeWindow ? "是" : "否");
        
        if (sampleWindow) {
            DEBUG_LOG << "  类型: SampleViewWindow";
            // DEBUG_LOG << "  ID:" << sampleWindow->getId();
            // DEBUG_LOG << "  项目:" << sampleWindow->projectName();
            // DEBUG_LOG << "  批次:" << sampleWindow->batchCode();
        } else {
            DEBUG_LOG << "  类型: 其他类型";
        }
        DEBUG_LOG << "-----------------------------------";
    }
    
    if (activeWindow) {
        DEBUG_LOG << "当前活动窗口:" << activeWindow->windowTitle();
    } else {
        DEBUG_LOG << "没有活动窗口";
    }
}

void MainWindow::onSampleDoubleClicked(QTreeWidgetItem *item, int /*column*/)
{
    if (!item) return;

    DEBUG_LOG << "Clicked item text:" << item->text(0);   // 显示节点文本
    if (m_mdiArea) {
        QMdiSubWindow* activeWindow = m_mdiArea->activeSubWindow();
        if (activeWindow) {
            DEBUG_LOG << "Clicked item:" << activeWindow->windowTitle();
        } else {
            DEBUG_LOG << "没有活动窗口";
        }
    }
        
    QVariant data = item->data(0, Qt::UserRole);
    if (!data.canConvert<NavigatorNodeInfo>()) return;

    NavigatorNodeInfo info = data.value<NavigatorNodeInfo>();

    DEBUG_LOG << "Clicked item:" << item->text(0)
             << "type =" << info.type
             << "projectName =" << info.projectName
             << "batchCode =" << info.batchCode
             << "shortCode =" << info.shortCode
             << "parallelNo =" << info.parallelNo
             << "dataType =" << info.dataType;
    
    // 只处理样本节点
    if (info.type != NavigatorNodeInfo::Sample) return;
    
    // 根据数据类型打开相应的处理对话框
    if (info.dataType == "大热重") {
        // 打开大热重数据处理对话框
        onTgBigDataProcessActionTriggered();
        DEBUG_LOG << "打开了大热重数据处理对话框";
        
        // 如果对话框存在，在左侧导航树中选中对应的样本
        if (tgBigDataProcessDialog) {
            DEBUG_LOG << "打开了大热重数据处理对话框";
            // 选中样本并更新导航树中的选择状态
            tgBigDataProcessDialog->selectSample(info.id, item->text(0));
            
            // 确保导航树中的选择框状态与对话框中的选择状态一致
            bool isChecked = item->checkState(0) == Qt::Checked;
            DEBUG_LOG << "样本双击：更新导航树中样本ID=" << info.id << "的选择状态为" << isChecked;
            if (m_navigator) {
                m_navigator->setSampleCheckState(info.id, isChecked);
            }
        }
    } 
    else if (info.dataType == "小热重") {
        // 打开小热重数据处理对话框
        onTgSmallDataProcessActionTriggered();
        
        // 如果对话框存在，在左侧导航树中选中对应的样本
        if (tgSmallDataProcessDialog) {
            // 选中样本并更新导航树中的选择状态
            tgSmallDataProcessDialog->selectSample(info.id, item->text(0));
            
            // 确保导航树中的选择框状态与对话框中的选择状态一致
            bool isChecked = item->checkState(0) == Qt::Checked;
            DEBUG_LOG << "样本双击：更新导航树中样本ID=" << info.id << "的选择状态为" << isChecked;
            if (m_navigator) {
                m_navigator->setSampleCheckState(info.id, isChecked);
            }
        }
    } 
    else if (info.dataType == "小热重（原始数据）") {
        onTgSmallRawDataProcessActionTriggered();

        if (tgSmallRawDataProcessDialog) {
            tgSmallRawDataProcessDialog->selectSample(info.id, item->text(0));

            bool isChecked = item->checkState(0) == Qt::Checked;
            DEBUG_LOG << "样本双击：更新导航树中样本ID=" << info.id << "的选择状态为" << isChecked;
            if (m_navigator) {
                m_navigator->setSampleCheckState(info.id, isChecked);
            }
        }
    }
    else if (info.dataType == "色谱") {
        // 打开色谱数据处理对话框
        onChromatographDataProcessActionTriggered();
        
        // 如果对话框存在，在左侧导航树中选中对应的样本
        if (chromatographDataProcessDialog) {
            // 选中样本并更新导航树中的选择状态
            chromatographDataProcessDialog->selectSample(info.id, item->text(0));
            
            // 确保导航树中的选择框状态与对话框中的选择状态一致
            bool isChecked = item->checkState(0) == Qt::Checked;
            DEBUG_LOG << "样本双击：更新导航树中样本ID=" << info.id << "的选择状态为" << isChecked;
            if (m_navigator) {
                m_navigator->setSampleCheckState(info.id, isChecked);
            }
        }
    }
    else if (info.dataType == "工序大热重") {
        // 打开工序大热重数据处理对话框
        onProcessTgBigDataProcessActionTriggered();
        
        // 如果对话框存在，在左侧导航树中选中对应的样本
        if (processTgBigDataProcessDialog) {
            // 选中样本并更新导航树中的选择状态
            processTgBigDataProcessDialog->selectSample(info.id, item->text(0));
            
            // 确保导航树中的选择框状态与对话框中的选择状态一致
            bool isChecked = item->checkState(0) == Qt::Checked;
            DEBUG_LOG << "样本双击：更新导航树中样本ID=" << info.id << "的选择状态为" << isChecked;
            if (m_navigator) {
                m_navigator->setSampleCheckState(info.id, isChecked);
            }
        }
    }
    // 暂时屏蔽原来的绘图窗口打开逻辑
    // else {
    //     // 对于其他数据类型，使用原来的逻辑
    //     m_navigator->requestOpenSampleViewWindow(info.id, info.projectName, 
    //                                           info.batchCode, info.shortCode,
    //                                           info.parallelNo, info.dataType);
    // }
}

// 处理打开样本窗口的请求
void MainWindow::onOpenSampleViewWindow(int sampleId,
    const QString &projectName, 
                                     const QString &batchCode, const QString &shortCode,
                                     int parallelNo, const QString &dataType)
{
    DEBUG_LOG << "Sample view window request received but redirecting to process dialogs:"
             << "projectName =" << projectName
             << "batchCode =" << batchCode
             << "shortCode =" << shortCode
             << "parallelNo =" << parallelNo
             << "dataType =" << dataType;
    
    // 根据数据类型打开相应的处理对话框
    if (dataType == "大热重") {
        // 打开大热重数据处理对话框
        onTgBigDataProcessActionTriggered();
        
        // 如果对话框存在，在左侧导航树中选中对应的样本
        if (tgBigDataProcessDialog) {
            DEBUG_LOG << "打开大热重数据处理对话框";
            QString sampleText = QString("%1-%2-%3").arg(batchCode).arg(shortCode).arg(parallelNo);
            // tgBigDataProcessDialog->selectSample(sampleId, sampleText);
        }
    } 
    else if (dataType == "小热重") {
        // 打开小热重数据处理对话框
        onTgSmallDataProcessActionTriggered();
        
        // 如果对话框存在，在左侧导航树中选中对应的样本
        if (tgSmallDataProcessDialog) {
            QString sampleText = QString("%1-%2-%3").arg(batchCode).arg(shortCode).arg(parallelNo);
            // tgSmallDataProcessDialog->selectSample(sampleId, sampleText);
        }
    } 
    else if (dataType == "小热重（原始数据）") {
        onTgSmallRawDataProcessActionTriggered();

        if (tgSmallRawDataProcessDialog) {
            QString sampleText = QString("%1-%2-%3").arg(batchCode).arg(shortCode).arg(parallelNo);
            Q_UNUSED(sampleText);
        }
    }
    else if (dataType == "色谱") {
        // 打开色谱数据处理对话框
        onChromatographDataProcessActionTriggered();
        
        // 如果对话框存在，在左侧导航树中选中对应的样本
        if (chromatographDataProcessDialog) {
            QString sampleText = QString("%1-%2-%3").arg(batchCode).arg(shortCode).arg(parallelNo);
            // chromatographDataProcessDialog->selectSample(sampleId, sampleText);
        }
    }
    else if (dataType == "工序大热重") {
        // 打开工序大热重数据处理对话框
        onProcessTgBigDataProcessActionTriggered();
        
        // 如果对话框存在，在左侧导航树中选中对应的样本
        if (processTgBigDataProcessDialog) {
            QString sampleText = QString("%1-%2-%3").arg(batchCode).arg(shortCode).arg(parallelNo);
            // processTgBigDataProcessDialog->selectSample(sampleId, sampleText);
        }
    }
    // 注意：不再创建单独的样本视图窗口，也不需要通知导航树
    m_navigator->enableSampleCheckboxesByDataType(dataType, true);
}





void MainWindow::updateActions()
{

    
    DEBUG_LOG;
    // QMdiSubWindow* activeSubWindow = m_mdiArea->activeSubWindow(); // 未使用的变量
    // 检查激活的窗口是否实现了我们的接口
    // bool isInteractive = activeSubWindow && qobject_cast<IInteractiveView*>(activeSubWindow->widget());
    bool isInteractive = false;
    if (qobject_cast<IInteractiveView*>(m_mdiArea->activeSubWindow())) {
        isInteractive = true;
    }

    DEBUG_LOG << isInteractive;
    LOG_DEBUG(QString("isInteractive: %1").arg(isInteractive));

    // bool hasActiveWindow = (activeSubWindow != nullptr);
    // m_zoomInAction->setEnabled(hasActiveWindow);
    // m_zoomOutAction->setEnabled(hasActiveWindow);
    // m_zoomResetAction->setEnabled(hasActiveWindow);

    m_zoomInAction->setEnabled(isInteractive);
    m_zoomOutAction->setEnabled(isInteractive);
    m_zoomResetAction->setEnabled(isInteractive);
    m_selectAction->setEnabled(isInteractive);
    m_panAction->setEnabled(isInteractive);
    m_zoomRectAction->setEnabled(isInteractive);
}

void MainWindow::onActiveSubWindowChanged(QMdiSubWindow *window)
{
    updateActions(); // 激活窗口变化时，更新按钮可用状态
    if (m_navigator) {
        m_navigator->setActiveView(window);
        
        // 根据窗口类型启用/禁用对应数据类型的样本选择框
        if (window) {
            QWidget* widget = window->widget();
            
            // 禁用所有数据类型的选择框
            m_navigator->enableSampleCheckboxesByDataType("大热重", false);
            m_navigator->enableSampleCheckboxesByDataType("小热重", false);
            m_navigator->enableSampleCheckboxesByDataType("小热重（原始数据）", false);
            m_navigator->enableSampleCheckboxesByDataType("色谱", false);
            m_navigator->enableSampleCheckboxesByDataType("工序大热重", false);
            
            // 根据窗口类型启用对应数据类型的选择框
            if (qobject_cast<TgBigDataProcessDialog*>(widget)) {
                m_navigator->enableSampleCheckboxesByDataType("大热重", true);
            } else if (qobject_cast<TgSmallDataProcessDialog*>(widget)) {
                QString dataTypeName = widget->property("dataTypeName").toString();
                if (dataTypeName.isEmpty()) {
                    dataTypeName = QStringLiteral("小热重");
                }
                m_navigator->enableSampleCheckboxesByDataType(dataTypeName, true);
            } else if (qobject_cast<ChromatographDataProcessDialog*>(widget)) {
                m_navigator->enableSampleCheckboxesByDataType("色谱", true);
            }else if (qobject_cast<ProcessTgBigDataProcessDialog*>(widget)) {
                m_navigator->enableSampleCheckboxesByDataType("工序大热重", true);
            }
        } else {
            // 如果没有活动窗口，禁用所有数据类型的选择框
            m_navigator->enableSampleCheckboxesByDataType("大热重", false);
            m_navigator->enableSampleCheckboxesByDataType("小热重", false);
            m_navigator->enableSampleCheckboxesByDataType("色谱", false);
            m_navigator->enableSampleCheckboxesByDataType("工序大热重", false);
        }
    }
}


// --- 命令路由槽函数 ---
IInteractiveView* MainWindow::activeInteractiveView() const
{
    if (QMdiSubWindow* activeSubWindow = m_mdiArea->activeSubWindow()) {
        DEBUG_PTR(activeSubWindow->widget());
        // return qobject_cast<IInteractiveView*>(activeSubWindow->widget());
        return qobject_cast<IInteractiveView*>(activeSubWindow->widget());
    }
    return nullptr;
}


// void ApplicationWindow::pickDataTool(QAction *action)
// {
//     if (!action)
//         return;

//     MultiLayer *m = qobject_cast<MultiLayer *>(d_workspace.activeSubWindow());
//     if (!m)
//         return;

//     Graph *g = m->activeGraph();
//     if (!g)
//         return;

//     g->disableTools();

//     // if (action == btnCursor)
//     //     showCursor();
//     // else if (action == btnSelect)
//     //     showRangeSelectors();
//     // else if (action == btnPicker)
//     //     showScreenReader();
//     // else if (action == btnMovePoints)
//     //     movePoints();
//     // else if (action == btnRemovePoints)
//     //     removePoints();
//     // else 
//     if (action == btnZoomIn)
//         onZoomIn();
//     else if (action == btnZoomOut)
//         onZoomOut();
//     else if (action == btnArrow)
//         onZoomReset();
//     // else if (action == btnLine)
//     //     drawLine();
// }

void MainWindow::onToolActionTriggered(QAction *action)
{
    if (IInteractiveView* view = activeInteractiveView()) {
        DEBUG_PTR(view);
        view->setCurrentTool(action->data().toString());
    }
}



void MainWindow::onZoomIn() {
    DEBUG_LOG;
    if (auto view = qobject_cast<SampleViewWindow*>(m_mdiArea->activeSubWindow())) {
        if (view->activeChartView()) {
            view->activeChartView()->zoomIn();
        }
    }
}

void MainWindow::onZoomOut() {
    if (auto view = qobject_cast<SampleViewWindow*>(m_mdiArea->activeSubWindow())) {
        if (view->activeChartView()) {
            DEBUG_LOG;
            // view->activeChartView()->setToolMode("select");  // 或 "pan"
            view->activeChartView()->zoomOutFixed();
        }
    }
}

void MainWindow::onZoomReset() {
    if (auto view = qobject_cast<SampleViewWindow*>(m_mdiArea->activeSubWindow())) {
        if (view->activeChartView()) {
            view->activeChartView()->zoomReset();
        }
    }
}



void MainWindow::onSelectArrowClicked() {
    if (auto view = qobject_cast<SampleViewWindow*>(m_mdiArea->activeSubWindow())) {
        if (view->activeChartView()) {
            view->activeChartView()->setToolMode("select");
            // view->setToolMode("select"); // 或 "pan" 等
        }
    }
}

// 算法设置菜单槽函数实现
void MainWindow::onAlignmentActionTriggered()
{
    m_algorithmSetting->show();
    m_algorithmSetting->showAlignmentPage();
}

void MainWindow::onNormalizeActionTriggered()
{
    m_algorithmSetting->show();
    m_algorithmSetting->showNormalizationPage();
}

void MainWindow::onSmoothActionTriggered()
{
    m_algorithmSetting->show();
    m_algorithmSetting->showSmoothPage();
}

void MainWindow::onBaseLineActionTriggered()
{
    m_algorithmSetting->show();
    m_algorithmSetting->showBaselinePage();
}

void MainWindow::onPeakLineActionTriggered()
{
    m_algorithmSetting->show();
    m_algorithmSetting->showPeakAlignPage();
}

void MainWindow::onDiffActionTriggered()
{
    m_algorithmSetting->show();
    m_algorithmSetting->showDifferencePage();
}

void MainWindow::onBigThermalAlgorithmTriggered()
{
    m_algorithmSetting->show();
    // 切换到大热重数据tab页面（索引0）
    m_algorithmSetting->findChild<QTabWidget*>("dataTypeTabWidget")->setCurrentIndex(0);
}

void MainWindow::onSmallThermalAlgorithmTriggered()
{
    m_algorithmSetting->show();
    // 切换到小热重数据tab页面（索引1）
    m_algorithmSetting->findChild<QTabWidget*>("dataTypeTabWidget")->setCurrentIndex(1);
}

void MainWindow::onChromatographyAlgorithmTriggered()
{
    m_algorithmSetting->show();
    // 切换到色谱数据tab页面（索引2）
    m_algorithmSetting->findChild<QTabWidget*>("dataTypeTabWidget")->setCurrentIndex(2);
}




// // ... createActions, createMenus, createToolBars, createStatusBar 的实现...
// // (这些函数的实现代码是正确的，不需要修改，但它们依赖于顶部的 #include)
// void MainWindow::createActions()
// {
//     m_importAction = new QAction(QIcon(":/icons/import.png"), STR("action.import"), this);
//     m_importAction->setShortcut(QKeySequence::Open);
    
//     m_exportAction = new QAction(QIcon(":/icons/export.png"), STR("action.export"), this);
//     m_exportAction->setShortcut(QKeySequence::Save);

//     m_exitAction = new QAction(STR("action.exit"), this);
//     m_exitAction->setShortcut(QKeySequence::Quit);
//     connect(m_exitAction, &QAction::triggered, this, &QWidget::close);

//     m_zoomInAction = new QAction(QIcon(":/icons/zoom-in.png"), STR("action.zoom_in"), this);
//     m_zoomInAction->setShortcut(QKeySequence::ZoomIn);

//     m_zoomOutAction = new QAction(QIcon(":/icons/zoom-out.png"), STR("action.zoom_out"), this);
//     m_zoomOutAction->setShortcut(QKeySequence::ZoomOut);

//     m_zoomResetAction = new QAction(QIcon(":/icons/zoom-reset.png"), STR("action.zoom_reset"), this);
    
//     m_aboutAction = new QAction(STR("action.about"), this);

//     connect(m_zoomInAction, &QAction::triggered, this, &MainWindow::onZoomIn);
//     connect(m_zoomOutAction, &QAction::triggered, this, &MainWindow::onZoomOut);
//     connect(m_zoomResetAction, &QAction::triggered, this, &MainWindow::onZoomReset);
// }

// void MainWindow::createMenus()
// {
//     QMenu* fileMenu = menuBar()->addMenu(STR("main.menu.file"));
//     fileMenu->addAction(m_importAction);
//     fileMenu->addAction(m_exportAction);
//     fileMenu->addSeparator();
//     fileMenu->addAction(m_exitAction);
    
//     QMenu* viewMenu = menuBar()->addMenu(STR("main.menu.view"));
//     viewMenu->addAction(m_zoomInAction);
//     viewMenu->addAction(m_zoomOutAction);
//     viewMenu->addAction(m_zoomResetAction);

//     QMenu* helpMenu = menuBar()->addMenu(STR("main.menu.help"));
//     helpMenu->addAction(m_aboutAction);
// }

// void MainWindow::createToolBars()
// {
//     QToolBar* fileToolBar = addToolBar(STR("main.toolbar.file"));
//     fileToolBar->addAction(m_importAction);
//     fileToolBar->addAction(m_exportAction);
    
//     QToolBar* commonToolBar = addToolBar(STR("main.toolbar.common"));
//     commonToolBar->addAction(m_zoomInAction);
//     commonToolBar->addAction(m_zoomOutAction);
//     commonToolBar->addAction(m_zoomResetAction);
// }



void MainWindow::createToolBars()
{
    DEBUG_LOG;
    // DEBUG_LOG << "MainWindow::createToolBars" <<fileToolBar ;
    // // 文件工具栏
    // fileToolBar = addToolBar(tr("文件"));
    // DEBUG_LOG << "MainWindow::createToolBars" <<fileToolBar ;
    // fileToolBar->setObjectName("fileToolBar");
    // fileToolBar->addAction(m_newAction);
    // fileToolBar->addAction(m_openAction);
    // fileToolBar->addAction(m_saveAction);
    // fileToolBar->addAction(m_printAction);
    
    // // 编辑工具栏
    // editToolBar = addToolBar(tr("编辑"));
    // editToolBar->setObjectName("editToolBar");
    // editToolBar->addAction(m_cutAction);
    // editToolBar->addAction(m_copyAction);
    // editToolBar->addAction(m_pasteAction);
    // editToolBar->addAction(m_deleteAction);
    // editToolBar->addSeparator();
    // editToolBar->addAction(m_undoAction);
    // editToolBar->addAction(m_redoAction);
    
    // // 视图工具栏
    // viewToolBar = addToolBar(tr("视图"));
    // viewToolBar->setObjectName("viewToolBar");
    // viewToolBar->addAction(m_zoomInAction);
    // viewToolBar->addAction(m_zoomOutAction);
    // viewToolBar->addAction(m_zoomResetAction);

    DEBUG_LOG;

    // ... (文件工具栏)
    // commonToolBar = addToolBar(STR("main.toolbar.common"));//程序崩溃
    commonToolBar = addToolBar(tr("工具"));
    // commonToolBar->addAction(m_zoomInAction);
    // commonToolBar->addAction(m_zoomOutAction);
    // commonToolBar->addAction(m_zoomResetAction);
    // commonToolBar->addSeparator();
    // commonToolBar->addAction(m_selectAction);
    // commonToolBar->addAction(m_panAction);
    // commonToolBar->addAction(m_zoomRectAction);


    // ... (算法工具栏)
    algorithmToolBar = addToolBar(tr("算法"));
    // algorithmToolBar->addAction(m_alignActionTriggered);
    // algorithmToolBar->addAction(m_normalizeActionTriggered);
    // algorithmToolBar->addAction(m_smoothActionTriggered);
    // // algorithmToolBar->addSeparator();
    // algorithmToolBar->addAction(m_baseLineActionTriggered);
    // algorithmToolBar->addAction(m_peakLineActionTriggered);
    // algorithmToolBar->addAction(m_diffActionTriggered);

    dataProcessToolBar = addToolBar(tr("数据处理"));
    dataProcessToolBar->addAction(tgBigDataProcessAction);
    dataProcessToolBar->addAction(tgSmallDataProcessAction);
    dataProcessToolBar->addAction(tgSmallRawDataProcessAction);
    dataProcessToolBar->addAction(chromatographDataProcessAction);
    dataProcessToolBar->addAction(processTgBigDataProcessAction);

    // --- 搜索工具栏：支持模糊与多条件查询（中文注释） ---
    m_searchToolBar = addToolBar(tr("搜索"));
    // 添加“搜索”字样标签
    m_searchLabel = new QLabel(tr("搜索"), m_searchToolBar);
    m_searchToolBar->addWidget(m_searchLabel);
    m_searchEdit = new QLineEdit(m_searchToolBar);
    // 设置占位提示与工具提示，符合层级搜索规则
    m_searchEdit->setPlaceholderText(tr("请输入 1~3 个关键词，空格分隔；示例：YOUXI 202311 01"));
    m_searchEdit->setToolTip(
        tr("导航树搜索提示\n\n"
           "请输入 1~3 个关键词进行模糊搜索，可以用空格分隔各个关键词。\n"
           "系统会根据关键词数量自动匹配不同层级：\n\n"
           "1 个关键词：模糊匹配整个树中包含该关键词的节点\n"
           "2 个关键词：第一个关键词匹配烟牌号层级，第二个关键词匹配批次或样本号层级\n"
           "3 个关键词：分别匹配烟牌号 → 批次 → 样本号\n\n"
           "匹配规则：\n"
           "- 支持部分匹配（模糊匹配），例如输入“YOUXI 202311”可匹配“YOUXI2023批次202311”的节点\n"
           "- 不区分大小写\n"
           "- 多余的空格会自动忽略\n\n"
           "示例：\n"
           "- YOUXI → 匹配所有烟牌号包含“YOUXI”的节点\n"
           "- YOUXI 202311 → 匹配烟牌号包含“YOUXI”，批次包含“202311”的节点\n"
           "- YOUXI 202311 01 → 精确匹配烟牌号/批次/样本号"));
    m_searchToolBar->addWidget(m_searchEdit);
    // 添加“搜索”按钮，点击执行搜索
    m_doSearchAction = new QAction(tr("搜索"), this);
    m_searchToolBar->addAction(m_doSearchAction);
    // 搜索帮助按钮，点击弹出详细说明
    m_searchHelpAction = new QAction(tr("搜索提示"), this);
    m_searchToolBar->addAction(m_searchHelpAction);
    m_clearSearchAction = new QAction(tr("清除"), this);
    m_searchToolBar->addAction(m_clearSearchAction);

    // 文本变化实时过滤导航树
    connect(m_searchEdit, &QLineEdit::textChanged, this, [this](const QString& text){
        if (m_navigator) m_navigator->applySearchFilter(text);
    });
    // 回车也触发过滤
    connect(m_searchEdit, &QLineEdit::returnPressed, this, [this](){
        if (m_navigator) m_navigator->applySearchFilter(m_searchEdit->text());
    });
    // 点击“搜索”按钮触发过滤
    connect(m_doSearchAction, &QAction::triggered, this, [this](){
        if (m_navigator) m_navigator->applySearchFilter(m_searchEdit->text());
    });
    // 清除按钮：清空输入并恢复导航树
    connect(m_clearSearchAction, &QAction::triggered, this, [this](){
        if (!m_searchEdit) return;
        m_searchEdit->clear();
        if (m_navigator) m_navigator->clearSearchFilter();
    });

    // 搜索提示动作，弹窗展示规则说明
    connect(m_searchHelpAction, &QAction::triggered, this, [this](){
        const QString title = tr("导航树搜索提示");
        const QString content = tr(
            "请输入 1~3 个关键词进行模糊搜索，可以用空格分隔各个关键词。\n\n"
            "系统会根据关键词数量自动匹配不同层级：\n\n"
            "1 个关键词：模糊匹配整个树中包含该关键词的节点\n"
            "2 个关键词：第一个关键词匹配烟牌号层级，第二个关键词匹配批次或样本号层级\n"
            "3 个关键词：分别匹配烟牌号 → 批次 → 样本号\n\n"
            "匹配规则：\n"
            "- 支持部分匹配（模糊匹配），例如输入“YOUXI 202311”可匹配“YOUXI2023批次202311”的节点\n"
            "- 不区分大小写\n"
            "- 多余的空格会自动忽略\n\n"
            "示例：\n"
            "- YOUXI → 匹配所有烟牌号包含“YOUXI”的节点\n"
            "- YOUXI 202311 → 匹配烟牌号包含“YOUXI”，批次包含“202311”的节点\n"
            "- YOUXI 202311 01 → 精确匹配烟牌号/批次/样本号");
        QMessageBox::information(this, title, content);
    });

}


void MainWindow::createStatusBar()
{
    statusBar()->showMessage(tr("准备就绪"));
}


void MainWindow::createActions()
{
    // ... (旧的 import, export, exit) ...
    m_importAction = new QAction(QIcon(":/icons/import.png"), STR("action.import"), this);
    m_importAction->setShortcut(QKeySequence::Open);

    // connect(m_importAction, &QAction::triggered, this, [this]() {
        
    //     // 1. 定义外部工具所在的文件夹名称和可执行文件名
    //     const QString toolDirName = "import_data";
    //     const QString executableName = "TobaccoData-t.exe";

    //     // 2. 获取我们自己的主程序 (.exe) 所在的目录
    //     const QString appDir = QCoreApplication::applicationDirPath();

    //     // 3. 构造到外部工具子目录的路径
    //     QDir toolDir(appDir);
    //     if (!toolDir.cd(toolDirName)) {
    //         WARNING_LOG << "错误：在主程序目录下找不到外部工具目录:" << toolDir.filePath(toolDirName);
    //         QMessageBox::critical(this, STR("error.title"), 
    //                               tr("启动失败：找不到外部工具目录 '%1'。").arg(toolDirName));
    //         return;
    //     }

    //     // 4. 构造到可执行文件的完整路径
    //     const QString executablePath = toolDir.filePath(executableName);

    //     // 5. 在启动前，检查可执行文件是否存在
    //     if (!QFile::exists(executablePath)) {
    //         WARNING_LOG << "错误：找不到外部工具程序:" << executablePath;
    //         QMessageBox::critical(this, STR("error.title"),
    //                               tr("启动失败：找不到外部工具 '%1'。").arg(executableName));
    //         return;
    //     }
        
    //     INFO_LOG << "正在尝试启动外部工具:" << executablePath;

    //     // 6. 以分离模式启动进程
    //     //    第三个参数是工作目录，确保外部工具能找到自己的依赖
    //     bool success = QProcess::startDetached(executablePath, QStringList(), toolDir.path());
        
    //     if (!success) {
    //         WARNING_LOG << "错误：无法启动外部进程。";
    //         QMessageBox::critical(this, STR("error.title"), 
    //                               tr("无法启动外部工具，请检查程序权限或联系管理员。"));
    //     }
    // });

    // 连接点击信号到现有槽函数
    connect(m_importAction, &QAction::triggered,
            this, &MainWindow::on_actionOpenSingleMaterialData_triggered);

    
    m_exportAction = new QAction(QIcon(":/icons/export.png"), STR("action.export"), this);
    m_exportAction->setShortcut(QKeySequence::Save);

    m_exitAction = new QAction(STR("action.exit"), this);
    m_exitAction->setShortcut(QKeySequence::Quit);
    connect(m_exitAction, &QAction::triggered, this, &QWidget::close);

    m_newAction = new QAction(style()->standardIcon(QStyle::SP_FileIcon), tr("新建(&N)..."), this);
    m_openAction = new QAction(style()->standardIcon(QStyle::SP_DirIcon), tr("打开(&O)..."), this);
    m_saveAction = new QAction(style()->standardIcon(QStyle::SP_DialogSaveButton), tr("保存(&S)"), this);
    m_saveAsAction = new QAction(tr("另存为(&A)..."), this);
    m_printAction = new QAction(tr("打印(&P)"), this);

    m_cutAction = new QAction(tr("剪切(&T)"), this);
    m_copyAction = new QAction(tr("复制(&C)"), this);
    m_pasteAction = new QAction(tr("粘贴(&P)"), this);
    m_deleteAction = new QAction(tr("删除(&D)"), this);

    m_exportPlotAction = new QAction(tr("导出图像(&P)"), this);
    // 在构造函数或初始化函数里
    connect(m_exportPlotAction, &QAction::triggered,
            this, &MainWindow::onExportPlot);

    m_exportTableAction = new QAction(tr("保存数据(&T)"), this);
    // 在构造函数或初始化函数里
    connect(m_exportTableAction, &QAction::triggered,
            this, &MainWindow::onExportTable);


    m_undoAction = new QAction(style()->standardIcon(QStyle::SP_ArrowBack), tr("撤销(&U)"), this);
    m_redoAction = new QAction(style()->standardIcon(QStyle::SP_ArrowForward), tr("重做(&R)"), this);
    // ... (cut, copy, paste)

    // ... (旧的 zoom) ...
    // m_zoomInAction = new QAction(QIcon(":/icons/zoom-in.png"), this);
    m_zoomInAction = new QAction(QIcon(":/icons/zoom-in.png"), STR("action.zoom_in"), this);
    m_zoomInAction->setShortcut(QKeySequence::ZoomIn);
    // connect(m_zoomInAction, &QAction::triggered, this, &SampleViewWindow::onZoomRectToolClicked);
    connect(m_zoomInAction, &QAction::triggered, this, [this](){
        if (auto view = qobject_cast<SampleViewWindow*>(m_mdiArea->activeSubWindow())) {
            if (view->activeChartView()) {
                view->activeChartView()->setToolMode("zoom_rect"); // 切换到框选放大
            }
        }

    });


    m_zoomOutAction = new QAction(QIcon(":/icons/zoom-out.png"), STR("action.zoom_out"), this);
    m_zoomOutAction->setShortcut(QKeySequence::ZoomOut);
    connect(m_zoomOutAction, &QAction::triggered, this, &MainWindow::onZoomOut);

    m_zoomResetAction = new QAction(QIcon(":/icons/zoom-reset.png"), STR("action.zoom_reset"), this);
    connect(m_zoomResetAction, &QAction::triggered, this, &MainWindow::onZoomReset);
    
    m_aboutAction = new QAction(STR("action.about"), this);
    connect(m_aboutAction, &QAction::triggered, this, &MainWindow::onAboutActionTriggered);

    // --- 模式工具 Actions ---
    m_selectAction = new QAction(QIcon(":/icons/tool-select.png"), STR("action.select_tool"), this);
    m_selectAction->setCheckable(true);
    m_selectAction->setData("select");
    connect(m_selectAction, &QAction::triggered, this, &MainWindow::onSelectArrowClicked);

    

    m_panAction = new QAction(QIcon(":/icons/tool-pan.png"), STR("action.pan_tool"), this);
    m_panAction->setCheckable(true);
    m_panAction->setData("pan");

    m_zoomRectAction = new QAction(QIcon(":/icons/tool-zoom-rect.png"), STR("action.zoom_rect_tool"), this);
    m_zoomRectAction->setCheckable(true);
    m_zoomRectAction->setData("zoom_rect");
    
    m_selectAction->setChecked(true); // 默认选中“选择”工具

    m_toolsActionGroup = new QActionGroup(this);
    m_toolsActionGroup->addAction(m_selectAction);
    m_toolsActionGroup->addAction(m_panAction);
    m_toolsActionGroup->addAction(m_zoomRectAction);
    m_toolsActionGroup->setExclusive(true);

    connect(m_toolsActionGroup, &QActionGroup::triggered, this, &MainWindow::onToolActionTriggered);

    // 算法工具 Actions
    m_alignAction = new QAction(STR("action.align"), this);
connect(m_alignAction, &QAction::triggered, this, &MainWindow::onAlignmentActionTriggered);
    
    m_normalizeAction = new QAction(STR("action.normalize"), this);
connect(m_normalizeAction, &QAction::triggered, this, &MainWindow::onNormalizeActionTriggered);
    
    m_smoothAction = new QAction(STR("action.smooth"), this);
connect(m_smoothAction, &QAction::triggered, this, &MainWindow::onSmoothActionTriggered);
    
    m_baseLineAction = new QAction(STR("action.base_line"), this);
connect(m_baseLineAction, &QAction::triggered, this, &MainWindow::onBaseLineActionTriggered);
    
    m_peakLineAction = new QAction(STR("action.peak_line"), this);
connect(m_peakLineAction, &QAction::triggered, this, &MainWindow::onPeakLineActionTriggered);
    
    m_diffAction = new QAction(STR("action.diff"), this);
connect(m_diffAction, &QAction::triggered, this, &MainWindow::onDiffActionTriggered);

    // 新的按数据类型分类的算法设置 Actions
    m_bigThermalAlgorithmAction = new QAction(tr("大热重算法"), this);
    connect(m_bigThermalAlgorithmAction, &QAction::triggered, this, &MainWindow::onBigThermalAlgorithmTriggered);
    
    m_smallThermalAlgorithmAction = new QAction(tr("小热重算法"), this);
    connect(m_smallThermalAlgorithmAction, &QAction::triggered, this, &MainWindow::onSmallThermalAlgorithmTriggered);
    
    m_chromatographyAlgorithmAction = new QAction(tr("色谱算法"), this);
    connect(m_chromatographyAlgorithmAction, &QAction::triggered, this, &MainWindow::onChromatographyAlgorithmTriggered);

    //  数据处理 Actions
    tgBigDataProcessAction = new QAction(STR("action.tg_big_data_process"), this);
    connect(tgBigDataProcessAction, &QAction::triggered, this, &MainWindow::onTgBigDataProcessActionTriggered);

    tgSmallDataProcessAction = new QAction(STR("action.tg_small_data_process"), this);
    connect(tgSmallDataProcessAction, &QAction::triggered, this, &MainWindow::onTgSmallDataProcessActionTriggered);

    tgSmallRawDataProcessAction = new QAction(tr("小热重（原始数据）"), this);
    connect(tgSmallRawDataProcessAction, &QAction::triggered, this, &MainWindow::onTgSmallRawDataProcessActionTriggered);

    chromatographDataProcessAction = new QAction(STR("action.chromatograph_data_process"), this);
    connect(chromatographDataProcessAction, &QAction::triggered, this, &MainWindow::onChromatographDataProcessActionTriggered);

    processTgBigDataProcessAction = new QAction(STR("action.process_tg_big_data_process"), this);
    connect(processTgBigDataProcessAction, &QAction::triggered, this, &MainWindow::onProcessTgBigDataProcessActionTriggered);



    m_alignActionTriggered = new QAction(STR("action.align"), this);
    connect(m_alignActionTriggered, &QAction::triggered, this, &MainWindow::on_m_alignAction_triggered);
    m_normalizeActionTriggered = new QAction(STR("action.normalize"), this);
    m_smoothActionTriggered = new QAction(STR("action.smooth"), this);
    m_baseLineActionTriggered = new QAction(STR("action.base_line"), this);
    m_peakLineActionTriggered = new QAction(STR("action.peak_line"), this);
    m_diffActionTriggered = new QAction(STR("action.diff"), this);
    connect(m_diffActionTriggered, &QAction::triggered, this, &MainWindow::on_m_diffAction_triggered);




    // connect(m_zoomInAction, &QAction::triggered, this, &MainWindow::onZoomIn);
    // connect(m_zoomOutAction, &QAction::triggered, this, &MainWindow::onZoomOut);
    // connect(m_zoomResetAction, &QAction::triggered, this, &MainWindow::onZoomReset);
    
    // 获取 Dock 控件的 toggleViewAction，这是 Qt 提供的标准功能
    m_navigatorDockAction = m_navigatorDock->toggleViewAction();
    m_navigatorDockAction->setText(tr("数据导航面板"));
    m_propertiesDockAction = m_propertiesDock->toggleViewAction();
    m_propertiesDockAction->setText(tr("属性详情面板"));
    m_propertiesDock->hide();
    m_operationLogDockAction = m_operationLogDock->toggleViewAction();
    m_operationLogDockAction->setText(tr("操作日志面板"));
    m_analysisDockAction = m_analysisDock->toggleViewAction();
    m_analysisDockAction->setText(tr("分析平台"));
    m_analysisDock->hide();   // 初始化时隐藏
    

}

void MainWindow::createMenus()
{
    // 文件菜单
    QMenu* fileMenu = menuBar()->addMenu(STR("main.menu.file"));
    // QMenu* fileMenu = menuBar()->addMenu(tr("文件(&F)"));
    // fileMenu->addAction(m_newAction);
    // fileMenu->addAction(m_openAction);
    fileMenu->addSeparator();
    // fileMenu->addAction(m_saveAction);
    DEBUG_LOG << "m_saveAsAction" << m_saveAsAction;
    // fileMenu->addAction(m_saveAsAction);
    fileMenu->addSeparator();
    DEBUG_LOG << "m_importAction" << m_importAction;
    fileMenu->addAction(m_importAction);
    DEBUG_LOG << m_exportAction;
    // fileMenu->addAction(m_exportAction);
    fileMenu->addSeparator();
    
    // fileMenu->addAction(m_exportPlotAction);
    // fileMenu->addAction(m_exportTableAction);
    fileMenu->addSeparator();
    fileMenu->addAction(m_exitAction);
    
    // 编辑菜单
    // QMenu* editMenu = menuBar()->addMenu(tr("编辑(&E)"));
    // editMenu->addAction(m_undoAction);
    // editMenu->addAction(m_redoAction);
    // ...
    
    // 视图菜单
    QMenu* viewMenu = menuBar()->addMenu(STR("main.menu.view"));
    viewMenu->addAction(m_navigatorDockAction); // 控制 Dock 显示/隐藏
    viewMenu->addAction(m_propertiesDockAction);
    viewMenu->addAction(m_operationLogDockAction);
    viewMenu->addAction(m_analysisDockAction);
    
    // viewMenu->addAction(m_zoomInAction);
    // viewMenu->addAction(m_zoomOutAction);
    // viewMenu->addAction(m_zoomResetAction);

    // 添加“打印选中样本（按类型分组）”菜单动作
    m_printSelectedSamplesGroupedAction = new QAction(tr("查看选中样本（按类型）"), this);
    connect(m_printSelectedSamplesGroupedAction, &QAction::triggered, this, &MainWindow::onPrintSelectedSamplesGrouped);
    viewMenu->addAction(m_printSelectedSamplesGroupedAction);

    // 设置菜单
    QMenu* settingsMenu = menuBar()->addMenu(STR("main.menu.settings"));
    // settingsMenu->addAction(m_bigThermalAlgorithmAction);
    // settingsMenu->addAction(m_smallThermalAlgorithmAction);
    // settingsMenu->addAction(m_chromatographyAlgorithmAction);

    // 数据处理菜单
    QMenu* dataProcessMenu = menuBar()->addMenu(STR("main.menu.dataProcess"));
    dataProcessMenu->addAction(tgBigDataProcessAction);
    dataProcessMenu->addAction(tgSmallDataProcessAction);
    dataProcessMenu->addAction(tgSmallRawDataProcessAction);
    dataProcessMenu->addAction(chromatographDataProcessAction);
    dataProcessMenu->addAction(processTgBigDataProcessAction);



    // ... (工具，帮助菜单)


    QMenu* helpMenu = menuBar()->addMenu(STR("main.menu.help"));
    helpMenu->addAction(m_aboutAction);
}


void MainWindow::logToOperationPanel(const QString &msg)
{
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    m_operationLogWidget->append(QString("[%1] %2").arg(timestamp, msg));
    m_operationLogWidget->append("----------------------------------------");
}


void MainWindow::onExportTable()
{
    ChartView* activeChartView = nullptr;

    if (m_mdiArea) {
        QMdiSubWindow* activeSubWindow = m_mdiArea->activeSubWindow();
        if (activeSubWindow) {
            // 直接查找ChartView
            activeChartView = activeSubWindow->findChild<ChartView*>();
        }
    }
    
    if (!activeChartView) {
        QMessageBox::warning(this, tr("导出失败"), tr("请先打开一个图表视图窗口"));
        return;
    }
    
    QString filePath = QFileDialog::getSaveFileName(this, tr("导出数据"), 
                                                  QString(), tr("Excel文件 (*.xlsx);;CSV文件 (*.csv)"));
    
    DEBUG_LOG << "导出图表视图:" << activeChartView->windowTitle();
    
    if (!filePath.isEmpty()) {
        bool success = false;
        
        // 根据文件扩展名选择导出格式
        if (filePath.endsWith(".xlsx", Qt::CaseInsensitive)) {
            success = activeChartView->exportDataToXLSX(filePath);
        } else if (filePath.endsWith(".csv", Qt::CaseInsensitive)) {
            success = activeChartView->exportDataToCSV(filePath);
        } else {
            // 如果没有扩展名，默认添加 .xlsx
            if (!filePath.contains(".")) {
                filePath += ".xlsx";
                success = activeChartView->exportDataToXLSX(filePath);
            }
        }
        
        if (success) {
            QMessageBox::information(this, tr("导出成功"), tr("数据已成功导出到文件"));
            // 操作日志记录导出数据成功
            logToOperationPanel(QStringLiteral("【导出】表格数据导出成功：%1").arg(filePath));
        } else {
            QMessageBox::warning(this, tr("导出失败"), tr("无法导出数据到文件"));
            // 操作日志记录导出数据失败
            logToOperationPanel(QStringLiteral("【导出】表格数据导出失败：%1").arg(filePath));
        }
    }
}

void MainWindow::onExportPlot()
{
    ChartView* activeChartView = nullptr;

    if (m_mdiArea) {
        // 获取当前活动子窗口
        QMdiSubWindow* activeSubWindow = m_mdiArea->activeSubWindow();
        if (activeSubWindow) {
            // 查找 ChartView 指针
            activeChartView = activeSubWindow->findChild<ChartView*>();
        }
    }

    if (!activeChartView) {
        QMessageBox::warning(this, tr("导出失败"), tr("请先打开一个图表视图窗口"));
        return;
    }

    // 弹出文件保存对话框，支持 PNG、PDF、SVG
    QString filePath = QFileDialog::getSaveFileName(
        this,
        tr("导出图像"),
        QString(),
        // tr("PNG 文件 (*.png);;PDF 文件 (*.pdf);;SVG 文件 (*.svg)")
        tr("PNG 文件 (*.png);;PDF 文件 (*.pdf)")
    );

    DEBUG_LOG << "导出图表视图:" << activeChartView->windowTitle();

    if (!filePath.isEmpty()) {
        // 调用 ChartView 的导出函数
        if (activeChartView->exportPlot(filePath)) {
            QMessageBox::information(this, tr("导出成功"), tr("图表已成功导出到文件"));
            // 操作日志记录导出图像成功
            logToOperationPanel(QStringLiteral("【导出】图像导出成功：%1").arg(filePath));
        } else {
            QMessageBox::warning(this, tr("导出失败"), tr("无法导出图像到文件"));
            // 操作日志记录导出图像失败
            logToOperationPanel(QStringLiteral("【导出】图像导出失败：%1").arg(filePath));
        }
    }
}


// void MainWindow::on_m_alignAction_triggered()
// {
//     AlignmentDialog dialog(this);
//     dialog.exec(); // 使用 exec() 使对话框以模态方式显示
// }

// void MainWindow::onAlignmentActionTriggered()
void MainWindow::on_m_alignAction_triggered()
{
    // ChartView* activeChartView = nullptr;

    // if (m_mdiArea) {
    //     // 获取当前活动子窗口
    //     QMdiSubWindow* activeSubWindow = m_mdiArea->activeSubWindow();
    //     if (activeSubWindow) {
    //         // 查找 ChartView 指针
    //         activeChartView = activeSubWindow->findChild<ChartView*>();
    //     }
    // }

    // if (!activeChartView) {
    //     QMessageBox::warning(this, tr("导出失败"), tr("请先打开一个图表视图窗口"));
    //     return;
    // }

    // // // 获取当前活跃窗口
    // // QWidget* activeWindow = QApplication::activeWindow();
    
    // // // 检查是否是 SampleViewWindow 类型
    // // SampleViewWindow* sampleWindow = qobject_cast<SampleViewWindow*>(activeWindow);
    // // if (!sampleWindow) {
    // //     QMessageBox::warning(this, tr("警告"), tr("请先选择一个样本窗口"));
    // //     return;
    // // }
    
    // // 获取窗口中的 ChartView
    // ChartView* chartView = sampleWindow->findChild<ChartView*>();
    // if (!chartView) {
    //     QMessageBox::warning(this, tr("警告"), tr("无法获取图表视图"));
    //     return;
    // }
    
    // // 获取曲线数据
    // QVector<QPair<QVector<double>, QVector<double>>> curvesData = chartView->getCurvesData();
    // if (curvesData.isEmpty()) {
    //     QMessageBox::warning(this, tr("警告"), tr("没有可处理的曲线数据"));
    //     return;
    // }
    
    // // 处理数据 - 对每条曲线的Y值加1
    // QVector<QPair<QVector<double>, QVector<double>>> processedData;
    // for (const auto& curve : curvesData) {
    //     QVector<double> xData = curve.first;
    //     QVector<double> yData;
        
    //     // Y值加1
    //     for (double y : curve.second) {
    //         yData.append(y + 1.0);
    //     }
        
    //     processedData.append(qMakePair(xData, yData));
    // }
    
    // // 保存处理后的数据到数据库
    // // saveAlgorithmResults(processedData, "alignment_algorithm", 1);
    
    // // 使用处理后的数据绘制新曲线
    // for (int i = 0; i < processedData.size(); ++i) {
    //     QString curveName = QString("对齐处理_%1").arg(i+1);
    //     QColor color = QColor::fromHsv(i * 60 % 360, 255, 255); // 不同颜色
    //     chartView->addGraph(processedData[i].first, processedData[i].second, curveName, color);
    // }
    
    // // 重绘图表
    // chartView->replot();
    
    // QMessageBox::information(this, tr("成功"), tr("对齐算法处理完成"));
}


void MainWindow::on_m_diffAction_triggered()
{
    DifferenceTableDialog dialog(this);
    dialog.exec(); // 使用 exec() 使对话框以模态方式显示
}



// void MainWindow::saveAlgorithmResults(const QVector<QPair<QVector<double>, QVector<double>>>& processedData, 
//                                      const QString& algoName, int runNo,
//                                      const QString& rawType, int rawId)
// {
//     // 获取数据库连接
//     QSqlDatabase db = QSqlDatabase::database();
//     if (!db.isOpen()) {
//         WARNING_LOG << "数据库未打开，无法保存算法结果";
//         return;
//     }
    
//     // 准备参数JSON
//     QJsonObject paramObj;
//     paramObj["description"] = "Y值加1的简单算法";
//     paramObj["offset"] = 1.0;
//     QJsonDocument paramDoc(paramObj);
//     QString paramJson = paramDoc.toJson(QJsonDocument::Compact);
    
//     // 开始事务
//     db.transaction();
    
//     QSqlQuery query(db);
//     query.prepare("INSERT INTO algo_results (raw_type, raw_id, algo_name, run_no, param_json, x_value, y_value) "
//                  "VALUES (?, ?, ?, ?, ?, ?, ?)");
    
//     // 遍历所有曲线和数据点
//     for (const auto& curve : processedData) {
//         for (int i = 0; i < curve.first.size(); ++i) {
//             query.bindValue(0, rawType);
//             query.bindValue(1, rawId);
//             query.bindValue(2, algoName);
//             query.bindValue(3, runNo);
//             query.bindValue(4, paramJson);
//             query.bindValue(5, curve.first[i]);  // x值
//             query.bindValue(6, curve.second[i]); // y值
            
//             if (!query.exec()) {
//                 WARNING_LOG << "保存算法结果失败:" << query.lastError().text();
//                 db.rollback();
//                 return;
//             }
//         }
//     }
    
//     // 提交事务
//     db.commit();
//     DEBUG_LOG << "算法结果已保存到数据库";
// }



void MainWindow::onTgBigDataProcessActionTriggered()
{
    // 如果窗口已经存在，则显示并激活它
    if (tgBigDataProcessDialog) {
        // 查找包含此窗口的子窗口
        for (QMdiSubWindow* window : m_mdiArea->subWindowList()) {
            if (window->widget() == tgBigDataProcessDialog) {
                m_mdiArea->setActiveSubWindow(window);
                if (m_navigator) m_navigator->enableSampleCheckboxesByDataType(QStringLiteral("大热重"), true);
                return;
            }
        }
    }
    
    // 创建新的窗口
    tgBigDataProcessDialog = new TgBigDataProcessDialog(this, m_appInitializer, m_navigator);
    // 创建一个完全透明的1x1图标，用于“看起来没有图标”
    {
        QPixmap px(1, 1);
        px.fill(Qt::transparent);
        tgBigDataProcessDialog->setWindowIcon(QIcon(px));
    }
    
    // 连接样本选中信号，用于同步主窗口导航树选择框状态
    connect(tgBigDataProcessDialog, &TgBigDataProcessDialog::sampleSelected, 
            this, [this](int sampleId, bool selected) {
                if (m_navigator) {
                    m_navigator->setSampleCheckState(sampleId, selected);
                }
            });
    // 当 dialog 发出请求时，让 MainWindow 的 onCreateDifferenceWorkbench 槽函数来处理
    connect(tgBigDataProcessDialog, &TgBigDataProcessDialog::requestNewTgBigDifferenceWorkbench,
            this, &MainWindow::onCreateTgBigDifferenceWorkbench);
    
    // 将窗口添加到MDI区域
    QMdiSubWindow* subWindow = m_mdiArea->addSubWindow(tgBigDataProcessDialog);
    subWindow->setAttribute(Qt::WA_DeleteOnClose);
    subWindow->setProperty("sampleId", -1); // 设置一个默认的sampleId属性
    
    // 连接销毁信号，确保指针被重置
    connect(tgBigDataProcessDialog, &QWidget::destroyed, this, [this]() {
        tgBigDataProcessDialog = nullptr;
    });
    
    // 添加到导航器的打开窗口列表
    m_navigator->addOpenView(subWindow);
    if (m_navigator) m_navigator->enableSampleCheckboxesByDataType(QStringLiteral("大热重"), true);
    
    // 显示窗口
    subWindow->show();
    // tgBigDataProcessDialog->show();
    subWindow->showMaximized();  //  这里最大化
}

void MainWindow::onTgSmallDataProcessActionTriggered()
{
    // 如果窗口已经存在，则显示并激活它
    if (tgSmallDataProcessDialog) {
        // 查找包含此窗口的子窗口
        for (QMdiSubWindow* window : m_mdiArea->subWindowList()) {
            if (window->widget() == tgSmallDataProcessDialog) {
                m_mdiArea->setActiveSubWindow(window);
                if (m_navigator) m_navigator->enableSampleCheckboxesByDataType(QStringLiteral("小热重"), true);
                return;
            }
        }
    }
    
    // 创建新的窗口
    tgSmallDataProcessDialog = new TgSmallDataProcessDialog(this, m_appInitializer, m_navigator);
    // 小热重窗口同样使用透明图标
    {
        QPixmap px(1, 1);
        px.fill(Qt::transparent);
        tgSmallDataProcessDialog->setWindowIcon(QIcon(px));
    }
    
    // 连接样本选中信号，用于同步主窗口导航树选择框状态
    connect(tgSmallDataProcessDialog, &TgSmallDataProcessDialog::sampleSelected, 
            this, [this](int sampleId, bool selected) {
                if (m_navigator) {
                    m_navigator->setSampleCheckState(sampleId, selected);
                }
            });

    // 当 dialog 发出请求时，让 MainWindow 的 onCreateDifferenceWorkbench 槽函数来处理
    connect(tgSmallDataProcessDialog, &TgSmallDataProcessDialog::requestNewTgSmallDifferenceWorkbench,
            this, &MainWindow::onCreateTgSmallDifferenceWorkbench);
    
    // 将窗口添加到MDI区域
    QMdiSubWindow* subWindow = m_mdiArea->addSubWindow(tgSmallDataProcessDialog);
    subWindow->setAttribute(Qt::WA_DeleteOnClose);
    subWindow->setProperty("sampleId", -1); // 设置一个默认的sampleId属性
    
    // 连接销毁信号，确保指针被重置
    connect(tgSmallDataProcessDialog, &QWidget::destroyed, this, [this]() {
        tgSmallDataProcessDialog = nullptr;
    });
    
    // 添加到导航器的打开窗口列表
    m_navigator->addOpenView(subWindow);
    if (m_navigator) m_navigator->enableSampleCheckboxesByDataType(QStringLiteral("小热重"), true);
    
    // 显示窗口
    subWindow->show();
    // tgSmallDataProcessDialog->show();
    subWindow->showMaximized();  //  这里最大化
}

void MainWindow::onTgSmallRawDataProcessActionTriggered()
{
    if (tgSmallRawDataProcessDialog) {
        for (QMdiSubWindow* window : m_mdiArea->subWindowList()) {
            if (window->widget() == tgSmallRawDataProcessDialog) {
                m_mdiArea->setActiveSubWindow(window);
                if (m_navigator) m_navigator->enableSampleCheckboxesByDataType(QStringLiteral("小热重（原始数据）"), true);
                return;
            }
        }
    }

    tgSmallRawDataProcessDialog = new TgSmallDataProcessDialog(this, m_appInitializer, m_navigator, QStringLiteral("小热重（原始数据）"));
    {
        QPixmap px(1, 1);
        px.fill(Qt::transparent);
        tgSmallRawDataProcessDialog->setWindowIcon(QIcon(px));
    }

    connect(tgSmallRawDataProcessDialog, &TgSmallDataProcessDialog::sampleSelected,
            this, [this](int sampleId, bool selected) {
                if (m_navigator) {
                    m_navigator->setSampleCheckStateForType(sampleId, QStringLiteral("小热重（原始数据）"), selected);
                }
            });

    connect(tgSmallRawDataProcessDialog, &TgSmallDataProcessDialog::requestNewTgSmallDifferenceWorkbench,
            this, &MainWindow::onCreateTgSmallDifferenceWorkbench);

    QMdiSubWindow* subWindow = m_mdiArea->addSubWindow(tgSmallRawDataProcessDialog);
    subWindow->setAttribute(Qt::WA_DeleteOnClose);
    subWindow->setProperty("sampleId", -1);

    connect(tgSmallRawDataProcessDialog, &QWidget::destroyed, this, [this]() {
        tgSmallRawDataProcessDialog = nullptr;
    });

    m_navigator->addOpenView(subWindow);
    if (m_navigator) m_navigator->enableSampleCheckboxesByDataType(QStringLiteral("小热重（原始数据）"), true);

    subWindow->show();
    subWindow->showMaximized();
}

// void MainWindow::onChromatographDataProcessActionTriggered()
// {
//     // 如果窗口已经存在，则显示并激活它
//     if (chromatographDataProcessDialog) {
//         // 查找包含此窗口的子窗口
//         for (QMdiSubWindow* window : m_mdiArea->subWindowList()) {
//             if (window->widget() == chromatographDataProcessDialog) {
//                 m_mdiArea->setActiveSubWindow(window);
//                 return;
//             }
//         }
//     }
    
//     // 创建新的窗口，传递主界面导航树
//     chromatographDataProcessDialog = new ChromatographDataProcessDialog(this, m_navigator);
    
//     // 连接样本选中信号，用于同步主窗口导航树选择框状态
//     connect(chromatographDataProcessDialog, &ChromatographDataProcessDialog::sampleSelected, 
//             this, [this](int sampleId, bool selected) {
//                 if (m_navigator) {
//                     m_navigator->setSampleCheckState(sampleId, selected);
//                 }
//             });
    
//     // 将窗口添加到MDI区域
//     QMdiSubWindow* subWindow = m_mdiArea->addSubWindow(chromatographDataProcessDialog);
//     subWindow->setAttribute(Qt::WA_DeleteOnClose);
//     subWindow->setProperty("sampleId", -1); // 设置一个默认的sampleId属性
    
//     // 添加到导航器的打开窗口列表
//     m_navigator->addOpenView(subWindow);
    
//     // 连接销毁信号，确保指针被重置
//     connect(chromatographDataProcessDialog, &QWidget::destroyed, this, [this]() {
//         chromatographDataProcessDialog = nullptr;
//     });
    
//     // 显示窗口
//     subWindow->show();
//     // chromatographDataProcessDialog->show();
//     subWindow->showMaximized();  //  这里最大化
// }


void MainWindow::onChromatographDataProcessActionTriggered()
{
    DEBUG_LOG << "MainWindow::onChromatographDataProcessActionTriggered called";
    // 如果窗口已经存在，则显示并激活它
    if (chromatographDataProcessDialog) {
        // 查找包含此窗口的子窗口
        for (QMdiSubWindow* window : m_mdiArea->subWindowList()) {
            if (window->widget() == chromatographDataProcessDialog) {
                m_mdiArea->setActiveSubWindow(window);
                if (m_navigator) m_navigator->enableSampleCheckboxesByDataType(QStringLiteral("色谱"), true);
                return;
            }
        }
    }
    
    DEBUG_LOG << "Creating new ChromatographDataProcessDialog";

    // 创建新的窗口
    chromatographDataProcessDialog = new ChromatographDataProcessDialog(this, m_appInitializer, m_navigator);
    // 色谱窗口使用透明图标
    {
        QPixmap px(1, 1);
        px.fill(Qt::transparent);
        chromatographDataProcessDialog->setWindowIcon(QIcon(px));
    }
    
    // 连接样本选中信号，用于同步主窗口导航树选择框状态
    connect(chromatographDataProcessDialog, &ChromatographDataProcessDialog::sampleSelected, 
            this, [this](int sampleId, bool selected) {
                if (m_navigator) {
                    m_navigator->setSampleCheckState(sampleId, selected);
                }
            });

    // 当 dialog 发出请求时，让 MainWindow 的 onCreateDifferenceWorkbench 槽函数来处理
    connect(chromatographDataProcessDialog, &ChromatographDataProcessDialog::requestNewChromatographDifferenceWorkbench,
            this, &MainWindow::onCreateChromatographDifferenceWorkbench);

    // 将窗口添加到MDI区域
    QMdiSubWindow* subWindow = m_mdiArea->addSubWindow(chromatographDataProcessDialog);
    subWindow->setAttribute(Qt::WA_DeleteOnClose);
    subWindow->setProperty("sampleId", -1); // 设置一个默认的sampleId属性
    
    // 连接销毁信号，确保指针被重置
    connect(chromatographDataProcessDialog, &QWidget::destroyed, this, [this]() {
        chromatographDataProcessDialog = nullptr;
    });
    
    // 添加到导航器的打开窗口列表
    m_navigator->addOpenView(subWindow);
    if (m_navigator) m_navigator->enableSampleCheckboxesByDataType(QStringLiteral("色谱"), true);
    
    // 显示窗口
    subWindow->show();
    // chromatographDataProcessDialog->show();
    subWindow->showMaximized();  //  这里最大化
}


// // onProcessTgBigDataProcessActionTriggered
// void MainWindow::onProcessTgBigDataProcessActionTriggered()
// {
//     // 如果窗口已经存在，则显示并激活它
//     if (processTgBigDataProcessDialog) {
//         // 查找包含此窗口的子窗口
//         for (QMdiSubWindow* window : m_mdiArea->subWindowList()) {
//             if (window->widget() == processTgBigDataProcessDialog) {
//                 m_mdiArea->setActiveSubWindow(window);
//                 return;
//             }
//         }
//     }

//     // 创建新的窗口，传递主界面导航树
//     processTgBigDataProcessDialog = new processTgBigDataProcessDialog(this, m_navigator);

//     // 连接样本选中信号，用于同步主窗口导航树选择框状态
//     connect(processTgBigDataProcessDialog, &processTgBigDataProcessDialog::sampleSelected, 
//             this, [this](int sampleId, bool selected) {
//                 if (m_navigator) {
//                     m_navigator->setSampleCheckState(sampleId, selected);
//                 }
//             });

    
//     // 将窗口添加到MDI区域
//     QMdiSubWindow* subWindow = m_mdiArea->addSubWindow(processTgBigDataProcessDialog);
//     subWindow->setProperty("sampleId", -1); // 设置一个默认的sampleId属性
    
//     // 添加到导航器的打开窗口列表
//     m_navigator->addOpenView(subWindow);
//     subWindow->setAttribute(Qt::WA_DeleteOnClose);
    
//     // 连接销毁信号，确保指针被重置
//     connect(processTgBigDataProcessDialog, &QWidget::destroyed, this, [this]() {
//         processTgBigDataProcessDialog = nullptr;
//     });

//     // 显示窗口
//     subWindow->show();
//     processTgBigDataProcessDialog->show();
// }



void MainWindow::onProcessTgBigDataProcessActionTriggered()
{
    DEBUG_LOG << "MainWindow::onProcessTgBigDataProcessActionTriggered called";
    // 如果窗口已经存在，则显示并激活它
    if (processTgBigDataProcessDialog) {
        // 查找包含此窗口的子窗口
        for (QMdiSubWindow* window : m_mdiArea->subWindowList()) {
            if (window->widget() == processTgBigDataProcessDialog) {
                m_mdiArea->setActiveSubWindow(window);
                if (m_navigator) m_navigator->enableSampleCheckboxesByDataType(QStringLiteral("工序大热重"), true);
                return;
            }
        }
    }

    DEBUG_LOG << "Creating new processTgBigDataProcessDialog";

    // 创建新的窗口
    processTgBigDataProcessDialog = new ProcessTgBigDataProcessDialog(this, m_appInitializer, m_navigator);
    // 工序大热重窗口使用透明图标
    {
        QPixmap px(1, 1);
        px.fill(Qt::transparent);
        processTgBigDataProcessDialog->setWindowIcon(QIcon(px));
    }

    // 连接样本选中信号，用于同步主窗口导航树选择框状态
    connect(processTgBigDataProcessDialog, &ProcessTgBigDataProcessDialog::sampleSelected,
            this, [this](int sampleId, bool selected) {
                if (m_navigator) {
                    m_navigator->setSampleCheckState(sampleId, selected);
                }
            });

    // 当 dialog 发出请求时，让 MainWindow 的  槽函数来处理
    connect(processTgBigDataProcessDialog, &ProcessTgBigDataProcessDialog::requestNewProcessTgBigDifferenceWorkbench,
            this, &MainWindow::onCreateProcessTgBigDifferenceWorkbench);

    // 将窗口添加到MDI区域
    QMdiSubWindow* subWindow = m_mdiArea->addSubWindow(processTgBigDataProcessDialog);
    subWindow->setAttribute(Qt::WA_DeleteOnClose);
    subWindow->setProperty("sampleId", -1); // 设置一个默认的sampleId属性
    
    // 连接销毁信号，确保指针被重置
    connect(processTgBigDataProcessDialog, &QWidget::destroyed, this, [this]() {
        processTgBigDataProcessDialog = nullptr;
    });
    
    // 添加到导航器的打开窗口列表
    m_navigator->addOpenView(subWindow);
    if (m_navigator) m_navigator->enableSampleCheckboxesByDataType(QStringLiteral("工序大热重"), true);
    
    // 显示窗口
    subWindow->show();
    // chromatographDataProcessDialog->show();
    subWindow->showMaximized();  //  这里最大化
}


void MainWindow::onSampleSelectionChanged(int sampleId, const QString &sampleName, 
                                         const QString &dataType, bool selected)
{
    // 为避免与统一管理器信号造成重复路由，这里仅同步导航树复选框状态，不再直接调用各处理界面的绘图逻辑。
    DEBUG_LOG << "MainWindow::onSampleSelectionChanged(UI) ID=" << sampleId << ", DataType=" << dataType << ", Selected=" << selected;

    if (m_navigator) {
        m_navigator->setSampleCheckState(sampleId, selected);
    }
}

// 打印所有选中的样本，按照数据类型分组显示（弹窗+日志）
void MainWindow::onPrintSelectedSamplesGrouped()
{
    // 创建一个带下拉框的对话框，用于按类型分组查看选中样本
    auto mgr = SampleSelectionManager::instance();
    QDialog dlg(this);
    dlg.setWindowTitle(tr("选中样本（按类型分组）"));
    QVBoxLayout* layout = new QVBoxLayout(&dlg);

    QLabel* label = new QLabel(tr("选择数据类型："), &dlg);
    QComboBox* combo = new QComboBox(&dlg);
    combo->addItems({
        QStringLiteral("大热重"),
        QStringLiteral("小热重"),
        QStringLiteral("小热重（原始数据）"),
        QStringLiteral("色谱"),
        QStringLiteral("工序大热重")
    });

    // 统计概览
    QLabel* statLabel = new QLabel(&dlg);
    auto refreshStats = [&]() {
        QHash<QString,int> counts = mgr->selectedCountsAllTypes();
        QString s = tr("【选中样本统计】\n");
        s += QString("大热重: %1个\n").arg(counts.value(QStringLiteral("大热重"), 0));
        s += QString("小热重: %1个\n").arg(counts.value(QStringLiteral("小热重"), 0));
        s += QString("小热重（原始数据）: %1个\n").arg(counts.value(QStringLiteral("小热重（原始数据）"), 0));
        s += QString("色谱: %1个\n").arg(counts.value(QStringLiteral("色谱"), 0));
        s += QString("工序大热重: %1个").arg(counts.value(QStringLiteral("工序大热重"), 0));
        statLabel->setText(s);
    };
    refreshStats();

    // 列表区域
    QListWidget* list = new QListWidget(&dlg);
    auto populate = [&](const QString& dataType){
        list->clear();
        QSet<int> ids = mgr->selectedIdsByType(dataType);
        SingleTobaccoSampleDAO dao;
        for (int id : ids) {
            QString line;
            try {
                SampleIdentifier sid = dao.getSampleIdentifierById(id);
                line = QString("%1-%2-%3-%4 (ID=%5)")
                        .arg(sid.projectName)
                        .arg(sid.batchCode)
                        .arg(sid.shortCode)
                        .arg(sid.parallelNo)
                        .arg(id);
            } catch (...) {
                line = QString("样本 %1").arg(id);
            }
            list->addItem(line);
        }
    };

    // 联动下拉框变化
    connect(combo, &QComboBox::currentTextChanged, &dlg, [&](const QString& t){ populate(t); });
    combo->setCurrentText(QStringLiteral("大热重"));
    populate(combo->currentText());

    // 底部关闭按钮
    QHBoxLayout* bottom = new QHBoxLayout();
    QPushButton* closeBtn = new QPushButton(tr("关闭"), &dlg);
    connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    bottom->addStretch();
    bottom->addWidget(closeBtn);

    layout->addWidget(label);
    layout->addWidget(combo);
    layout->addWidget(statLabel);
    layout->addWidget(list);
    layout->addLayout(bottom);

    dlg.resize(520, 420);
    dlg.exec();
}

// // 数据管理界面
// void MainWindow::on_actionOpenSingleMaterialData_triggered()
// {
//     if (!m_singleTobaccoSampleService) {
//         QMessageBox::critical(this, "错误", "单料烟服务未初始化，无法打开数据界面。", QMessageBox::Ok);
//         statusBar()->showMessage("错误: 单料烟服务未就绪。", 5000);
//         return;
//     }

//     SingleMaterialDataWidget* widget =
//         new SingleMaterialDataWidget(m_singleTobaccoSampleService, m_appInitializer, this);

//     // 可选：如果你用 MDI 区域管理子窗口
//     QMdiSubWindow* subWindow = m_mdiArea->addSubWindow(widget);
//     subWindow->setWindowTitle("单料烟样本数据库");
//     widget->show();

//     subWindow->showMaximized();


//     // 信号连接
//     connect(widget, &SingleMaterialDataWidget::statusMessage,
//             this, [=](const QString& msg, int timeout){ statusBar()->showMessage(msg, timeout); });

//     // setWindowTitle("数据管理"); // 更新窗口标题
//     // statusBar()->showMessage("已打开单料烟数据管理界面。", 3000);
//     // singleMaterialDataWidget->show(); // 确保显示
// }

void MainWindow::on_actionOpenSingleMaterialData_triggered()
{
    if (!m_singleTobaccoSampleService) {
        QMessageBox::critical(this, "错误", "单料烟服务未初始化，无法打开数据界面。", QMessageBox::Ok);
        statusBar()->showMessage("错误: 单料烟服务未就绪。", 5000);
        return;
    }

    // 检查是否已有同类窗口
    for (QMdiSubWindow* subWindow : m_mdiArea->subWindowList()) {
        QWidget* w = subWindow->widget();
        if (qobject_cast<SingleMaterialDataWidget*>(w)) {
            // 已存在，直接激活
            m_mdiArea->setActiveSubWindow(subWindow);
            subWindow->showMaximized();
            statusBar()->showMessage("已切换到单料烟数据界面。", 3000);
            return;
        }
    }

    // 没有则新建
    SingleMaterialDataWidget* widget =
        new SingleMaterialDataWidget(m_singleTobaccoSampleService, m_appInitializer, this);
    // 单料烟数据窗口使用透明图标
    {
        QPixmap px(1, 1);
        px.fill(Qt::transparent);
        widget->setWindowIcon(QIcon(px));
    }

    QMdiSubWindow* subWindow = m_mdiArea->addSubWindow(widget);
    subWindow->setWindowTitle("单料烟样本数据库");
    widget->show();
    subWindow->showMaximized();

    connect(widget, &SingleMaterialDataWidget::statusMessage,
            this, [=](const QString& msg, int timeout){ statusBar()->showMessage(msg, timeout); });

    // 连接数据导入完成信号，自动刷新导航树
    // TG/小热重/色谱数据归入数据源刷新；工序大热重归入工序数据刷新
    connect(widget, &SingleMaterialDataWidget::dataImportFinished,
            this, [this](const QString& category) {
                // 根据导入类别刷新导航树，并写入操作日志
                if (!m_navigator) return; // 防御性检查
                QString typeName;
                if (category == "PROCESS_TG_BIG") {
                    m_navigator->refreshProcessData();
                    typeName = QStringLiteral("工序大热重");
                } else if (category == "TG_BIG") {
                    m_navigator->refreshDataSource();
                    typeName = QStringLiteral("大热重");
                } else if (category == "TG_SMALL") {
                    m_navigator->refreshDataSource();
                    typeName = QStringLiteral("小热重");
                } else if (category == "TG_SMALL_RAW") {
                    m_navigator->refreshDataSource();
                    typeName = QStringLiteral("小热重（原始数据）");
                } else if (category == "CHROMATOGRAPHY") {
                    m_navigator->refreshDataSource();
                    typeName = QStringLiteral("色谱");
                }
                if (!typeName.isEmpty()) {
                    logToOperationPanel(QStringLiteral("【导入】%1 数据导入成功，已刷新导航树").arg(typeName));
                }
            });
}



void MainWindow::onCreateTgBigDifferenceWorkbench(int referenceSampleId,
                                             const BatchGroupData& allProcessedData,
                                             const ProcessingParameters& params)
{
    // --- 1. 如果已有差异度窗口，先关闭 ---
    if (m_tgBigDifferenceWorkbench) {
        // 从导航器移除对应条目
        m_navigator->removeOpenView(m_tgBigDifferenceWorkbench);

        // 关闭窗口（触发 TgBigDifferenceWorkbench::closeEvent）
        m_tgBigDifferenceWorkbench->close();
        m_tgBigDifferenceWorkbench = nullptr;
    }

    // --- 2. 创建新的 TgBigDifferenceWorkbench ---
    m_tgBigDifferenceWorkbench = new TgBigDifferenceWorkbench(
        referenceSampleId,        // 参考曲线
        allProcessedData,             // 所有对比曲线
        m_appInitializer,      // AppInitializer
        params,                // 处理参数
        this
    );
    // 差异度工作台窗口也统一使用透明图标
    {
        QPixmap px(1, 1);
        px.fill(Qt::transparent);
        m_tgBigDifferenceWorkbench->setWindowIcon(QIcon(px));
    }

    // 创建窗口时连接
    connect(m_tgBigDifferenceWorkbench, &TgBigDifferenceWorkbench::closed,
            this, &MainWindow::onTgBigDifferenceWorkbenchClosed);


    // --- 3. 添加到 MDI 区域 ---
    m_mdiArea->addSubWindow(m_tgBigDifferenceWorkbench);

    // 窗口关闭时自动删除
    m_tgBigDifferenceWorkbench->setAttribute(Qt::WA_DeleteOnClose);

    // --- 4. 显示窗口 ---
    m_tgBigDifferenceWorkbench->show();

    // --- 5. 添加到导航器 ---
    m_navigator->addOpenView(m_tgBigDifferenceWorkbench);
}



void MainWindow::onCreateTgSmallDifferenceWorkbench(int referenceSampleId,
                                             const BatchGroupData& allProcessedData,
                                             const ProcessingParameters& params)
{
    // --- 1. 如果已有差异度窗口，先关闭 ---
    if (m_tgSmallDifferenceWorkbench) {
        // 从导航器移除对应条目
        m_navigator->removeOpenView(m_tgSmallDifferenceWorkbench);

        // 关闭窗口（触发 TgBigDifferenceWorkbench::closeEvent）
        m_tgSmallDifferenceWorkbench->close();
        m_tgSmallDifferenceWorkbench = nullptr;
    }

    // --- 2. 创建新的 TgBigDifferenceWorkbench ---
    m_tgSmallDifferenceWorkbench = new TgSmallDifferenceWorkbench(
        referenceSampleId,        // 参考曲线
        allProcessedData,             // 所有对比曲线
        m_appInitializer,      // AppInitializer
        params,                // 处理参数（包含权重）
        this
    );
    {
        QPixmap px(1, 1);
        px.fill(Qt::transparent);
        m_tgSmallDifferenceWorkbench->setWindowIcon(QIcon(px));
    }

    // 创建窗口时连接
    connect(m_tgSmallDifferenceWorkbench, &TgSmallDifferenceWorkbench::closed,
            this, &MainWindow::onTgSmallDifferenceWorkbenchClosed);

   


    // --- 3. 添加到 MDI 区域 ---
    m_mdiArea->addSubWindow(m_tgSmallDifferenceWorkbench);

    // 窗口关闭时自动删除
    m_tgSmallDifferenceWorkbench->setAttribute(Qt::WA_DeleteOnClose);

    // --- 4. 显示窗口 ---
    m_tgSmallDifferenceWorkbench->show();

    // --- 5. 添加到导航器 ---
    m_navigator->addOpenView(m_tgSmallDifferenceWorkbench);
}





void MainWindow::onCreateChromatographDifferenceWorkbench(int referenceSampleId,
                                             const BatchGroupData& allProcessedData,
                                             const ProcessingParameters& params)
{
    // --- 1. 如果已有差异度窗口，先关闭 ---
    if (m_chromatographDifferenceWorkbench) {
        // 从导航器移除对应条目
        m_navigator->removeOpenView(m_chromatographDifferenceWorkbench);

        // 关闭窗口（触发 TgBigDifferenceWorkbench::closeEvent）
        m_chromatographDifferenceWorkbench->close();
        m_chromatographDifferenceWorkbench = nullptr;
    }

    // --- 2. 创建新的 TgChromatographDifferenceWorkbench ---
    m_chromatographDifferenceWorkbench = new ChromatographDifferenceWorkbench(
        referenceSampleId,        // 参考曲线
        allProcessedData,             // 所有对比曲线
        m_appInitializer,      // AppInitializer
        params,                // 处理参数
        this
    );
    {
        QPixmap px(1, 1);
        px.fill(Qt::transparent);
        m_chromatographDifferenceWorkbench->setWindowIcon(QIcon(px));
    }

    // 创建窗口时连接
    connect(m_chromatographDifferenceWorkbench, &ChromatographDifferenceWorkbench::closed,
            this, &MainWindow::onChromatographDifferenceWorkbenchClosed);

    // --- 3. 添加到 MDI 区域 ---
    m_mdiArea->addSubWindow(m_chromatographDifferenceWorkbench);

    // 窗口关闭时自动删除
    m_chromatographDifferenceWorkbench->setAttribute(Qt::WA_DeleteOnClose);

    // --- 4. 显示窗口 ---
    m_chromatographDifferenceWorkbench->show();

    // --- 5. 添加到导航器 ---
    m_navigator->addOpenView(m_chromatographDifferenceWorkbench);
}



void MainWindow::onCreateProcessTgBigDifferenceWorkbench(int referenceSampleId,
                                             const BatchGroupData& allProcessedData,
                                             const ProcessingParameters& params)
{
    // --- 1. 如果已有差异度窗口，先关闭 ---
    if (m_processTgBigDifferenceWorkbench) {
        // 从导航器移除对应条目
        m_navigator->removeOpenView(m_processTgBigDifferenceWorkbench);

        // 关闭窗口（触发 TgBigDifferenceWorkbench::closeEvent）
        m_processTgBigDifferenceWorkbench->close();
        m_processTgBigDifferenceWorkbench = nullptr;
    }

    // --- 2. 创建新的 TgProcessTgBigDifferenceWorkbench ---
    m_processTgBigDifferenceWorkbench = new ProcessTgBigDifferenceWorkbench(
        referenceSampleId,        // 参考曲线
        allProcessedData,             // 所有对比曲线
        m_appInitializer,      // AppInitializer
        params,                // 处理参数
        this
    );
    {
        QPixmap px(1, 1);
        px.fill(Qt::transparent);
        m_processTgBigDifferenceWorkbench->setWindowIcon(QIcon(px));
    }

    // 创建窗口时连接
    connect(m_processTgBigDifferenceWorkbench, &ProcessTgBigDifferenceWorkbench::closed,
            this, &MainWindow::onProcessTgBigDifferenceWorkbenchClosed);

    // --- 3. 添加到 MDI 区域 ---
    m_mdiArea->addSubWindow(m_processTgBigDifferenceWorkbench);

    // 窗口关闭时自动删除
    m_processTgBigDifferenceWorkbench->setAttribute(Qt::WA_DeleteOnClose);

    // --- 4. 显示窗口 ---
    m_processTgBigDifferenceWorkbench->show();

    // --- 5. 添加到导航器 ---
    m_navigator->addOpenView(m_processTgBigDifferenceWorkbench);
}



// void MainWindow::onCreateDifferenceWorkbench(
//     int referenceSampleId, 
//     const BatchMultiStageData &allProcessedData)
// {
//     // 直接将接收到的数据转发给新的工作台
//     TgBigDifferenceWorkbench* wb = new TgBigDifferenceWorkbench(referenceSampleId, allProcessedData, this);
//     m_mdiArea->addSubWindow(wb);
//     wb->show();
// }



void MainWindow::onTgBigDifferenceWorkbenchClosed(TgBigDifferenceWorkbench* wb)
{
    if (wb == m_tgBigDifferenceWorkbench) {
        m_tgBigDifferenceWorkbench = nullptr;
    }

    // 从导航器中删除对应条目
    m_navigator->removeOpenView(wb);
}

void MainWindow::onTgSmallDifferenceWorkbenchClosed(TgSmallDifferenceWorkbench* wb)
{
    if (wb == m_tgSmallDifferenceWorkbench) {
        m_tgSmallDifferenceWorkbench = nullptr;
    }

    // 从导航器中删除对应条目
    m_navigator->removeOpenView(wb);
}

void MainWindow::onChromatographDifferenceWorkbenchClosed(ChromatographDifferenceWorkbench* wb)
{
    if (wb == m_chromatographDifferenceWorkbench) {
        m_chromatographDifferenceWorkbench = nullptr;
    }

    // 从导航器中删除对应条目
    m_navigator->removeOpenView(wb);
}

void MainWindow::onProcessTgBigDifferenceWorkbenchClosed(ProcessTgBigDifferenceWorkbench* wb)
{
    if (wb == m_processTgBigDifferenceWorkbench) {
        m_processTgBigDifferenceWorkbench = nullptr;
    }

    // 从导航器中删除对应条目
    m_navigator->removeOpenView(wb);
}



// 放在 #include 后面
static QString nodeTypeToString(NavigatorNodeInfo::NodeType type)
{
    switch(type) {
        case NavigatorNodeInfo::Root: return "Root";
        case NavigatorNodeInfo::Model: return "Model";
        case NavigatorNodeInfo::Batch: return "Batch";
        case NavigatorNodeInfo::ShortCode: return "ShortCode";
        case NavigatorNodeInfo::DataType: return "DataType";
        case NavigatorNodeInfo::Sample: return "Sample";
        default: return "Unknown";
    }
}


// 批次添加样本
void MainWindow::onSelectAllSamplesInBatch(const NavigatorNodeInfo& batchInfo)
{
    DEBUG_LOG << "===== onSelectAllSamplesInBatch called =====";
    DEBUG_LOG << "节点类型:" << nodeTypeToString(batchInfo.type);
    DEBUG_LOG << "ID:" << batchInfo.id;
    DEBUG_LOG << "项目名:" << batchInfo.projectName;
    DEBUG_LOG << "批次号:" << batchInfo.batchCode;
    DEBUG_LOG << "样本编码:" << batchInfo.shortCode;
    DEBUG_LOG << "平行号:" << batchInfo.parallelNo;
    DEBUG_LOG << "数据类型:" << batchInfo.dataType;

    printWindowInfo();

    

    // 仅处理批次类型节点
    if (batchInfo.type != NavigatorNodeInfo::Batch) {
        DEBUG_LOG << "节点不是批次类型，跳过";
        return;
    }

    // 获取活动窗口
    QMdiSubWindow* activeWindow = m_mdiArea->activeSubWindow();

    if(activeWindow){
        QString title = activeWindow->windowTitle();

        // 为避免将“工序大热重”误判为“大热重”，先匹配更具体的窗口标题
        // 1) 工序大热重
        if (title.contains("工序大热重")) {
            // 检查工序大热重数据处理对话框是否已打开
            if (!processTgBigDataProcessDialog || !processTgBigDataProcessDialog->isVisible()) {
                DEBUG_LOG << "工序大热重数据处理对话框未打开，创建它";
                onProcessTgBigDataProcessActionTriggered();
            }

            if (processTgBigDataProcessDialog && processTgBigDataProcessDialog->isVisible()) {
                DEBUG_LOG << "调用工序大热重对话框方法选择批次下的所有样本";
                // 将项目名与批次号传入对话框方法
                processTgBigDataProcessDialog->onSelectAllSamplesInBatch(batchInfo.projectName, batchInfo.batchCode);
                // 批次添加后，同步统一管理器与主导航复选框（确保未展开节点也能被勾选）
                SingleTobaccoSampleDAO dao;
                auto samples = dao.getSampleIdentifiersByProjectAndBatch(batchInfo.projectName, batchInfo.batchCode, BatchType::PROCESS);
                for (const auto& sid : samples) {
                    SampleSelectionManager::instance()->setSelectedWithType(sid.sampleId, QStringLiteral("工序大热重"), true, QStringLiteral("BatchSelect"));
                    if (m_navigator) m_navigator->setSampleCheckStateForType(sid.sampleId, QStringLiteral("工序大热重"), true);
                }
            }
        }
        // 2) 色谱
        else if (title.contains("色谱")) {
            // 检查色谱数据处理对话框是否已打开
            if (!chromatographDataProcessDialog || !chromatographDataProcessDialog->isVisible()) {
                DEBUG_LOG << "色谱数据处理对话框未打开，创建它";
                onChromatographDataProcessActionTriggered();
            }

            if (chromatographDataProcessDialog && chromatographDataProcessDialog->isVisible()) {
                DEBUG_LOG << "调用色谱对话框方法选择批次下的所有样本";
                // 将项目名与批次号传入对话框方法
                chromatographDataProcessDialog->onSelectAllSamplesInBatch(batchInfo.projectName, batchInfo.batchCode);
                // 批次添加后，同步统一管理器与主导航复选框（色谱）
                SingleTobaccoSampleDAO dao;
                auto samples = dao.getSampleIdentifiersByProjectAndBatch(batchInfo.projectName, batchInfo.batchCode, BatchType::NORMAL);
                for (const auto& sid : samples) {
                    SampleSelectionManager::instance()->setSelectedWithType(sid.sampleId, QStringLiteral("色谱"), true, QStringLiteral("BatchSelect"));
                    if (m_navigator) m_navigator->setSampleCheckStateForType(sid.sampleId, QStringLiteral("色谱"), true);
                }
            }
        }
        // 3) 大热重
        else if (title.contains("大热重")) {
            // 检查大热重数据处理对话框是否已打开
            if (!tgBigDataProcessDialog || !tgBigDataProcessDialog->isVisible()) {
                DEBUG_LOG << "大热重数据处理对话框未打开，创建它";
                onTgBigDataProcessActionTriggered();
            }

            if (tgBigDataProcessDialog && tgBigDataProcessDialog->isVisible()) {
                DEBUG_LOG << "调用大热重对话框方法选择批次下的所有样本00";
                // 将项目名与批次号传入对话框方法
                tgBigDataProcessDialog->onSelectAllSamplesInBatch(batchInfo.projectName, batchInfo.batchCode);
                // 批次添加后，同步统一管理器与主导航复选框（大热重）
                SingleTobaccoSampleDAO dao;
                auto samples = dao.getSampleIdentifiersByProjectAndBatch(batchInfo.projectName, batchInfo.batchCode, BatchType::NORMAL);
                for (const auto& sid : samples) {
                    DEBUG_LOG << "调用大热重对话框方法选择批次下的所有样本11";
                    SampleSelectionManager::instance()->setSelectedWithType(sid.sampleId, QStringLiteral("大热重"), true, QStringLiteral("BatchSelect"));
                    if (m_navigator) m_navigator->setSampleCheckStateForType(sid.sampleId, QStringLiteral("大热重"), true);
                }
            }
        }
        // 4) 小热重（原始数据）
        else if (title.contains("小热重（原始数据）")) {
            if (!tgSmallRawDataProcessDialog || !tgSmallRawDataProcessDialog->isVisible()) {
                DEBUG_LOG << "小热重（原始数据）处理对话框未打开，创建它";
                onTgSmallRawDataProcessActionTriggered();
            }

            if (tgSmallRawDataProcessDialog && tgSmallRawDataProcessDialog->isVisible()) {
                DEBUG_LOG << "调用小热重（原始数据）对话框方法选择批次下的所有样本";
                tgSmallRawDataProcessDialog->onSelectAllSamplesInBatch(batchInfo.projectName, batchInfo.batchCode);
                SingleTobaccoSampleDAO dao;
                auto samples = dao.getSampleIdentifiersByProjectAndBatch(batchInfo.projectName, batchInfo.batchCode, BatchType::NORMAL);
                for (const auto& sid : samples) {
                    SampleSelectionManager::instance()->setSelectedWithType(sid.sampleId, QStringLiteral("小热重（原始数据）"), true, QStringLiteral("BatchSelect"));
                    if (m_navigator) m_navigator->setSampleCheckStateForType(sid.sampleId, QStringLiteral("小热重（原始数据）"), true);
                }
            }
        }
        // 5) 小热重
        else if (title.contains("小热重")){
            // 检查小热重数据处理对话框是否已打开
            if (!tgSmallDataProcessDialog || !tgSmallDataProcessDialog->isVisible()) {
                DEBUG_LOG << "小热重数据处理对话框未打开，创建它";
                onTgSmallDataProcessActionTriggered();
            }

            if (tgSmallDataProcessDialog && tgSmallDataProcessDialog->isVisible()) {
                DEBUG_LOG << "调用小热重对话框方法选择批次下的所有样本";
                // 将项目名与批次号传入对话框方法
                tgSmallDataProcessDialog->onSelectAllSamplesInBatch(batchInfo.projectName, batchInfo.batchCode);
                // 批次添加后，同步统一管理器与主导航复选框（小热重）
                SingleTobaccoSampleDAO dao;
                auto samples = dao.getSampleIdentifiersByProjectAndBatch(batchInfo.projectName, batchInfo.batchCode, BatchType::NORMAL);
                for (const auto& sid : samples) {
                    SampleSelectionManager::instance()->setSelectedWithType(sid.sampleId, QStringLiteral("小热重"), true, QStringLiteral("BatchSelect"));
                    if (m_navigator) m_navigator->setSampleCheckStateForType(sid.sampleId, QStringLiteral("小热重"), true);
                }
            }
        }
        // 其他窗口类型（未匹配到）
        else {
            DEBUG_LOG << "未匹配到可处理批次样本的窗口类型: " << title;
            QMessageBox::information(this, tr("提示"), tr("当前窗口不支持按批次添加样本"));
        }
    }else{
        DEBUG_LOG << "当前没有激活的窗口，无法处理批量样本选择";
        //使用提示窗口
        QMessageBox::warning(this, tr("操作失败"), tr("当前没有激活的窗口，无法处理批量样本选择"));
    }
}



void MainWindow::onSubWindowActivated(QMdiSubWindow *window)
{
    
    if (!window) {
        DEBUG_LOG << "当前没有激活的窗口";
        return;
    }
    DEBUG_LOG << "子窗口激活:" << window->windowTitle();
    DEBUG_LOG << "============================";
    DEBUG_LOG << "激活窗口标题:" << window->windowTitle();
    DEBUG_LOG << "窗口对象名称:" << window->objectName();
    DEBUG_LOG << "窗口位置:" << window->geometry().topLeft();
    DEBUG_LOG << "窗口大小:" << window->geometry().size();
    DEBUG_LOG << "是否最大化:" << window->isMaximized();
    DEBUG_LOG << "============================";

    QWidget *widget = window->widget();
    if (widget) {
        DEBUG_LOG << "内部Widget对象类型:" << widget->metaObject()->className();
    }
}
void MainWindow::onAboutActionTriggered()
{
    // 构造“关于”信息内容，包含版本、支持数据类型、联系方式等
    // QString appName = QCoreApplication::applicationName();
    QString appName;
    if (appName.isEmpty()) appName = QStringLiteral("烟草数据分析系统");
    QString version = QCoreApplication::applicationVersion();
    if (version.isEmpty()) version = QStringLiteral("v1.3");
    // QString os = QSysInfo::prettyProductName();
    QString arch = QSysInfo::currentCpuArchitecture();

    QString content = QString(
        "<b>%1</b><br/>"
        "版本：%2<br/>"
        // "操作系统：%3（%4）<br/>"
        "支持数据类型：大热重、小热重、小热重（原始数据）、色谱、工序大热重<br/>"
        // "主要功能：数据导入与浏览、参数配置、批量处理、基线校正、差异度分析、绘图导出等<br/>"
        // "联系方式：support@example.com<br/>"
        ).arg(appName).arg(version)
        // .arg(os)
        .arg(arch);

    QMessageBox::about(this, tr("关于"), content);
}
