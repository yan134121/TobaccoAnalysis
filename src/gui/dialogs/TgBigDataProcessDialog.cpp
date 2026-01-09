#include "TgBigDataProcessDialog.h"
#include <QDebug>
#include <QSplitter>
#include <QHeaderView>
#include <QMessageBox>
#include <QMainWindow>
#include <QStatusBar>
#include <QDateTime>
#include <QCoreApplication>
#include <QtConcurrent>
#include <QElapsedTimer>
#include <QTimer>


#include "Logger.h"
#include "gui/views/DataNavigator.h"
#include "core/common.h"
#include "TgBigParameterSettingsDialog.h"
#include "AppInitializer.h"
#include "gui/views/ChartView.h"
#include "core/entities/Curve.h"
#include "SingleTobaccoSampleDAO.h" // 修改: 包含对应的头文件
#include "data_access/SampleDAO.h"
#include "ColorUtils.h"
#include "core/singletons/SampleSelectionManager.h"
#include "InfoAutoClose.h"
// 代表样选择服务与导出所需控件
#include "services/analysis/ParallelSampleAnalysisService.h"
#include "third_party/QXlsx/header/xlsxdocument.h"
#include <QCursor>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QComboBox>
#include <QFormLayout>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QFileDialog>
#include <QTableWidget>
#include <QTableWidgetItem>

TgBigDataProcessDialog::TgBigDataProcessDialog(QWidget *parent, AppInitializer* appInitializer, DataNavigator *mainNavigator) :
    QWidget(parent), m_appInitializer(appInitializer), m_mainNavigator(mainNavigator)
{
    // 安全检查
    if (!m_appInitializer) {
        
        qFatal("TgBigDataProcessDialog created without a valid AppInitializer!");
    }

    // 【关键】使用传入的 appInitializer 来获取服务
    m_processingService = m_appInitializer->getDataProcessingService();

    setWindowTitle(tr("大热重数据处理"));
    // setMinimumSize(1200, 800);
    


    // 提前创建参数设置窗口
    m_paramDialog = new TgBigParameterSettingsDialog(m_currentParams, this);

    // 连接信号槽（只需一次）
    connect(m_paramDialog, &TgBigParameterSettingsDialog::parametersApplied,
            this, &TgBigDataProcessDialog::onParametersApplied);
    
    setupUI();
    setupConnections();
    loadNavigatorData();

    // 首次打开时同步主导航已选中的“大热重”样本，并立即绘制
    {
        QSet<int> preselected = SampleSelectionManager::instance()->selectedIdsByType(QStringLiteral("大热重"));
        if (!preselected.isEmpty()) {
            m_suppressItemChanged = true; // 防止触发 onItemChanged 递归
            for (int sampleId : preselected) {
                // 1) 在左侧导航树中勾选对应样本
                for (int i = 0; i < m_leftNavigator->topLevelItemCount(); ++i) {
                    QTreeWidgetItem* projectItem = m_leftNavigator->topLevelItem(i);
                    for (int j = 0; j < projectItem->childCount(); ++j) {
                        QTreeWidgetItem* batchItem = projectItem->child(j);
                        for (int k = 0; k < batchItem->childCount(); ++k) {
                            QTreeWidgetItem* sampleItem = batchItem->child(k);
                            if (sampleItem->data(0, Qt::UserRole).toInt() == sampleId) {
                                sampleItem->setFlags(sampleItem->flags() | Qt::ItemIsUserCheckable);
                                sampleItem->setCheckState(0, Qt::Checked);
                            }
                        }
                    }
                }
                // 2) 添加到本界面选中集合，供绘制使用（统一名称格式）
                QString name = buildSampleDisplayName(sampleId);
                m_selectedSamples.insert(sampleId, name);
            }
            m_suppressItemChanged = false;
            drawSelectedSampleCurves();
        }
    }

    // showMaximized();   //  在这里最大化窗口

    if (!m_mainNavigator) {
        WARNING_LOG << "⚠️ mainNavigator is nullptr!";
    } else {
        DEBUG_LOG << " mainNavigator is valid:" << m_mainNavigator;
    }

    DEBUG_LOG << "TgBigDataProcessDialog::TgBigDataProcessDialog - Finished";
}

TgBigDataProcessDialog::~TgBigDataProcessDialog()
{
    // delete ui; // Removed as UI is code-built
}

QMap<int, QString> TgBigDataProcessDialog::getSelectedSamples() const
{
    QMap<int, QString> selectedSamples;
    
    // 遍历导航树中所有样本节点
    for (int i = 0; i < m_leftNavigator->topLevelItemCount(); ++i) {
        QTreeWidgetItem* projectItem = m_leftNavigator->topLevelItem(i);
        
        for (int j = 0; j < projectItem->childCount(); ++j) {
            QTreeWidgetItem* batchItem = projectItem->child(j);
            
            for (int k = 0; k < batchItem->childCount(); ++k) {
                QTreeWidgetItem* sampleItem = batchItem->child(k);
                
                // 检查样本是否被选中
                if (sampleItem->checkState(0) == Qt::Checked) {
                    int sampleId = sampleItem->data(0, Qt::UserRole).toInt();
                    QString sampleName = sampleItem->text(0);
                    selectedSamples.insert(sampleId, sampleName);
                }
            }
        }
    }
    
    for (auto it = selectedSamples.begin(); it != selectedSamples.end(); ++it) {
        DEBUG_LOG << "sampleId:" << it.key() << "sampleName:" << it.value();
    }


    return selectedSamples;
}

int TgBigDataProcessDialog::countSelectedSamples() const
{
    return getSelectedSamples().size();
}

void TgBigDataProcessDialog::showSelectedSamplesStatistics()
{
    QMap<int, QString> selectedSamples = getSelectedSamples();
    int count = selectedSamples.size();
    
    QString message = tr("当前选中的大热重样本数量: %1\n\n").arg(count);
    
    if (count > 0) {
        message += tr("选中的样本列表:\n");
        QMapIterator<int, QString> i(selectedSamples);
        while (i.hasNext()) {
            i.next();
            message += tr("ID: %1, 名称: %2\n").arg(i.key()).arg(i.value());
        }
    } else {
        message += tr("没有选中任何大热重样本");
    }
    
    QMessageBox::information(this, tr("大热重样本统计"), message);
}

void TgBigDataProcessDialog::selectSample(int sampleId, const QString& sampleName)
{
    DEBUG_LOG << "TgBigDataProcessDialog::selectSample - ID:" << sampleId << "Name:" << sampleName;
    // 确保窗口处于激活状态
    this->activateWindow();
    this->raise();
    
    // 在左侧导航树中查找并选中指定的样本
    bool found = false;
    
    DEBUG_LOG << "Top level count:" << m_leftNavigator->topLevelItemCount();

    DEBUG_LOG;
    // 遍历所有项目节点
    for (int i = 0; i < m_leftNavigator->topLevelItemCount(); ++i) {
        QTreeWidgetItem* projectItem = m_leftNavigator->topLevelItem(i);
        DEBUG_LOG;
        
        // 遍历项目下的批次节点
        for (int j = 0; j < projectItem->childCount(); ++j) {
            QTreeWidgetItem* batchItem = projectItem->child(j);
            DEBUG_LOG;
            
            // 遍历批次下的样本节点
            for (int k = 0; k < batchItem->childCount(); ++k) {
                QTreeWidgetItem* sampleItem = batchItem->child(k);

                DEBUG_LOG;
                
                // 检查样本ID是否匹配
                if (sampleItem->data(0, Qt::UserRole).toInt() == sampleId) {

                    DEBUG_LOG;
                    // 展开父节点
                    projectItem->setExpanded(true);
                    batchItem->setExpanded(true);
                    
                    // 选中样本节点
                    m_leftNavigator->setCurrentItem(sampleItem);
                    
                    // 确保样本节点可见
                    m_leftNavigator->scrollToItem(sampleItem);
                    
                    // // 更新右侧面板
                    // updateRightPanel(sampleItem);
                    
                    // 加载样本曲线
                    loadSampleCurve(sampleId);
                    
                    // 设置复选框为选中状态
                    sampleItem->setCheckState(0, Qt::Checked);
                    
                    // 确保导航树标题显示为"大热重 - 样本导航"
                    m_leftNavigator->setHeaderLabel(tr("大热重 - 样本导航"));
                    
                    DEBUG_LOG << "大热重样本已选中:" << sampleId << sampleName;
                    
                    // 发出信号通知主窗口设置导航树选择框状态
                    emit sampleSelected(sampleId, true);
                    
                    found = true;
                    break;
                }
            }
            
            if (found) break;
        }
        
        if (found) break;
    }
    
    if (!found) {
        DEBUG_LOG << "Sample not found in navigator:" << sampleId << sampleName;
    }

    DEBUG_LOG;
}

QString TgBigDataProcessDialog::buildSampleDisplayName(int sampleId)
{
    // 统一构造样本显示名称 short_code(parallel_no)-timestamp
    QString displayName;
    try {
        QVariantMap info = m_sampleDao.getSampleById(sampleId);
        QString shortCode = info.value("short_code").toString();
        int parallelNo = info.value("parallel_no").toInt();

        QDateTime dt;
        QVariant detectVar = info.value("detect_date");
        if (detectVar.isValid()) {
            if (detectVar.canConvert<QDateTime>()) {
                dt = detectVar.toDateTime();
            } else {
                QDate d = detectVar.toDate();
                if (d.isValid()) {
                    dt = QDateTime(d);
                }
            }
        }
        if (!dt.isValid()) {
            QVariant createdVar = info.value("created_at");
            if (createdVar.isValid()) {
                if (createdVar.canConvert<QDateTime>()) {
                    dt = createdVar.toDateTime();
                }
            }
        }

        QString timestamp = QStringLiteral("N/A");
        if (dt.isValid()) {
            timestamp = dt.toString("yyyyMMdd_HHmmss");
        }

        if (!shortCode.isEmpty() && parallelNo > 0) {
            displayName = QString("%1(%2)-%3")
                              .arg(shortCode)
                              .arg(parallelNo)
                              .arg(timestamp);
        }

        if (displayName.trimmed().isEmpty()) {
            QString sampleName = info.value("sample_name").toString();
            if (!sampleName.trimmed().isEmpty()) {
                displayName = sampleName;
            } else {
                displayName = QString("样本 %1").arg(sampleId);
            }
        }
    } catch (...) {
        displayName = QString("样本 %1").arg(sampleId);
    }
    return displayName;
}

void TgBigDataProcessDialog::addSampleCurve(int sampleId, const QString& sampleName)
{
    DEBUG_LOG << "TgBigDataProcessDialog::addSampleCurve - ID:" << sampleId << "Name:" << sampleName;
    
    // 将样本加入“可见曲线”集合，不影响被选中样本集合
    m_visibleSamples.insert(sampleId);
    DEBUG_LOG << "当前可见样本数:" << m_visibleSamples.size();
    updateSelectedSamplesList();
    
    // 不在此处触发绘图；统一由 selectionChangedByType 路由中合并刷新
    
    // 在导航树中找到对应的样本节点并设置为选中状态
    for (int i = 0; i < m_leftNavigator->topLevelItemCount(); ++i) {
        QTreeWidgetItem* projectItem = m_leftNavigator->topLevelItem(i);
        
        for (int j = 0; j < projectItem->childCount(); ++j) {
            QTreeWidgetItem* batchItem = projectItem->child(j);
            
            for (int k = 0; k < batchItem->childCount(); ++k) {
                QTreeWidgetItem* sampleItem = batchItem->child(k);
                
                if (sampleItem->data(0, Qt::UserRole).toInt() == sampleId) {
                    // 设置复选框为选中状态，但不改变当前选中项
                    sampleItem->setCheckState(0, Qt::Checked);
                    DEBUG_LOG << "大热重样本曲线已添加:" << sampleId << sampleName;
                    return;
                }
            }
        }
    }
    
    DEBUG_LOG << "Sample not found for adding curve:" << sampleId << sampleName;
}


