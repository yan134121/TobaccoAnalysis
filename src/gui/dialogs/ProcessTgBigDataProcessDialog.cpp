#include "ProcessTgBigDataProcessDialog.h"
#include <QDebug>
#include <QSplitter>
#include <QHeaderView>
#include <QMessageBox>
#include <QMainWindow>
#include <QStatusBar>
#include <QDateTime>
#include <QCoreApplication>
#include <QtConcurrent>
#include <QTimer>

#include "Logger.h"
#include "gui/views/DataNavigator.h"
#include "core/common.h"
#include "ProcessTgBigParameterSettingsDialog.h"
#include "AppInitializer.h"
#include "xlsxdocument.h"
#include "gui/views/ChartView.h"
#include "core/entities/Curve.h"
#include "SingleTobaccoSampleDAO.h" // 修改: 包含对应的头文件
#include "ColorUtils.h"
#include "core/singletons/SampleSelectionManager.h"
#include "InfoAutoClose.h"
#include <QCursor>
#include <QStyle>
#include <QStyleOptionViewItem>
#include "services/algorithm/processing/IProcessingStep.h"
#include "services/algorithm/Clipping.h"
#include "services/algorithm/Normalization.h"
#include "services/algorithm/SavitzkyGolay.h"
#include "services/algorithm/Loess.h"

ProcessTgBigDataProcessDialog::ProcessTgBigDataProcessDialog(QWidget *parent, AppInitializer* appInitializer, DataNavigator *mainNavigator) :
    QWidget(parent), m_appInitializer(appInitializer), m_mainNavigator(mainNavigator)
{
    // 安全检查
    if (!m_appInitializer) {
        // 这是一个致命的编程错误，程序无法在没有服务提供者的情况下工作
        qFatal("ProcessTgBigDataProcessDialog created without a valid AppInitializer!");
    }

    // 【关键】使用传入的 appInitializer 来获取服务
    m_processingService = m_appInitializer->getDataProcessingService();

    setWindowTitle(tr("工序大热重数据处理"));
    // setMinimumSize(1200, 800);
    
    // resize(1400, 900);

    // // 如果有 DataNavigator 对象，初始化它
    // m_mainNavigator = new DataNavigator(this);

    // // 创建 TabWidget
    // QTabWidget* tabWidget = new QTabWidget(this);
    // setCentralWidget(tabWidget);  // QMainWindow 设置中央窗口

    // // =========================
    // // Tab 1: 工序大热重数据处理
    // // =========================
    // ProcessTgBigDataProcessDialog* tab1 = new ProcessTgBigDataProcessDialog(this, m_mainNavigator);
    // tabWidget->addTab(tab1, tr("工序大热重数据处理"));

    // 提前创建参数设置窗口
    m_paramDialog = new ProcessTgBigParameterSettingsDialog(m_currentParams, this);

    // 连接信号槽（只需一次）
    connect(m_paramDialog, &ProcessTgBigParameterSettingsDialog::parametersApplied,
            this, &ProcessTgBigDataProcessDialog::onParametersApplied);
    
    setupUI();
    setupConnections();
    loadNavigatorData();

    // showMaximized();   //  在这里最大化窗口

    // 首次打开时同步主导航已选中的“工序大热重”样本，并立即绘制
    {
        QSet<int> preselected = SampleSelectionManager::instance()->selectedIdsByType(QStringLiteral("工序大热重"));
        if (!preselected.isEmpty()) {
            m_suppressItemChanged = true; // 防止触发 onItemChanged 递归
            for (int sampleId : preselected) {
                // 1) 在左侧导航树中勾选对应样本
                int topN = m_leftNavigator->topLevelItemCount();
                for (int i = 0; i < topN; ++i) {
                    QTreeWidgetItem* projectItem = m_leftNavigator->topLevelItem(i);
                    if (!projectItem) continue;
                    for (int j = 0; j < projectItem->childCount(); ++j) {
                        QTreeWidgetItem* batchItem = projectItem->child(j);
                        if (!batchItem) continue;
                        for (int k = 0; k < batchItem->childCount(); ++k) {
                            QTreeWidgetItem* sampleItem = batchItem->child(k);
                            if (!sampleItem) continue;
                            if (sampleItem->data(0, Qt::UserRole).toInt() == sampleId) {
                                sampleItem->setFlags(sampleItem->flags() | Qt::ItemIsUserCheckable);
                                sampleItem->setCheckState(0, Qt::Checked);
                            }
                        }
                    }
                }
                // 2) 添加到本界面选中集合，供绘制使用
                QString err; QVariantMap info = m_navigatorDao.getSampleDetailInfo(sampleId, err);
                QString name = info.value("sample_name").toString();
                if (name.isEmpty()) name = QStringLiteral("样本 %1").arg(sampleId);
                m_selectedSamples.insert(sampleId, name);
            }
            m_suppressItemChanged = false;
            drawSelectedSampleCurves();
        }
    }

    if (!m_mainNavigator) {
        WARNING_LOG << " mainNavigator is nullptr!";
    } else {
        DEBUG_LOG << " mainNavigator is valid:" << m_mainNavigator;
    }

    DEBUG_LOG << "ProcessTgBigDataProcessDialog::ProcessTgBigDataProcessDialog - Finished";
}

ProcessTgBigDataProcessDialog::~ProcessTgBigDataProcessDialog()
{
    // delete ui; // Removed as UI is code-built
    DEBUG_LOG << "ProcessTgBigParameterSettingsDialog destroyed!";
}

QMap<int, QString> ProcessTgBigDataProcessDialog::getSelectedSamples() const
{
    QMap<int, QString> selectedSamples;
    
    try {
        // 检查导航树是否为空
        if (!m_leftNavigator) {
            DEBUG_LOG << "导航树为空，无法获取选中样本";
            return selectedSamples;
        }
        
        // 遍历导航树中所有样本节点
        int topLevelCount = 0;
        try {
            topLevelCount = m_leftNavigator->topLevelItemCount();
        } catch (const std::exception& e) {
            DEBUG_LOG << "获取顶层项目数量时发生异常:" << e.what();
            return selectedSamples;
        } catch (...) {
            DEBUG_LOG << "获取顶层项目数量时发生未知异常";
            return selectedSamples;
        }
        
        for (int i = 0; i < topLevelCount; ++i) {
            QTreeWidgetItem* projectItem = nullptr;
            
            try {
                projectItem = m_leftNavigator->topLevelItem(i);
            } catch (const std::exception& e) {
                DEBUG_LOG << "获取项目项时发生异常:" << e.what();
                continue;
            } catch (...) {
                DEBUG_LOG << "获取项目项时发生未知异常";
                continue;
            }
            
            if (!projectItem) {
                DEBUG_LOG << "项目项为空，跳过";
                continue;
            }
            
            int batchCount = 0;
            try {
                batchCount = projectItem->childCount();
            } catch (const std::exception& e) {
                DEBUG_LOG << "获取批次数量时发生异常:" << e.what();
                continue;
            } catch (...) {
                DEBUG_LOG << "获取批次数量时发生未知异常";
                continue;
            }
            
            for (int j = 0; j < batchCount; ++j) {
                QTreeWidgetItem* batchItem = nullptr;
                
                try {
                    batchItem = projectItem->child(j);
                } catch (const std::exception& e) {
                    DEBUG_LOG << "获取批次项时发生异常:" << e.what();
                    continue;
                } catch (...) {
                    DEBUG_LOG << "获取批次项时发生未知异常";
                    continue;
                }
                
                if (!batchItem) {
                    DEBUG_LOG << "批次项为空，跳过";
                    continue;
                }
                
                int sampleCount = 0;
                try {
                    sampleCount = batchItem->childCount();
                } catch (const std::exception& e) {
                    DEBUG_LOG << "获取样本数量时发生异常:" << e.what();
                    continue;
                } catch (...) {
                    DEBUG_LOG << "获取样本数量时发生未知异常";
                    continue;
                }
                
                for (int k = 0; k < sampleCount; ++k) {
                    QTreeWidgetItem* sampleItem = nullptr;
                    
                    try {
                        sampleItem = batchItem->child(k);
                    } catch (const std::exception& e) {
                        DEBUG_LOG << "获取样本项时发生异常:" << e.what();
                        continue;
                    } catch (...) {
                        DEBUG_LOG << "获取样本项时发生未知异常";
                        continue;
                    }
                    
                    if (!sampleItem) {
                        DEBUG_LOG << "样本项为空，跳过";
                        continue;
                    }
                    
                    // 检查样本是否被选中
                    try {
                        Qt::CheckState checkState = sampleItem->checkState(0);
                        if (checkState == Qt::Checked) {
                            bool ok = false;
                            QVariant idVariant = sampleItem->data(0, Qt::UserRole);
                            if (!idVariant.isValid()) {
                                DEBUG_LOG << "样本ID无效";
                                continue;
                            }
                            
                            int sampleId = idVariant.toInt(&ok);
                            if (!ok || sampleId <= 0) {
                                DEBUG_LOG << "样本ID转换失败或无效:" << idVariant;
                                continue;
                            }
                            
                            QString sampleName = sampleItem->text(0);
                            selectedSamples.insert(sampleId, sampleName);
                        }
                    } catch (const std::exception& e) {
                        DEBUG_LOG << "检查样本选中状态时发生异常:" << e.what();
                        continue;
                    } catch (...) {
                        DEBUG_LOG << "检查样本选中状态时发生未知异常";
                        continue;
                    }
                }
            }
        }
        
        for (auto it = selectedSamples.begin(); it != selectedSamples.end(); ++it) {
            DEBUG_LOG << "sampleId:" << it.key() << "sampleName:" << it.value();
        }
    } catch (const std::exception& e) {
        DEBUG_LOG << "获取选中样本时发生异常:" << e.what();
    } catch (...) {
        DEBUG_LOG << "获取选中样本时发生未知异常";
    }

    return selectedSamples;
}

int ProcessTgBigDataProcessDialog::countSelectedSamples() const
{
    return getSelectedSamples().size();
}

void ProcessTgBigDataProcessDialog::showSelectedSamplesStatistics()
{
    QMap<int, QString> selectedSamples = getSelectedSamples();
    int count = selectedSamples.size();
    
    QString message = tr("当前选中的工序大热重样本数量: %1\n\n").arg(count);
    
    if (count > 0) {
        message += tr("选中的样本列表:\n");
        QMapIterator<int, QString> i(selectedSamples);
        while (i.hasNext()) {
            i.next();
            message += tr("ID: %1, 名称: %2\n").arg(i.key()).arg(i.value());
        }
    } else {
        message += tr("没有选中任何工序大热重样本");
    }
    
    QMessageBox::information(this, tr("工序大热重样本统计"), message);
}

void ProcessTgBigDataProcessDialog::selectSample(int sampleId, const QString& sampleName)
{
    try {
        if (sampleId <= 0) {
            DEBUG_LOG << "无效的样本ID:" << sampleId;
            return;
        }
        
        DEBUG_LOG << "ProcessTgBigDataProcessDialog::selectSample - ID:" << sampleId << "Name:" << sampleName;
        // 确保窗口处于激活状态
        this->activateWindow();
        this->raise();
        
        // 在左侧导航树中查找并选中指定的样本
        bool found = false;
        
        if (!m_leftNavigator) {
            DEBUG_LOG << "导航树为空";
            return;
        }
        
        DEBUG_LOG << "Top level count:" << m_leftNavigator->topLevelItemCount();

        DEBUG_LOG;
        // 遍历所有项目节点
        for (int i = 0; i < m_leftNavigator->topLevelItemCount(); ++i) {
            QTreeWidgetItem* projectItem = m_leftNavigator->topLevelItem(i);
            if (!projectItem) continue;
            DEBUG_LOG;
            
            // 遍历项目下的批次节点
            for (int j = 0; j < projectItem->childCount(); ++j) {
                QTreeWidgetItem* batchItem = projectItem->child(j);
                if (!batchItem) continue;
                DEBUG_LOG;
                
                // 遍历批次下的样本节点
                for (int k = 0; k < batchItem->childCount(); ++k) {
                    QTreeWidgetItem* sampleItem = batchItem->child(k);
                    if (!sampleItem) continue;

                    DEBUG_LOG;
                    
                    // 检查样本ID是否匹配
                    bool ok = false;
                    int itemSampleId = sampleItem->data(0, Qt::UserRole).toInt(&ok);
                    if (!ok) continue;
                    
                    if (itemSampleId == sampleId) {
                        DEBUG_LOG;
                        try {
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
                            try {
                                loadSampleCurve(sampleId);
                            } catch (const std::exception& e) {
                                DEBUG_LOG << "加载样本曲线异常:" << e.what();
                            } catch (...) {
                                DEBUG_LOG << "加载样本曲线未知异常";
                            }
                            
                            // 设置复选框为选中状态
                            sampleItem->setCheckState(0, Qt::Checked);
                            
                            // 确保导航树标题显示为"工序大热重 - 样本导航"
                            m_leftNavigator->setHeaderLabel(tr("工序大热重 - 样本导航"));
                            
                            DEBUG_LOG << "工序大热重样本已选中:" << sampleId << sampleName;
                            
                            // 发出信号通知主窗口设置导航树选择框状态
                            emit sampleSelected(sampleId, true);
                        } catch (const std::exception& e) {
                            DEBUG_LOG << "处理样本选择异常:" << e.what();
                        } catch (...) {
                            DEBUG_LOG << "处理样本选择未知异常";
                        }
                        
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
    } catch (const std::exception& e) {
        DEBUG_LOG << "selectSample方法异常:" << e.what();
    } catch (...) {
        DEBUG_LOG << "selectSample方法未知异常";
    }
}

void ProcessTgBigDataProcessDialog::addSampleCurve(int sampleId, const QString& sampleName)
{
    try {
        if (sampleId <= 0) {
            DEBUG_LOG << "无效的样本ID:" << sampleId;
            return;
        }
        
        DEBUG_LOG << "ProcessTgBigDataProcessDialog::addSampleCurve - ID:" << sampleId << "Name:" << sampleName;
        
        // 将样本加入“可见曲线”集合，不影响被选中样本集合
        m_visibleSamples.insert(sampleId);
        DEBUG_LOG << "当前可见样本数:" << m_visibleSamples.size();
        updateSelectedSamplesList();
        
        // 不在此处触发绘图；统一由 selectionChangedByType 路由中合并刷新
        
        // 检查导航树是否存在
        if (!m_leftNavigator) {
            DEBUG_LOG << "导航树为空";
            return;
        }
        
        // 在导航树中找到对应的样本节点并设置为选中状态
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
                    if (!ok) continue;
                    
                    if (itemSampleId == sampleId) {
                        try {
                            // 设置复选框为选中状态，但不改变当前选中项
                            sampleItem->setCheckState(0, Qt::Checked);
                            DEBUG_LOG << "工序大热重样本曲线已添加:" << sampleId << sampleName;
                            return;
                        } catch (const std::exception& e) {
                            DEBUG_LOG << "设置样本选中状态异常:" << e.what();
                        } catch (...) {
                            DEBUG_LOG << "设置样本选中状态未知异常";
                        }
                    }
                }
            }
        }
        
        DEBUG_LOG << "Sample not found for adding curve:" << sampleId << sampleName;
    } catch (const std::exception& e) {
        DEBUG_LOG << "addSampleCurve方法异常:" << e.what();
    } catch (...) {
        DEBUG_LOG << "addSampleCurve方法未知异常";
    }
}


void ProcessTgBigDataProcessDialog::drawSelectedSampleCurves()
{
    DEBUG_LOG << "绘制所有可见样本，数量:" << m_visibleSamples.size();

    QElapsedTimer timer;  //  先声明
    timer.restart();




    // 清空单一绘图区域中的所有曲线
    if (m_chartView1) m_chartView1->clearGraphs();

    DEBUG_LOG;

    // 预定义颜色列表
    QList<QColor> colorList = { Qt::blue, Qt::red, Qt::green, Qt::black, Qt::magenta,
                                Qt::darkYellow, Qt::cyan, Qt::darkGreen, Qt::darkBlue,
                                Qt::darkRed, Qt::darkMagenta, Qt::darkCyan };

    int colorIndex = 0;
    if (m_chartView1) m_chartView1->setPlotTitle(QStringLiteral("工序大热重 - 原始数据"));

    // 遍历所有当前可见（勾选）的样本
    for (int sampleId : m_visibleSamples) {
        QString sampleName = m_selectedSamples.value(sampleId);

        DEBUG_LOG;

        try {
            QString error;
            QVector<QPointF> points = m_navigatorDao.getSampleCurveData(sampleId, QStringLiteral("工序大热重"), error);
            QVariantMap sampleInfo = m_navigatorDao.getSampleDetailInfo(sampleId, error);
            QString shortCode = sampleInfo.value("short_code").toString();
            // 统一图例名为“project-batch-short-parallel”，确保悬停提示完整显示
            QString projectName = sampleInfo.value("project_name").toString();
            QString batchCode   = sampleInfo.value("batch_code").toString();
            int parallelNo      = sampleInfo.value("parallel_no").toInt();
            QString fullLegend  = QString("%1-%2-%3-%4")
                                    .arg(projectName)
                                    .arg(batchCode)
                                    .arg(shortCode)
                                    .arg(parallelNo);
            if (!fullLegend.trimmed().isEmpty()) sampleName = fullLegend;

            if (!points.isEmpty()) {
                QVector<double> xData, yData;
                for (const QPointF& p : points) { xData.append(p.x()); yData.append(p.y()); }

                QColor curveColor = ColorUtils::setCurveColor(colorIndex);

                // 不再区分 Q/Z/H 段，所有样本曲线叠加绘制在同一绘图区域
                if (m_chartView1) m_chartView1->addGraph(xData, yData, sampleName, curveColor, sampleId);
                colorIndex++;
            } else {
                DEBUG_LOG << "未找到样本曲线数据，样本ID:" << sampleId << ", 错误:" << error;
            }
        } catch (const std::exception& e) {
            DEBUG_LOG << "加载样本曲线异常，样本ID:" << sampleId << ", 错误:" << e.what();
        }
    }

    // 更新单一图表
    if (m_chartView1) m_chartView1->setLegendVisible(false);

    DEBUG_LOG << "ProcessTgBigDataProcessDialog::drawSelectedSampleCurves - 绘制原始数据图表用时:" << timer.elapsed() << "ms";
    timer.restart();

    if (m_chartView1) m_chartView1->replot();
    

    DEBUG_LOG << "ProcessTgBigDataProcessDialog::drawSelectedSampleCurves - Elapsed time:" << timer.elapsed() << "ms";

    updateLegendPanel();
}


void ProcessTgBigDataProcessDialog::removeSampleCurve(int sampleId)
{
    // 如果该样本当前并不可见，直接返回
    if (!m_visibleSamples.contains(sampleId)) {
        DEBUG_LOG << "ProcessTgBigDataProcessDialog::removeSampleCurve: sampleId" << sampleId << "not visible, skip.";
        
        return;
    }
    
    DEBUG_LOG << "ProcessTgBigDataProcessDialog::removeSampleCurve - ID:" << sampleId;
    
    m_visibleSamples.remove(sampleId);
    DEBUG_LOG << "当前可见样本数:" << m_visibleSamples.size();
    // 移除后不立即重绘，由上层批量逻辑统一处理，避免闪烁
    updateSelectedSamplesList();
}

void ProcessTgBigDataProcessDialog::setupUI()
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

    DEBUG_LOG << "ProcessTgBigDataProcessDialog::setupUI - Splitter sizes:" << m_mainSplitter->sizes();

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
    m_processAndPlotButton = new QPushButton("处理并绘图", tab1Widget); // 点击后按当前参数处理并刷新绘图
    m_processAndPlotButton->setVisible(false); // 按需隐藏该按钮
    // 增加一个按钮
    m_startComparisonButton = new QPushButton(tr("计算差异度"));
    // 新增显示/隐藏左侧标签页按钮（导航 + 选中样本）
    m_toggleNavigatorButton = new QPushButton(tr("样本浏览页"), tab1Widget);
    // 新增清除曲线按钮
    m_clearCurvesButton = new QPushButton(tr("清除曲线"), tab1Widget);
    // 新增绘制所有选中曲线与取消所有选中样本按钮
    m_drawAllButton = new QPushButton(tr("绘制所有选中曲线"), tab1Widget);
    m_unselectAllButton = new QPushButton(tr("取消所有选中样本"), tab1Widget);
    m_pickBestCurveButton = new QPushButton(tr("返回最优曲线"), tab1Widget);
    
    

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
    m_buttonLayout->addWidget(m_pickBestCurveButton);

    // m_mainLayout->addLayout(m_buttonLayout);

    tab1Layout->addLayout(m_buttonLayout);

    // 将 Tab1 添加到 tabWidget
    tabWidget->addTab(tab1Widget, tr("工序大热重数据处理"));

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

    DEBUG_LOG << "ProcessTgBigDataProcessDialog::setupUI - Splitter sizes:" << m_mainSplitter->sizes();
}