void TgBigDataProcessDialog::drawSelectedSampleCurves()
{
    DEBUG_LOG << "绘制所有可见样本，数量:" << m_visibleSamples.size();

    QElapsedTimer timer;  //  先声明
    timer.restart();

    // if (!m_paramDialog) {
    //     qWarning() << "Parameter dialog not initialized!";
    //     return;
    // }

    // DEBUG_LOG << "TgBigDataProcessDialog::drawSelectedSampleCurves - Current parameters:" << m_currentParams.toString();

    // m_currentParams = m_paramDialog->getParameters();

    // DEBUG_LOG << "TgBigDataProcessDialog::drawSelectedSampleCurves - New parameters:" << m_currentParams.toString();

    // // onParametersApplied(m_currentParams);
    // recalculateAndUpdatePlot();
    // DEBUG_LOG << "TgBigDataProcessDialog::drawSelectedSampleCurves - Parameters applied";
    // // return;


    // 清空五个绘图区域
    if (m_chartView1) m_chartView1->clearGraphs();
    // if (m_chartView2) m_chartView2->clearGraphs();
    // if (m_chartView3) m_chartView3->clearGraphs();
    // if (m_chartView4) m_chartView4->clearGraphs();
    // if (m_chartView5) m_chartView5->clearGraphs();

    DEBUG_LOG;

    // 预定义颜色列表
    QList<QColor> colorList = { Qt::blue, Qt::red, Qt::green, Qt::black, Qt::magenta,
                                Qt::darkYellow, Qt::cyan, Qt::darkGreen, Qt::darkBlue,
                                Qt::darkRed, Qt::darkMagenta, Qt::darkCyan };

    int colorIndex = 0;
    m_chartView1->setPlotTitle("原始数据");

    // 遍历所有当前可见（勾选）的样本
    for (int sampleId : m_visibleSamples) {
        DEBUG_LOG;

        try {
            QString error;
            QVector<QPointF> points = m_navigatorDao.getSampleCurveData(sampleId, "大热重", error);
            QString legendName = buildSampleDisplayName(sampleId);

            if (!points.isEmpty()) {
                QVector<double> xData, yData;
                for (const QPointF& point : points) {
                    xData.append(point.x());
                    yData.append(point.y());
                }

                // QColor curveColor = colorList.at(colorIndex % colorList.size());
                // QColor curveColor = QColor::fromHsv((240 + colorIndex * 50) % 360, 180, 200);
                QColor curveColor = ColorUtils::setCurveColor(colorIndex);
                // curve->setColor(QColor::fromHsv((240 + sampleId * 50) % 360, 200, 220));

                // 分别绘制到五个区域，后续可修改为不同数据
                if (m_chartView1) m_chartView1->addGraph(xData, yData, legendName, curveColor, sampleId);
                // if (m_chartView2) m_chartView2->addGraph(xData, yData, legendName, curveColor, sampleId);
                // if (m_chartView3) m_chartView3->addGraph(xData, yData, legendName, curveColor, sampleId);
                // if (m_chartView4) m_chartView4->addGraph(xData, yData, legendName, curveColor, sampleId);
                // if (m_chartView5) m_chartView5->addGraph(xData, yData, legendName, curveColor, sampleId);

                colorIndex++;
            } else {
                DEBUG_LOG << "未找到样本曲线数据，样本ID:" << sampleId << ", 错误:" << error;
            }
        } catch (const std::exception& e) {
            DEBUG_LOG << "加载样本曲线异常，样本ID:" << sampleId << ", 错误:" << e.what();
        }
    }

    // 更新五个图表
    m_chartView1->setLegendVisible(false);

    DEBUG_LOG << "TgBigDataProcessDialog::drawSelectedSampleCurves - 绘制原始数据图表用时:" << timer.elapsed() << "ms";
    timer.restart();

    if (m_chartView1) m_chartView1->replot();
    // if (m_chartView2) m_chartView2->replot();
    // if (m_chartView3) m_chartView3->replot();
    // if (m_chartView4) m_chartView4->replot();
    // if (m_chartView5) m_chartView5->replot();

    DEBUG_LOG << "TgBigDataProcessDialog::drawSelectedSampleCurves - Elapsed time:" << timer.elapsed() << "ms";

    updateLegendPanel();
}


void TgBigDataProcessDialog::removeSampleCurve(int sampleId)
{
    // 如果该样本当前并不可见，直接返回
    if (!m_visibleSamples.contains(sampleId)) {
        DEBUG_LOG << "TgBigDataProcessDialog::removeSampleCurve: sampleId" << sampleId << "not visible, skip.";
        return;
    }

    DEBUG_LOG << "TgBigDataProcessDialog::removeSampleCurve - ID:" << sampleId;
    m_visibleSamples.remove(sampleId);
    DEBUG_LOG << "当前可见样本数:" << m_visibleSamples.size();
    // 移除后不立即重绘，由上层批量逻辑统一处理，避免闪烁
    updateSelectedSamplesList();
}

void TgBigDataProcessDialog::setupUI()
{
    // 创建 TabWidget
    tabWidget = new QTabWidget(this);

    // --- Tab1 ---
    tab1Widget = new QWidget();
    tab1Layout = new QVBoxLayout(tab1Widget);

    // 原来的主布局（左右分割 + 底部按钮）
    m_mainSplitter = new QSplitter(Qt::Horizontal, tab1Widget);

    //     // 主布局
    // m_mainLayout = new QVBoxLayout(this);
    
    // // 创建水平分割器
    // m_mainSplitter = new QSplitter(Qt::Horizontal, this);

    // 左中右面板
    setupLeftNavigator();
    setupMiddlePanel();
    setupRightPanel();

    m_mainSplitter->setStretchFactor(0, 1);
    m_mainSplitter->setStretchFactor(1, 40);
    m_mainSplitter->setStretchFactor(2, 1);

    DEBUG_LOG << "TgBigDataProcessDialog::setupUI - Splitter sizes:" << m_mainSplitter->sizes();

    QList<int> sizes;
    sizes << 200 << 700 << 20;
    m_mainSplitter->setSizes(sizes);

    // m_mainLayout->addWidget(m_mainSplitter);
    tab1Layout->addWidget(m_mainSplitter);

    // 底部按钮布局
    m_buttonLayout = new QHBoxLayout();
    // m_processButton = new QPushButton(tr("开始处理"), tab1Widget);
    // m_resetButton = new QPushButton(tr("重置参数"), tab1Widget);
    // m_cancelButton = new QPushButton(tr("取消"), tab1Widget);
    m_parameterButton = new QPushButton("参数设置", tab1Widget);
    m_processAndPlotButton = new QPushButton("处理并绘图", tab1Widget);
    // 【新】增加一个按钮
    m_startComparisonButton = new QPushButton(tr("计算差异度"));
    // 显示/隐藏左侧标签页按钮（导航 + 选中样本）
    m_toggleNavigatorButton = new QPushButton(tr("样本浏览页"), tab1Widget);
    // 清除曲线按钮
    m_clearCurvesButton = new QPushButton(tr("清除曲线"), tab1Widget);
    // 绘制所有选中曲线与取消所有选中样本按钮
    m_drawAllButton = new QPushButton(tr("绘制所有选中曲线"), tab1Widget);
    m_unselectAllButton = new QPushButton(tr("取消所有选中样本"), tab1Widget);
    
    

    m_buttonLayout->addStretch();
    // m_buttonLayout->addWidget(m_processButton);
    // m_buttonLayout->addWidget(m_resetButton);
    // m_buttonLayout->addWidget(m_cancelButton);
    m_buttonLayout->addWidget(m_toggleNavigatorButton);
    m_buttonLayout->addWidget(m_parameterButton);
    m_buttonLayout->addWidget(m_processAndPlotButton);
    m_buttonLayout->addWidget(m_startComparisonButton);
    m_buttonLayout->addWidget(m_clearCurvesButton);
    m_buttonLayout->addWidget(m_drawAllButton);
    m_buttonLayout->addWidget(m_unselectAllButton);

    // m_mainLayout->addLayout(m_buttonLayout);

    tab1Layout->addLayout(m_buttonLayout);

    // 将 Tab1 添加到 tabWidget
    tabWidget->addTab(tab1Widget, tr("大热重数据处理"));

    // // --- Tab2 和 Tab3 ---
    // tab2Widget = new QWidget();
    // tab2Widget->setLayout(new QVBoxLayout(tab2Widget));
    // tab2Widget->layout()->addWidget(new QLabel(tr("Tab2 内容占位")));

    // tab3Widget = new QWidget();
    // tab3Widget->setLayout(new QVBoxLayout(tab3Widget));
    // tab3Widget->layout()->addWidget(new QLabel(tr("Tab3 内容占位")));

    // tabWidget->addTab(tab2Widget, tr("Tab2"));
    // tabWidget->addTab(tab3Widget, tr("Tab3"));

    // 设置整体布局
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(tabWidget);

    DEBUG_LOG << "TgBigDataProcessDialog::setupUI - Splitter sizes:" << m_mainSplitter->sizes();
}


void TgBigDataProcessDialog::setupLeftNavigator()
{
    // 创建左侧面板
    m_leftPanel = new QWidget();
    m_leftLayout = new QVBoxLayout(m_leftPanel);
    
    // 创建左侧标签页容器，仅包含“选中样本”一页
    m_leftTabWidget = new QTabWidget(m_leftPanel);

    // 创建（隐藏的）导航树实例，不加入到界面，仅供内部函数保留兼容（可选）
    m_leftNavigator = new QTreeWidget();
    m_leftNavigator->setHeaderLabel(tr("大热重 - 样本导航"));
    m_leftNavigator->setMinimumWidth(125);
    m_leftNavigator->setAlternatingRowColors(true);
    m_leftNavigator->header()->setStretchLastSection(true);

    // 创建选中样本列表（唯一可见标签页）
    m_selectedSamplesList = new QListWidget();
    m_selectedSamplesList->setSelectionMode(QAbstractItemView::NoSelection);
    m_selectedSamplesList->setAlternatingRowColors(true);

    // 在“选中样本”页顶部加入统计标签：已选中样本总数 / 绘图样本数
    QWidget* selectedTab = new QWidget(m_leftTabWidget);
    QVBoxLayout* selectedLayout = new QVBoxLayout(selectedTab);
    m_selectedStatsLabel = new QLabel(tr("已选中样本: 0 / 绘图样本: 0"), selectedTab);
    selectedLayout->addWidget(m_selectedStatsLabel);
    selectedLayout->addWidget(m_selectedSamplesList);
    selectedTab->setLayout(selectedLayout);

    // 仅加入“选中样本”页
    m_leftTabWidget->addTab(selectedTab, tr("选中样本"));
    
    // 添加到左侧布局
    m_leftLayout->addWidget(m_leftTabWidget);
    m_leftLayout->setContentsMargins(5, 5, 5, 5);
    
    // 添加到分割器
    m_mainSplitter->addWidget(m_leftPanel);

    // 隐藏左侧面板
    m_leftPanel->hide();
}

void TgBigDataProcessDialog::setupMiddlePanel()
{
    DEBUG_LOG << "TgBigDataProcessDialog::setupMiddlePanel - Setting up middle panel";
    
    // 创建中间面板
    m_middlePanel = new QWidget();
    m_middlePanel->setMinimumWidth(500); // 确保中间面板有足够的宽度
    m_middleLayout = new QGridLayout(m_middlePanel);
    
    m_chartView1 = new ChartView();
    m_chartView2 = new ChartView();
    m_chartView3 = new ChartView();
    m_chartView4 = new ChartView();
    m_chartView5 = new ChartView();
    if (!m_chartView1 || !m_chartView2 || !m_chartView3 || !m_chartView4 || !m_chartView5) {
        DEBUG_LOG << "TgBigDataProcessDialog::setupMiddlePanel--VIEW - ERROR: Failed to create ChartView!";
    } else {
        DEBUG_LOG << "TgBigDataProcessDialog::setupMiddlePanel--VIEW - ChartView created successfully";
    }

    // 将 ChartView 添加到网格中
    m_middleLayout->addWidget(m_chartView1, 0, 0);
    m_middleLayout->addWidget(m_chartView2, 0, 1);
    m_middleLayout->addWidget(m_chartView3, 1, 0);
    m_middleLayout->addWidget(m_chartView4, 1, 1);
    m_middleLayout->addWidget(m_chartView5, 2, 0); // 显示

    // m_legendPanel = new QWidget();
    // m_legendLayout = new QVBoxLayout(m_legendPanel);
    // m_legendPanel->setMinimumWidth(150); // 图例区域宽度

    // // 添加到右侧
    // m_middleLayout->addWidget(m_legendPanel, 0, 2, 3, 1); // 占3行1列

    // 创建滚动区域（用于承载图例面板），改为配合分割器支持拖拽调整宽度
    QScrollArea* legendScrollArea = new QScrollArea();
    legendScrollArea->setWidgetResizable(true); // 自动调整大小
    legendScrollArea->setMinimumWidth(100); // 保底最小宽度，避免完全压缩不可见


    // 创建图例面板，放到滚动区域
    m_legendPanel = new QWidget();
    m_legendLayout = new QVBoxLayout(m_legendPanel);
    m_legendLayout->setAlignment(Qt::AlignTop); // 顶部对齐
    m_legendPanel->setLayout(m_legendLayout);
    

    // 将图例面板放入滚动区域
    legendScrollArea->setWidget(m_legendPanel);

    // 使用水平分割器将“绘图区域”和“图例区域”并列，支持用户拖拽调整图例宽度
    QSplitter* plotLegendSplitter = new QSplitter(Qt::Horizontal);
    plotLegendSplitter->addWidget(m_middlePanel);     // 左侧：绘图区（网格布局）
    plotLegendSplitter->addWidget(legendScrollArea);  // 右侧：图例滚动区域
    plotLegendSplitter->setStretchFactor(0, 4);       // 绘图区更宽
    plotLegendSplitter->setStretchFactor(1, 1);       // 图例区相对较窄
    QList<int> splitSizes; splitSizes << 800 << 180;  // 初始宽度比例
    plotLegendSplitter->setSizes(splitSizes);

    // // 创建水平分割器，把绘图区和图例区放进去
    // QSplitter* hSplitter = new QSplitter(Qt::Horizontal);

    // // 绘图面板
    // hSplitter->addWidget(m_middlePanel); // 你的 2x3 绘图布局

    // // 图例滚动区域
    // QScrollArea* legendScrollArea = new QScrollArea();
    // legendScrollArea->setWidgetResizable(true);
    // legendScrollArea->setMinimumWidth(100); // 初始宽度小一点

    // m_legendPanel = new QWidget();
    // m_legendLayout = new QVBoxLayout(m_legendPanel);
    // m_legendLayout->setAlignment(Qt::AlignTop);
    // m_legendPanel->setLayout(m_legendLayout);

    // legendScrollArea->setWidget(m_legendPanel);
    // hSplitter->addWidget(legendScrollArea);

    // // 设置比例，保证绘图区更宽
    // hSplitter->setStretchFactor(0, 3); // 绘图区
    // hSplitter->setStretchFactor(1, 1); // 图例区

    // // 添加到主布局
    // m_mainLayout->addWidget(hSplitter);



    m_middlePanel->setLayout(m_middleLayout);
    // 将“绘图区+图例”分割器作为中间面板加入主分割器
    m_mainSplitter->addWidget(plotLegendSplitter);
    
    // 确保中间面板可见
    // m_middlePanel->setVisible(true);
    
    DEBUG_LOG << "TgBigDataProcessDialog::setupMiddlePanel - Middle panel setup completed";
    // DEBUG_LOG << "TgBigDataProcessDialog::setupMiddlePanel - ChartView visibility:" << m_chartView->isVisible();
    DEBUG_LOG << "TgBigDataProcessDialog::setupMiddlePanel - Middle panel visibility:" << m_middlePanel->isVisible();
}