void ProcessTgBigDataProcessDialog::setupLeftNavigator()
{
    // 创建左侧面板
    m_leftPanel = new QWidget();
    m_leftLayout = new QVBoxLayout(m_leftPanel);
    
    // 创建左侧标签页容器，仅包含“选中样本”一页
    m_leftTabWidget = new QTabWidget(m_leftPanel);

    // 创建（隐藏的）导航树实例，不加入到界面，仅供内部函数保留兼容（可选）
    m_leftNavigator = new QTreeWidget();
    m_leftNavigator->setHeaderLabel(tr("工序大热重 - 样本导航"));
    m_leftNavigator->setMinimumWidth(250);
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
    
    // 确保左侧面板可见
    m_leftPanel->hide();
}

void ProcessTgBigDataProcessDialog::setupMiddlePanel()
{
    DEBUG_LOG << "ProcessTgBigDataProcessDialog::setupMiddlePanel - Setting up middle panel";
    
    // 创建中间面板
    m_middlePanel = new QWidget();
    m_middlePanel->setMinimumWidth(500); // 确保中间面板有足够的宽度
    m_middleLayout = new QGridLayout(m_middlePanel);
    
    // 仅创建一个绘图区域，所有曲线叠加显示在同一图上
    m_chartView1 = new ChartView();
    m_chartView2 = nullptr;
    m_chartView3 = nullptr;
    // m_chartView4 = new ChartView();
    // m_chartView5 = new ChartView();
    if (!m_chartView1) {
        DEBUG_LOG << "ProcessTgBigDataProcessDialog::setupMiddlePanel--VIEW - ERROR: Failed to create ChartView!";
    } else {
        DEBUG_LOG << "ProcessTgBigDataProcessDialog::setupMiddlePanel--VIEW - ChartView created successfully";
    }

    // 单一绘图区域
    m_middleLayout->addWidget(m_chartView1, 0, 0);
    // m_middleLayout->addWidget(m_chartView4, 1, 1);
    // m_middleLayout->addWidget(m_chartView5, 2, 0); // 显示

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
    plotLegendSplitter->addWidget(m_middlePanel);     // 左侧：绘图区（垂直布局：Q/Z/H）
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
    
    DEBUG_LOG << "ProcessTgBigDataProcessDialog::setupMiddlePanel - Middle panel setup completed";
    // DEBUG_LOG << "ProcessTgBigDataProcessDialog::setupMiddlePanel - ChartView visibility:" << m_chartView->isVisible();
    DEBUG_LOG << "ProcessTgBigDataProcessDialog::setupMiddlePanel - Middle panel visibility:" << m_middlePanel->isVisible();
}

void ProcessTgBigDataProcessDialog::setupRightPanel()
{
    DEBUG_LOG << "ProcessTgBigDataProcessDialog::setupRightPanel - Setting up right panel";
    // 创建右侧面板
    m_rightPanel = new QWidget();
    m_rightLayout = new QVBoxLayout(m_rightPanel);
    
    // 阶段选择控件
    m_stageLabel = new QLabel(QStringLiteral("阶段选择"), m_rightPanel);
    m_stageCombo = new QComboBox(m_rightPanel);
    m_stageCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    
    // 填充工序大热重阶段模板
    for (const auto &tpl : tgBigProcessTemplates) {
        m_stageCombo->addItem(stageNameToString(tpl.stageName), QVariant::fromValue(static_cast<int>(tpl.stageName)));
    }

    // 默认选中“原始数据”阶段
    int rawIndex = m_stageCombo->findText(QStringLiteral("原始数据"));
    if (rawIndex >= 0) m_stageCombo->setCurrentIndex(rawIndex);

    // 布局添加
    m_rightLayout->addWidget(m_stageLabel);
    m_rightLayout->addWidget(m_stageCombo);
    
    // 右侧快捷勾选项（显示均值曲线 / 绘图时插值 / 原始（预修复）叠加 / 显示坏点）
    {
        // 创建四个勾选框
        m_showMeanCurveCheckRight = new QCheckBox(QStringLiteral("显示均值曲线"), m_rightPanel);
        m_plotInterpolationCheckRight = new QCheckBox(QStringLiteral("绘图时插值"), m_rightPanel);
        m_showRawOverlayCheckRight = new QCheckBox(QStringLiteral("显示原始（预修复）叠加"), m_rightPanel);
        m_showBadPointsCheckRight = new QCheckBox(QStringLiteral("显示坏点"), m_rightPanel);
        // 同步当前参数初始值
        if (m_showMeanCurveCheckRight) m_showMeanCurveCheckRight->setChecked(m_currentParams.showMeanCurve);
        if (m_plotInterpolationCheckRight) m_plotInterpolationCheckRight->setChecked(m_currentParams.plotInterpolation);
        if (m_showRawOverlayCheckRight) m_showRawOverlayCheckRight->setChecked(m_currentParams.showRawOverlay);
        if (m_showBadPointsCheckRight) m_showBadPointsCheckRight->setChecked(m_currentParams.showBadPoints);
        // 添加到右侧布局
        m_rightLayout->addWidget(m_showMeanCurveCheckRight);
        m_rightLayout->addWidget(m_plotInterpolationCheckRight);
        m_rightLayout->addWidget(m_showRawOverlayCheckRight);
        m_rightLayout->addWidget(m_showBadPointsCheckRight);
        // 连接勾选变化到参数与绘图更新
        auto onToggle = [this](){
            // 更新当前参数
            if (m_showMeanCurveCheckRight) m_currentParams.showMeanCurve = m_showMeanCurveCheckRight->isChecked();
            if (m_plotInterpolationCheckRight) m_currentParams.plotInterpolation = m_plotInterpolationCheckRight->isChecked();
            if (m_showRawOverlayCheckRight) m_currentParams.showRawOverlay = m_showRawOverlayCheckRight->isChecked();
            if (m_showBadPointsCheckRight) m_currentParams.showBadPoints = m_showBadPointsCheckRight->isChecked();
            // 优先使用缓存快速刷新；无缓存但有样本时触发重算
            if (!m_stageDataCache.isEmpty()) {
                updatePlotQZH();
            } else if (!m_visibleSamples.isEmpty()) {
                recalculateAndUpdatePlot();
            }
        };
        connect(m_showMeanCurveCheckRight, &QCheckBox::toggled, this, onToggle);
        connect(m_plotInterpolationCheckRight, &QCheckBox::toggled, this, onToggle);
        connect(m_showRawOverlayCheckRight, &QCheckBox::toggled, this, onToggle);
        connect(m_showBadPointsCheckRight, &QCheckBox::toggled, this, onToggle);
    }

    // 阶段数据对比显示块（多阶段叠加对比）
    {
        m_stageCompareGroup = new QGroupBox(QStringLiteral("阶段数据对比显示"), m_rightPanel);
        QVBoxLayout* stageLayout = new QVBoxLayout(m_stageCompareGroup);

        m_stageRawCheck        = new QCheckBox(QStringLiteral("原始数据"), m_stageCompareGroup);
        m_stageBadRepairCheck  = new QCheckBox(QStringLiteral("坏点修复"), m_stageCompareGroup);
        m_stageClipCheck       = new QCheckBox(QStringLiteral("裁剪"), m_stageCompareGroup);
        m_stageNormalizeCheck  = new QCheckBox(QStringLiteral("归一化"), m_stageCompareGroup);
        m_stageSmoothCheck     = new QCheckBox(QStringLiteral("平滑"), m_stageCompareGroup);
        m_stageDerivativeCheck = new QCheckBox(QStringLiteral("微分"), m_stageCompareGroup);

        stageLayout->addWidget(m_stageRawCheck);
        stageLayout->addWidget(m_stageBadRepairCheck);
        stageLayout->addWidget(m_stageClipCheck);
        stageLayout->addWidget(m_stageNormalizeCheck);
        stageLayout->addWidget(m_stageSmoothCheck);
        stageLayout->addWidget(m_stageDerivativeCheck);
        stageLayout->addStretch();

        m_stageCompareGroup->setLayout(stageLayout);
        m_rightLayout->addWidget(m_stageCompareGroup);

        // 任意阶段勾选变化时，刷新绘图（若已有阶段数据缓存则直接使用）
        auto onStageToggle = [this](bool){
            if (!m_stageDataCache.isEmpty()) {
                updatePlotQZH();
            } else if (!m_selectedSamples.isEmpty()) {
                recalculateAndUpdatePlot();
            }
        };
        connect(m_stageRawCheck,        &QCheckBox::toggled, this, onStageToggle);
        connect(m_stageBadRepairCheck,  &QCheckBox::toggled, this, onStageToggle);
        connect(m_stageClipCheck,       &QCheckBox::toggled, this, onStageToggle);
        connect(m_stageNormalizeCheck,  &QCheckBox::toggled, this, onStageToggle);
        connect(m_stageSmoothCheck,     &QCheckBox::toggled, this, onStageToggle);
        connect(m_stageDerivativeCheck, &QCheckBox::toggled, this, onStageToggle);
    }

    m_rightLayout->addStretch();

    // 阶段变化时刷新绘图；同时取消“多阶段对比”勾选，回到单阶段模式
    connect(m_stageCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int){
        // 阶段切换时，先取消多阶段对比勾选（使用信号屏蔽避免触发重复计算）
        if (m_stageRawCheck || m_stageBadRepairCheck || m_stageClipCheck ||
            m_stageNormalizeCheck || m_stageSmoothCheck || m_stageDerivativeCheck) {
            QSignalBlocker b1(m_stageRawCheck);
            QSignalBlocker b2(m_stageBadRepairCheck);
            QSignalBlocker b3(m_stageClipCheck);
            QSignalBlocker b4(m_stageNormalizeCheck);
            QSignalBlocker b5(m_stageSmoothCheck);
            QSignalBlocker b6(m_stageDerivativeCheck);
            if (m_stageRawCheck)        m_stageRawCheck->setChecked(false);
            if (m_stageBadRepairCheck)  m_stageBadRepairCheck->setChecked(false);
            if (m_stageClipCheck)       m_stageClipCheck->setChecked(false);
            if (m_stageNormalizeCheck)  m_stageNormalizeCheck->setChecked(false);
            if (m_stageSmoothCheck)     m_stageSmoothCheck->setChecked(false);
            if (m_stageDerivativeCheck) m_stageDerivativeCheck->setChecked(false);
        }

        // 阶段切换时优先使用已计算缓存；若缓存为空但有选中样本，则触发重算以获取对应阶段曲线
        if (!m_stageDataCache.isEmpty()) {
            DEBUG_LOG << "使用已计算缓存";
            updatePlotQZH();
        } else if (!m_selectedSamples.isEmpty()) {
            DEBUG_LOG << "缓存为空但有选中样本，触发重算";
            recalculateAndUpdatePlot();
        } else {
            DEBUG_LOG << "缓存为空且无选中样本";
            drawSelectedSampleCurves();
        }
    });

    // 添加到分割器
    m_mainSplitter->addWidget(m_rightPanel);

    DEBUG_LOG << "ProcessTgBigDataProcessDialog::setupRightPanel - Right panel setup complete";
}


void ProcessTgBigDataProcessDialog::setupConnections()
{
    // 按钮连接
    connect(m_parameterButton, &QPushButton::clicked, this, &ProcessTgBigDataProcessDialog::onParameterSettingsClicked);
    connect(m_processAndPlotButton, &QPushButton::clicked, this, &ProcessTgBigDataProcessDialog::onProcessAndPlotButtonClicked); // 处理并绘图
    connect(m_startComparisonButton, &QPushButton::clicked, this, &ProcessTgBigDataProcessDialog::onStartComparison);
    // connect(m_toggleNavigatorButton, &QPushButton::clicked, this, &ProcessTgBigDataProcessDialog::onToggleNavigatorClicked);
    connect(m_toggleNavigatorButton, &QPushButton::clicked, this, &ProcessTgBigDataProcessDialog::onToggleNavigatorClicked);
    connect(m_clearCurvesButton, &QPushButton::clicked, this, &ProcessTgBigDataProcessDialog::onClearCurvesClicked);
    connect(m_drawAllButton, &QPushButton::clicked, this, &ProcessTgBigDataProcessDialog::onDrawAllSelectedCurvesClicked);
    connect(m_unselectAllButton, &QPushButton::clicked, this, &ProcessTgBigDataProcessDialog::onUnselectAllSamplesClicked);
    connect(m_pickBestCurveButton, &QPushButton::clicked, this, &ProcessTgBigDataProcessDialog::onPickBestCurveClicked);
    
    // 选中样本列表勾选变化驱动左侧导航勾选状态
    connect(m_selectedSamplesList, &QListWidget::itemChanged, this, &ProcessTgBigDataProcessDialog::onSelectedSamplesListItemChanged);
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
                if (dataType != QStringLiteral("工序大热重")) return;
                if (selected) {
                    // 加入“被选中样本”集合；首次出现则默认可见（勾选）并绘制曲线
                    QString name = m_selectedSamples.value(sampleId);
                    if (name.isEmpty()) {
                        QVariantMap info = m_sampleDao.getSampleById(sampleId);
                        name = info.value("sample_name").toString();
                        if (name.trimmed().isEmpty()) name = QString("样本 %1").arg(sampleId);
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



void ProcessTgBigDataProcessDialog::onDrawAllSelectedCurvesClicked()
{
    // 从全局选择管理器获取“工序大热重”类型已选样本，全部设为可见并绘制
    QSet<int> ids = SampleSelectionManager::instance()->selectedIdsByType(QStringLiteral("工序大热重"));
    m_visibleSamples = ids;
    // 确保 m_selectedSamples 包含名称
    for (int sampleId : ids) {
        if (!m_selectedSamples.contains(sampleId)) {
            QVariantMap info = m_sampleDao.getSampleById(sampleId);
            QString fullName = info.value("sample_name").toString();
            if (fullName.trimmed().isEmpty()) fullName = QString("样本 %1").arg(sampleId);
            m_selectedSamples.insert(sampleId, fullName);
        }
    }
    drawSelectedSampleCurves();
    updateSelectedSamplesList();
}

void ProcessTgBigDataProcessDialog::onUnselectAllSamplesClicked()
{
    // 清空全局选择管理器中“工序大热重”类型的所有样本，同时清空本界面的可见与选中集合
    const QString type = QStringLiteral("工序大热重");
    QSet<int> ids = SampleSelectionManager::instance()->selectedIdsByType(type);
    for (int sampleId : ids) {
        SampleSelectionManager::instance()->setSelectedWithType(sampleId, type, false, QStringLiteral("Dialog-UnselectAll"));
    }

    m_selectedSamples.clear();
    m_visibleSamples.clear();
    updateSelectedSamplesList();
}

void ProcessTgBigDataProcessDialog::onPickBestCurveClicked()
{
    // 直接从图表中获取所有曲线
    if (!m_chartView1) {
        QMessageBox::warning(this, tr("提示"), tr("图表视图未初始化。"));
        return;
    }

    // 获取图表中所有曲线及其样本ID
    QList<QPair<int, QPair<QVector<double>, QVector<double>>>> allCurves = m_chartView1->getAllCurvesWithSampleIds();
    
    if (allCurves.size() < 2) {
        QMessageBox::warning(this, tr("提示"), tr("至少需要2条曲线才能比较并返回最优曲线。当前图表中有 %1 条曲线。").arg(allCurves.size()));
        return;
    }

    // 获取当前已处理并缓存的样本数据（已废弃，保留用于兼容）
    if (false && m_stageDataCache.isEmpty()) {
        QMessageBox::warning(this, tr("提示"), tr("请先点击“处理并绘图”按钮处理数据。"));
        return;
    }

    // 对每条曲线进行处理（裁剪、归一化、平滑、微分），但不绘图
    QList<QPair<int, QSharedPointer<Curve>>> candidateCurves;
    for (const auto& curveData : allCurves) {
        int sampleId = curveData.first;
        const QVector<double>& x = curveData.second.first;
        const QVector<double>& y = curveData.second.second;
        
        if (x.isEmpty() || y.isEmpty() || x.size() != y.size()) {
            WARNING_LOG << "样本" << sampleId << "的曲线数据无效，跳过";
            continue;
        }

        // 处理曲线：裁剪、归一化、平滑、微分
        QSharedPointer<Curve> processedCurve = processCurveForBestSelection(x, y, sampleId, m_currentParams);
        if (!processedCurve.isNull() && processedCurve->pointCount() > 0) {
            candidateCurves.append({sampleId, processedCurve});
        } else {
            WARNING_LOG << "样本" << sampleId << "的曲线处理失败，跳过";
        }
    }

    if (candidateCurves.size() < 2) {
        QString msg = tr("至少需要2条有效曲线才能选择最优曲线。");
        msg += tr("\n\n图表中曲线数：%1\n成功处理后的曲线数：%2").arg(allCurves.size()).arg(candidateCurves.size());
        QMessageBox::warning(this, tr("提示"), msg);
        return;
    }

    // 以下代码已废弃，保留用于兼容
    if (false) {
        QString msg = tr("至少需要2条有效曲线才能选择最优曲线。");
        if (m_visibleSamples.size() >= 2) {
            msg += tr("\n\n当前可见样本数：%1\n实际获取到的曲线数：%2\n\n可能原因：\n1. 新勾选的样本尚未进行处理；\n2. 数据处理未生成目标阶段曲线（如Derivative）。\n\n建议：请重新点击“处理并绘图”按钮。")
                    .arg(m_visibleSamples.size())
                    .arg(candidateCurves.size());
        }
        QMessageBox::warning(this, tr("提示"), msg);
        return;
    }

    // 如果只有2条，直接比较
    int bestSampleId = -1;
    if (candidateCurves.size() == 2) {
        SampleComparisonService* comparer = m_appInitializer ? m_appInitializer->getSampleComparisonService() : nullptr;
        if (!comparer) {
            QMessageBox::warning(this, tr("错误"), tr("无法获取样本比较服务。"));
            return;
        }

        auto result = comparer->pickBestOfTwo(candidateCurves[0].second, candidateCurves[1].second, m_currentParams.loessSpan);
        bestSampleId = candidateCurves[result.bestIndex].first;

        QString bestName = m_selectedSamples.value(bestSampleId);
        if (bestName.isEmpty()) {
            QString err;
            QVariantMap info = m_navigatorDao.getSampleDetailInfo(bestSampleId, err);
            bestName = info.value("sample_name").toString();
            if (bestName.isEmpty()) bestName = QString("样本 %1").arg(bestSampleId);
        }
        QString msg = tr("最优样本: %1\n质量分数1: %2\n质量分数2: %3")
                      .arg(bestName)
                      .arg(result.quality1, 0, 'f', 6)
                      .arg(result.quality2, 0, 'f', 6);
        QMessageBox::information(this, tr("最优曲线选择结果"), msg);
    } else {
        // 多条曲线：两两比较，选择平均质量分数最优的
        SampleComparisonService* comparer = m_appInitializer ? m_appInitializer->getSampleComparisonService() : nullptr;
        if (!comparer) {
            QMessageBox::warning(this, tr("错误"), tr("无法获取样本比较服务。"));
            return;
        }

        QMap<int, double> qualityScores;
        QMap<int, int> comparisonCounts;

        for (int i = 0; i < candidateCurves.size(); ++i) {
            for (int j = i + 1; j < candidateCurves.size(); ++j) {
                auto result = comparer->pickBestOfTwo(candidateCurves[i].second, candidateCurves[j].second, m_currentParams.loessSpan);
                int id_i = candidateCurves[i].first;
                int id_j = candidateCurves[j].first;
                qualityScores[id_i] = qualityScores.value(id_i, 0.0) + result.quality1;
                qualityScores[id_j] = qualityScores.value(id_j, 0.0) + result.quality2;
                comparisonCounts[id_i] = comparisonCounts.value(id_i, 0) + 1;
                comparisonCounts[id_j] = comparisonCounts.value(id_j, 0) + 1;
            }
        }

        double bestScore = std::numeric_limits<double>::max();
        for (auto it = qualityScores.constBegin(); it != qualityScores.constEnd(); ++it) {
            int count = comparisonCounts.value(it.key(), 1);
            double avgScore = it.value() / count;
            if (avgScore < bestScore) {
                bestScore = avgScore;
                bestSampleId = it.key();
            }
        }
    }

    if (bestSampleId >= 0) {
        QString bestName = m_selectedSamples.value(bestSampleId);
        if (bestName.isEmpty()) {
            QString err;
            QVariantMap info = m_navigatorDao.getSampleDetailInfo(bestSampleId, err);
            bestName = info.value("sample_name").toString();
            if (bestName.isEmpty()) bestName = QString("样本 %1").arg(bestSampleId);
        }
        if (m_chartView1) {
            m_chartView1->highlightGraph(bestName);
        }
        QMessageBox::information(this, tr("最优曲线"), tr("已选择并高亮显示最优曲线: %1").arg(bestName));
    } else {
        QMessageBox::warning(this, tr("错误"), tr("无法确定最优曲线。"));
    }
}

void ProcessTgBigDataProcessDialog::onClearCurvesClicked()
{
    // 仅清除单一绘图区域中的曲线；不改变“被选中样本”，只将其设为不可见
    if (m_chartView1) m_chartView1->clearGraphs();

    m_visibleSamples.clear();
    updateLegendPanel();
    updateSelectedSamplesList();
}

QSharedPointer<Curve> ProcessTgBigDataProcessDialog::processCurveForBestSelection(
    const QVector<double>& x, const QVector<double>& y, int sampleId, const ProcessingParameters& params)
{
    // 创建初始曲线
    QVector<QPointF> points;
    points.reserve(x.size());
    for (int i = 0; i < x.size() && i < y.size(); ++i) {
        points.append(QPointF(x[i], y[i]));
    }
    QSharedPointer<Curve> currentCurve = QSharedPointer<Curve>::create(points, "原始数据");
    currentCurve->setSampleId(sampleId);

    if (!m_processingService) {
        WARNING_LOG << "DataProcessingService 未初始化";
        return QSharedPointer<Curve>();
    }

    // 创建处理步骤实例
    Clipping clippingStep;
    Normalization normalizationStep;
    SavitzkyGolay sgStep;
    Loess loessStep;

    // 阶段1: 裁剪
    if (params.clippingEnabled_ProcessTgBig) {
        QVariantMap clipParams;
        clipParams["min_x"] = params.clipMinX_ProcessTgBig;
        clipParams["max_x"] = params.clipMaxX_ProcessTgBig;
        QString error;
        ProcessingResult res = clippingStep.process({currentCurve.data()}, clipParams, error);
        if (!res.namedCurves.isEmpty() && res.namedCurves.contains("clipped")) {
            currentCurve = QSharedPointer<Curve>(res.namedCurves.value("clipped").first());
            currentCurve->setSampleId(sampleId);
        }
    }

    // 阶段2: 归一化
    if (params.normalizationEnabled) {
        QVariantMap normParams;
        if (params.normalizationMethod == "absmax") {
            normParams["method"] = "absmax";
            normParams["rangeMin"] = 0.0;
            normParams["rangeMax"] = 100.0;
        }
        QString error;
        ProcessingResult res = normalizationStep.process({currentCurve.data()}, normParams, error);
        if (!res.namedCurves.isEmpty() && res.namedCurves.contains("normalized")) {
            currentCurve = QSharedPointer<Curve>(res.namedCurves.value("normalized").first());
            currentCurve->setSampleId(sampleId);
        }
    }

    // 阶段3: 平滑
    if (params.smoothingEnabled) {
        QString smMethod = params.smoothingMethod;
        if (smMethod == "savitzky_golay") {
            QVariantMap smoothParams;
            int sgWin = params.sgWindowSize;
            if (sgWin % 2 == 0) sgWin += 1;
            if (currentCurve && sgWin >= currentCurve->pointCount()) {
                sgWin = qMax(3, currentCurve->pointCount() - 1);
                if (sgWin % 2 == 0) sgWin -= 1;
            }
            smoothParams["window_size"] = sgWin;
            smoothParams["poly_order"] = params.sgPolyOrder;
            smoothParams["derivative_order"] = 0;
            
            if (currentCurve && currentCurve->pointCount() > 0 && 
                smoothParams["window_size"].toInt() > 2 && 
                smoothParams["window_size"].toInt() <= currentCurve->pointCount()) {
                QString error;
                ProcessingResult res = sgStep.process({currentCurve.data()}, smoothParams, error);
                if (!res.namedCurves.isEmpty() && res.namedCurves.contains("smoothed")) {
                    currentCurve = QSharedPointer<Curve>(res.namedCurves.value("smoothed").first());
                    currentCurve->setSampleId(sampleId);
                }
            }
        } else if (smMethod == "loess") {
            QVariantMap smoothParams;
            smoothParams["fraction"] = params.loessSpan;
            if (currentCurve && currentCurve->pointCount() > 2) {
                QString error;
                ProcessingResult res = loessStep.process({currentCurve.data()}, smoothParams, error);
                if (!res.namedCurves.isEmpty() && res.namedCurves.contains("smoothed")) {
                    currentCurve = QSharedPointer<Curve>(res.namedCurves.value("smoothed").first());
                    currentCurve->setSampleId(sampleId);
                }
            }
        }
    }

    // 阶段4: 微分
    if (params.derivativeEnabled) {
        QString dMethod = params.derivativeMethod;
        if (dMethod == "derivative_sg") {
            QVariantMap derivParams;
            int dWin = params.derivSgWindowSize;
            if (dWin % 2 == 0) dWin += 1;
            if (currentCurve && dWin >= currentCurve->pointCount()) {
                dWin = qMax(3, currentCurve->pointCount() - 1);
                if (dWin % 2 == 0) dWin -= 1;
            }
            derivParams["window_size"] = dWin;
            derivParams["poly_order"] = params.derivSgPolyOrder;
            derivParams["derivative_order"] = 1;
            
            if (currentCurve && currentCurve->pointCount() > 1 && 
                derivParams["window_size"].toInt() > 2 && 
                derivParams["window_size"].toInt() <= currentCurve->pointCount()) {
                QString error;
                ProcessingResult res = sgStep.process({currentCurve.data()}, derivParams, error);
                if (res.namedCurves.contains("derivative1")) {
                    currentCurve = QSharedPointer<Curve>(res.namedCurves.value("derivative1").first());
                    currentCurve->setSampleId(sampleId);
                }
            }
        } else if (dMethod == "first_diff") {
            if (currentCurve && currentCurve->pointCount() > 1) {
                QVector<QPointF> src = currentCurve->data();
                QVector<QPointF> diff;
                diff.reserve(src.size());
                for (int i = 0; i < src.size(); ++i) {
                    if (i == 0) { 
                        diff.append(QPointF(src[i].x(), 0.0)); 
                        continue; 
                    }
                    double dx = src[i].x() - src[i-1].x(); 
                    if (qAbs(dx) < 1e-12) dx = 1.0;
                    double dy = src[i].y() - src[i-1].y();
                    diff.append(QPointF(src[i].x(), dy / dx));
                }
                currentCurve = QSharedPointer<Curve>::create(diff, currentCurve->name() + tr(" (一阶差分)"));
                currentCurve->setSampleId(sampleId);
            }
        }
    }

    return currentCurve;
}


// 显示/隐藏左侧标签页（包含样本导航与选中样本列表）
void ProcessTgBigDataProcessDialog::onToggleNavigatorClicked()
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
void ProcessTgBigDataProcessDialog::updateSelectedSamplesList()
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

void ProcessTgBigDataProcessDialog::updateSelectedStatsInfo()
{
    if (!m_selectedStatsLabel) return;
    int selectedCount = m_selectedSamples.size();
    int visibleCount = m_visibleSamples.size();
    m_selectedStatsLabel->setText(tr("已选中样本: %1 / 绘图样本: %2").arg(selectedCount).arg(visibleCount));
}

// 统一调度一次重绘与图例刷新，避免同一事件内重复调用造成卡顿
void ProcessTgBigDataProcessDialog::scheduleRedraw()
{
    if (!m_drawScheduled) {
        m_drawScheduled = true;
        QTimer::singleShot(0, this, [this]{
            m_drawScheduled = false;
            if (!m_stageDataCache.isEmpty()) {
                updatePlotQZH();
            } else {
                drawSelectedSampleCurves();
            }
            updateSelectedStatsInfo();
        });
    }
}

void ProcessTgBigDataProcessDialog::ensureSelectedListItem(int sampleId, const QString& displayName)
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

void ProcessTgBigDataProcessDialog::removeSelectedListItem(int sampleId)
{
    QListWidgetItem* item = m_selectedItemMap.take(sampleId);
    if (item) delete item;
}

void ProcessTgBigDataProcessDialog::prefetchCurveIfNeeded(int sampleId)
{
    if (m_curveCache.contains(sampleId)) return;
    QString dataType = QStringLiteral("工序大热重");
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
void ProcessTgBigDataProcessDialog::onSelectedSamplesListItemChanged(QListWidgetItem* item)
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


// 处理左侧导航器的右键菜单
void ProcessTgBigDataProcessDialog::onLeftNavigatorContextMenu(const QPoint &pos)
{
    
}


void ProcessTgBigDataProcessDialog::onSelectAllSamplesInBatch(const QString& projectName, const QString& batchCode)
{
    DEBUG_LOG << "===== onSelectAllSamplesInBatch called =====";
    DEBUG_LOG << "项目名:" << projectName << " 批次号:" << batchCode;

    if (projectName.isEmpty() || batchCode.isEmpty()) {
        DEBUG_LOG << "项目名或批次号为空，无法获取样本";
        QMessageBox::warning(this, tr("错误"), tr("项目名或批次号为空"));
        return;
    }

    DEBUG_LOG << "获取批次下的样本";


    const QList<int> keys = m_selectedSamples.keys();
    QSet<int> selectedIds(keys.begin(), keys.end());

    DEBUG_LOG << "当前已选中样本数量:" << selectedIds.size();
    // 使用 DAO 获取该批次下所有样本
    SingleTobaccoSampleDAO dao;
    QList<SampleIdentifier> samples = dao.getSampleIdentifiersByProjectAndBatch(projectName, batchCode, BatchType::PROCESS);

    DEBUG_LOG << "获取到批次下的样本数量:" << samples.size();
    int addedCount = 0;
    int totalSamples = samples.size();

    DEBUG_LOG << "获取到批次下的样本数量:" << samples.size();
    // 批量勾选时屏蔽本地导航树的信号，避免 onItemChanged 反复触发造成卡顿
    QSignalBlocker leftBlocker(m_leftNavigator);
    m_suppressItemChanged = true;

    for (const SampleIdentifier& sample : samples) {
        if (!selectedIds.contains(sample.sampleId)) {
            // 添加到 m_selectedSamples：使用完整样本名称
            QVariantMap info = m_sampleDao.getSampleById(sample.sampleId);
            QString fullName = info.value("sample_name").toString();
            if (fullName.trimmed().isEmpty()) fullName = QString("样本 %1").arg(sample.sampleId);
            m_selectedSamples.insert(sample.sampleId, fullName);

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
                            sampleItem->setCheckState(0, Qt::Checked); // 程序化勾选，信号已屏蔽
                            DEBUG_LOG << "已勾选工序大热重样本复选框:" << sample.sampleId;
                            break;
                        }
                    }
                }
            }

            if (m_mainNavigator) {
                // 1) 数据源树（若存在镜像数据则勾选）
                m_mainNavigator->setSampleCheckState(sample.sampleId, true);

                // 2) 工序数据树：按 sampleId 遍历查找并勾选
                QTreeWidgetItem* processRoot = m_mainNavigator->getProcessDataRoot();
                if (processRoot) {
                    // 使用 QSignalBlocker 临时屏蔽主导航树的信号，避免触发私有槽 onItemChanged
                    // QSignalBlocker 构造时屏蔽，析构时恢复，无需手动断开/重连私有槽
                    QSignalBlocker blocker(m_mainNavigator);
                    for (int mi = 0; mi < processRoot->childCount(); ++mi) {
                        QTreeWidgetItem* modelItem = processRoot->child(mi);
                        for (int bi = 0; bi < modelItem->childCount(); ++bi) {
                            QTreeWidgetItem* batchItem = modelItem->child(bi);
                            for (int si = 0; si < batchItem->childCount(); ++si) {
                                QTreeWidgetItem* sampleItem = batchItem->child(si);
                                QVariant v = sampleItem->data(0, Qt::UserRole);
                                if (v.canConvert<NavigatorNodeInfo>()) {
                                    NavigatorNodeInfo info = v.value<NavigatorNodeInfo>();
                                    if (info.type == NavigatorNodeInfo::Sample && info.id == sample.sampleId) {
                                        sampleItem->setCheckState(0, Qt::Checked);
                                        DEBUG_LOG << "主导航树(工序)已勾选样本复选框:" << sample.sampleId;
                                        mi = processRoot->childCount(); // 跳出外层循环
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            }

            addedCount++;
            DEBUG_LOG << "添加样本:" << sample.sampleId << sample.shortCode;
        } else {
            DEBUG_LOG << "样本已存在，跳过:" << sample.sampleId;
        }
    }

    m_suppressItemChanged = false; // 恢复 onItemChanged 响应
    DEBUG_LOG << "批量添加完成，总样本数:" << totalSamples << " 新增数:" << addedCount;


    drawSelectedSampleCurves();

    // 显示提示
    if (addedCount > 0) {
        QMessageBox::information(this, tr("批量添加成功"),
                                 tr("成功添加 %1 个样本\n共处理 %2 个有效样本")
                                 .arg(addedCount)
                                 .arg(totalSamples));
    } else if (totalSamples == 0) {
        QMessageBox::warning(this, tr("未找到有效样本"),
                             tr("批次下没有可添加的工序大热重类型样本"));
    } else {
        QMessageBox::information(this, tr("无新样本可添加"),
                                 tr("批次中的所有有效样本(%1 个)已经添加")
                                 .arg(totalSamples));
    }

    DEBUG_LOG << "批量添加完成，总样本数:" << totalSamples << " 新增数:" << addedCount;
}


void ProcessTgBigDataProcessDialog::loadNavigatorData()
{
    try {
        // 清空现有项目
        m_leftNavigator->clear();
        
        // 检查是否有主界面导航树引用
        if (!m_mainNavigator) {
            // 没有主界面导航树引用，从数据库加载工序大热重数据
            loadNavigatorDataFromDatabase();
        } else {
            // 有主界面导航树引用，从主界面导航树加载数据
            loadNavigatorDataFromMainNavigator();
        }

        // 统一校正所有样本节点为“可勾选 + 初始未勾选”，确保首次展开即可显示复选框
        auto ensureSampleCheckboxesVisible = [this]() {
            m_suppressItemChanged = true; // 避免遍历设置时触发联动
            for (int i = 0; i < m_leftNavigator->topLevelItemCount(); ++i) {
                QTreeWidgetItem* projectItem = m_leftNavigator->topLevelItem(i);
                if (!projectItem) continue;
                for (int j = 0; j < projectItem->childCount(); ++j) {
                    QTreeWidgetItem* batchItem = projectItem->child(j);
                    if (!batchItem) continue;
                    for (int k = 0; k < batchItem->childCount(); ++k) {
                        QTreeWidgetItem* sampleItem = batchItem->child(k);
                        if (!sampleItem) continue;
                        // 只处理具有样本ID的数据节点
                        QVariant idVar = sampleItem->data(0, Qt::UserRole);
                        if (!idVar.isValid()) continue;
                        // 设置为可勾选
                        sampleItem->setFlags(sampleItem->flags() | Qt::ItemIsUserCheckable);
                        // 若未设置过复选状态，则初始化为未勾选（使勾选框可见）
                        if (sampleItem->checkState(0) != Qt::Checked && sampleItem->checkState(0) != Qt::Unchecked) {
                            sampleItem->setCheckState(0, Qt::Unchecked);
                        }
                    }
                }
            }
            m_suppressItemChanged = false;
        };
        ensureSampleCheckboxesVisible();
        
    } catch (const std::exception& e) {
        DEBUG_LOG << "Error loading navigator data:" << e.what();
        QMessageBox::warning(this, tr("错误"), tr("加载导航数据失败: %1").arg(e.what()));
    }
}

void ProcessTgBigDataProcessDialog::loadNavigatorDataFromDatabase()
{
    // 获取工序大热重样本数据
    m_processTgBigSamples = m_sampleDao.getSamplesByDataType("工序大热重");
    
    // 按项目分组
    QMap<QString, QTreeWidgetItem*> projectItems;
    QMap<QString, QMap<QString, QTreeWidgetItem*>> batchItems;
    
    for (const auto &sample : m_processTgBigSamples) {
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
        m_suppressItemChanged = true; // 初始化为未勾选时不触发变更事件
        sampleItem->setCheckState(0, Qt::Unchecked);
        m_suppressItemChanged = false;
    }
}

void ProcessTgBigDataProcessDialog::loadNavigatorDataFromMainNavigator()
{
    // 获取主界面导航树的工序数据根节点
    QTreeWidgetItem* processDataRoot = m_mainNavigator->getProcessDataRoot();
    if (!processDataRoot) {
        DEBUG_LOG << "ProcessTgBigDataProcessDialog: 无法获取主界面导航树工序数据根节点";
        return;
    }
    
    // 按项目分组
    QMap<QString, QTreeWidgetItem*> projectItems;
    QMap<QString, QMap<QString, QTreeWidgetItem*>> batchItems;
    
    // 遍历主界面导航树，筛选工序大热重数据（仅复制主导航“已选中”的样本）
    for (int i = 0; i < processDataRoot->childCount(); ++i) {
        QTreeWidgetItem* modelItem = processDataRoot->child(i);
        
        // 遍历实验节点
        for (int j = 0; j < modelItem->childCount(); ++j) {
            QTreeWidgetItem* expItem = modelItem->child(j);
            
            // 遍历短代码节点
            for (int k = 0; k < expItem->childCount(); ++k) {
                QTreeWidgetItem* shortCodeItem = expItem->child(k);
                
                // 遍历数据类型节点
                for (int l = 0; l < shortCodeItem->childCount(); ++l) {
                    QTreeWidgetItem* dataTypeItem = shortCodeItem->child(l);
                    
                    // 检查是否为工序大热重数据类型
                    QVariant nodeData = dataTypeItem->data(0, Qt::UserRole);
                    if (nodeData.canConvert<NavigatorNodeInfo>()) {
                        NavigatorNodeInfo nodeInfo = nodeData.value<NavigatorNodeInfo>();
                        
                        if (nodeInfo.type == NavigatorNodeInfo::DataType && 
                             nodeInfo.dataType == "工序大热重") {
                            
                            // 遍历样本节点
                            for (int m = 0; m < dataTypeItem->childCount(); ++m) {
                                QTreeWidgetItem* sampleItem = dataTypeItem->child(m);
                                
                                QVariant sampleData = sampleItem->data(0, Qt::UserRole);
                                if (sampleData.canConvert<NavigatorNodeInfo>()) {
                                    NavigatorNodeInfo sampleInfo = sampleData.value<NavigatorNodeInfo>();
                                    
                                    if (sampleInfo.type == NavigatorNodeInfo::Sample) {
                                        // 修改：不再只复制主导航“已选中”的样本，全部复制并设为可勾选
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
                                            
                                            // 设置批次节点的数据，用于在右键菜单中识别
                                            NavigatorNodeInfo batchInfo;
                                            batchInfo.type = NavigatorNodeInfo::Batch;
                                            batchInfo.projectName = projectName;
                                            batchInfo.batchCode = batchCode;
                                            batchItem->setData(0, Qt::UserRole, QVariant::fromValue(batchInfo));
                                            
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
                                        // 添加可勾选复选框，仅影响本界面曲线可见性
                                        newSampleItem->setFlags(newSampleItem->flags() | Qt::ItemIsUserCheckable);
                                        // 默认未勾选；若主导航样本已选中，则设为勾选
                                        m_suppressItemChanged = true; // 初始化复选框状态时避免触发 onItemChanged
                                        newSampleItem->setCheckState(0, Qt::Unchecked);
                                        if (SampleSelectionManager::instance()->isSelected(sampleInfo.id)) {
                                            newSampleItem->setCheckState(0, Qt::Checked);
                                        }
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

void ProcessTgBigDataProcessDialog::loadSampleCurve(int sampleId)
{
    try {
        DEBUG_LOG << "ProcessTgBigDataProcessDialog::loadSampleCurve - Loading curve for sampleId:" << sampleId;
        
        // 不再单独加载一个样本的曲线，而是绘制所有选中的样本曲线
        drawSelectedSampleCurves();
        return;
        
    } catch (const std::exception& e) {
        DEBUG_LOG << "ProcessTgBigDataProcessDialog::loadSampleCurve - 加载样本曲线失败:" << e.what();
        QMessageBox::warning(this, tr("警告"), tr("加载样本曲线失败: %1").arg(e.what()));
    }
}

void ProcessTgBigDataProcessDialog::onLeftItemClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column)
    
    if (!item) return;
    
    // updateRightPanel(item);
}

void ProcessTgBigDataProcessDialog::onSampleItemClicked(QTreeWidgetItem *item, int column)
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
        
        // 获取样本全称
        QString sampleFullName = item->data(0, Qt::UserRole + 1).toString();
        if (sampleFullName.isEmpty()) {
            try {
                QVariantMap sampleInfo = m_sampleDao.getSampleById(sampleId);
                sampleFullName = sampleInfo.value("sample_name", QString("样本 %1").arg(sampleId)).toString();
            } catch (const std::exception& e) {
                DEBUG_LOG << "Error getting sample info:" << e.what();
                sampleFullName = QString("样本 %1").arg(sampleId);
            }
        }
        
        // 设置复选框为选中状态
        item->setCheckState(0, Qt::Checked);
        
        // 加载样本曲线
        loadSampleCurve(sampleId);
        
        // 确保导航树标题显示为"工序大热重 - 样本导航"
        m_leftNavigator->setHeaderLabel(tr("工序大热重 - 样本导航"));
        
        DEBUG_LOG << "工序大热重样本已选中:" << sampleId << sampleFullName;
    }
}



void ProcessTgBigDataProcessDialog::onCancelButtonClicked()
{
    // 在MDI模式下，close()会关闭MDI子窗口
    close();
}


 void ProcessTgBigDataProcessDialog::onProcessAndPlotButtonClicked()
 {
    DEBUG_LOG << "ProcessTgBigDataProcessDialog::onProcessAndPlotButtonClicked - Processing and plotting samples...";
    m_currentParams = m_paramDialog->getParameters();

    DEBUG_LOG << "ProcessTgBigDataProcessDialog::onProcessAndPlotButtonClicked - Current parameters:" << m_currentParams.toString();
    onParametersApplied( m_currentParams );
    DEBUG_LOG << "ProcessTgBigDataProcessDialog::onProcessAndPlotButtonClicked - Parameters applied";
 }

void ProcessTgBigDataProcessDialog::onParameterSettingsClicked()
{


    // // 1. 创建对话框实例
    // //    m_currentParams 是 Workbench 的一个 ProcessingParameters 成员变量
    // ProcessTgBigParameterSettingsDialog *dialog = new ProcessTgBigParameterSettingsDialog(m_currentParams, this);

    // // 2. 【关键】连接对话框的 `parametersApplied` 信号到 Workbench 的槽函数
    // connect(dialog, &ProcessTgBigParameterSettingsDialog::parametersApplied, this, &ProcessTgBigDataProcessDialog::onParametersApplied);

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
void ProcessTgBigDataProcessDialog::onParametersApplied(const ProcessingParameters &newParams)
{
    DEBUG_LOG << "ProcessTgBigDataProcessDialog::onParametersApplied - Processing parameters:" << newParams.toString();
    // 1. 更新 Workbench 内部缓存的参数
    m_currentParams = newParams;
    // 同步右侧快捷勾选框的状态
    if (m_showMeanCurveCheckRight) m_showMeanCurveCheckRight->setChecked(m_currentParams.showMeanCurve);
    if (m_plotInterpolationCheckRight) m_plotInterpolationCheckRight->setChecked(m_currentParams.plotInterpolation);
    if (m_showRawOverlayCheckRight) m_showRawOverlayCheckRight->setChecked(m_currentParams.showRawOverlay);
    if (m_showBadPointsCheckRight) m_showBadPointsCheckRight->setChecked(m_currentParams.showBadPoints);

    DEBUG_LOG << "ProcessTgBigDataProcessDialog::onParametersApplied - Current parameters:" << m_currentParams.toString();

    // 2. 触发重新计算和绘图的流程
    recalculateAndUpdatePlot();
}




void ProcessTgBigDataProcessDialog::recalculateAndUpdatePlot()
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

    DEBUG_LOG << "ProcessTgBigDataProcessDialog::recalculateAndUpdatePlot() - Processing samples:"
              << idStrList.join(", ");

    // --- 3. 异步调用 Service 层 (使用新数据结构 BatchGroupData) ---
    QFuture<BatchGroupData> future = QtConcurrent::run(
        m_processingService,
        &DataProcessingService::runProcessTgBigPipelineForMultiple,
        sampleIds,
        m_currentParams
    );

    DEBUG_LOG << "ProcessTgBigDataProcessDialog::recalculateAndUpdatePlot() - Future created";

    // --- 4. 监控后台任务 ---
    QFutureWatcher<BatchGroupData>* watcher = new QFutureWatcher<BatchGroupData>(this);
    connect(watcher, &QFutureWatcher<BatchGroupData>::finished,
            this, &ProcessTgBigDataProcessDialog::onCalculationFinished);
    watcher->setFuture(future);

    DEBUG_LOG << "ProcessTgBigDataProcessDialog::recalculateAndUpdatePlot() - Watcher created";
}


void ProcessTgBigDataProcessDialog::onCalculationFinished()
{
    DEBUG_LOG << "ProcessTgBigDataProcessDialog::onCalculationFinished()";

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
    updatePlotQZH(); 

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

    DEBUG_LOG << "ProcessTgBigDataProcessDialog::onCalculationFinished() - Finished";
}



// 确保包含了 ChartView.h 和 core/entities/Curve.h
#include "gui/views/ChartView.h"
#include "core/entities/Curve.h"

// ... (其他函数的实现)




// ...


void ProcessTgBigDataProcessDialog::updatePlot()
{
    updatePlotQZH();//测试使用
    return;

    // --- 0. 安全检查 ---
    if (m_stageDataCache.isEmpty()) {
        qWarning() << "updatePlot called with empty data cache. Clearing chart.";
        // 清空图表
        if (m_chartView1) m_chartView1->clearGraphs();
        return;
    }

    printBatchGroupData(m_stageDataCache);

    //根据索引计算颜色
    int colorIndex = 0;



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


    // --- 2. 更新【裁剪】图表（单一绘图区域） ---
    if (m_chartView1) {
        m_chartView1->clearGraphs();
        m_chartView1->setLabels(tr(""), tr("重量"));
        m_chartView1->setPlotTitle("裁剪数据");
    }



    QElapsedTimer timer;  //  先声明
    timer.restart();

    DEBUG_LOG << "Updating cleaned curves...";
    for (auto groupIt = m_stageDataCache.constBegin(); groupIt != m_stageDataCache.constEnd(); ++groupIt) {

        DEBUG_LOG << "工序大热重基线校准数据后绘图每次循环用时：" << timer.elapsed() << "ms";
    timer.restart();

        const QString &groupKey = groupIt.key();
        const SampleGroup &group = groupIt.value();

        // DEBUG_LOG << "Processing group:" << groupKey
        //         << " sample count:" << group.sampleDatas.size();

        for (const auto &sample : group.sampleDatas) {
            int sampleId = sample.sampleId;
            DEBUG_LOG << "  Sample ID:" << sampleId
                    << " stage count:" << sample.stages.size();



            // 在 sample.stages 中查找名字是 "cleaned" 的阶段
            for (const auto &stage : sample.stages) {
                // DEBUG_LOG << "    Stage name:" << stage.stageName;
                if (stage.stageName == StageName::BaselineCorrection && stage.curve) {

                    DEBUG_LOG << "工序大热重基线校准数据后绘图每次循环用时：" << timer.elapsed() << "ms";
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

                    if (m_chartView1) m_chartView1->addCurve(curve);
                    

                    // DEBUG_LOG << "    Added cleaned curve:" << legendName;

                    DEBUG_LOG << "工序大热重基线校准数据后绘图每次循环用时clip：" << timer.elapsed() << "ms";
                    timer.restart();
                }
            }
        }

        // DEBUG_LOG << "大热重裁剪数据后绘图每次循环用时：" << timer.elapsed() << "ms";
    timer.restart();

    }

    // DEBUG_LOG << "大热重裁剪数据处理用时：" << timer.elapsed() << "ms";
    timer.restart();

    if (m_chartView1) {
        m_chartView1->setLegendVisible(false);// 耗时会累加，一定要放在循环外面
        m_chartView1->replot();
    }

    // DEBUG_LOG << "大热重裁剪数据后绘图用时：" << timer.elapsed() << "ms";
    timer.restart();

    // // --- 2. 更新【归一化】图表 (m_chartView3) ---
    // m_chartView3->clearGraphs();
    // m_chartView3->setLabels(tr(""), tr("重量"));
    // m_chartView3->setPlotTitle("归一化数据");

    // for (auto groupIt = m_stageDataCache.constBegin(); groupIt != m_stageDataCache.constEnd(); ++groupIt) {
    //     const QString &groupKey = groupIt.key();
    //     const SampleGroup &group = groupIt.value();

    //     DEBUG_LOG << "Processing group:" << groupKey
    //             << " sample count:" << group.sampleDatas.size();

    //     for (const auto &sample : group.sampleDatas) {
    //         int sampleId = sample.sampleId;
    //         DEBUG_LOG << "  Sample ID:" << sampleId
    //                 << " stage count:" << sample.stages.size();

    //         // 在 sample.stages 中查找名字是 "cleaned" 的阶段
    //         for (const auto &stage : sample.stages) {
    //             DEBUG_LOG << "    Stage name:" << stage.stageName;
    //             if (stage.stageName == StageName::Normalize && stage.curve) {
    //                 QSharedPointer<Curve> curve = stage.curve;

    //                 curve->setColor(ColorUtils::setCurveColor(colorIndex++));
    //                 // curve->setName(legendName);
    //                 DEBUG_LOG << "    Adding normalized curve:" << curve->name();

    //                 m_chartView3->addCurve(curve);
    //                 m_chartView3->setLegendVisible(false);

    //                 // DEBUG_LOG << "    Added normalized curve:" << legendName;
    //             }
    //         }
    //     }
    // }

    // m_chartView3->replot();

    // DEBUG_LOG << "大热重归一化数据后绘图用时：" << timer.elapsed() << "ms";
    // timer.restart();

    // // --- 4. 更新【平滑】图表 (m_chartView4) ---
    // m_chartView4->clearGraphs();
    // m_chartView4->setLabels(tr(""), tr("重量"));
    // m_chartView4->setPlotTitle("平滑数据");

    // for (auto groupIt = m_stageDataCache.constBegin(); groupIt != m_stageDataCache.constEnd(); ++groupIt) {
    //     const QString &groupKey = groupIt.key();
    //     const SampleGroup &group = groupIt.value();

    //     DEBUG_LOG << "Processing group:" << groupKey
    //             << " sample count:" << group.sampleDatas.size();

    //     for (const auto &sample : group.sampleDatas) {
    //         int sampleId = sample.sampleId;
    //         DEBUG_LOG << "  Sample ID:" << sampleId
    //                 << " stage count:" << sample.stages.size();

    //         // 在 sample.stages 中查找名字是 "cleaned" 的阶段
    //         for (const auto &stage : sample.stages) {
    //             DEBUG_LOG << "    Stage name:" << stage.stageName;
    //             if (stage.stageName == StageName::Smooth && stage.curve) {
    //                 QSharedPointer<Curve> curve = stage.curve;

    //                 curve->setColor(ColorUtils::setCurveColor(colorIndex++));
    //                 // curve->setName(legendName);
    //                 DEBUG_LOG << "    Adding smoothed curve:" << curve->name();

    //                 m_chartView4->addCurve(curve);
    //                 m_chartView4->setLegendVisible(false);

    //                 // DEBUG_LOG << "    Added normalized curve:" << legendName;
    //             }
    //         }
    //     }
    // }
    // m_chartView4->replot();

    // DEBUG_LOG << "大热重平滑数据后绘图用时：" << timer.elapsed() << "ms";
    // timer.restart();

    // // --- 5. 更新【微分】图表 (m_chartView5) ---
    // m_chartView5->clearGraphs();
    // m_chartView5->setLabels(tr(""), tr("重量"));
    // m_chartView5->setPlotTitle("微分数据");

    // for (auto groupIt = m_stageDataCache.constBegin(); groupIt != m_stageDataCache.constEnd(); ++groupIt) {
    //     const QString &groupKey = groupIt.key();
    //     const SampleGroup &group = groupIt.value();

    //     DEBUG_LOG << "Processing group:" << groupKey
    //             << " sample count:" << group.sampleDatas.size();

    //     for (const auto &sample : group.sampleDatas) {
    //         int sampleId = sample.sampleId;
    //         DEBUG_LOG << "  Sample ID:" << sampleId
    //                 << " stage count:" << sample.stages.size();

    //         // 在 sample.stages 中查找名字是 "cleaned" 的阶段
    //         for (const auto &stage : sample.stages) {
    //             DEBUG_LOG << "    Stage name:" << stage.stageName;
    //             if (stage.stageName == StageName::Derivative && stage.curve) {
    //                 QSharedPointer<Curve> curve = stage.curve;

    //                 curve->setColor(ColorUtils::setCurveColor(colorIndex++));
    //                 // curve->setName(legendName);
    //                 DEBUG_LOG << "    Adding derivative curve:" << curve->name();

    //                 m_chartView5->addCurve(curve);
    //                 m_chartView5->setLegendVisible(false);

    //                 // DEBUG_LOG << "    Added normalized curve:" << legendName;
    //             }
    //         }
    //     }
    // }
    // m_chartView5->replot();

    // DEBUG_LOG << "大热重微分数据后绘图用时：" << timer.elapsed() << "ms";
    timer.restart();



    updateLegendPanel();
}

// 新的绘图更新函数，按 Q/Z/H 三段与右侧阶段选择进行绘图
void ProcessTgBigDataProcessDialog::updatePlotQZH()
{
    // 空数据则清空图
    if (m_stageDataCache.isEmpty()) {
        if (m_chartView1) m_chartView1->clearGraphs();
        return;
    }

    // 若当前没有任何可见样本，则清空绘图并返回
    if (m_visibleSamples.isEmpty()) {
        if (m_chartView1) {
            m_chartView1->clearGraphs();
            m_chartView1->replot();
        }
        updateLegendPanel();
        return;
    }

    // 收集右侧“阶段数据对比显示”勾选的所有阶段
    QVector<StageName> drawStages;
    if (m_stageRawCheck && m_stageRawCheck->isChecked())        drawStages.append(StageName::RawData);
    if (m_stageBadRepairCheck && m_stageBadRepairCheck->isChecked()) drawStages.append(StageName::BadPointRepair);
    if (m_stageClipCheck && m_stageClipCheck->isChecked())      drawStages.append(StageName::Clip);
    if (m_stageNormalizeCheck && m_stageNormalizeCheck->isChecked()) drawStages.append(StageName::Normalize);
    if (m_stageSmoothCheck && m_stageSmoothCheck->isChecked())  drawStages.append(StageName::Smooth);
    if (m_stageDerivativeCheck && m_stageDerivativeCheck->isChecked()) drawStages.append(StageName::Derivative);

    // 若没有勾选任何阶段，则退化为使用右侧下拉框的单阶段选择逻辑
    StageName xBaseStage = StageName::RawData; // 用于统一X轴的基准阶段
    if (drawStages.isEmpty()) {
        StageName selectedStage = StageName::RawData;
        if (m_stageCombo && m_stageCombo->currentIndex() >= 0) {
            selectedStage = static_cast<StageName>(m_stageCombo->currentData().toInt());
        }
        drawStages.append(selectedStage);
        xBaseStage = selectedStage;
    } else {
        // 勾选了多个阶段时，优先使用 RawData 作为X轴基准；若未勾选 RawData，则使用第一个勾选阶段
        if (drawStages.contains(StageName::RawData)) {
            xBaseStage = StageName::RawData;
        } else {
            xBaseStage = drawStages.first();
        }
    }

    // 根据是否多阶段勾选设置图表标题
    QString stageTitle;
    if (drawStages.size() == 1) {
        stageTitle = stageNameToString(drawStages.first());
    } else {
        stageTitle = QStringLiteral("多阶段对比");
    }

    if (m_chartView1) {
        m_chartView1->clearGraphs();
        m_chartView1->setLabels(tr(""), tr("重量"));
        m_chartView1->setPlotTitle(stageTitle);
    }

    // 全局颜色计数器，确保所有样本的基础颜色连续且互不相同
    int colorIndex = 0;
    // 样本ID到基础颜色的映射，用于保证同一样本各阶段颜色一致
    QHash<int, QColor> sampleColorCache;

    // 预先准备样本标识查询器，用于生成完整曲线名称（project-batch-short-parallel）
        SingleTobaccoSampleDAO legendDao;

        for (auto groupIt = m_stageDataCache.constBegin(); groupIt != m_stageDataCache.constEnd(); ++groupIt) {
        const SampleGroup &group = groupIt.value();
        // 不再依据 Q/Z/H 段路由到不同图，统一绘制到单一图表
        ChartView* targetView = m_chartView1;

        // 统一X轴选择策略——优先使用组内“最佳样本”的所选阶段；否则退化为首个可用样本的所选阶段；最后退化为原始阶段
        QVector<double> xCommon;
        bool xCommonReady = false;
        auto extractX = [&](const QSharedPointer<Curve>& c){
            if (c.isNull() || c->pointCount() == 0) return;
            const QVector<QPointF>& pts = c->data();
            xCommon.reserve(pts.size());
            xCommon.clear();
            for (const auto& p : pts) xCommon.append(p.x());
            xCommonReady = true;
        };
        auto findStageCurve = [&](const SampleDataFlexible& s, StageName sn)->QSharedPointer<Curve>{
            for (const auto& st : s.stages) {
                if (st.stageName == sn && st.useForPlot && st.curve) return st.curve;
            }
            return {};
        };
        // 统一X轴选择策略：优先使用组内“最佳样本”的基准阶段；否则退化为首个可用样本的基准阶段；最后退化为原始阶段/任一勾选阶段
        // 1) 尝试最佳样本
        for (const auto& s : group.sampleDatas) {
            if (!s.bestInGroup) continue;
            auto c = findStageCurve(s, xBaseStage);
            if (c.isNull() && xBaseStage != StageName::RawData) {
                c = findStageCurve(s, StageName::RawData);
            }
            // 若基准阶段也不可用，则在已勾选阶段中寻找任意一条可用曲线作为X轴基准
            if (c.isNull()) {
                for (StageName stName : drawStages) {
                    c = findStageCurve(s, stName);
                    if (!c.isNull()) break;
                }
            }
            if (!c.isNull()) { extractX(c); break; }
        }
        // 2) 首个可用样本
        if (!xCommonReady) {
            for (const auto& s : group.sampleDatas) {
                auto c = findStageCurve(s, xBaseStage);
                if (c.isNull() && xBaseStage != StageName::RawData) {
                    c = findStageCurve(s, StageName::RawData);
                }
                if (c.isNull()) {
                    for (StageName stName : drawStages) {
                        c = findStageCurve(s, stName);
                        if (!c.isNull()) break;
                    }
                }
                if (!c.isNull()) { extractX(c); break; }
            }
        }
        // 线性插值到统一X
        auto interpToCommon = [&](const QVector<QPointF>& srcPts){
            QVector<double> yOut;
            if (srcPts.isEmpty() || !xCommonReady) return yOut;
            yOut.reserve(xCommon.size());
            QVector<double> sx, sy; sx.reserve(srcPts.size()); sy.reserve(srcPts.size());
            for (const auto& p : srcPts) { sx.append(p.x()); sy.append(p.y()); }
            int j = 0;
            for (double xr : xCommon) {
                while (j + 1 < sx.size() && sx[j + 1] < xr) j++;
                if (j >= sx.size() - 1) { yOut.append(sy.last()); continue; }
                double x1 = sx[j], y1 = sy[j];
                double x2 = sx[j + 1], y2 = sy[j + 1];
                double t = (qFuzzyCompare(x1, x2) ? 0.0 : (xr - x1) / (x2 - x1));
                if (t < 0.0) t = 0.0; if (t > 1.0) t = 1.0;
                yOut.append(y1 * (1 - t) + y2 * t);
            }
            return yOut;
        };
        // 均值累加器（总是基于统一X进行累加，确保均值意义稳定）
        QVector<double> meanAcc;
        int meanCount = 0;

        for (const auto &sample : group.sampleDatas) {
            // 仅绘制当前“可见样本”集合中的样本
            if (!m_visibleSamples.isEmpty() && !m_visibleSamples.contains(sample.sampleId)) {
                continue;
            }
            // 为当前样本生成完整图例名（用于各阶段曲线展示与悬停提示）
            SampleIdentifier sid = legendDao.getSampleIdentifierById(sample.sampleId);
            QString legendName = QString("%1-%2-%3-%4")
                                    .arg(sid.projectName)
                                    .arg(sid.batchCode)
                                    .arg(sid.shortCode)
                                    .arg(sid.parallelNo);
            // 不再按 Q/Z/H 段拆分，所有样本都绘制到同一图表
            ChartView* sampleView = m_chartView1;
            bool derivativeAdded = false;
            bool rawOverlayAdded = false; // 控制原始叠加曲线每个样本仅绘制一次

            // 辅助函数：判断某个阶段是否在当前需要绘制的阶段集合中
            auto stageSelected = [&](StageName sn) -> bool {
                for (StageName sName : drawStages) {
                    if (sName == sn) return true;
                }
                return false;
            };

            for (const auto &stage : sample.stages) {
                if (stage.stageName == StageName::Derivative && derivativeAdded) continue;
                // 原始阶段不受 useForPlot 影响，保证切换到“原始数据”时总能绘制
                bool allowDraw = stageSelected(stage.stageName) &&
                                 !stage.curve.isNull() &&
                                 ((stage.stageName == StageName::RawData) || stage.useForPlot);
                if (!allowDraw) continue;

                // 获取当前样本的基础颜色（与原始曲线相同），同一样本各阶段共用
                QColor baseColor;
                if (sampleColorCache.contains(sample.sampleId)) {
                    baseColor = sampleColorCache.value(sample.sampleId);
                } else {
                    baseColor = ColorUtils::setCurveColor(colorIndex++);
                    sampleColorCache.insert(sample.sampleId, baseColor);
                }

                QString displayName = legendName;
                QString stageText = stageNameToString(stage.stageName);
                if (!stageText.isEmpty()) {
                    displayName = legendName + QStringLiteral("-") + stageText;
                }

                QSharedPointer<Curve> curveToDraw = stage.curve;
                bool isBadRepairStage = (stage.stageName == StageName::BadPointRepair);
                // 坏点修复阶段只显示修复部分，不做整段插值重绘
                if (!isBadRepairStage && m_currentParams.plotInterpolation && xCommonReady) {
                    const QVector<QPointF>& pts = stage.curve->data();
                    QVector<double> yInterp = interpToCommon(pts);
                    QSharedPointer<Curve> resampled = QSharedPointer<Curve>::create(xCommon, yInterp, displayName);
                    resampled->setSampleId(stage.curve->sampleId());
                    curveToDraw = resampled;
                }

                if (isBadRepairStage && sampleView) {
                    auto bxVar = stage.metrics.value("bad_points_x");
                    if (bxVar.isValid()) {
                        QVector<double> bx = bxVar.value<QVector<double>>();
                        const QVector<QPointF>& pts = stage.curve->data();
                        QVector<double> xRepaired, yRepaired;
                        xRepaired.reserve(bx.size());
                        yRepaired.reserve(bx.size());
                        int k = 0;
                        for (const auto& p : pts) {
                            while (k < bx.size() && bx[k] < p.x()) k++;
                            if (k < bx.size() && qFuzzyCompare(bx[k] + 1.0, p.x() + 1.0)) {
                                xRepaired.append(p.x());
                                yRepaired.append(p.y());
                                k++;
                            }
                        }
                        if (!xRepaired.isEmpty()) {
                            sampleView->addScatterPoints(xRepaired, yRepaired, displayName, QColor(0, 0, 0), 5);
                        }
                    }
                } else {
                    // 非坏点修复阶段按阶段设置线型与样本颜色
                    curveToDraw->setName(displayName);
                    Qt::PenStyle lineStyle = Qt::SolidLine;
                    int lineWidth = 2;
                    switch (stage.stageName) {
                    case StageName::Clip:             // 裁剪：中虚线
                        lineStyle = Qt::DashLine;
                        lineWidth = 2;
                        break;
                    case StageName::Normalize:        // 归一化：点划线
                        lineStyle = Qt::DashDotLine;
                        lineWidth = 2;
                        break;
                    case StageName::Smooth:           // 平滑：细实线
                        lineStyle = Qt::SolidLine;
                        lineWidth = 1;
                        break;
                    case StageName::Derivative:       // 微分：细实线
                        lineStyle = Qt::SolidLine;
                        lineWidth = 1;
                        break;
                    case StageName::RawData:          // 原始数据：正常实线
                    default:
                        lineStyle = Qt::SolidLine;
                        lineWidth = 2;
                        break;
                    }
                    curveToDraw->setColor(baseColor);
                    curveToDraw->setLineStyle(lineStyle);
                    curveToDraw->setLineWidth(lineWidth);
                    if (sampleView) sampleView->addCurve(curveToDraw);
                }
                if (stage.stageName == StageName::Derivative) derivativeAdded = true;

                // 均值累加始终在统一X上进行（即使未选择“绘图时插值”，也进行内部对齐以保证均值可靠）
                if (m_currentParams.showMeanCurve && xCommonReady) {
                    const QVector<QPointF>& ptsForMean = stage.curve->data();
                    QVector<double> yAligned = interpToCommon(ptsForMean);
                    if (!yAligned.isEmpty()) {
                        if (meanAcc.isEmpty()) meanAcc = QVector<double>(yAligned.size(), 0.0);
                        int L = qMin(meanAcc.size(), yAligned.size());
                        for (int i = 0; i < L; ++i) meanAcc[i] += yAligned[i];
                        meanCount += 1;
                    }
                }

                // 原始（预修复）叠加（同样尊重显示插值开关）
                if (m_currentParams.showRawOverlay && !rawOverlayAdded) {
                    for (const auto& rawStage : sample.stages) {
                        if (rawStage.stageName == StageName::RawData && rawStage.curve) {
                            QSharedPointer<Curve> rawCurve = rawStage.curve;
                            // 原始叠加曲线名称在完整图例后附加标识；不参与悬停映射（sampleId 置为 -1）
                            QString overlayName = legendName + QStringLiteral("（原始叠加）");
                            if (m_currentParams.plotInterpolation && xCommonReady) {
                                const QVector<QPointF>& pts = rawCurve->data();
                                QVector<double> yInterp = interpToCommon(pts);
                                QSharedPointer<Curve> resampled = QSharedPointer<Curve>::create(xCommon, yInterp, overlayName);
                                resampled->setSampleId(-1);
                                rawCurve = resampled;
                            }
                            rawCurve->setName(overlayName);
                            rawCurve->setColor(QColor(120,120,120));
                            rawCurve->setLineStyle(Qt::DashLine);
                            rawCurve->setSampleId(-1);
                            if (sampleView) sampleView->addCurve(rawCurve);
                            rawOverlayAdded = true;
                            break;
                        }
                    }
                }
                // 坏点，仅在坏点修复阶段绘制
                if (m_currentParams.showBadPoints && stage.stageName == StageName::BadPointRepair && sampleView) {
                    auto bxVar = stage.metrics.value("bad_points_x");
                    auto byVar = stage.metrics.value("bad_points_y");
                    if (bxVar.isValid() && byVar.isValid()) {
                        QVector<double> bx = bxVar.value<QVector<double>>();
                        QVector<double> by = byVar.value<QVector<double>>();
                        if (!bx.isEmpty() && bx.size() == by.size()) {
                            sampleView->addScatterPoints(bx, by, QStringLiteral("坏点"), QColor(200, 30, 30), 6);
                        }
                    }
                }
            }
        }
        // 绘制均值曲线（统一X + 至少一条曲线）
        if (m_currentParams.showMeanCurve && xCommonReady && meanCount > 0 && targetView) {
            QVector<double> yMean; yMean.reserve(meanAcc.size());
            for (int i = 0; i < meanAcc.size(); ++i) yMean.append(meanAcc[i] / double(meanCount));
            QSharedPointer<Curve> meanCurve = QSharedPointer<Curve>::create(xCommon, yMean, QStringLiteral("均值曲线"));
            meanCurve->setColor(QColor(0,0,0));
            QPen pen = meanCurve->pen();
            pen.setWidth(3);
            meanCurve->setLineWidth(3);
            // 均值曲线不参与样本悬停映射
            meanCurve->setSampleId(-1);
            targetView->addCurve(meanCurve);
        }
    }

    if (m_chartView1) { m_chartView1->setLegendVisible(false); m_chartView1->replot(); }

    updateLegendPanel();
}

void ProcessTgBigDataProcessDialog::updateLegendPanel()
{
    if (!m_legendPanel) return;

        // 提前批量获取所有 SampleIdentifier
    // QMap<int, SampleIdentifier> sampleIdMap;
    SingleTobaccoSampleDAO sampleDao;
    // 提前批量获取所有 SampleIdentifier
    for (int sampleId : m_selectedSamples.keys()) {
        sampleIdMap[sampleId] = sampleDao.getSampleIdentifierById(sampleId);
    }

    // 清空原来的图例
    QLayoutItem* item;
    while ((item = m_legendLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    int colorIndex = 0;
    for (int sampleId : m_selectedSamples.keys()) {
        SampleIdentifier sid = sampleIdMap.value(sampleId); // 之前缓存的 SampleIdentifier
        QString legendText = QString("%1-%2-%3-%4")
                                .arg(sid.projectName)
                                .arg(sid.batchCode)
                                .arg(sid.shortCode)
                                .arg(sid.parallelNo);

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




void ProcessTgBigDataProcessDialog::onParameterChanged()
{
    // 更新右侧面板显示
    QTreeWidgetItem* currentItem = m_leftNavigator->currentItem();
    if (currentItem) {
        // updateRightPanel(currentItem);
    }
}



void ProcessTgBigDataProcessDialog::onItemChanged(QTreeWidgetItem *item, int column)
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
    QString sampleFullName = item->data(0, Qt::UserRole + 1).toString();
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
                    QString sampleType = "工序大热重"; // 默认为工序大热重类型
                    
                    // 尝试从样本名称中提取更详细的类型信息
                    if (sampleFullName.contains("大热重")) {
                        sampleType = "大热重";
                    } else if (sampleFullName.contains("小热重")) {
                        sampleType = "小热重";
                    } else if (sampleFullName.contains("色谱")) {
                        sampleType = "色谱";
                    }else if(sampleFullName.contains("工序大热重")){
                        sampleType = "工序大热重";
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
QTreeWidgetItem* ProcessTgBigDataProcessDialog::findSampleItemById(int sampleId) const
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

// 处理主导航样本选中状态变化，仅对 dataType=="工序大热重" 生效，动态增删导航树节点与曲线
void ProcessTgBigDataProcessDialog::onMainNavigatorSampleSelectionChanged(int sampleId, const QString& sampleName, const QString& dataType, bool selected)
{
    try {
        if (dataType != QStringLiteral("工序大热重")) {
            return;
        }

        if (selected) {
            QTreeWidgetItem* existing = findSampleItemById(sampleId);
            if (existing) {
                m_suppressItemChanged = true;
                existing->setCheckState(0, Qt::Checked);
                m_suppressItemChanged = false;
                loadSampleCurve(sampleId);
                return;
            }

            QVariantMap info = m_sampleDao.getSampleById(sampleId);
            QString projectName = info.value("project_name").toString();
            QString batchCode   = info.value("batch_code").toString();
            QString shortCode   = info.value("short_code").toString();
            int parallelNo      = info.value("parallel_no").toInt();

            QTreeWidgetItem* projectItem = nullptr;
            for (int i = 0; i < m_leftNavigator->topLevelItemCount(); ++i) {
                QTreeWidgetItem* item = m_leftNavigator->topLevelItem(i);
                if (item && item->text(0) == projectName) { projectItem = item; break; }
            }
            if (!projectItem) {
                projectItem = new QTreeWidgetItem(m_leftNavigator, QStringList() << projectName);
            }

            QTreeWidgetItem* batchItem = nullptr;
            for (int j = 0; j < projectItem->childCount(); ++j) {
                QTreeWidgetItem* item = projectItem->child(j);
                if (item && item->text(0) == batchCode) { batchItem = item; break; }
            }
            if (!batchItem) {
                batchItem = new QTreeWidgetItem(projectItem, QStringList() << batchCode);
            }

            // 创建样本节点（使用完整样本名称）
            QString sampleFullName = info.value("sample_name").toString();
            if (sampleFullName.trimmed().isEmpty()) sampleFullName = sampleName;
            QTreeWidgetItem* sampleItem = new QTreeWidgetItem(batchItem, QStringList() << sampleFullName);
            
            sampleItem->setFlags(sampleItem->flags() | Qt::ItemIsUserCheckable);
            sampleItem->setData(0, Qt::UserRole, sampleId);
            m_suppressItemChanged = true;
            sampleItem->setCheckState(0, Qt::Checked);
            m_suppressItemChanged = false;

            // 同步选中样本集合与左侧“选中样本”列表（使用完整名称）
            m_selectedSamples.insert(sampleId, sampleFullName);
            updateSelectedSamplesList();

            loadSampleCurve(sampleId);
        } else {
            QTreeWidgetItem* sampleItem = findSampleItemById(sampleId);
            if (!sampleItem) return;
            removeSampleCurve(sampleId);
            m_suppressItemChanged = true;
            sampleItem->setCheckState(0, Qt::Unchecked);
            m_suppressItemChanged = false;

            m_selectedSamples.remove(sampleId);
            updateSelectedSamplesList();
        }
    } catch (const std::exception& e) {
        ERROR_LOG << "onMainNavigatorSampleSelectionChanged error:" << e.what();
    }
}



// // 启动差异度分析的槽函数
// void ProcessTgBigDataProcessDialog::onStartComparison()
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
// void ProcessTgBigDataProcessDialog::onStartComparison()
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

// void ProcessTgBigDataProcessDialog::onStartComparison()
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

void ProcessTgBigDataProcessDialog::onStartComparison()
{
    DEBUG_LOG << "ProcessTgBigDataProcessDialog::onStartComparison";
    // 差异度计算至少需要两个样本
    if (m_selectedSamples.count() < 2) {
        QMessageBox::warning(this, "提示", "样本数量不足，至少选择两个样本才能进行差异度计算。");
        return;
    }
    // 如尚无处理结果，自动执行算法处理后再进入差异度
    if (m_stageDataCache.isEmpty()) {
        if (m_visibleSamples.isEmpty()) {
            for (int id : m_selectedSamples.keys()) m_visibleSamples.insert(id);
            updateSelectedSamplesList();
        }
        m_autoStartComparisonAfterProcessing = true;
        recalculateAndUpdatePlot();
        return;
    }
    // 检查选中样本是否已有“微分”结果；若缺失则自动处理以生成
    {
        QSet<int> selectedIds = QSet<int>::fromList(m_selectedSamples.keys());
        bool derivativeMissing = false;
        for (auto it = m_stageDataCache.constBegin(); it != m_stageDataCache.constEnd() && !derivativeMissing; ++it) {
            const SampleGroup& group = it.value();
            for (const SampleDataFlexible& sample : group.sampleDatas) {
                if (!selectedIds.contains(sample.sampleId)) continue;
                bool hasDeriv = false;
                for (const StageData& s : sample.stages) {
                    if (s.stageName == StageName::Derivative && !s.curve.isNull()) { hasDeriv = true; break; }
                }
                if (!hasDeriv) { derivativeMissing = true; break; }
            }
        }
        if (derivativeMissing) {
            if (m_visibleSamples.isEmpty()) {
                for (int id : m_selectedSamples.keys()) m_visibleSamples.insert(id);
                updateSelectedSamplesList();
            }
            m_backupParamsForAuto = m_currentParams;
            ProcessingParameters forced = m_currentParams;
            forced.smoothingEnabled = true;
            forced.derivativeEnabled = true;
            if (forced.derivativeMethod.isEmpty()) forced.derivativeMethod = "derivative_sg";
            m_currentParams = forced;
            m_autoEnsureDerivative = true;
            m_autoStartComparisonAfterProcessing = true;
            recalculateAndUpdatePlot();
            return;
        }
    }
    // 直接进入 V2.2.1_origin 的三张表输出界面
    showV221OriginProcessOutputsDialog();
    return;

    // --- 1. 【关键修正】准备用于显示的样本名称列表 ---
    
    // a. 获取所有选中样本的 ID
    QList<int> selectedIds = m_selectedSamples.keys();
    
    // b. 使用 DAO 查询这些 ID 对应的详细标识符
    SingleTobaccoSampleDAO dao; // 临时创建 DAO 实例
    QString error;
    // (理想情况下，DAO 应该有一个批量查询方法，这里简化为循环查询)
    QMap<QString, int> nameToIdMap; // Key: 完整的友好名称, Value: sampleId
    QStringList displayNames;

    for (int id : selectedIds) {
        SampleIdentifier sid = dao.getSampleIdentifierById(id);
        if (sid.projectName.isEmpty()) continue; // 跳过无效查询结果
        
        QString fullName = QString("%1-%2-%3-%4")
                             .arg(sid.projectName)
                             .arg(sid.batchCode)
                             .arg(sid.shortCode)
                             .arg(sid.parallelNo);
        displayNames.append(fullName);
        nameToIdMap[fullName] = id;
    }

    // --- 2. 弹出对话框，让用户选择 ---
    bool ok;
    // 使用我们刚刚构建的、格式完美的 displayNames 列表
    QString selectedFullName = QInputDialog::getItem(this, tr("选择参考样本"), 
                                                     tr("请选择一个样本作为对比基准:"), 
                                                     displayNames, 0, false, &ok);
    if (!ok || selectedFullName.isEmpty()) return;

    // --- 3. 获取用户选择的参考样本的 ID ---
    int referenceSampleId = nameToIdMap.value(selectedFullName, -1);
    if (referenceSampleId == -1) { /* 错误处理 */ return; }

    // --- 4. 【关键】直接发出信号，不再准备数据 ---
    //    我们将数据准备的工作完全交给 MainWindow 和 DifferenceWorkbench
    emit requestNewProcessTgBigDifferenceWorkbench(referenceSampleId, m_stageDataCache, m_currentParams);
    DEBUG_LOG << "0000000";
}

// 显示 V2.2.1_origin 风格的输出对话框（含两张表与导出按钮）
void ProcessTgBigDataProcessDialog::showV221OriginProcessOutputsDialog()
{
    try {
        // ===== 数据准备：与导出函数相同的分组与计算 =====
        QRegularExpression reSuffix(QStringLiteral("-(Q|Z|H)(?:-|$)"), QRegularExpression::CaseInsensitiveOption);
        QMap<QString, QMap<QString, QVector<QSharedPointer<Curve>>>> baseStageCurves;
        for (auto it = m_stageDataCache.constBegin(); it != m_stageDataCache.constEnd(); ++it) {
            const SampleGroup& group = it.value();
            QString shortCodeBase = group.shortCode;
            QRegularExpressionMatch m = reSuffix.match(shortCodeBase);
            QString stageChar = "ALL";
            if (m.hasMatch()) {
                QString cap = m.captured(1);
                if (!cap.isEmpty()) {
                    stageChar = cap.toUpper();
                    QString pattern = QStringLiteral("-(?:") + QRegularExpression::escape(cap) + QStringLiteral(")$");
                    shortCodeBase.replace(QRegularExpression(pattern, QRegularExpression::CaseInsensitiveOption), QString());
                }
            }
            QString baseKey = QString("%1-%2-%3").arg(group.projectName).arg(group.batchCode).arg(shortCodeBase);
            for (const SampleDataFlexible& sample : group.sampleDatas) {
                QSharedPointer<Curve> curve;
                for (const StageData& s : sample.stages) {
                    if (s.stageName == StageName::Derivative && !s.curve.isNull()) { curve = s.curve; break; }
                }
                if (curve.isNull()) {
                    for (const StageData& s : sample.stages) {
                        if (s.stageName == StageName::RawData && !s.curve.isNull()) { curve = s.curve; break; }
                    }
                }
                if (!curve.isNull()) {
                    baseStageCurves[baseKey][stageChar].append(curve);
                }
            }
        }

        struct Summary { QString base; QString stage; int n; double total; double avg; bool best; };
        QVector<Summary> summaryRows;
        for (auto it = baseStageCurves.constBegin(); it != baseStageCurves.constEnd(); ++it) {
            const QString& baseKey = it.key();
            const auto& stageMap = it.value();
            QVector<Summary> vecSumm;
            for (auto jt = stageMap.constBegin(); jt != stageMap.constEnd(); ++jt) {
                const QString& stage = jt.key();
                const QVector<QSharedPointer<Curve>>& curves = jt.value();
                int nGood = curves.size();
                if (nGood < 2) {
                    vecSumm.append({baseKey, stage, nGood, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN()});
                    continue;
                }
                int minLen = std::numeric_limits<int>::max();
                for (const auto& c : curves) minLen = std::min(minLen, c->pointCount());
                if (minLen <= 1 || minLen == std::numeric_limits<int>::max()) {
                    vecSumm.append({baseKey, stage, nGood, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN()});
                    continue;
                }
                QVector<double> valsNR, valsP, valsE;
                for (int i = 0; i < curves.size(); ++i) {
                    for (int j = i + 1; j < curves.size(); ++j) {
                        const QVector<QPointF>& da = curves[i]->data();
                        const QVector<QPointF>& db = curves[j]->data();
                        double mse = 0.0, eucl = 0.0;
                        double meanA = 0.0, meanB = 0.0;
                        for (int k = 0; k < minLen; ++k) {
                            double ya = da[k].y();
                            double yb = db[k].y();
                            double d = ya - yb;
                            mse += d * d;
                            eucl += d * d;
                            meanA += ya; meanB += yb;
                        }
                        mse /= minLen;
                        double rmse = std::sqrt(mse);
                        double euclVal = std::sqrt(eucl);
                        meanA /= minLen; meanB /= minLen;
                        double num = 0.0, denA = 0.0, denB = 0.0;
                        for (int k = 0; k < minLen; ++k) {
                            double da1 = da[k].y() - meanA;
                            double db1 = db[k].y() - meanB;
                            num += da1 * db1;
                            denA += da1 * da1;
                            denB += db1 * db1;
                        }
                        double denom = std::sqrt(denA) * std::sqrt(denB);
                        double p = (denom <= 1e-12) ? 1.0 : (num / denom);
                        if (p > 1.0) p = 1.0; if (p < -1.0) p = -1.0;
                        double minA = std::numeric_limits<double>::infinity();
                        double maxA = -std::numeric_limits<double>::infinity();
                        for (int k = 0; k < minLen; ++k) { double v = da[k].y(); minA = std::min(minA, v); maxA = std::max(maxA, v); }
                        double rangeA = std::max(maxA - minA, 1e-6);
                        double nrmse = rmse / rangeA;
                        valsNR.append(nrmse);
                        valsP.append(p);
                        valsE.append(euclVal);
                    }
                }
                auto vmin = [](const QVector<double>& v){ return v.isEmpty()? 0.0 : *std::min_element(v.begin(), v.end()); };
                auto vmax = [](const QVector<double>& v){ return v.isEmpty()? 1.0 : *std::max_element(v.begin(), v.end()); };
                auto norm01 = [](double v, double vmin0, double vmax0){ if (vmax0 <= vmin0) return 0.0; return (v - vmin0) / (vmax0 - vmin0); };
                double nrMin = vmin(valsNR), nrMax = vmax(valsNR);
                double pMin  = vmin(valsP),  pMax  = vmax(valsP);
                double eMin  = vmin(valsE),  eMax  = vmax(valsE);
                QVector<double> comp;
                comp.reserve(valsNR.size());
                for (int i = 0; i < valsNR.size(); ++i) {
                    double nNR = norm01(valsNR[i], nrMin, nrMax);
                    double nP  = norm01(valsP[i],  pMin,  pMax);
                    double nE  = norm01(valsE[i],  eMin,  eMax);
                    double score = m_currentParams.weightNRMSE * nNR
                                 + m_currentParams.weightEuclidean * nE
                                 + m_currentParams.weightPearson * (1.0 - nP);
                    comp.append(score);
                }
                double total = 0.0, avg = 0.0;
                for (double v : comp) total += v;
                avg = comp.isEmpty()? std::numeric_limits<double>::quiet_NaN() : (total / comp.size());
                double totalAdj = (nGood > 0) ? (total / std::sqrt(nGood)) : total;
                vecSumm.append({baseKey, stage, nGood, totalAdj, avg, false});
            }
            int bestIdx = -1; double bestVal = std::numeric_limits<double>::infinity();
            for (int i = 0; i < vecSumm.size(); ++i) {
                double v = vecSumm[i].total;
                if (!std::isnan(v) && v < bestVal) { bestVal = v; bestIdx = i; }
            }
            for (int i = 0; i < vecSumm.size(); ++i) {
                Summary s = vecSumm[i];
                s.best = (i == bestIdx);
                summaryRows.append(s);
            }
        }

        struct Detail { QString base; QString stage; QString prefix; double nr; double p; double e; double comp; int rank; };
        QVector<Detail> detailRows;
        for (auto it = baseStageCurves.constBegin(); it != baseStageCurves.constEnd(); ++it) {
            const QString& baseKey = it.key();
            const auto& stageMapAll = it.value();
            struct MeanInfo { QVector<double> mean; double refRange; int len; };
            QMap<QString, MeanInfo> stageMeans;
            for (auto jt = stageMapAll.constBegin(); jt != stageMapAll.constEnd(); ++jt) {
                const QString& stage = jt.key();
                const QVector<QSharedPointer<Curve>>& curves = jt.value();
                if (curves.isEmpty()) continue;
                int minLen = std::numeric_limits<int>::max();
                for (const auto& c : curves) minLen = std::min(minLen, c->pointCount());
                if (minLen <= 1 || minLen == std::numeric_limits<int>::max()) continue;
                QVector<QVector<double>> Y;
                Y.reserve(curves.size());
                for (const auto& c : curves) {
                    QVector<double> y; y.reserve(minLen);
                    const QVector<QPointF>& data = c->data();
                    for (int k = 0; k < minLen; ++k) y.append(data[k].y());
                    Y.append(y);
                }
                QVector<double> meanCurve(minLen, 0.0);
                for (int k = 0; k < minLen; ++k) {
                    double s = 0.0;
                    for (int i = 0; i < Y.size(); ++i) s += Y[i][k];
                    meanCurve[k] = s / Y.size();
                }
                double minMean = *std::min_element(meanCurve.begin(), meanCurve.end());
                double maxMean = *std::max_element(meanCurve.begin(), meanCurve.end());
                double localRefRange = std::max(maxMean - minMean, 1e-6);
                stageMeans[stage] = { meanCurve, localRefRange, minLen };
            }
            QVector<Detail> rowsBase;
            SingleTobaccoSampleDAO dao;
            auto calcPearson = [&](const QVector<double>& a, const QVector<double>& b)->double{
                int n = std::min(a.size(), b.size());
                if (n <= 1) return 1.0;
                double ma=0.0, mb=0.0; for (int i=0;i<n;++i){ ma+=a[i]; mb+=b[i]; } ma/=n; mb/=n;
                double num=0.0, da2=0.0, db2=0.0;
                for (int i=0;i<n;++i){ double da=a[i]-ma; double db=b[i]-mb; num+=da*db; da2+=da*da; db2+=db*db; }
                double denom = std::sqrt(da2)*std::sqrt(db2);
                if (denom <= 1e-12) return 1.0;
                double r = num/denom; if (r>1.0) r=1.0; if (r<-1.0) r=-1.0; return r;
            };
            for (auto jt = stageMapAll.constBegin(); jt != stageMapAll.constEnd(); ++jt) {
                const QString& stage = jt.key();
                const QVector<QSharedPointer<Curve>>& curves = jt.value();
                const auto mi = stageMeans.value(stage);
                if (mi.len <= 1) continue;
                for (const auto& c : curves) {
                    QVector<double> y; y.reserve(mi.len);
                    const QVector<QPointF>& data = c->data();
                    for (int k=0;k<mi.len;++k) y.append(data[k].y());
                    double mse=0.0, e=0.0;
                    for (int k=0;k<mi.len;++k){ double d=y[k]-mi.mean[k]; mse+=d*d; e+=d*d; }
                    mse/=mi.len;
                    double rmse = std::sqrt(mse);
                    double nrmse = rmse / mi.refRange;
                    double p = calcPearson(y, mi.mean);
                    double ee = std::sqrt(e);
                    double nNR = nrmse; // 单段内无需再二次归一化，排序直接按综合得分
                    double nP  = p;
                    double nE  = ee;
                    double comp = m_currentParams.weightNRMSE * nNR
                                + m_currentParams.weightEuclidean * nE
                                + m_currentParams.weightPearson * (1.0 - nP);
                    SampleIdentifier sid = dao.getSampleIdentifierById(c->sampleId());
                    QString prefix = QString("%1-%2-%3-%4").arg(sid.projectName).arg(sid.batchCode).arg(sid.shortCode).arg(sid.parallelNo);
                    rowsBase.append({baseKey, stage, prefix, nrmse, p, ee, comp, 0});
                }
            }
            std::sort(rowsBase.begin(), rowsBase.end(), [](const Detail& a, const Detail& b){ return a.comp < b.comp; });
            for (int i=0;i<rowsBase.size();++i){
                Detail d = rowsBase[i];
                d.rank = i+1;
                detailRows.append(d);
            }
        }

        // ===== 构建对话框 UI =====
        QDialog* dialog = new QDialog(this);
        dialog->setWindowTitle("工序大热重差异度（V2.2.1_origin）");
        dialog->setMinimumSize(1000, 640);

        QVBoxLayout* layout = new QVBoxLayout(dialog);
        QTabWidget* tabs = new QTabWidget(dialog);

        // 阶段汇总表
        QWidget* summaryTab = new QWidget(dialog);
        QVBoxLayout* summaryLayout = new QVBoxLayout(summaryTab);
        QTableWidget* summaryTable = new QTableWidget(summaryTab);
        summaryTable->setColumnCount(6);
        summaryTable->setHorizontalHeaderLabels(QStringList()
            << "工艺基础" << "阶段" << "平行样数量" << "总差异度" << "平均差异度" << "是否最优");
        // 列宽允许用户拖拽调整
        summaryTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
        summaryTable->horizontalHeader()->setSectionsMovable(true);
        summaryTable->setRowCount(summaryRows.size());
        for (int i=0;i<summaryRows.size();++i){
            const Summary& s = summaryRows[i];
            summaryTable->setItem(i,0,new QTableWidgetItem(s.base));
            summaryTable->setItem(i,1,new QTableWidgetItem(s.stage));
            summaryTable->setItem(i,2,new QTableWidgetItem(QString::number(s.n)));
            summaryTable->setItem(i,3,new QTableWidgetItem(QString::number(s.total, 'f', 8)));
            summaryTable->setItem(i,4,new QTableWidgetItem(QString::number(s.avg, 'f', 8)));
            summaryTable->setItem(i,5,new QTableWidgetItem(s.best ? "是" : "否"));
        }
        QPushButton* exportSummaryBtn = new QPushButton("导出阶段汇总", summaryTab);
        summaryLayout->addWidget(summaryTable);
        summaryLayout->addWidget(exportSummaryBtn);
        tabs->addTab(summaryTab, "大热重工序步骤差异度分析表");

        // 详细表
        QWidget* detailTab = new QWidget(dialog);
        QVBoxLayout* detailLayout = new QVBoxLayout(detailTab);
        QTableWidget* detailTable = new QTableWidget(detailTab);
        detailTable->setColumnCount(8);
        detailTable->setHorizontalHeaderLabels(QStringList()
            << "工艺基础" << "阶段" << "样品前缀" << "归一化均方根误差" << "皮尔逊相关系数" << "欧几里得距离" << "组合得分" << "组内排名");
        // 列宽允许用户拖拽调整
        detailTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
        detailTable->horizontalHeader()->setSectionsMovable(true);
        detailTable->setRowCount(detailRows.size());
        for (int i=0;i<detailRows.size();++i){
            const Detail& d = detailRows[i];
            detailTable->setItem(i,0,new QTableWidgetItem(d.base));
            detailTable->setItem(i,1,new QTableWidgetItem(d.stage));
            detailTable->setItem(i,2,new QTableWidgetItem(d.prefix));
            detailTable->setItem(i,3,new QTableWidgetItem(QString::number(d.nr, 'f', 8)));
            detailTable->setItem(i,4,new QTableWidgetItem(QString::number(d.p, 'f', 8)));
            detailTable->setItem(i,5,new QTableWidgetItem(QString::number(d.e, 'f', 8)));
            detailTable->setItem(i,6,new QTableWidgetItem(QString::number(d.comp, 'f', 8)));
            detailTable->setItem(i,7,new QTableWidgetItem(QString::number(d.rank)));
        }
        QPushButton* exportDetailBtn = new QPushButton("导出详细表", detailTab);
        detailLayout->addWidget(detailTable);
        detailLayout->addWidget(exportDetailBtn);
        tabs->addTab(detailTab, "工艺内差异度详细表");

        // ===== 新增：工序间差异度排序表（ALL）Tab =====
        QTableWidget* interTable = new QTableWidget(dialog);
        interTable->setColumnCount(5);
        interTable->setHorizontalHeaderLabels(QStringList()
            << "工艺基础" << "平行样数量" << "总差异度(ALL)" << "平均差异度(ALL)" << "排名");
        // 列宽允许用户拖拽调整
        interTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
        interTable->horizontalHeader()->setSectionsMovable(true);
        // 参考 V2.2.1_origin 的流程，计算 ALL 排序并填充表格
        struct AllRow { QString base; int n; double totalAll; double avgAll; };
        QVector<AllRow> allRows;
        for (auto itAll = baseStageCurves.constBegin(); itAll != baseStageCurves.constEnd(); ++itAll) {
            const QString& baseKey = itAll.key();
            QVector<QSharedPointer<Curve>> curvesAll;
            for (auto jt = itAll.value().constBegin(); jt != itAll.value().constEnd(); ++jt) {
                const QVector<QSharedPointer<Curve>>& v = jt.value();
                for (const auto& c : v) curvesAll.append(c);
            }
            int nGood = curvesAll.size();
            if (nGood < 1) continue;
            int minLen = std::numeric_limits<int>::max();
            for (const auto& c : curvesAll) minLen = std::min(minLen, c->pointCount());
            if (minLen <= 1 || minLen == std::numeric_limits<int>::max()) continue;
            QVector<QVector<double>> Y;
            Y.reserve(curvesAll.size());
            for (const auto& c : curvesAll) {
                QVector<double> y; y.reserve(minLen);
                const QVector<QPointF>& data = c->data();
                for (int k = 0; k < minLen; ++k) y.append(data[k].y());
                Y.append(y);
            }
            QVector<double> meanCurve(minLen, 0.0);
            for (int k = 0; k < minLen; ++k) {
                double s = 0.0;
                for (int i = 0; i < Y.size(); ++i) s += Y[i][k];
                meanCurve[k] = s / Y.size();
            }
            double minMean = *std::min_element(meanCurve.begin(), meanCurve.end());
            double maxMean = *std::max_element(meanCurve.begin(), meanCurve.end());
            double localRefRange = std::max(maxMean - minMean, 1e-6);
            QVector<double> nr, pp, ee, comp;
            nr.reserve(Y.size()); pp.reserve(Y.size()); ee.reserve(Y.size()); comp.reserve(Y.size());
            auto calcPearsonAll = [&](const QVector<double>& a, const QVector<double>& b)->double{
                int n = std::min(a.size(), b.size());
                if (n <= 1) return 1.0;
                double ma=0.0, mb=0.0; for (int i=0;i<n;++i){ ma+=a[i]; mb+=b[i]; } ma/=n; mb/=n;
                double num=0.0, da2=0.0, db2=0.0;
                for (int i=0;i<n;++i){ double da=a[i]-ma; double db=b[i]-mb; num+=da*db; da2+=da*da; db2+=db*db; }
                double denom = std::sqrt(da2)*std::sqrt(db2);
                if (denom <= 1e-12) return 1.0;
                double r = num/denom; if (r>1.0) r=1.0; if (r<-1.0) r=-1.0; return r;
            };
            for (int i = 0; i < Y.size(); ++i) {
                const QVector<double>& a = Y[i];
                double mse=0.0, e=0.0;
                for (int k=0;k<minLen;++k){ double d=a[k]-meanCurve[k]; mse+=d*d; e+=d*d; }
                mse/=minLen;
                double rmse = std::sqrt(mse);
                double nrmse = rmse / localRefRange;
                nr.append(nrmse);
                double p = calcPearsonAll(a, meanCurve);
                pp.append(p);
                ee.append(std::sqrt(e));
            }
            auto vminAll=[](const QVector<double>& v){ return v.isEmpty()?0.0:*std::min_element(v.begin(), v.end()); };
            auto vmaxAll=[](const QVector<double>& v){ return v.isEmpty()?1.0:*std::max_element(v.begin(), v.end()); };
            auto norm01All=[](double v,double vmin0,double vmax0){ if (vmax0<=vmin0) return 0.0; return (v - vmin0)/(vmax0 - vmin0); };
            double nrMin=vminAll(nr), nrMax=vmaxAll(nr);
            double pMin =vminAll(pp), pMax =vmaxAll(pp);
            double eMin =vminAll(ee), eMax =vmaxAll(ee);
            for (int i=0;i<Y.size();++i){
                double nNR=norm01All(nr[i],nrMin,nrMax);
                double nP =norm01All(pp[i],pMin,pMax);
                double nE =norm01All(ee[i],eMin,eMax);
                double c = m_currentParams.weightNRMSE * nNR
                         + m_currentParams.weightEuclidean * nE
                         + m_currentParams.weightPearson * (1.0 - nP);
                comp.append(c);
            }
            double total = 0.0;
            for (double v : comp) total += v;
            double avg = comp.isEmpty()? std::numeric_limits<double>::quiet_NaN() : (total / comp.size());
            double totalAdj = (nGood > 0) ? (total / std::sqrt(nGood)) : total;
            allRows.append({baseKey, nGood, totalAdj, avg});
        }
        QVector<AllRow> ranked = allRows;
        std::sort(ranked.begin(), ranked.end(), [](const AllRow& a, const AllRow& b){
            if (std::isnan(a.avgAll) && !std::isnan(b.avgAll)) return false;
            if (!std::isnan(a.avgAll) && std::isnan(b.avgAll)) return true;
            return a.avgAll < b.avgAll;
        });
        QMap<QString,int> ranks;
        for (int i=0;i<ranked.size();++i) ranks[ranked[i].base] = i+1;
        interTable->setRowCount(allRows.size());
        for (int i=0;i<allRows.size();++i){
            const AllRow& r = allRows[i];
            interTable->setItem(i,0,new QTableWidgetItem(r.base));
            interTable->setItem(i,1,new QTableWidgetItem(QString::number(r.n)));
            interTable->setItem(i,2,new QTableWidgetItem(QString::number(r.totalAll, 'f', 8)));
            interTable->setItem(i,3,new QTableWidgetItem(QString::number(r.avgAll, 'f', 8)));
            interTable->setItem(i,4,new QTableWidgetItem(QString::number(ranks.value(r.base, 0))));
        }
        QPushButton* exportInterBtn = new QPushButton("导出工序间差异度排序表", dialog);
        QWidget* interTab = new QWidget(dialog);
        QVBoxLayout* interLayout = new QVBoxLayout(interTab);
        interLayout->addWidget(interTable);
        interLayout->addWidget(exportInterBtn);
        tabs->addTab(interTab, "工序间差异度排序表");

        layout->addWidget(tabs);

        // 连接导出按钮
        connect(exportSummaryBtn, &QPushButton::clicked, [this](){
            // 仅导出阶段汇总
            try {
                QString filePath = QFileDialog::getSaveFileName(this, "保存Excel文件",
                    QDir::homePath() + "/大热重工序步骤差异度分析表.xlsx",
                    "Excel文件 (*.xlsx)");
                if (filePath.isEmpty()) return;
                QXlsx::Document xlsx;
                xlsx.addSheet("大热重工序步骤差异度分析表");
                xlsx.selectSheet("大热重工序步骤差异度分析表");
                // 复用汇总计算
                int row = 1;
                xlsx.write(row,1,"工艺基础"); xlsx.write(row,2,"阶段"); xlsx.write(row,3,"平行样数量");
                xlsx.write(row,4,"总差异度"); xlsx.write(row,5,"平均差异度"); xlsx.write(row,6,"是否最优");
                row++;
                // 由于此处无法直接访问 summaryRows（捕获），改用调用总导出函数以保证一致性
                exportV221OriginProcessOutputs();
            } catch (...) { QMessageBox::critical(this, "错误", "导出阶段汇总失败。"); }
        });
        connect(exportDetailBtn, &QPushButton::clicked, [this](){
            try {
                QString filePath = QFileDialog::getSaveFileName(this, "保存Excel文件",
                    QDir::homePath() + "/工序-大热重工艺内差异度详细表.xlsx",
                    "Excel文件 (*.xlsx)");
                if (filePath.isEmpty()) return;
                // 直接复用工作台的详细表导出或总导出
                exportV221OriginProcessOutputs();
            } catch (...) { QMessageBox::critical(this, "错误", "导出详细表失败。"); }
        });
        connect(exportInterBtn, &QPushButton::clicked, [this](){
            try {
                QString filePath = QFileDialog::getSaveFileName(this, "保存Excel文件",
                    QDir::homePath() + "/大热重工序间差异度排序表.xlsx",
                    "Excel文件 (*.xlsx)");
                if (filePath.isEmpty()) return;
                exportV221OriginProcessOutputs();
            } catch (...) { QMessageBox::critical(this, "错误", "导出工序间差异度排序表失败。"); }
        });

        dialog->show();
        dialog->exec();
    } catch (...) {
        QMessageBox::critical(this, "错误", "生成对话框过程中发生异常。");
    }
}

// 导出 V2.2.1_origin 的工序大热重标准输出（两张表：大热重工序步骤差异度分析表 + 工艺内差异度详细表）
void ProcessTgBigDataProcessDialog::exportV221OriginProcessOutputs()
{
    try {
        QString filePath = QFileDialog::getSaveFileName(
            this,
            "保存Excel文件",
            QDir::homePath() + "/工序大热重_V2.2.1_origin标准输出.xlsx",
            "Excel文件 (*.xlsx)");
        if (filePath.isEmpty()) return;

        QXlsx::Document xlsx;

        // ====== 1) 大热重工序步骤差异度分析表表 ======
        xlsx.addSheet("大热重工序步骤差异度分析表");
        xlsx.selectSheet("大热重工序步骤差异度分析表");
        int row1 = 1;
        xlsx.write(row1, 1, "工艺基础");
        xlsx.write(row1, 2, "阶段");
        xlsx.write(row1, 3, "平行样数量");
        xlsx.write(row1, 4, "总差异度");
        xlsx.write(row1, 5, "平均差异度");
        xlsx.write(row1, 6, "是否最优");
        row1++;

        // 构造 baseKey → stage → curves
        QRegularExpression reSuffix(QStringLiteral("-(Q|Z|H)(?:-|$)"), QRegularExpression::CaseInsensitiveOption);
        QMap<QString, QMap<QString, QVector<QSharedPointer<Curve>>>> baseStageCurves;
        for (auto it = m_stageDataCache.constBegin(); it != m_stageDataCache.constEnd(); ++it) {
            const SampleGroup& group = it.value();
            QString shortCodeBase = group.shortCode;
            QRegularExpressionMatch m = reSuffix.match(shortCodeBase);
            QString stageChar = "ALL";
            if (m.hasMatch()) {
                QString cap = m.captured(1);
                if (!cap.isEmpty()) {
                    stageChar = cap.toUpper();
                    QString pattern = QStringLiteral("-(?:") + QRegularExpression::escape(cap) + QStringLiteral(")$");
                    shortCodeBase.replace(QRegularExpression(pattern, QRegularExpression::CaseInsensitiveOption), QString());
                }
            }
            QString baseKey = QString("%1-%2-%3").arg(group.projectName).arg(group.batchCode).arg(shortCodeBase);
            for (const SampleDataFlexible& sample : group.sampleDatas) {
                QSharedPointer<Curve> curve;
                for (const StageData& s : sample.stages) {
                    if (s.stageName == StageName::Derivative && !s.curve.isNull()) { curve = s.curve; break; }
                }
                if (curve.isNull()) {
                    for (const StageData& s : sample.stages) {
                        if (s.stageName == StageName::RawData && !s.curve.isNull()) { curve = s.curve; break; }
                    }
                }
                if (!curve.isNull()) {
                    baseStageCurves[baseKey][stageChar].append(curve);
                }
            }
        }

        // 计算每个 baseKey-stage 的总/平均差异度
        struct Summary { QString base; QString stage; int n; double total; double avg; };
        QMap<QString, QVector<Summary>> summaryByBase;
        for (auto it = baseStageCurves.constBegin(); it != baseStageCurves.constEnd(); ++it) {
            const QString& baseKey = it.key();
            const auto& stageMap = it.value();
            QVector<Summary> vecSumm;
            for (auto jt = stageMap.constBegin(); jt != stageMap.constEnd(); ++jt) {
                const QString& stage = jt.key();
                const QVector<QSharedPointer<Curve>>& curves = jt.value();
                int nGood = curves.size();
                if (nGood < 2) {
                    vecSumm.append({baseKey, stage, nGood, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN()});
                    continue;
                }
                // 统一长度到最短
                int minLen = std::numeric_limits<int>::max();
                for (const auto& c : curves) minLen = std::min(minLen, c->pointCount());
                if (minLen <= 1 || minLen == std::numeric_limits<int>::max()) {
                    vecSumm.append({baseKey, stage, nGood, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN()});
                    continue;
                }
                // 计算所有配对原始指标
                QVector<double> valsNR, valsP, valsE;
                for (int i = 0; i < curves.size(); ++i) {
                    for (int j = i + 1; j < curves.size(); ++j) {
                        const QVector<QPointF>& da = curves[i]->data();
                        const QVector<QPointF>& db = curves[j]->data();
                        double mse = 0.0, eucl = 0.0;
                        double meanA = 0.0, meanB = 0.0;
                        for (int k = 0; k < minLen; ++k) {
                            double ya = da[k].y();
                            double yb = db[k].y();
                            double d = ya - yb;
                            mse += d * d;
                            eucl += d * d;
                            meanA += ya; meanB += yb;
                        }
                        mse /= minLen;
                        double rmse = std::sqrt(mse);
                        double euclVal = std::sqrt(eucl);
                        meanA /= minLen; meanB /= minLen;
                        // Pearson
                        double num = 0.0, denA = 0.0, denB = 0.0;
                        for (int k = 0; k < minLen; ++k) {
                            double da1 = da[k].y() - meanA;
                            double db1 = db[k].y() - meanB;
                            num += da1 * db1;
                            denA += da1 * da1;
                            denB += db1 * db1;
                        }
                        double denom = std::sqrt(denA) * std::sqrt(denB);
                        double p = (denom <= 1e-12) ? 1.0 : (num / denom);
                        if (p > 1.0) p = 1.0; if (p < -1.0) p = -1.0;
                        // NRMSE 使用组均值幅度近似
                        double minA = std::numeric_limits<double>::infinity();
                        double maxA = -std::numeric_limits<double>::infinity();
                        for (int k = 0; k < minLen; ++k) { double v = da[k].y(); minA = std::min(minA, v); maxA = std::max(maxA, v); }
                        double rangeA = std::max(maxA - minA, 1e-6);
                        double nrmse = rmse / rangeA;
                        valsNR.append(nrmse);
                        valsP.append(p);
                        valsE.append(euclVal);
                    }
                }
                // 归一化并合成差异度
                auto vmin = [](const QVector<double>& v){ return v.isEmpty()? 0.0 : *std::min_element(v.begin(), v.end()); };
                auto vmax = [](const QVector<double>& v){ return v.isEmpty()? 1.0 : *std::max_element(v.begin(), v.end()); };
                auto norm01 = [](double v, double vmin0, double vmax0){ if (vmax0 <= vmin0) return 0.0; return (v - vmin0) / (vmax0 - vmin0); };
                double nrMin = vmin(valsNR), nrMax = vmax(valsNR);
                double pMin  = vmin(valsP),  pMax  = vmax(valsP);
                double eMin  = vmin(valsE),  eMax  = vmax(valsE);
                QVector<double> comp;
                comp.reserve(valsNR.size());
                for (int i = 0; i < valsNR.size(); ++i) {
                    double nNR = norm01(valsNR[i], nrMin, nrMax);     // 差异
                    double nP  = norm01(valsP[i],  pMin,  pMax);      // 相似
                    double nE  = norm01(valsE[i],  eMin,  eMax);      // 差异
                    double score = m_currentParams.weightNRMSE * nNR
                                 + m_currentParams.weightEuclidean * nE
                                 + m_currentParams.weightPearson * (1.0 - nP);
                    comp.append(score);
                }
                double total = 0.0, avg = 0.0;
                for (double v : comp) total += v;
                avg = comp.isEmpty()? std::numeric_limits<double>::quiet_NaN() : (total / comp.size());
                // 样本量加权（近似）：除以 sqrt(n)
                double totalAdj = (nGood > 0) ? (total / std::sqrt(nGood)) : total;
                vecSumm.append({baseKey, stage, nGood, totalAdj, avg});
            }
            // 最优标记：总差异度最小
            int bestIdx = -1; double bestVal = std::numeric_limits<double>::infinity();
            for (int i = 0; i < vecSumm.size(); ++i) {
                double v = vecSumm[i].total;
                if (!std::isnan(v) && v < bestVal) { bestVal = v; bestIdx = i; }
            }
            for (int i = 0; i < vecSumm.size(); ++i) {
                bool isBest = (i == bestIdx);
                xlsx.write(row1, 1, vecSumm[i].base);
                xlsx.write(row1, 2, vecSumm[i].stage);
                xlsx.write(row1, 3, vecSumm[i].n);
                xlsx.write(row1, 4, vecSumm[i].total);
                xlsx.write(row1, 5, vecSumm[i].avg);
                xlsx.write(row1, 6, isBest ? "是" : "否");
                row1++;
            }
        }

        // ====== 2) 工艺内差异度详细表（与工作台导出一致） ======
        // 直接复用计算逻辑：构造 baseKey → 曲线集合，均值对比指标与排名
        xlsx.addSheet("工艺内差异度详细表");
        xlsx.selectSheet("工艺内差异度详细表");
        int row2 = 1;
        xlsx.write(row2, 1, "工艺基础");
        xlsx.write(row2, 2, "阶段");
        xlsx.write(row2, 3, "样品前缀");
        xlsx.write(row2, 4, "归一化均方根误差");
        xlsx.write(row2, 5, "皮尔逊相关系数");
        xlsx.write(row2, 6, "欧几里得距离");
        xlsx.write(row2, 7, "组合得分");
        xlsx.write(row2, 8, "组内排名");
        row2++;

        // 详细表改为：对同一工艺基础下的所有组内样本（各阶段的每个平行样）统一排序
        SingleTobaccoSampleDAO dao;
        for (auto it = baseStageCurves.constBegin(); it != baseStageCurves.constEnd(); ++it) {
            const QString& baseKey = it.key();
            const auto& stageMap = it.value();
            struct MeanInfo { QVector<double> mean; double refRange; int len; };
            QMap<QString, MeanInfo> stageMeans;
            for (auto jt = stageMap.constBegin(); jt != stageMap.constEnd(); ++jt) {
                const QString& stage = jt.key();
                const QVector<QSharedPointer<Curve>>& curves = jt.value();
                if (curves.isEmpty()) continue;
                int minLen = std::numeric_limits<int>::max();
                for (const auto& c : curves) minLen = std::min(minLen, c->pointCount());
                if (minLen <= 1 || minLen == std::numeric_limits<int>::max()) continue;
                QVector<QVector<double>> Y;
                Y.reserve(curves.size());
                for (const auto& c : curves) {
                    QVector<double> y; y.reserve(minLen);
                    const QVector<QPointF>& data = c->data();
                    for (int k = 0; k < minLen; ++k) y.append(data[k].y());
                    Y.append(y);
                }
                QVector<double> meanCurve(minLen, 0.0);
                for (int k = 0; k < minLen; ++k) {
                    double s = 0.0;
                    for (int i = 0; i < Y.size(); ++i) s += Y[i][k];
                    meanCurve[k] = s / Y.size();
                }
                double minMean = *std::min_element(meanCurve.begin(), meanCurve.end());
                double maxMean = *std::max_element(meanCurve.begin(), meanCurve.end());
                double localRefRange = std::max(maxMean - minMean, 1e-6);
                stageMeans[stage] = { meanCurve, localRefRange, minLen };
            }
            auto calcPearson = [&](const QVector<double>& a, const QVector<double>& b)->double{
                int n = std::min(a.size(), b.size());
                if (n <= 1) return 1.0;
                double ma=0.0, mb=0.0; for (int i=0;i<n;++i){ ma+=a[i]; mb+=b[i]; } ma/=n; mb/=n;
                double num=0.0, da2=0.0, db2=0.0;
                for (int i=0;i<n;++i){ double da=a[i]-ma; double db=b[i]-mb; num+=da*db; da2+=da*da; db2+=db*db; }
                double denom = std::sqrt(da2)*std::sqrt(db2);
                if (denom <= 1e-12) return 1.0;
                double r = num/denom; if (r>1.0) r=1.0; if (r<-1.0) r=-1.0; return r;
            };
            struct Row { QString stage; QString prefix; double nr; double p; double e; double comp; };
            QVector<Row> rows;
            for (auto jt = stageMap.constBegin(); jt != stageMap.constEnd(); ++jt) {
                const QString& stage = jt.key();
                const QVector<QSharedPointer<Curve>>& curves = jt.value();
                const auto mi = stageMeans.value(stage);
                if (mi.len <= 1) continue;
                for (const auto& c : curves) {
                    QVector<double> y; y.reserve(mi.len);
                    const QVector<QPointF>& data = c->data();
                    for (int k=0;k<mi.len;++k) y.append(data[k].y());
                    double mse=0.0, e=0.0;
                    for (int k=0;k<mi.len;++k){ double d=y[k]-mi.mean[k]; mse+=d*d; e+=d*d; }
                    mse/=mi.len;
                    double rmse = std::sqrt(mse);
                    double nrmse = rmse / mi.refRange;
                    double p = calcPearson(y, mi.mean);
                    double ee = std::sqrt(e);
                    double comp = m_currentParams.weightNRMSE * nrmse
                                + m_currentParams.weightEuclidean * ee
                                + m_currentParams.weightPearson * (1.0 - p);
                    SampleIdentifier sid = dao.getSampleIdentifierById(c->sampleId());
                    QString prefix = QString("%1-%2-%3-%4").arg(sid.projectName).arg(sid.batchCode).arg(sid.shortCode).arg(sid.parallelNo);
                    rows.append({stage, prefix, nrmse, p, ee, comp});
                }
            }
            std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b){ return a.comp < b.comp; });
            for (int i=0;i<rows.size();++i){
                const Row& r = rows[i];
                xlsx.write(row2, 1, baseKey);
                xlsx.write(row2, 2, r.stage);
                xlsx.write(row2, 3, r.prefix);
                xlsx.write(row2, 4, r.nr);
                xlsx.write(row2, 5, r.p);
                xlsx.write(row2, 6, r.e);
                xlsx.write(row2, 7, r.comp);
                xlsx.write(row2, 8, i+1);
                row2++;
            }
        }

        // ====== 3) 工序间差异度排序表（ALL） ======
        xlsx.addSheet("工序间差异度排序表");
        xlsx.selectSheet("工序间差异度排序表");
        int row3 = 1;
        xlsx.write(row3, 1, "工艺基础");
        xlsx.write(row3, 2, "平行样数量");
        xlsx.write(row3, 3, "总差异度(ALL)");
        xlsx.write(row3, 4, "平均差异度(ALL)");
        xlsx.write(row3, 5, "排名");
        row3++;

        struct AllRow { QString base; int n; double totalAll; double avgAll; };
        QVector<AllRow> allRows;
        for (auto it = baseStageCurves.constBegin(); it != baseStageCurves.constEnd(); ++it) {
            const QString& baseKey = it.key();
            // 聚合所有阶段的曲线到 ALL
            QVector<QSharedPointer<Curve>> curvesAll;
            for (auto jt = it.value().constBegin(); jt != it.value().constEnd(); ++jt) {
                const QVector<QSharedPointer<Curve>>& v = jt.value();
                for (const auto& c : v) curvesAll.append(c);
            }
            int nGood = curvesAll.size();
            if (nGood < 1) continue;
            int minLen = std::numeric_limits<int>::max();
            for (const auto& c : curvesAll) minLen = std::min(minLen, c->pointCount());
            if (minLen <= 1 || minLen == std::numeric_limits<int>::max()) continue;
            // 均值曲线
            QVector<QVector<double>> Y;
            Y.reserve(curvesAll.size());
            for (const auto& c : curvesAll) {
                QVector<double> y; y.reserve(minLen);
                const QVector<QPointF>& data = c->data();
                for (int k = 0; k < minLen; ++k) y.append(data[k].y());
                Y.append(y);
            }
            QVector<double> meanCurve(minLen, 0.0);
            for (int k = 0; k < minLen; ++k) {
                double s = 0.0;
                for (int i = 0; i < Y.size(); ++i) s += Y[i][k];
                meanCurve[k] = s / Y.size();
            }
            double minMean = *std::min_element(meanCurve.begin(), meanCurve.end());
            double maxMean = *std::max_element(meanCurve.begin(), meanCurve.end());
            double localRefRange = std::max(maxMean - minMean, 1e-6);
            // 指标
            QVector<double> nr, pp, ee, comp;
            nr.reserve(Y.size()); pp.reserve(Y.size()); ee.reserve(Y.size()); comp.reserve(Y.size());
            auto calcPearson = [&](const QVector<double>& a, const QVector<double>& b)->double{
                int n = std::min(a.size(), b.size());
                if (n <= 1) return 1.0;
                double ma=0.0, mb=0.0; for (int i=0;i<n;++i){ ma+=a[i]; mb+=b[i]; } ma/=n; mb/=n;
                double num=0.0, da2=0.0, db2=0.0;
                for (int i=0;i<n;++i){ double da=a[i]-ma; double db=b[i]-mb; num+=da*db; da2+=da*da; db2+=db*db; }
                double denom = std::sqrt(da2)*std::sqrt(db2);
                if (denom <= 1e-12) return 1.0;
                double r = num/denom; if (r>1.0) r=1.0; if (r<-1.0) r=-1.0; return r;
            };
            for (int i = 0; i < Y.size(); ++i) {
                const QVector<double>& a = Y[i];
                double mse=0.0, e=0.0;
                for (int k=0;k<minLen;++k){ double d=a[k]-meanCurve[k]; mse+=d*d; e+=d*d; }
                mse/=minLen;
                double rmse = std::sqrt(mse);
                double nrmse = rmse / localRefRange;
                nr.append(nrmse);
                double p = calcPearson(a, meanCurve);
                pp.append(p);
                ee.append(std::sqrt(e));
            }
            auto vmin=[](const QVector<double>& v){ return v.isEmpty()?0.0:*std::min_element(v.begin(), v.end()); };
            auto vmax=[](const QVector<double>& v){ return v.isEmpty()?1.0:*std::max_element(v.begin(), v.end()); };
            auto norm01=[](double v,double vmin0,double vmax0){ if (vmax0<=vmin0) return 0.0; return (v - vmin0)/(vmax0 - vmin0); };
            double nrMin=vmin(nr), nrMax=vmax(nr);
            double pMin =vmin(pp), pMax =vmax(pp);
            double eMin =vmin(ee), eMax =vmax(ee);
            for (int i=0;i<Y.size();++i){
                double nNR=norm01(nr[i],nrMin,nrMax);
                double nP =norm01(pp[i],pMin,pMax);
                double nE =norm01(ee[i],eMin,eMax);
                double c = m_currentParams.weightNRMSE * nNR
                         + m_currentParams.weightEuclidean * nE
                         + m_currentParams.weightPearson * (1.0 - nP);
                comp.append(c);
            }
            double total = 0.0;
            for (double v : comp) total += v;
            double avg = comp.isEmpty()? std::numeric_limits<double>::quiet_NaN() : (total / comp.size());
            double totalAdj = (nGood > 0) ? (total / std::sqrt(nGood)) : total;
            allRows.append({baseKey, nGood, totalAdj, avg});
        }
        // 排名（按平均差异度升序）
        QVector<AllRow> ranked = allRows;
        std::sort(ranked.begin(), ranked.end(), [](const AllRow& a, const AllRow& b){
            if (std::isnan(a.avgAll) && !std::isnan(b.avgAll)) return false;
            if (!std::isnan(a.avgAll) && std::isnan(b.avgAll)) return true;
            return a.avgAll < b.avgAll;
        });
        QMap<QString,int> ranks;
        for (int i=0;i<ranked.size();++i) ranks[ranked[i].base] = i+1;
        for (const auto& r : allRows) {
            xlsx.write(row3, 1, r.base);
            xlsx.write(row3, 2, r.n);
            xlsx.write(row3, 3, r.totalAll);
            xlsx.write(row3, 4, r.avgAll);
            xlsx.write(row3, 5, ranks.value(r.base, 0));
            row3++;
        }

        if (!xlsx.saveAs(filePath)) {
            QMessageBox::warning(this, "提示", "保存Excel文件失败，请确认路径与权限。");
            return;
        }
        QMessageBox::information(this, "成功", "已导出 V2.2.1_origin 标准输出（工艺阶段汇总与组内详细表）。");
    } catch (...) {
        QMessageBox::critical(this, "错误", "导出过程中发生异常。");
    }
}