void TgBigDataProcessDialog::setupRightPanel()
{
    DEBUG_LOG << "TgBigDataProcessDialog::setupRightPanel - Setting up right panel";
    // 创建右侧面板
    m_rightPanel = new QWidget();
    m_rightLayout = new QVBoxLayout(m_rightPanel);
    
        DEBUG_LOG << "TgBigDataProcessDialog::setupRightPanel - Right panel setup complete";
}


void TgBigDataProcessDialog::setupConnections()
{
    // 按钮连接
    connect(m_parameterButton, &QPushButton::clicked, this, &TgBigDataProcessDialog::onParameterSettingsClicked);
    connect(m_processAndPlotButton, &QPushButton::clicked, this, &TgBigDataProcessDialog::onProcessAndPlotButtonClicked); // m_processAndPlotButton
    connect(m_startComparisonButton, &QPushButton::clicked, this, &TgBigDataProcessDialog::onStartComparison);
    connect(m_toggleNavigatorButton, &QPushButton::clicked, this, &TgBigDataProcessDialog::onToggleNavigatorClicked);
    connect(m_clearCurvesButton, &QPushButton::clicked, this, &TgBigDataProcessDialog::onClearCurvesClicked);
    connect(m_drawAllButton, &QPushButton::clicked, this, &TgBigDataProcessDialog::onDrawAllSelectedCurvesClicked);
    connect(m_unselectAllButton, &QPushButton::clicked, this, &TgBigDataProcessDialog::onUnselectAllSamplesClicked);
    connect(m_clearCurvesButton, &QPushButton::clicked, this, &TgBigDataProcessDialog::onClearCurvesClicked);
    // 选中样本列表勾选变化驱动左侧导航勾选状态
    connect(m_selectedSamplesList, &QListWidget::itemChanged, this, &TgBigDataProcessDialog::onSelectedSamplesListItemChanged);
    // 点击样本文字时，也等效于切换“显示/隐藏曲线”的复选框状态（避免只能点小勾选框）
    connect(m_selectedSamplesList, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        if (!item) return;
        if (!(item->flags() & Qt::ItemIsUserCheckable)) return;

        const QPoint viewPos = m_selectedSamplesList->viewport()->mapFromGlobal(QCursor::pos());
        const QRect itemRect = m_selectedSamplesList->visualItemRect(item);

        QStyleOptionViewItem option;
        option.initFrom(m_selectedSamplesList);
        option.rect = itemRect;
        option.features |= QStyleOptionViewItem::HasCheckIndicator;
        option.checkState = item->checkState();

        const QRect checkRect = m_selectedSamplesList->style()->subElementRect(QStyle::SE_ItemViewItemCheckIndicator, &option, m_selectedSamplesList);
        if (checkRect.contains(viewPos)) {
            return;
        }

        item->setCheckState(item->checkState() == Qt::Checked ? Qt::Unchecked : Qt::Checked);
    });
    
    // 参数变化连接

    // 订阅统一管理器按类型变化。仅当左侧导航复选框为“选中”时才显示曲线；
    // 首次出现的新样本节点默认勾选并显示曲线。
    connect(SampleSelectionManager::instance(), &SampleSelectionManager::selectionChangedByType,
            this, [this](int sampleId, const QString& dataType, bool selected, const QString& origin){
                if (dataType != QStringLiteral("大热重")) return;
                if (selected) {
                    // 加入“被选中样本”集合；首次出现则默认可见（勾选）并绘制曲线
                    QString name = m_selectedSamples.value(sampleId);
                    if (name.isEmpty()) {
                        name = buildSampleDisplayName(sampleId);
                    }
                    bool first = !m_selectedSamples.contains(sampleId);
                    m_selectedSamples.insert(sampleId, name);
                    if (first) {
                        m_visibleSamples.insert(sampleId);
                        addSampleCurve(sampleId, name);
                    }
                    updateSelectedSamplesList();
                    if (origin == QStringLiteral("BatchSelect")) {
                        if (!m_drawScheduled) { m_drawScheduled = true; QTimer::singleShot(0, this, [this]{ m_drawScheduled = false; drawSelectedSampleCurves(); }); }
                    } else {
                        drawSelectedSampleCurves();
                    }
                } else {
                    // 从“被选中样本”集合移除；同时移除可见集合与曲线
                    m_selectedSamples.remove(sampleId);
                    if (m_visibleSamples.contains(sampleId)) {
                        m_visibleSamples.remove(sampleId);
                        removeSampleCurve(sampleId);
                    }
                    updateSelectedSamplesList();
                    if (origin == QStringLiteral("BatchSelect") || origin == QStringLiteral("Dialog-UnselectAll")) {
                        if (!m_drawScheduled) { m_drawScheduled = true; QTimer::singleShot(0, this, [this]{ m_drawScheduled = false; drawSelectedSampleCurves(); }); }
                    } else {
                        drawSelectedSampleCurves();
                    }
                }
            });
}

void TgBigDataProcessDialog::loadNavigatorData()
{
    try {
        // 清空现有项目
        m_leftNavigator->clear();
        
        // 检查是否有主界面导航树引用
        if (!m_mainNavigator) {
            // 没有主界面导航树引用，从数据库加载大热重数据
            loadNavigatorDataFromDatabase();
        } else {
            // 有主界面导航树引用，从主界面导航树加载数据
            loadNavigatorDataFromMainNavigator();
        }
        
    } catch (const std::exception& e) {
        DEBUG_LOG << "Error loading navigator data:" << e.what();
        QMessageBox::warning(this, tr("错误"), tr("加载导航数据失败: %1").arg(e.what()));
    }
}

void TgBigDataProcessDialog::onClearCurvesClicked()
{
    // 仅清除绘图区域的曲线；不改变“被选中样本”，只将其设为不可见
    if (m_chartView1) m_chartView1->clearGraphs();
    if (m_chartView2) m_chartView2->clearGraphs();
    if (m_chartView3) m_chartView3->clearGraphs();
    if (m_chartView4) m_chartView4->clearGraphs();
    if (m_chartView5) m_chartView5->clearGraphs();

    m_visibleSamples.clear();
    updateLegendPanel();
    updateSelectedSamplesList();
}

void TgBigDataProcessDialog::onDrawAllSelectedCurvesClicked()
{
    // 从全局选择管理器获取“大热重”类型已选样本，全部设为可见并绘制
    QSet<int> ids = SampleSelectionManager::instance()->selectedIdsByType(QStringLiteral("大热重"));
    m_visibleSamples = ids;
    // 确保 m_selectedSamples 包含名称
    for (int sampleId : ids) {
        if (!m_selectedSamples.contains(sampleId)) {
            QString fullName = buildSampleDisplayName(sampleId);
            m_selectedSamples.insert(sampleId, fullName);
        }
    }
    drawSelectedSampleCurves();
    updateSelectedSamplesList();
}

void TgBigDataProcessDialog::onUnselectAllSamplesClicked()
{
    // 清空全局选择管理器中“大热重”类型的所有样本，同时清空本界面的可见与选中集合
    const QString type = QStringLiteral("大热重");
    QSet<int> ids = SampleSelectionManager::instance()->selectedIdsByType(type);
    for (int sampleId : ids) {
        SampleSelectionManager::instance()->setSelectedWithType(sampleId, type, false, QStringLiteral("Dialog-UnselectAll"));
    }
    if (m_mainNavigator) {
        m_mainNavigator->clearSampleChecksForType(type);
    }
    if (m_leftNavigator) {
        m_suppressItemChanged = true;
        for (int i = 0; i < m_leftNavigator->topLevelItemCount(); ++i) {
            QTreeWidgetItem* projectItem = m_leftNavigator->topLevelItem(i);
            if (!projectItem) continue;
            for (int j = 0; j < projectItem->childCount(); ++j) {
                QTreeWidgetItem* batchItem = projectItem->child(j);
                if (!batchItem) continue;
                for (int k = 0; k < batchItem->childCount(); ++k) {
                    QTreeWidgetItem* sampleItem = batchItem->child(k);
                    if (!sampleItem) continue;
                    sampleItem->setCheckState(0, Qt::Unchecked);
                }
            }
        }
        m_suppressItemChanged = false;
    }
    m_selectedSamples.clear();
    m_visibleSamples.clear();
    if (m_chartView1) m_chartView1->clearGraphs();
    if (m_chartView2) m_chartView2->clearGraphs();
    if (m_chartView3) m_chartView3->clearGraphs();
    if (m_chartView4) m_chartView4->clearGraphs();
    if (m_chartView5) m_chartView5->clearGraphs();
    updateLegendPanel();
    updateSelectedSamplesList();
}

// 显示/隐藏左侧标签页（包含样本导航与选中样本列表）
void TgBigDataProcessDialog::onToggleNavigatorClicked()
{
    if (!m_leftPanel) return;
    bool willShow = m_leftPanel->isHidden();
    m_leftPanel->setVisible(willShow);
    // 调整分割器尺寸，确保左侧显示时有合理宽度
    if (willShow) {
        QList<int> sizes;
        sizes << 250 << 650 << 50;
        m_mainSplitter->setSizes(sizes);
    }
}

// 刷新左侧“选中样本”列表显示
void TgBigDataProcessDialog::updateSelectedSamplesList()
{
    if (!m_selectedSamplesList) return;
    m_selectedSamplesList->blockSignals(true);
    m_selectedSamplesList->setUpdatesEnabled(false);
    // 差量刷新。添加或更新需要存在的条目
    for (auto it = m_selectedSamples.constBegin(); it != m_selectedSamples.constEnd(); ++it) {
        int sampleId = it.key();
        const QString name = it.value();
        ensureSelectedListItem(sampleId, name);
        QListWidgetItem* item = m_selectedItemMap.value(sampleId, nullptr);
        if (item) {
            item->setCheckState(m_visibleSamples.contains(sampleId) ? Qt::Checked : Qt::Unchecked);
        }
    }
    // 移除不再在选中集合中的条目
    QList<int> existing = m_selectedItemMap.keys();
    for (int id : existing) {
        if (!m_selectedSamples.contains(id)) {
            removeSelectedListItem(id);
        }
    }
    m_selectedSamplesList->blockSignals(false);
    m_selectedSamplesList->setUpdatesEnabled(true);
    updateSelectedStatsInfo();
}

void TgBigDataProcessDialog::updateSelectedStatsInfo()
{
    if (!m_selectedStatsLabel) return;
    int selectedCount = m_selectedSamples.size();
    int visibleCount = m_visibleSamples.size();
    m_selectedStatsLabel->setText(tr("已选中样本: %1 / 绘图样本: %2").arg(selectedCount).arg(visibleCount));
}

// 统一调度一次重绘与图例刷新，避免同一事件内重复调用造成卡顿
void TgBigDataProcessDialog::scheduleRedraw()
{
    if (!m_drawScheduled) {
        m_drawScheduled = true;
        QTimer::singleShot(0, this, [this]{
            m_drawScheduled = false;
            drawSelectedSampleCurves();
            updateLegendPanel();
            updateSelectedStatsInfo();
        });
    }
}

void TgBigDataProcessDialog::ensureSelectedListItem(int sampleId, const QString& displayName)
{
    QListWidgetItem* existing = m_selectedItemMap.value(sampleId, nullptr);
    if (existing) {
        existing->setText(displayName);
        return;
    }
    QListWidgetItem* item = new QListWidgetItem(displayName, m_selectedSamplesList);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setData(Qt::UserRole, sampleId);
    item->setCheckState(m_visibleSamples.contains(sampleId) ? Qt::Checked : Qt::Unchecked);
    m_selectedItemMap.insert(sampleId, item);
}

void TgBigDataProcessDialog::removeSelectedListItem(int sampleId)
{
    QListWidgetItem* item = m_selectedItemMap.take(sampleId);
    if (item) delete item;
}

void TgBigDataProcessDialog::prefetchCurveIfNeeded(int sampleId)
{
    if (m_curveCache.contains(sampleId)) return;
    QString dataType = QStringLiteral("大热重");
    auto future = QtConcurrent::run([this, sampleId, dataType]{
        QString error;
        return m_navigatorDao.getSampleCurveData(sampleId, dataType, error);
    });
    QFutureWatcher<QVector<QPointF>>* watcher = new QFutureWatcher<QVector<QPointF>>(this);
    connect(watcher, &QFutureWatcher<QVector<QPointF>>::finished, this, [this, watcher, sampleId]{
        QVector<QPointF> points = watcher->future().result();
        watcher->deleteLater();
        if (!points.isEmpty()) {
            m_curveCache.insert(sampleId, points);
            scheduleRedraw();
        }
    });
    watcher->setFuture(future);
}

// 处理“选中样本”列表中的复选框变化，同步左侧导航树的勾选状态
void TgBigDataProcessDialog::onSelectedSamplesListItemChanged(QListWidgetItem* item)
{
    if (!item) return;
    bool isChecked = (item->checkState() == Qt::Checked);
    int sampleId = item->data(Qt::UserRole).toInt();
    if (isChecked) {
        m_visibleSamples.insert(sampleId);
        QString legendName = m_selectedSamples.value(sampleId);
        if (legendName.isEmpty()) legendName = QString("样本 %1").arg(sampleId);
        addSampleCurve(sampleId, legendName);
        prefetchCurveIfNeeded(sampleId);
        scheduleRedraw();
    } else {
        m_visibleSamples.remove(sampleId);
        removeSampleCurve(sampleId);
        scheduleRedraw();
    }
    updateSelectedStatsInfo();
}

void TgBigDataProcessDialog::loadNavigatorDataFromDatabase()
{
    // 获取大热重样本数据
    m_bigTgSamples = m_sampleDao.getSamplesByDataType("大热重");
    
    // 按项目分组
    QMap<QString, QTreeWidgetItem*> projectItems;
    QMap<QString, QMap<QString, QTreeWidgetItem*>> batchItems;
    
    for (const auto &sample : m_bigTgSamples) {
        QString projectName = sample["project_name"].toString();
        QString batchCode = sample["batch_code"].toString();
        
        // 创建项目节点
        if (!projectItems.contains(projectName)) {
            QTreeWidgetItem* projectItem = new QTreeWidgetItem(m_leftNavigator, {projectName});
            projectItem->setIcon(0, QIcon(":/icons/project.png"));
            projectItem->setExpanded(true);
            projectItems[projectName] = projectItem;
            batchItems[projectName] = QMap<QString, QTreeWidgetItem*>();
        }
        
        // 创建批次节点
        if (!batchItems[projectName].contains(batchCode)) {
            QTreeWidgetItem* batchItem = new QTreeWidgetItem(projectItems[projectName], {batchCode});
            batchItem->setIcon(0, QIcon(":/icons/batch.png"));
            batchItem->setExpanded(true);
            batchItems[projectName][batchCode] = batchItem;
        }
        
        // 创建样本节点
        QString displayName = QString("%1-%2")
            .arg(sample["short_code"].toString())
            .arg(sample["parallel_no"].toInt());
            
        QTreeWidgetItem* sampleItem = new QTreeWidgetItem(batchItems[projectName][batchCode], {displayName});
        sampleItem->setIcon(0, QIcon(":/icons/sample.png"));
        
        // 存储样本ID和名称
        sampleItem->setData(0, Qt::UserRole, sample["sample_id"].toInt());
        sampleItem->setData(0, Qt::UserRole + 1, sample["sample_name"].toString());
        
        // 添加复选框
        sampleItem->setFlags(sampleItem->flags() | Qt::ItemIsUserCheckable);
        sampleItem->setCheckState(0, Qt::Unchecked);
    }
}

void TgBigDataProcessDialog::loadNavigatorDataFromMainNavigator()
{
    // 获取主界面导航树的数据源根节点
    QTreeWidgetItem* dataSourceRoot = m_mainNavigator->getDataSourceRoot();
    if (!dataSourceRoot) {
        DEBUG_LOG << "TgBigDataProcessDialog: 无法获取主界面导航树数据源根节点";
        return;
    }
    
    // 按项目分组
    QMap<QString, QTreeWidgetItem*> projectItems;
    QMap<QString, QMap<QString, QTreeWidgetItem*>> batchItems;
    
    // 遍历主界面导航树，筛选大热重数据（仅复制主导航“已选中”的样本）
    for (int i = 0; i < dataSourceRoot->childCount(); ++i) {
        QTreeWidgetItem* modelItem = dataSourceRoot->child(i);
        
        // 遍历实验节点
        for (int j = 0; j < modelItem->childCount(); ++j) {
            QTreeWidgetItem* expItem = modelItem->child(j);
            
            // 遍历短代码节点
            for (int k = 0; k < expItem->childCount(); ++k) {
                QTreeWidgetItem* shortCodeItem = expItem->child(k);
                
                // 遍历数据类型节点
                for (int l = 0; l < shortCodeItem->childCount(); ++l) {
                    QTreeWidgetItem* dataTypeItem = shortCodeItem->child(l);
                    
                    // 检查是否为大热重数据类型
                    QVariant nodeData = dataTypeItem->data(0, Qt::UserRole);
                    if (nodeData.canConvert<NavigatorNodeInfo>()) {
                        NavigatorNodeInfo nodeInfo = nodeData.value<NavigatorNodeInfo>();
                        
                        if (nodeInfo.type == NavigatorNodeInfo::DataType && 
                             nodeInfo.dataType == "大热重") {
                            
                            // 遍历样本节点
                            for (int m = 0; m < dataTypeItem->childCount(); ++m) {
                                QTreeWidgetItem* sampleItem = dataTypeItem->child(m);
                                
                                QVariant sampleData = sampleItem->data(0, Qt::UserRole);
                                if (sampleData.canConvert<NavigatorNodeInfo>()) {
                                    NavigatorNodeInfo sampleInfo = sampleData.value<NavigatorNodeInfo>();
                                    
                                    if (sampleInfo.type == NavigatorNodeInfo::Sample) {
                                        // 仅复制主导航中“已选中”的样本
                                        if (!SampleSelectionManager::instance()->isSelected(sampleInfo.id)) {
                                            continue;
                                        }
                                        QString projectName = sampleInfo.projectName;
                                        QString batchCode = sampleInfo.batchCode;
                                        
                                        // 创建项目节点
                                        if (!projectItems.contains(projectName)) {
                                            QTreeWidgetItem* projectItem = new QTreeWidgetItem(m_leftNavigator, {projectName});
                                            projectItem->setIcon(0, QIcon(":/icons/project.png"));
                                            projectItem->setExpanded(true);
                                            projectItems[projectName] = projectItem;
                                            batchItems[projectName] = QMap<QString, QTreeWidgetItem*>();
                                        }
                                        
                                        // 创建批次节点
                                        if (!batchItems[projectName].contains(batchCode)) {
                                            QTreeWidgetItem* batchItem = new QTreeWidgetItem(projectItems[projectName], {batchCode});
                                            batchItem->setIcon(0, QIcon(":/icons/batch.png"));
                                            batchItem->setExpanded(true);
                                            batchItems[projectName][batchCode] = batchItem;
                                        }
                                        
                                        // 创建样本节点 - 显示完整的样本信息
                                        QString displayName = QString("%1-%2 (平行样 %3)")
                                            .arg(sampleInfo.shortCode)
                                            .arg(sampleInfo.batchCode)
                                            .arg(sampleInfo.parallelNo);
                                            
                                        QTreeWidgetItem* newSampleItem = new QTreeWidgetItem(batchItems[projectName][batchCode], {displayName});
                                        newSampleItem->setIcon(0, QIcon(":/icons/sample.png"));
                                        
                                        // 存储样本ID和名称
                                        newSampleItem->setData(0, Qt::UserRole, sampleInfo.id);
                                        newSampleItem->setData(0, Qt::UserRole + 1, sampleItem->text(0));
                                        // 添加可勾选复选框，仅影响本界面曲线显示
                                        newSampleItem->setFlags(newSampleItem->flags() | Qt::ItemIsUserCheckable);
                                        m_suppressItemChanged = true;
                                        newSampleItem->setCheckState(0, Qt::Checked);
                                        m_suppressItemChanged = false;
                                    }
                                }
                            }
                        }
                    }
                 }
             }
         }
     }
}

void TgBigDataProcessDialog::loadSampleCurve(int sampleId)
{
    try {
        DEBUG_LOG << "TgBigDataProcessDialog::loadSampleCurve - Loading curve for sampleId:" << sampleId;
        
        // 不再单独加载一个样本的曲线，而是绘制所有选中的样本曲线
        drawSelectedSampleCurves();
        return;
        
    } catch (const std::exception& e) {
        DEBUG_LOG << "TgBigDataProcessDialog::loadSampleCurve - 加载样本曲线失败:" << e.what();
        QMessageBox::warning(this, tr("警告"), tr("加载样本曲线失败: %1").arg(e.what()));
    }
}

void TgBigDataProcessDialog::onLeftItemClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column)
    
    if (!item) return;
    
    // updateRightPanel(item);
}

void TgBigDataProcessDialog::onSampleItemClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column)
    
    if (!item) return;
    
    // // 更新右侧面板
    // updateRightPanel(item);
    
    // 检查是否为样本项目
    QVariant sampleIdVariant = item->data(0, Qt::UserRole);
    if (sampleIdVariant.isValid() && sampleIdVariant.toInt() > 0) {
        // 这是一个样本项目
        int sampleId = sampleIdVariant.toInt();
        
        // 获取样本全称（统一格式）
        QString sampleFullName = buildSampleDisplayName(sampleId);
        
        // 设置复选框为选中状态
        item->setCheckState(0, Qt::Checked);
        
        // 加载样本曲线
        loadSampleCurve(sampleId);
        
        // 确保导航树标题显示为"大热重 - 样本导航"
        m_leftNavigator->setHeaderLabel(tr("大热重 - 样本导航"));
        
        DEBUG_LOG << "大热重样本已选中:" << sampleId << sampleFullName;
    }
}



void TgBigDataProcessDialog::onCancelButtonClicked()
{
    // 在MDI模式下，close()会关闭MDI子窗口
    close();
}


 void TgBigDataProcessDialog::onProcessAndPlotButtonClicked()
 {
    m_currentParams = m_paramDialog->getParameters();

    onParametersApplied( m_currentParams );
 }

void TgBigDataProcessDialog::onParameterSettingsClicked()
{


    // // 1. 创建对话框实例
    // //    m_currentParams 是 Workbench 的一个 ProcessingParameters 成员变量
    // TgBigParameterSettingsDialog *dialog = new TgBigParameterSettingsDialog(m_currentParams, this);

    // // 2. 【关键】连接对话框的 `parametersApplied` 信号到 Workbench 的槽函数
    // connect(dialog, &TgBigParameterSettingsDialog::parametersApplied, this, &TgBigDataProcessDialog::onParametersApplied);

    // // 3. 以【非模态】方式显示对话框，这样用户可以同时与主窗口和对话框交互
    // dialog->show(); 
    // // dialog->exec(); // 如果仍然想用模态，也可以，逻辑依然成立

    if (m_paramDialog) {
        // 更新窗口显示当前参数
        // m_paramDialog->setParameters(m_currentParams);
        m_paramDialog->show();  // 或 exec() 模态显示
    }

}

// 【关键】实现响应参数应用的槽函数
void TgBigDataProcessDialog::onParametersApplied(const ProcessingParameters &newParams)
{
    // 1. 更新 Workbench 内部缓存的参数
    m_currentParams = newParams;

    // 2. 触发重新计算和绘图的流程
    recalculateAndUpdatePlot();
}


// void TgBigDataProcessDialog::recalculateAndUpdatePlot()
// {
//     // --- 1. UI 反馈 ---
//     QApplication::setOverrideCursor(Qt::WaitCursor);
//     m_parameterButton->setEnabled(false);

//     // --- 2. 异步调用 Service 层 ---
    
//     // 【升级】获取所有需要处理的样本ID列表
//     const QList<int> sampleIds = m_selectedSamples.keys();
//     // DEBUG_LOG << "TgBigDataProcessDialog::recalculateAndUpdatePlot() - Processing samples:" << sampleIds;
//     QStringList idStrList;
//     for (int id : sampleIds)
//         idStrList << QString::number(id);

//     DEBUG_LOG << "TgBigDataProcessDialog::recalculateAndUpdatePlot() - Processing samples:"
//             << idStrList.join(", ");

//     // 【升级】调用新的批量处理方法
//     QFuture<BatchMultiStageData> future = QtConcurrent::run(
//         m_processingService,
//         &DataProcessingService::runTgBigPipelineForMultiple,
//         sampleIds,       // 传递 ID 列表
//         m_currentParams
//     );
//     DEBUG_LOG << "TgBigDataProcessDialog::recalculateAndUpdatePlot() - Future created";
    
//     // --- 3. 监控后台任务 ---
//     QFutureWatcher<BatchMultiStageData>* watcher = new QFutureWatcher<BatchMultiStageData>(this);
//     connect(watcher, &QFutureWatcher<BatchMultiStageData>::finished, this, &TgBigDataProcessDialog::onCalculationFinished);
//     watcher->setFuture(future);

//     DEBUG_LOG << "TgBigDataProcessDialog::recalculateAndUpdatePlot() - Watcher created";
// }

// void TgBigDataProcessDialog::onCalculationFinished()
// {
//     DEBUG_LOG << "TgBigDataProcessDialog::onCalculationFinished()";
//     // QFutureWatcher<BatchMultiStageData>* watcher = qobject_cast<QFutureWatcher<BatchMultiStageData>*>(sender());
//     QFutureWatcher<BatchMultiStageData>* watcher = static_cast<QFutureWatcher<BatchMultiStageData>*>(sender());

//     if (!watcher) { /* ... 恢复UI ... */ return; }

//     // a. 【升级】获取并缓存批量结果
//     m_stageDataCache.clear(); // 需要为 BatchMultiStageData 实现 clear
//     m_stageDataCache = watcher->result();
//     watcher->deleteLater();

//     // 【新】当数据处理完成后，使“计算差异度”按钮可用
//     m_startComparisonButton->setEnabled(m_selectedSamples.count() >= 2);

//     // b. 调用绘图更新
//     updatePlot(); 
    
//     // c. 恢复 UI
//     QApplication::restoreOverrideCursor();
//     m_parameterButton->setEnabled(true);

//     DEBUG_LOG << "TgBigDataProcessDialog::onCalculationFinished() - Finished";
// }




void TgBigDataProcessDialog::recalculateAndUpdatePlot()
{
    // --- 1. UI 反馈 ---
    QApplication::setOverrideCursor(Qt::WaitCursor);
    m_parameterButton->setEnabled(false);

    // --- 2. 获取所有需要处理的样本ID列表 ---
    // 仅处理左侧“选中样本”列表中被勾选（可见）的样本
    QList<int> sampleIds;
    for (int id : m_visibleSamples) {
        sampleIds.append(id);
    }
    if (sampleIds.isEmpty()) {
        DEBUG_LOG << "recalculateAndUpdatePlot - 可见样本为空，跳过处理";
        QApplication::restoreOverrideCursor();
        m_parameterButton->setEnabled(true);
        return;
    }

    QStringList idStrList;
    for (int id : sampleIds)
        idStrList << QString::number(id);

    DEBUG_LOG << "TgBigDataProcessDialog::recalculateAndUpdatePlot() - Processing samples:"
              << idStrList.join(", ");

    // --- 3. 异步调用 Service 层 (使用新数据结构 BatchGroupData) ---
    QFuture<BatchGroupData> future = QtConcurrent::run(
        m_processingService,
        &DataProcessingService::runTgBigPipelineForMultiple,
        sampleIds,
        m_currentParams
    );

    DEBUG_LOG << "TgBigDataProcessDialog::recalculateAndUpdatePlot() - Future created";

    // --- 4. 监控后台任务 ---
    QFutureWatcher<BatchGroupData>* watcher = new QFutureWatcher<BatchGroupData>(this);
    connect(watcher, &QFutureWatcher<BatchGroupData>::finished,
            this, &TgBigDataProcessDialog::onCalculationFinished);
    watcher->setFuture(future);

    DEBUG_LOG << "TgBigDataProcessDialog::recalculateAndUpdatePlot() - Watcher created";
}


void TgBigDataProcessDialog::onCalculationFinished()
{
    DEBUG_LOG << "TgBigDataProcessDialog::onCalculationFinished()";

    QFutureWatcher<BatchGroupData>* watcher = static_cast<QFutureWatcher<BatchGroupData>*>(sender());
    if (!watcher) {
        QApplication::restoreOverrideCursor();
        m_parameterButton->setEnabled(true);
        return;
    }

    // --- 1. 获取并缓存批量结果 ---
    m_stageDataCache.clear(); // m_stageDataCache 类型需要为 BatchGroupData
    m_stageDataCache = watcher->result();
    watcher->deleteLater();

    DEBUG_LOG << "BatchGroupData size:" << m_stageDataCache.size();

        // b. 调用绘图更新
    updatePlot(); 

    // --- 2. 启用“计算差异度”按钮（至少两个样本组才能计算差异度） ---
    m_startComparisonButton->setEnabled(m_stageDataCache.size() >= 2);

    // // --- 3. 调用绘图更新（根据 SampleGroup -> ParallelSampleData -> StageData 遍历） ---
    // for (auto it = m_stageDataCache.constBegin(); it != m_stageDataCache.constEnd(); ++it) {
    //     const QString &groupKey = it.key();
    //     const SampleGroup &group = it.value();

    //     // DEBUG_LOG << "Processing SampleGroup:" << groupKey
    //     //           << "parallelSamples:" << group.parallelSamples.size();

    //     // 遍历平行样本
    //     for (const ParallelSampleData &pSample : group.parallelSamples) {
    //         const SampleDataFlexible &sample = pSample.sampleData;

    //         for (const StageData &stage : sample.stages) {
    //             if (stage.useForPlot && stage.curve) {
    //                 // 绘图逻辑，例如调用你的 plot 函数
    //                 // plotStageCurve(stage.curve, stage.name);
    //             }
    //         }
    //     }

    //     // 如果需要绘制最优代表：
    //     if (group.bestSampleIndex >= 0 &&
    //         group.bestSampleIndex < group.parallelSamples.size()) {
    //         const ParallelSampleData &bestSample = group.parallelSamples[group.bestSampleIndex];
    //         DEBUG_LOG << "Best parallel sample for group" << groupKey
    //                   << "is parallelNo:" << bestSample.parallelNo;
    //     }
    // }

    for (const auto &groupKey : m_stageDataCache.keys()) {
        const SampleGroup &group = m_stageDataCache[groupKey];

        // 遍历组内样本
        for (const SampleDataFlexible &sample : group.sampleDatas) {
            for (const StageData &stage : sample.stages) {
                if (stage.useForPlot && !stage.curve.isNull()) {
                    // 绘图逻辑
                }
            }
        }

        // 找最优样本
        auto bestIt = std::find_if(group.sampleDatas.constBegin(), group.sampleDatas.constEnd(),
                                [](const SampleDataFlexible &s){ return s.bestInGroup; });
        if (bestIt != group.sampleDatas.constEnd()) {
            const SampleDataFlexible &bestSample = *bestIt;
            DEBUG_LOG << "Best sample for group" << groupKey
                    << "is sampleId:" << bestSample.sampleId;
        }
    }


    // // 遍历组内所有平行样本
    // for (const SampleDataFlexible &sample : group.sampleDatas) {
    //     for (const StageData &stage : sample.stages) {
    //         if (stage.useForPlot && stage.curve) {
    //             // 绘图逻辑，例如调用你的 plot 函数
    //             // plotStageCurve(stage.curve, stage.stageName);
    //         }
    //     }
    // }

    // // 如果需要绘制最优代表：
    // auto itBest = std::find_if(group.sampleDatas.begin(), group.sampleDatas.end(),
    //                         [](const SampleDataFlexible &s){ return s.bestInGroup; });
    // if (itBest != group.sampleDatas.end()) {
    //     DEBUG_LOG << "Best sample in group" << groupKey
    //             << "is sampleId:" << itBest->sampleId;
    // }


    // --- 4. 恢复 UI ---
    QApplication::restoreOverrideCursor();
    m_parameterButton->setEnabled(true);

    DEBUG_LOG << "TgBigDataProcessDialog::onCalculationFinished() - Finished";
}


// 确保包含了 ChartView.h 和 core/entities/Curve.h
#include "gui/views/ChartView.h"
#include "core/entities/Curve.h"

// ... (其他函数的实现)




// ...

void TgBigDataProcessDialog::updatePlot()
{
    // --- 0. 安全检查 ---
    if (m_stageDataCache.isEmpty()) {
        qWarning() << "updatePlot called with empty data cache. Clearing all charts.";
        // 清空所有图表
        // m_chartView1->clearGraphs();
        m_chartView2->clearGraphs();
        m_chartView3->clearGraphs();
        m_chartView4->clearGraphs();
        m_chartView5->clearGraphs();
        return;
    }

    //根据索引计算颜色
    int colorIndex = 0;
    double clipMaxX = 0.0;
    double normalizeMaxX = 0.0;
    double smoothMaxX = 0.0;
    double derivativeMaxX = 0.0;
    bool hasClipData = false;
    bool hasNormalizeData = false;
    bool hasSmoothData = false;
    bool hasDerivativeData = false;
    auto updateMaxX = [](double& maxX, bool& hasData, const QSharedPointer<Curve>& curve) {
        if (!curve) {
            return;
        }
        const auto& points = curve->data();
        if (points.isEmpty()) {
            return;
        }
        for (const auto& point : points) {
            if (!hasData || point.x() > maxX) {
                maxX = point.x();
                hasData = true;
            }
        }
    };


    // // --- 1. 更新【原始数据】图表 (m_chartView1) ---
    // m_chartView1->clearGraphs();
    // m_chartView1->setLabels(tr(""), tr("重量"));
    // // m_chartView1->setPlotTitle("原始数据");

    // for (int sampleId : m_selectedSamples.keys()) {
    //     if (m_stageDataCache.contains(sampleId) && m_stageDataCache[sampleId].original) {
    //         QSharedPointer<Curve> curve = m_stageDataCache[sampleId].original;
    //         curve->setColor(QColor::fromHsv((240 + sampleId * 50) % 360, 200, 220));
    //         curve->setName(m_selectedSamples.value(sampleId));
    //         m_chartView1->addCurve(curve);
    //     }
    // }
    // m_chartView1->replot();


    // --- 2. 更新【裁剪】图表 (m_chartView2) ---
    m_chartView2->clearGraphs();
    m_chartView2->setLabels(tr(""), tr(""));
    m_chartView2->setPlotTitle("裁剪数据");

    // for (int sampleId : m_selectedSamples.keys()) {
    //     DEBUG_LOG << "sampleId0000: " << sampleId;

    //     // 【关键】从 m_stageDataCache.cleaned 获取数据
    //     if (m_stageDataCache.contains(sampleId) && m_stageDataCache[sampleId].cleaned) {
    //         QSharedPointer<Curve> curve = m_stageDataCache[sampleId].cleaned;

    //         SampleIdentifier sid = sampleIdMap[sampleId]; // 直接取缓存
    //         QString legendName = QString("%1-%2-%3-%4")
    //                                 .arg(sid.projectName)
    //                                 .arg(sid.batchCode)
    //                                 .arg(sid.shortCode)
    //                                 .arg(sid.parallelNo);

    //         // curve->setName(legendName);
            
    //         // curve->setColor(QColor::fromHsv((240 + colorIndex * 50) % 360, 200, 220));
    //         curve->setColor(ColorUtils::setCurveColor(colorIndex));
    //         // curve->setName(m_selectedSamples.value(sampleId));
    //         m_chartView2->addCurve(curve);
    //         m_chartView2->setLegendVisible(false);
    //         colorIndex++;
    //     }
    // }
    // m_chartView2->replot();

    QElapsedTimer timer;  //  先声明
    timer.restart();

    DEBUG_LOG << "Updating cleaned curves...";
    for (auto groupIt = m_stageDataCache.constBegin(); groupIt != m_stageDataCache.constEnd(); ++groupIt) {

        DEBUG_LOG << "大热重裁剪数据后绘图每次循环用时：" << timer.elapsed() << "ms";
    timer.restart();

        const QString &groupKey = groupIt.key();
        const SampleGroup &group = groupIt.value();

        // DEBUG_LOG << "Processing group:" << groupKey
        //         << " sample count:" << group.sampleDatas.size();

        for (const auto &sample : group.sampleDatas) {
            int sampleId = sample.sampleId;
            // DEBUG_LOG << "  Sample ID:" << sampleId
            //         << " stage count:" << sample.stages.size();



            // 在 sample.stages 中查找名字是 "cleaned" 的阶段
            for (const auto &stage : sample.stages) {
                // DEBUG_LOG << "    Stage name:" << stage.stageName;
                if (stage.stageName == StageName::Clip && stage.curve) {

                    DEBUG_LOG << "大热重裁剪数据后绘图每次循环用时clip：" << timer.elapsed() << "ms";
                    timer.restart();

                    QSharedPointer<Curve> curve = stage.curve;

                    // // 构建图例名称（根据 SampleIdentifier 逻辑）
                    // QString legendName = QString("%1-%2-%3-%4")
                    //                         .arg(group.projectName)
                    //                         .arg(group.batchCode)
                    //                         .arg(group.shortCode)
                    //                         .arg(sample.shortCode.isEmpty() ? QString::number(sampleId) : sample.shortCode);

                    curve->setColor(ColorUtils::setCurveColor(colorIndex++));
                    // curve->setName(legendName);
                    // DEBUG_LOG << "    Adding cleaned curve:" << curve->name();

                    m_chartView2->addCurve(curve);
                    updateMaxX(clipMaxX, hasClipData, curve);
                    // m_chartView2->setLegendVisible(false);

                    // DEBUG_LOG << "    Added cleaned curve:" << legendName;

                    DEBUG_LOG << "大热重裁剪数据后绘图每次循环用时clip：" << timer.elapsed() << "ms";
                    timer.restart();
                }
            }
        }

        DEBUG_LOG << "大热重裁剪数据后绘图每次循环用时：" << timer.elapsed() << "ms";
    timer.restart();

    }

    DEBUG_LOG << "大热重裁剪数据处理用时：" << timer.elapsed() << "ms";
    timer.restart();

    m_chartView2->setLegendVisible(false);// 放在循环里面会导致绘图时长暴增，特别是曲线多的时候
    m_chartView2->replot();
    if (hasClipData) {
        m_chartView2->setXAxisRange(0.0, clipMaxX);
    }

    DEBUG_LOG << "大热重裁剪数据后绘图用时：" << timer.elapsed() << "ms";
    timer.restart();

    // --- 2. 更新【归一化】图表 (m_chartView3) ---
    m_chartView3->clearGraphs();
    m_chartView3->setLabels(tr(""), tr(""));
    m_chartView3->setPlotTitle("归一化数据");

    colorIndex = 0;

    for (auto groupIt = m_stageDataCache.constBegin(); groupIt != m_stageDataCache.constEnd(); ++groupIt) {
        const QString &groupKey = groupIt.key();
        const SampleGroup &group = groupIt.value();

        DEBUG_LOG << "Processing group:" << groupKey
                << " sample count:" << group.sampleDatas.size();

        for (const auto &sample : group.sampleDatas) {
            int sampleId = sample.sampleId;
            DEBUG_LOG << "  Sample ID:" << sampleId
                    << " stage count:" << sample.stages.size();

            // 在 sample.stages 中查找名字是 "cleaned" 的阶段
            for (const auto &stage : sample.stages) {
                DEBUG_LOG << "    Stage name:" << stage.stageName;
                if (stage.stageName == StageName::Normalize && stage.curve) {
                    QSharedPointer<Curve> curve = stage.curve;

                    curve->setColor(ColorUtils::setCurveColor(colorIndex++));
                    // curve->setName(legendName);
                    DEBUG_LOG << "    Adding normalized curve:" << curve->name();

                    m_chartView3->addCurve(curve);
                    updateMaxX(normalizeMaxX, hasNormalizeData, curve);
                    // m_chartView3->setLegendVisible(false);

                    // DEBUG_LOG << "    Added normalized curve:" << legendName;
                }
            }
        }
    }

    m_chartView3->setLegendVisible(false);// 放在循环里面会导致绘图时长暴增，特别是曲线多的时候

    m_chartView3->replot();
    if (hasNormalizeData) {
        m_chartView3->setXAxisRange(0.0, normalizeMaxX);
    }

    DEBUG_LOG << "大热重归一化数据后绘图用时：" << timer.elapsed() << "ms";
    timer.restart();

    // --- 4. 更新【平滑】图表 (m_chartView4) ---
    m_chartView4->clearGraphs();
    m_chartView4->setLabels(tr(""), tr(""));
    m_chartView4->setPlotTitle("平滑数据");

    colorIndex = 0;

    for (auto groupIt = m_stageDataCache.constBegin(); groupIt != m_stageDataCache.constEnd(); ++groupIt) {
        const QString &groupKey = groupIt.key();
        const SampleGroup &group = groupIt.value();

        DEBUG_LOG << "Processing group:" << groupKey
                << " sample count:" << group.sampleDatas.size();

        for (const auto &sample : group.sampleDatas) {
            int sampleId = sample.sampleId;
            DEBUG_LOG << "  Sample ID:" << sampleId
                    << " stage count:" << sample.stages.size();

            // 在 sample.stages 中查找名字是 "cleaned" 的阶段
            for (const auto &stage : sample.stages) {
                DEBUG_LOG << "    Stage name:" << stage.stageName;
                if (stage.stageName == StageName::Smooth && stage.curve) {
                    QSharedPointer<Curve> curve = stage.curve;

                    curve->setColor(ColorUtils::setCurveColor(colorIndex++));
                    // curve->setName(legendName);
                    DEBUG_LOG << "    Adding smoothed curve:" << curve->name();

                    m_chartView4->addCurve(curve);
                    updateMaxX(smoothMaxX, hasSmoothData, curve);
                    // m_chartView4->setLegendVisible(false);

                    // DEBUG_LOG << "    Added normalized curve:" << legendName;
                }
            }
        }
    }
    m_chartView4->setLegendVisible(false);
    m_chartView4->replot();
    if (hasSmoothData) {
        m_chartView4->setXAxisRange(0.0, smoothMaxX);
    }

    DEBUG_LOG << "大热重平滑数据后绘图用时：" << timer.elapsed() << "ms";
    timer.restart();

    // --- 5. 更新【微分】图表 (m_chartView5) ---
    m_chartView5->clearGraphs();
    m_chartView5->setLabels(tr(""), tr("重量"));
    m_chartView5->setPlotTitle("微分数据");

    colorIndex = 0;

    for (auto groupIt = m_stageDataCache.constBegin(); groupIt != m_stageDataCache.constEnd(); ++groupIt) {
        const QString &groupKey = groupIt.key();
        const SampleGroup &group = groupIt.value();

        DEBUG_LOG << "Processing group:" << groupKey
                << " sample count:" << group.sampleDatas.size();

        for (const auto &sample : group.sampleDatas) {
            int sampleId = sample.sampleId;
            DEBUG_LOG << "  Sample ID:" << sampleId
                    << " stage count:" << sample.stages.size();

            // 在 sample.stages 中查找名字是 "cleaned" 的阶段
            for (const auto &stage : sample.stages) {
                DEBUG_LOG << "    Stage name:" << stage.stageName;
                if (stage.stageName == StageName::Derivative && stage.curve) {
                    QSharedPointer<Curve> curve = stage.curve;

                    curve->setColor(ColorUtils::setCurveColor(colorIndex++));
                    // curve->setName(legendName);
                    DEBUG_LOG << "    Adding derivative curve:" << curve->name();

                    m_chartView5->addCurve(curve);
                    updateMaxX(derivativeMaxX, hasDerivativeData, curve);
                    // m_chartView5->setLegendVisible(false);

                    // DEBUG_LOG << "    Added normalized curve:" << legendName;
                }
            }
        }
    }
    m_chartView5->setLegendVisible(false);
    m_chartView5->replot();
    if (hasDerivativeData) {
        m_chartView5->setXAxisRange(0.0, derivativeMaxX);
    }

    DEBUG_LOG << "大热重微分数据后绘图用时：" << timer.elapsed() << "ms";
    timer.restart();



    updateLegendPanel();
}


void TgBigDataProcessDialog::updateLegendPanel()
{
    if (!m_legendPanel) return;

    // 清空原来的图例
    QLayoutItem* item;
    while ((item = m_legendLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    int colorIndex = 0;
    for (int sampleId : m_selectedSamples.keys()) {
        QString legendText = buildSampleDisplayName(sampleId);

        QLabel* label = new QLabel(legendText);
        QColor color = ColorUtils::setCurveColor(colorIndex);
        QPalette pal = label->palette();
        pal.setColor(QPalette::WindowText, color);
        label->setPalette(pal);

        m_legendLayout->addWidget(label);
        colorIndex++;
    }
    m_legendLayout->addStretch();
}




void TgBigDataProcessDialog::onParameterChanged()
{
    // 更新右侧面板显示
    QTreeWidgetItem* currentItem = m_leftNavigator->currentItem();
    if (currentItem) {
        // updateRightPanel(currentItem);
    }
}



void TgBigDataProcessDialog::onItemChanged(QTreeWidgetItem *item, int column)
{
    // 当由代码设置复选框时，抑制递归触发
    if (m_suppressItemChanged) {
        return;
    }
    // 只处理样本节点的复选框变化
    if (!item || !item->parent() || !item->parent()->parent()) {
        return; // 不是样本节点
    }
    
    // 获取当前被点击的样本ID和选中状态
    int clickedSampleId = item->data(0, Qt::UserRole).toInt();
    bool isChecked = (item->checkState(0) == Qt::Checked);
    
    // 本地勾选仅控制本界面绘图显示，不影响主导航
    QString sampleFullName = buildSampleDisplayName(clickedSampleId);
    if (isChecked) {
        m_selectedSamples[clickedSampleId] = sampleFullName;
        loadSampleCurve(clickedSampleId);
        updateSelectedSamplesList();
    } else {
        m_selectedSamples.remove(clickedSampleId);
        removeSampleCurve(clickedSampleId);
        updateSelectedSamplesList();
    }
    
    // 获取所有选中的样本
    QList<QPair<int, QString>> selectedSamples;
    
    // 用于统计不同种类样本的数量
    QMap<QString, int> sampleTypeCount;
    
    // 遍历所有项目
    for (int i = 0; i < m_leftNavigator->topLevelItemCount(); ++i) {
        QTreeWidgetItem* projectItem = m_leftNavigator->topLevelItem(i);
        
        // 遍历项目下的所有批次
        for (int j = 0; j < projectItem->childCount(); ++j) {
            QTreeWidgetItem* batchItem = projectItem->child(j);
            
            // 遍历批次下的所有样本
            for (int k = 0; k < batchItem->childCount(); ++k) {
                QTreeWidgetItem* sampleItem = batchItem->child(k);
                
                // 如果样本被选中，添加到列表
                if (sampleItem->checkState(0) == Qt::Checked) {
                    int sampleId = sampleItem->data(0, Qt::UserRole).toInt();
                    QString sampleFullName = sampleItem->data(0, Qt::UserRole + 1).toString();
                    selectedSamples.append(qMakePair(sampleId, sampleFullName));
                    
                    // 提取样本类型并统计
                    QString sampleType = "大热重"; // 默认为大热重类型
                    
                    // 尝试从样本名称中提取更详细的类型信息
                    if (sampleFullName.contains("大热重")) {
                        sampleType = "大热重";
                    } else if (sampleFullName.contains("小热重")) {
                        sampleType = "小热重";
                    } else if (sampleFullName.contains("色谱")) {
                        sampleType = "色谱";
                    }
                    
                    // 增加对应类型的计数
                    sampleTypeCount[sampleType] = sampleTypeCount.value(sampleType, 0) + 1;
                }
            }
        }
    }
    
    // 打印统计结果到日志文件
    INFO_LOG << "\n【样本选择统计】时间: " << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    INFO_LOG << "【样本选择统计】操作: " << (isChecked ? "选中" : "取消选中");
    INFO_LOG << "【样本选择统计】样本ID: " << clickedSampleId;
    INFO_LOG << "【样本选择统计】=== 详细统计 ===";
    
    int totalCount = 0;
    QStringList statDetails;
    
    QMapIterator<QString, int> it(sampleTypeCount);
    while (it.hasNext()) {
        it.next();
        QString typeInfo = QString("%1: %2个").arg(it.key()).arg(it.value());
        statDetails.append(typeInfo);
        totalCount += it.value();
        INFO_LOG << "【样本选择统计】" << it.key() << ": " << it.value() << "个样本";
    }
    
    INFO_LOG << "【样本选择统计】总计: " << totalCount << "个样本被选中";
    INFO_LOG << "【样本选择统计】日志文件位置: " << QCoreApplication::applicationDirPath() + "/logs/app.log";
    INFO_LOG << "【样本选择统计】=== 统计结束 ===\n";
    
    // 在状态栏显示统计信息
    if (parentWidget() && parentWidget()->parentWidget()) {
        QMainWindow* mainWindow = qobject_cast<QMainWindow*>(parentWidget()->parentWidget());
        if (mainWindow) {
            QString statusMsg = QString("已选择 %1 个样本 (%2)").arg(totalCount).arg(statDetails.join(", "));
            mainWindow->statusBar()->showMessage(statusMsg, 5000);
        }
    }
    
    
    // 记录当前选中的样本信息
    INFO_LOG << "【曲线显示】当前选中的样本数量: " << selectedSamples.size();
    for (const auto& sample : selectedSamples) {
        INFO_LOG << "【曲线显示】选中样本: ID=" << sample.first << ", 名称=" << sample.second;
    }
}

// 在左侧导航树中查找样本节点（通过样本ID），若不存在返回nullptr
QTreeWidgetItem* TgBigDataProcessDialog::findSampleItemById(int sampleId) const
{
    if (!m_leftNavigator) return nullptr;
    for (int i = 0; i < m_leftNavigator->topLevelItemCount(); ++i) {
        QTreeWidgetItem* projectItem = m_leftNavigator->topLevelItem(i);
        if (!projectItem) continue;
        for (int j = 0; j < projectItem->childCount(); ++j) {
            QTreeWidgetItem* batchItem = projectItem->child(j);
            if (!batchItem) continue;
            for (int k = 0; k < batchItem->childCount(); ++k) {
                QTreeWidgetItem* sampleItem = batchItem->child(k);
                if (!sampleItem) continue;
                bool ok = false;
                int itemSampleId = sampleItem->data(0, Qt::UserRole).toInt(&ok);
                if (ok && itemSampleId == sampleId) {
                    return sampleItem;
                }
            }
        }
    }
    return nullptr;
}

// 处理主导航样本选中状态变化，仅对 dataType=="大热重" 生效，动态增删导航树节点与曲线
void TgBigDataProcessDialog::onMainNavigatorSampleSelectionChanged(int sampleId, const QString& sampleName, const QString& dataType, bool selected)
{
    try {
        // 仅处理大热重数据类型
        if (dataType != QStringLiteral("大热重")) {
            return;
        }

        if (selected) {
            // 若已存在，更新勾选状态并加载曲线
            QTreeWidgetItem* existing = findSampleItemById(sampleId);
            if (existing) {
                m_suppressItemChanged = true; // 防止递归触发 onItemChanged
                existing->setCheckState(0, Qt::Checked);
                m_suppressItemChanged = false;
                loadSampleCurve(sampleId);
                return;
            }

            // 查询样本详细信息以构建节点
            QVariantMap info = m_sampleDao.getSampleById(sampleId);
            QString projectName = info.value("project_name").toString();
            QString batchCode   = info.value("batch_code").toString();
            QString shortCode   = info.value("short_code").toString();
            int parallelNo      = info.value("parallel_no").toInt();

            // 构建项目节点（如不存在则创建）
            QTreeWidgetItem* projectItem = nullptr;
            for (int i = 0; i < m_leftNavigator->topLevelItemCount(); ++i) {
                QTreeWidgetItem* item = m_leftNavigator->topLevelItem(i);
                if (item && item->text(0) == projectName) { projectItem = item; break; }
            }
            if (!projectItem) {
                projectItem = new QTreeWidgetItem(m_leftNavigator, QStringList() << projectName);
            }

            // 构建批次节点（如不存在则创建）
            QTreeWidgetItem* batchItem = nullptr;
            for (int j = 0; j < projectItem->childCount(); ++j) {
                QTreeWidgetItem* item = projectItem->child(j);
                if (item && item->text(0) == batchCode) { batchItem = item; break; }
            }
            if (!batchItem) {
                batchItem = new QTreeWidgetItem(projectItem, QStringList() << batchCode);
            }

            // 创建样本节点（使用统一格式的完整样本名称）
            QString sampleFullName = buildSampleDisplayName(sampleId);
            QTreeWidgetItem* sampleItem = new QTreeWidgetItem(batchItem, QStringList() << sampleFullName);
            sampleItem->setFlags(sampleItem->flags() | Qt::ItemIsUserCheckable);
            sampleItem->setData(0, Qt::UserRole, sampleId); // 存样本ID
            m_suppressItemChanged = true;
            sampleItem->setCheckState(0, Qt::Checked);
            m_suppressItemChanged = false;

            // 同步选中样本集合与左侧“选中样本”列表（使用完整名称）
            m_selectedSamples.insert(sampleId, sampleFullName);
            updateSelectedSamplesList();

            // 加载曲线
            loadSampleCurve(sampleId);
        } else {
            // 取消选择：移除曲线并从导航树删除样本节点
            QTreeWidgetItem* sampleItem = findSampleItemById(sampleId);
            if (!sampleItem) return;
            removeSampleCurve(sampleId);

            QTreeWidgetItem* batchItem = sampleItem->parent();
            QTreeWidgetItem* projectItem = batchItem ? batchItem->parent() : nullptr;
            delete sampleItem;

            // 清理空的批次与项目节点
            if (batchItem && batchItem->childCount() == 0) {
                if (projectItem) {
                    projectItem->removeChild(batchItem);
                    delete batchItem;
                    if (projectItem->childCount() == 0) {
                        m_leftNavigator->invisibleRootItem()->removeChild(projectItem);
                        delete projectItem;
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        ERROR_LOG << "onMainNavigatorSampleSelectionChanged error:" << e.what();
    }
}



// // 【新】启动差异度分析的槽函数
// void TgBigDataProcessDialog::onStartComparison()
// {
//     if (m_stageDataCache.isEmpty() || m_selectedSamples.count() < 2) {
//         // ... 提示错误 ...
//         return;
//     }

//     // 1. 弹出对话框，让用户选择一个参考样本
//     QStringList sampleNames = m_selectedSamples.values();
//     bool ok;
//     QString refName = QInputDialog::getItem(this, tr("选择参考样本"), tr("请选择一个样本作为对比基准:"), sampleNames, 0, false, &ok);
//     if (!ok || refName.isEmpty()) return;

//     // 2. 准备数据：参考曲线 和 所有曲线的列表
//     QSharedPointer<Curve> referenceCurve;
//     QList<QSharedPointer<Curve>> allDerivativeCurves;
//     int refId = m_selectedSamples.key(refName);

//     for (int id : m_selectedSamples.keys()) {
//         if (m_stageDataCache.contains(id) && m_stageDataCache[id].derivative) {
//             allDerivativeCurves.append(m_stageDataCache[id].derivative);
//             if (id == refId) {
//                 referenceCurve = m_stageDataCache[id].derivative;
//             }
//         }
//     }

//     if (referenceCurve.isNull()) { /* ...错误处理... */ return; }

//     // 3. 【关键】通知 MainWindow 创建新的工作台
//     //    这里使用 EventBus 是最佳实践
//     // EventBus::instance()->emit requestNewDifferenceWorkbench(referenceCurve, allDerivativeCurves);
// }


// // 【关键】修改 onStartComparison 的最后一步
// void TgBigDataProcessDialog::onStartComparison()
// {
//     if (m_stageDataCache.isEmpty() || m_selectedSamples.count() < 2) {
//         QMessageBox::warning(this, "提示", "样本数量不足（需要至少2个），无法进行对比。");
//         return;
//     }

//     // 1. 弹出对话框，让用户选择一个参考样本
//     QStringList sampleNames = m_selectedSamples.values();
//     bool ok;
//     QString refName = QInputDialog::getItem(this, tr("选择参考样本"), tr("请选择一个样本作为对比基准:"), sampleNames, 0, false, &ok);
//     if (!ok || refName.isEmpty()) return;

//     // 2. 准备数据：参考曲线 和 所有曲线的列表
//     QSharedPointer<Curve> referenceCurve;
//     QList<QSharedPointer<Curve>> allDerivativeCurves;
//     int refId = m_selectedSamples.key(refName);

//     for (int id : m_selectedSamples.keys()) {
//         if (m_stageDataCache.contains(id) && m_stageDataCache[id].derivative) {
//             allDerivativeCurves.append(m_stageDataCache[id].derivative);
//             if (id == refId) {
//                 referenceCurve = m_stageDataCache[id].derivative;
//             }
//         }
//     }

//     if (referenceCurve.isNull()) {
//         QMessageBox::critical(this, tr("错误"), tr("未能找到所选参考样本的有效微分数据。"));
//         return;
//     }

//     DEBUG_LOG << "0000000";

//     // 3. 【修改】不再使用 EventBus，而是发出自己的信号
//     DEBUG_LOG << "[DEBUG] About to emit requestNewDifferenceWorkbench";
//     emit requestNewDifferenceWorkbench(referenceCurve, allDerivativeCurves);
//     DEBUG_LOG << "[DEBUG] Signal emitted";

// }

// ...

// void TgBigDataProcessDialog::onStartComparison()
// {
//     // 检查前提条件
//     if (m_stageDataCache.isEmpty() || m_selectedSamples.count() < 2) {
//         QMessageBox::warning(this, "提示", "样本数量不足，无法进行对比。");
//         return;
//     }

//     // 1. 让用户选择参考样本 (这部分逻辑不变)
//     QStringList sampleNames = m_selectedSamples.values();
//     bool ok;
//     QString refName = QInputDialog::getItem(this, tr("选择参考样本"), tr("请选择一个样本作为对比基准:"), sampleNames, 0, false, &ok);
//     if (!ok || refName.isEmpty()) return;

//     // 2. 获取参考样本的ID
//     int referenceSampleId = m_selectedSamples.key(refName);

//     // 3. 【升级】直接发出信号，将【完整的缓存数据】和【参考ID】传递出去
//     //    我们不再需要手动构建 QList<QSharedPointer<Curve>>
//     emit requestNewDifferenceWorkbench(referenceSampleId, m_stageDataCache);
// }

void TgBigDataProcessDialog::onStartComparison()
{
    if (m_stageDataCache.isEmpty() || m_selectedSamples.count() < 2) {
        QMessageBox::warning(this, "提示", "样本数量不足（需要至少2个），无法进行对比。");
        return;
    }

    // 仅列举“组内最优样本”作为参考候选
    // 1) 复制当前批次数据，并确保计算出 bestInGroup 标记
    BatchGroupData batchCopy = m_stageDataCache;
    if (m_appInitializer && m_appInitializer->getParallelSampleAnalysisService()) {
        m_appInitializer->getParallelSampleAnalysisService()->selectRepresentativesInBatch(
            batchCopy, m_currentParams, StageName::Derivative
        );
    }

    // 2) 遍历各组，收集 bestInGroup 的样本 ID → 统一显示名称映射
    QMap<QString, int> nameToIdMap; // Key: 统一显示名称, Value: sampleId
    QStringList displayNames;
    for (auto it = batchCopy.constBegin(); it != batchCopy.constEnd(); ++it) {
        const SampleGroup& group = it.value();
        int bestId = -1;
        for (const SampleDataFlexible& s : group.sampleDatas) {
            if (s.bestInGroup) { bestId = s.sampleId; break; }
        }
        if (bestId <= 0) continue;
        QString fullName = buildSampleDisplayName(bestId);
        if (fullName.trimmed().isEmpty()) {
            fullName = QStringLiteral("样本 %1").arg(bestId);
        }
        displayNames.append(fullName);
        nameToIdMap[fullName] = bestId;
    }

    // --- 2. 自定义对话框：上下左右使用下拉框（项目/批次/短码/平行号） + 按钮 ---
    QDialog dlg(this);
    dlg.setWindowTitle(tr("选择参考样本"));
    QVBoxLayout* vbox = new QVBoxLayout(&dlg);
    QLabel* tip = new QLabel(tr("请选择一个样本作为对比基准（可通过下拉框逐级筛选）:"));
    vbox->addWidget(tip);

    // 改为“仅列举组内最优样本 + 复选框”，不再展示全部样本
    QListWidget* refList = new QListWidget(&dlg);
    for (const QString& name : displayNames) {
        QListWidgetItem* it = new QListWidgetItem(name, refList);
        it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
        it->setCheckState(Qt::Unchecked);
    }
    vbox->addWidget(refList);

    QHBoxLayout* hbox = new QHBoxLayout();
    QPushButton* showBestBtn = new QPushButton(tr("显示组内最优样本"), &dlg);
    QPushButton* okBtn = new QPushButton(tr("确定"), &dlg);
    QPushButton* cancelBtn = new QPushButton(tr("取消"), &dlg);
    hbox->addWidget(showBestBtn);
    hbox->addStretch();
    hbox->addWidget(okBtn);
    hbox->addWidget(cancelBtn);
    vbox->addLayout(hbox);

    // 显示组内最优样本子对话框
    connect(showBestBtn, &QPushButton::clicked, [&](){
        if (!m_appInitializer || !m_appInitializer->getParallelSampleAnalysisService()) {
            QMessageBox::warning(&dlg, tr("提示"), tr("代表样选择服务不可用"));
            return;
        }
        BatchGroupData batchCopy = m_stageDataCache;
        m_appInitializer->getParallelSampleAnalysisService()->selectRepresentativesInBatch(
            batchCopy, m_currentParams, StageName::Derivative
        );
        QDialog bestDlg(&dlg);
        bestDlg.setWindowTitle(tr("组内最优样本"));
        QVBoxLayout* bv = new QVBoxLayout(&bestDlg);
        QTableWidget* table = new QTableWidget(&bestDlg);
        table->setColumnCount(7);
        table->setHorizontalHeaderLabels(QStringList() << "组Key" << "样本ID" << "样本名称" << "项目" << "批次" << "短码" << "平行号");
        table->horizontalHeader()->setStretchLastSection(true);
        int row = 0;
        for (auto it = batchCopy.begin(); it != batchCopy.end(); ++it) {
            const QString groupKey = it.key();
            const SampleGroup& g = it.value();
            int bestId = -1;
            for (const auto& s : g.sampleDatas) { if (s.bestInGroup) { bestId = s.sampleId; break; } }
            if (bestId == -1) continue;
            QString bestName = buildSampleDisplayName(bestId);
            table->insertRow(row);
            table->setItem(row, 0, new QTableWidgetItem(groupKey));
            table->setItem(row, 1, new QTableWidgetItem(QString::number(bestId)));
            table->setItem(row, 2, new QTableWidgetItem(bestName));
            table->setItem(row, 3, new QTableWidgetItem());
            table->setItem(row, 4, new QTableWidgetItem());
            table->setItem(row, 5, new QTableWidgetItem());
            table->setItem(row, 6, new QTableWidgetItem());
            row++;
        }
        bv->addWidget(table);
        QHBoxLayout* bh = new QHBoxLayout();
        QPushButton* exportBtn2 = new QPushButton(tr("导出Excel"), &bestDlg);
        QPushButton* closeBtn = new QPushButton(tr("关闭"), &bestDlg);
        bh->addWidget(exportBtn2);
        bh->addStretch();
        bh->addWidget(closeBtn);
        bv->addLayout(bh);
        connect(exportBtn2, &QPushButton::clicked, [&](){
            QString filePath = QFileDialog::getSaveFileName(&bestDlg, tr("导出组内最优样本"), QDir::homePath()+"/best_in_group.xlsx", tr("Excel文件 (*.xlsx)"));
            if (filePath.isEmpty()) return;
            QXlsx::Document xlsx;
            xlsx.write(1,1,"组Key"); xlsx.write(1,2,"样本ID"); xlsx.write(1,3,"样本名称");
            xlsx.write(1,4,"项目"); xlsx.write(1,5,"批次"); xlsx.write(1,6,"短码"); xlsx.write(1,7,"平行号");
            int r=2;
            for (int i = 0; i < table->rowCount(); ++i) {
                xlsx.write(r,1, table->item(i,0)->text());
                xlsx.write(r,2, table->item(i,1)->text());
                xlsx.write(r,3, table->item(i,2)->text());
                xlsx.write(r,4, table->item(i,3)->text());
                xlsx.write(r,5, table->item(i,4)->text());
                xlsx.write(r,6, table->item(i,5)->text());
                xlsx.write(r,7, table->item(i,6)->text());
                r++;
            }
            if (!xlsx.saveAs(filePath)) {
                QMessageBox::critical(&bestDlg, tr("错误"), tr("无法保存XLSX文件：%1").arg(filePath));
                return;
            }
            QMessageBox::information(&bestDlg, tr("成功"), tr("已导出：%1").arg(filePath));
        });
        connect(closeBtn, &QPushButton::clicked, &bestDlg, &QDialog::accept);
        bestDlg.exec();
    });

    // 按要求移除“导出Excel”按钮（参考样本对话框不再提供导出功能）

    // 确定与取消逻辑
    connect(okBtn, &QPushButton::clicked, [&](){ dlg.accept(); });
    connect(cancelBtn, &QPushButton::clicked, [&](){ dlg.reject(); });

    if (dlg.exec() != QDialog::Accepted) return;

    // --- 3. 从勾选列表获取选择的参考样本 ID（要求仅选中一个） ---
    QString selectedFullName; int referenceSampleId = -1; int checkedCount = 0;
    for (int i = 0; i < refList->count(); ++i) {
        QListWidgetItem* it = refList->item(i);
        if (it && it->checkState() == Qt::Checked) {
            checkedCount++;
            selectedFullName = it->text();
        }
    }
    if (checkedCount != 1) {
        QMessageBox::warning(&dlg, tr("提示"), tr("请仅勾选一个参考样本"));
        return;
    }
    referenceSampleId = nameToIdMap.value(selectedFullName, -1);
    if (referenceSampleId == -1) { return; }

    // --- 4. 发出信号，交由主窗口与工作台处理对比 ---
    emit requestNewTgBigDifferenceWorkbench(referenceSampleId, m_stageDataCache, m_currentParams);
}

// void TgBigDataProcessDialog::onSelectAllSamplesInBatch(const QString& projectName, const QString& batchCode)
// {
//     DEBUG_LOG << "===== onSelectAllSamplesInBatch called =====";
//     DEBUG_LOG << "项目名:" << projectName << " 批次号:" << batchCode;

//     if (projectName.isEmpty() || batchCode.isEmpty()) {
//         DEBUG_LOG << "项目名或批次号为空，无法获取样本";
//         QMessageBox::warning(this, tr("错误"), tr("项目名或批次号为空"));
//         return;
//     }

//     DEBUG_LOG << "获取批次下的样本";

//     // 获取已有已选样本ID，避免重复
//     const QList<int> keys = m_selectedSamples.keys();
//     QSet<int> selectedIds(keys.begin(), keys.end());

//     DEBUG_LOG << "当前已选中样本数量:" << selectedIds.size();
    
//     // 使用 DAO 获取该批次下所有样本
//     // SampleDAO dao;
//     SingleTobaccoSampleDAO dao;
//     QList<SampleIdentifier> samples = dao.getSampleIdentifiersByProjectAndBatch(projectName, batchCode);
    
//     DEBUG_LOG << "批次下样本数量:" << samples.size();
    
//     // 遍历样本并选中它们
//     for (const SampleIdentifier& sample : samples) {
//         int sampleId = sample["id"].toInt();
//         QString sampleName = sample["sample_name"].toString();
        
//         // 如果样本未被选中，则选中它
//         if (!selectedIds.contains(sampleId)) {
//             // 添加到选中集合
//             m_selectedSamples.insert(sampleId, sampleName);
            
//             // 在导航树中找到并选中对应项
//             QTreeWidgetItemIterator it(m_leftNavigator);
//             while (*it) {
//                 if ((*it)->data(0, Qt::UserRole).toInt() == sampleId) {
//                     (*it)->setCheckState(0, Qt::Checked);
//                     break;
//                 }
//                 ++it;
//             }
            
//             // 添加曲线
//             addSampleCurve(sampleId, sampleName);
            
//             // 通知主窗口更新导航树
//             emit sampleSelected(sampleId, true);
//         }
//     }
    
//     // 更新统计信息
//     showSelectedSamplesStatistics();
    
//     // 重新计算并更新图表
//     recalculateAndUpdatePlot();
// }




void TgBigDataProcessDialog::onSelectAllSamplesInBatch(const QString& projectName, const QString& batchCode)
{
    DEBUG_LOG << "===== onSelectAllSamplesInBatch called =====";
    DEBUG_LOG << "项目名:" << projectName << " 批次号:" << batchCode;

    if (projectName.isEmpty() || batchCode.isEmpty()) {
        DEBUG_LOG << "项目名或批次号为空，无法获取样本";
        QMessageBox::warning(this, tr("错误"), tr("项目名或批次号为空"));
        return;
    }

    DEBUG_LOG << "获取批次下的样本";


    // 获取已有已选样本ID，避免重复
    // 最安全的写法（适用所有 Qt 版本）
    const QList<int> keys = m_selectedSamples.keys();
    QSet<int> selectedIds(keys.begin(), keys.end());

    DEBUG_LOG << "当前已选中样本数量:" << selectedIds.size();
    // 使用 DAO 获取该批次下所有样本
    SingleTobaccoSampleDAO dao;
    QList<SampleIdentifier> samples = dao.getSampleIdentifiersByProjectAndBatch(projectName, batchCode, BatchType::NORMAL);

    DEBUG_LOG << "获取到批次下的样本数量:" << samples.size();
    int addedCount = 0;
    int totalSamples = samples.size();

    DEBUG_LOG << "获取到批次下的样本数量:" << samples.size();
    for (const SampleIdentifier& sample : samples) {
        if (!selectedIds.contains(sample.sampleId)) {
            // 添加到 m_selectedSamples：使用统一格式的完整样本名称
            QString fullName = buildSampleDisplayName(sample.sampleId);
            m_selectedSamples.insert(sample.sampleId, fullName);

            // 更新界面树控件状态：在左侧导航树中勾选对应样本的复选框
            // 根据项目名与批次号定位到批次节点，再根据样本ID定位到样本节点并勾选
            QTreeWidgetItem* projectItem = nullptr;
            for (int i = 0; i < m_leftNavigator->topLevelItemCount(); ++i) {
                QTreeWidgetItem* item = m_leftNavigator->topLevelItem(i);
                if (item && item->text(0) == projectName) {
                    projectItem = item;
                    break;
                }
            }
            if (projectItem) {
                QTreeWidgetItem* batchItem = nullptr;
                for (int j = 0; j < projectItem->childCount(); ++j) {
                    QTreeWidgetItem* child = projectItem->child(j);
                    if (child && child->text(0) == batchCode) {
                        batchItem = child;
                        break;
                    }
                }
                if (batchItem) {
                    for (int k = 0; k < batchItem->childCount(); ++k) {
                        QTreeWidgetItem* sampleItem = batchItem->child(k);
                        if (sampleItem && sampleItem->data(0, Qt::UserRole).toInt() == sample.sampleId) {
                            sampleItem->setCheckState(0, Qt::Checked);
                            DEBUG_LOG << "已勾选大热重样本复选框:" << sample.sampleId;
                            break;
                        }
                    }
                }
            }

            // 同步主界面导航树的复选框（如果存在主导航树引用）
            // 调用DataNavigator提供的方法，按sampleId勾选数据源树中的对应样本
            if (m_mainNavigator) {
                m_mainNavigator->setSampleCheckState(sample.sampleId, true);
            }

            addedCount++;
            DEBUG_LOG << "添加样本id:" << sample.sampleId << "样本:" << sample.shortCode;
        } else {
            DEBUG_LOG << "样本已存在，跳过:" << sample.sampleId;
        }
    }

    DEBUG_LOG << "批量添加完成，总样本数:" << totalSamples << " 新增数:" << addedCount;
    // 更新图表显示（如果有）
    // updatePlot();
    // updateLegendPanel();

    drawSelectedSampleCurves();

    // 显示提示
    if (addedCount > 0) {
        QMessageBox::information(this, tr("批量添加成功"),
                                 tr("成功添加 %1 个样本\n共处理 %2 个有效样本00")
                                 .arg(addedCount)
                                 .arg(totalSamples));
    } else if (totalSamples == 0) {
        QMessageBox::warning(this, tr("未找到有效样本"),
                             tr("批次下没有可添加的小热重类型样本"));
    } else {
        QMessageBox::information(this, tr("无新样本可添加"),
                                 tr("批次中的所有有效样本(%1 个)已经添加")
                                 .arg(totalSamples));
    }

    DEBUG_LOG << "批量添加完成，总样本数:" << totalSamples << " 新增数:" << addedCount;
}
