#include "ChromatographDataProcessDialog.h"
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
#include "ChromatographParameterSettingsDialog.h"
#include "AppInitializer.h"
#include "gui/views/ChartView.h"
#include "core/entities/Curve.h"
#include "SingleTobaccoSampleDAO.h" // 修改: 包含对应的头文件
#include "ColorUtils.h"
#include "core/singletons/SampleSelectionManager.h"
#include "InfoAutoClose.h"
#include <QCursor>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QInputDialog>
#include <QSizePolicy>

ChromatographDataProcessDialog::ChromatographDataProcessDialog(QWidget *parent, AppInitializer* appInitializer, DataNavigator *mainNavigator) :
    QWidget(parent), m_appInitializer(appInitializer), m_mainNavigator(mainNavigator)
{
    // 安全检查
    if (!m_appInitializer) {
        // 这是一个致命的编程错误，程序无法在没有服务提供者的情况下工作
        qFatal("ChromatographDataProcessDialog created without a valid AppInitializer!");
    }

    // 【关键】使用传入的 appInitializer 来获取服务
    m_processingService = m_appInitializer->getDataProcessingService();

    setWindowTitle(tr("色谱数据处理"));
    // setMinimumSize(1200, 800);
    
    // resize(1400, 900);

    // // 如果有 DataNavigator 对象，初始化它
    // m_mainNavigator = new DataNavigator(this);

    // // 创建 TabWidget
    // QTabWidget* tabWidget = new QTabWidget(this);
    // setCentralWidget(tabWidget);  // QMainWindow 设置中央窗口

    // // =========================
    // // Tab 1: 色谱数据处理
    // // =========================
    // ChromatographDataProcessDialog* tab1 = new ChromatographDataProcessDialog(this, m_mainNavigator);
    // tabWidget->addTab(tab1, tr("色谱数据处理"));

    // 提前设置默认行为：四图流程默认全开（原始->基线->裁剪->对齐）
    m_currentParams.baselineEnabled = true;
    m_currentParams.chromClipEnabled = true;
    m_currentParams.alignmentEnabled = true;
    m_currentParams.peakSegCowEnabled = true;
    m_currentParams.peakDetectionEnabled = true;

    // 提前创建参数设置窗口
    m_paramDialog = new ChromatographParameterSettingsDialog(m_currentParams, this);

    // 连接信号槽（只需一次）
    connect(m_paramDialog, &ChromatographParameterSettingsDialog::parametersApplied,
            this, &ChromatographDataProcessDialog::onParametersApplied);
    
    setupUI();
    setupConnections();
    loadNavigatorData();

    // 首次打开时同步主导航已选中的“色谱”样本，并立即自动重算绘图
    {
        QSet<int> preselected = SampleSelectionManager::instance()->selectedIdsByType(QStringLiteral("色谱"));
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
                // 2) 添加到本界面选中集合，供绘制使用（统一名称格式）
                QString name = buildSampleDisplayName(sampleId);
                m_selectedSamples.insert(sampleId, name);
            }
            m_suppressItemChanged = false;
            for (int sampleId : preselected) {
                m_visibleSamples.insert(sampleId);
            }
            updateSelectedSamplesList();
            scheduleRedraw();
        }
    }
    // showMaximized();   //  在这里最大化窗口

    if (!m_mainNavigator) {
        WARNING_LOG << "⚠️ mainNavigator is nullptr!";
    } else {
        DEBUG_LOG << " mainNavigator is valid:" << m_mainNavigator;
    }

    DEBUG_LOG << "ChromatographDataProcessDialog::ChromatographDataProcessDialog - Finished";
}

ChromatographDataProcessDialog::~ChromatographDataProcessDialog()
{
    // delete ui; // Removed as UI is code-built
    DEBUG_LOG << "ChromatographParameterSettingsDialog destroyed!";
}

QMap<int, QString> ChromatographDataProcessDialog::getSelectedSamples() const
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

int ChromatographDataProcessDialog::countSelectedSamples() const
{
    return getSelectedSamples().size();
}

void ChromatographDataProcessDialog::showSelectedSamplesStatistics()
{
    QMap<int, QString> selectedSamples = getSelectedSamples();
    int count = selectedSamples.size();
    
    QString message = tr("当前选中的色谱样本数量: %1\n\n").arg(count);
    
    if (count > 0) {
        message += tr("选中的样本列表:\n");
        QMapIterator<int, QString> i(selectedSamples);
        while (i.hasNext()) {
            i.next();
            message += tr("ID: %1, 名称: %2\n").arg(i.key()).arg(i.value());
        }
    } else {
        message += tr("没有选中任何色谱样本");
    }
    
    QMessageBox::information(this, tr("色谱样本统计"), message);
}

void ChromatographDataProcessDialog::selectSample(int sampleId, const QString& sampleName)
{
    try {
        if (sampleId <= 0) {
            DEBUG_LOG << "无效的样本ID:" << sampleId;
            return;
        }
        
        DEBUG_LOG << "ChromatographDataProcessDialog::selectSample - ID:" << sampleId << "Name:" << sampleName;
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
                            
                            // 确保导航树标题显示为"色谱 - 样本导航"
                            m_leftNavigator->setHeaderLabel(tr("色谱 - 样本导航"));
                            
                            DEBUG_LOG << "色谱样本已选中:" << sampleId << sampleName;
                            
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

void ChromatographDataProcessDialog::addSampleCurve(int sampleId, const QString& sampleName)
{
    try {
        if (sampleId <= 0) {
            DEBUG_LOG << "无效的样本ID:" << sampleId;
            return;
        }
        
        DEBUG_LOG << "ChromatographDataProcessDialog::addSampleCurve - ID:" << sampleId << "Name:" << sampleName;

        {
            const QString nm = sampleName.trimmed().isEmpty() ? buildSampleDisplayName(sampleId) : sampleName;
            m_selectedSamples.insert(sampleId, nm);
        }

        const int prevVisible = m_visibleSamples.size();
        // 将样本加入“可见曲线”集合（与主导航、列表勾选共用）
        m_visibleSamples.insert(sampleId);
        DEBUG_LOG << "当前可见样本数:" << m_visibleSamples.size();
        updateSelectedSamplesList();
        promptChromatographReferenceSampleIfNeeded(prevVisible);
        
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
                            DEBUG_LOG << "色谱样本曲线已添加:" << sampleId << sampleName;
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




void ChromatographDataProcessDialog::drawSelectedSampleCurves()
{
    // 历史函数名保留以兼容已有调用，实际统一走“自动重算并绘图”流程
    recalculateAndUpdatePlot();
}


void ChromatographDataProcessDialog::removeSampleCurve(int sampleId)
{
    // 如果该样本当前并不可见，直接返回
    if (!m_visibleSamples.contains(sampleId)) {
        DEBUG_LOG << "ChromatographDataProcessDialog::removeSampleCurve: sampleId" << sampleId << "not visible, skip.";
        return;
    }
    
    DEBUG_LOG << "ChromatographDataProcessDialog::removeSampleCurve - ID:" << sampleId;
    
    m_visibleSamples.remove(sampleId);
    DEBUG_LOG << "当前可见样本数:" << m_visibleSamples.size();
    // 移除后不立即重绘，由上层批量逻辑统一处理，避免闪烁
    updateSelectedSamplesList();
}

QString ChromatographDataProcessDialog::buildSampleDisplayName(int sampleId)
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
            if (createdVar.isValid() && createdVar.canConvert<QDateTime>()) {
                dt = createdVar.toDateTime();
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

QString ChromatographDataProcessDialog::chromatographLegendExtraTags(const SampleIdentifier& sid) const
{
    const QString sc = sid.shortCode.trimmed();
    if (sc.startsWith(QStringLiteral("0000_")))
        return QStringLiteral("（基准）");
    return QString();
}

void ChromatographDataProcessDialog::promptChromatographReferenceSampleIfNeeded(int previousVisibleCount)
{
    const int newVisibleCount = m_visibleSamples.size();
    if (newVisibleCount < 2)
        return;
    // 仅在首次从「少于 2 条可见曲线」变为「不少于 2 条」时询问参考样；
    // 继续添加第 3、4…条不再弹窗，避免打断选择且防止与主导航勾选状态冲突。
    if (previousVisibleCount >= 2)
        return;

    QList<int> ids = m_visibleSamples.values();
    std::sort(ids.begin(), ids.end());

    QStringList labels;
    QMap<QString, int> labelToId;
    for (int id : ids) {
        QString name = buildSampleDisplayName(id);
        if (name.trimmed().isEmpty())
            name = QStringLiteral("样本 %1").arg(id);
        const QString label = QStringLiteral("%1 (ID:%2)").arg(name).arg(id);
        labels.append(label);
        labelToId.insert(label, id);
    }

    int currentIdx = ids.indexOf(m_currentParams.referenceSampleId);
    if (currentIdx < 0)
        currentIdx = 0;

    bool ok = false;
    const QString selected = QInputDialog::getItem(
        this,
        tr("选择色谱对齐参考样本"),
        tr("请在当前可见曲线中选择参考样本（对齐模板）："),
        labels,
        currentIdx,
        false,
        &ok);

    if (ok && !selected.isEmpty()) {
        const int refId = labelToId.value(selected, -1);
        if (refId > 0)
            m_currentParams.referenceSampleId = refId;
    }
}

void ChromatographDataProcessDialog::setupUI()
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

    DEBUG_LOG << "ChromatographDataProcessDialog::setupUI - Splitter sizes:" << m_mainSplitter->sizes();

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
    // 【新】增加按钮
    m_startComparisonButton = new QPushButton(tr("计算差异度"));
    // 新增显示/隐藏左侧标签页按钮（导航 + 选中样本）
    m_toggleNavigatorButton = new QPushButton(tr("样本浏览页"), tab1Widget);
   
    // 新增清除曲线按钮
    m_clearCurvesButton = new QPushButton(tr("清除曲线"), tab1Widget);
    // 新增绘制所有选中曲线与取消所有选中样本按钮
    m_drawAllButton = new QPushButton(tr("绘制所有选中曲线"), tab1Widget);
    m_unselectAllButton = new QPushButton(tr("取消所有选中样本"), tab1Widget);
    
    

    m_buttonLayout->addStretch();
    // m_buttonLayout->addWidget(m_processButton);
    // m_buttonLayout->addWidget(m_resetButton);
    // m_buttonLayout->addWidget(m_cancelButton);
    m_buttonLayout->addWidget(m_toggleNavigatorButton);
    m_buttonLayout->addWidget(m_parameterButton);
    m_buttonLayout->addWidget(m_startComparisonButton);
    m_buttonLayout->addWidget(m_clearCurvesButton);
    m_buttonLayout->addWidget(m_drawAllButton);
    m_buttonLayout->addWidget(m_unselectAllButton);

    // m_mainLayout->addLayout(m_buttonLayout);

    tab1Layout->addLayout(m_buttonLayout);

    // 将 Tab1 添加到 tabWidget
    tabWidget->addTab(tab1Widget, tr("色谱数据处理"));

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

    DEBUG_LOG << "ChromatographDataProcessDialog::setupUI - Splitter sizes:" << m_mainSplitter->sizes();
}


void ChromatographDataProcessDialog::setupLeftNavigator()
{
    // 创建左侧面板
    m_leftPanel = new QWidget();
    m_leftLayout = new QVBoxLayout(m_leftPanel);
    
    // 创建左侧标签页容器，仅包含“选中样本”一页
    m_leftTabWidget = new QTabWidget(m_leftPanel);

    // 创建（隐藏的）导航树实例，不加入到界面，仅供内部函数保留兼容（可选）
    m_leftNavigator = new QTreeWidget();
    m_leftNavigator->setHeaderLabel(tr("色谱 - 样本导航"));
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

void ChromatographDataProcessDialog::setupMiddlePanel()
{
    DEBUG_LOG << "ChromatographDataProcessDialog::setupMiddlePanel - Setting up middle panel";
    
    // 创建中间面板
    m_middlePanel = new QWidget();
    m_middlePanel->setMinimumWidth(500); // 确保中间面板有足够的宽度
    m_middleLayout = new QGridLayout(m_middlePanel);
    
    m_chartView1 = new ChartView();
    m_chartView2 = new ChartView();
    m_chartView3 = new ChartView();
    m_chartView4 = new ChartView();
    if (!m_chartView1
        || !m_chartView2
        || !m_chartView3
        || !m_chartView4) {
        DEBUG_LOG << "ChromatographDataProcessDialog::setupMiddlePanel--VIEW - ERROR: Failed to create ChartView!";
    } else {
        DEBUG_LOG << "ChromatographDataProcessDialog::setupMiddlePanel--VIEW - ChartView created successfully";
    }

    // 2x2：原始数据 / 基线校正 / 裁剪数据 / 色谱对齐
    m_middleLayout->addWidget(m_chartView1, 0, 0);
    m_middleLayout->addWidget(m_chartView2, 0, 1);
    m_middleLayout->addWidget(m_chartView3, 1, 0);
    m_middleLayout->addWidget(m_chartView4, 1, 1);

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
    
    DEBUG_LOG << "ChromatographDataProcessDialog::setupMiddlePanel - Middle panel setup completed";
    // DEBUG_LOG << "ChromatographDataProcessDialog::setupMiddlePanel - ChartView visibility:" << m_chartView->isVisible();
    DEBUG_LOG << "ChromatographDataProcessDialog::setupMiddlePanel - Middle panel visibility:" << m_middlePanel->isVisible();
}

void ChromatographDataProcessDialog::setupRightPanel()
{
    DEBUG_LOG << "ChromatographDataProcessDialog::setupRightPanel - Setting up right panel";
    // 创建右侧面板
    m_rightPanel = new QWidget();
    m_rightLayout = new QVBoxLayout(m_rightPanel);
    
        DEBUG_LOG << "ChromatographDataProcessDialog::setupRightPanel - Right panel setup complete";
}


void ChromatographDataProcessDialog::setupConnections()
{
    // 按钮连接
    connect(m_parameterButton, &QPushButton::clicked, this, &ChromatographDataProcessDialog::onParameterSettingsClicked);
    connect(m_startComparisonButton, &QPushButton::clicked, this, &ChromatographDataProcessDialog::onStartComparison);
    connect(m_toggleNavigatorButton, &QPushButton::clicked, this, &ChromatographDataProcessDialog::onToggleNavigatorClicked);
    // connect(m_toggleNavigatorButton, &QPushButton::clicked, this, &ChromatographDataProcessDialog::onToggleNavigatorClicked);
    connect(m_clearCurvesButton, &QPushButton::clicked, this, &ChromatographDataProcessDialog::onClearCurvesClicked);
    connect(m_drawAllButton, &QPushButton::clicked, this, &ChromatographDataProcessDialog::onDrawAllSelectedCurvesClicked);
    connect(m_unselectAllButton, &QPushButton::clicked, this, &ChromatographDataProcessDialog::onUnselectAllSamplesClicked);
    
    // 选中样本列表勾选变化驱动左侧导航勾选状态
    connect(m_selectedSamplesList, &QListWidget::itemChanged, this, &ChromatographDataProcessDialog::onSelectedSamplesListItemChanged);
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

    
    // 添加右键菜单
    m_leftNavigator->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_leftNavigator, &QTreeWidget::customContextMenuRequested, this, &ChromatographDataProcessDialog::onLeftNavigatorContextMenu);
    
    // 参数变化连接

    // 订阅统一管理器按类型变化。仅当左侧导航复选框为“选中”时才显示曲线；
    // 首次出现的新样本节点默认勾选并显示曲线。
    connect(SampleSelectionManager::instance(), &SampleSelectionManager::selectionChangedByType,
            this, [this](int sampleId, const QString& dataType, bool selected, const QString& origin){
                if (dataType != QStringLiteral("色谱")) return;
                if (selected) {
                    // 加入“被选中样本”集合；首次出现则默认可见（勾选）并绘制曲线
                    QString name = m_selectedSamples.value(sampleId);
                    if (name.isEmpty()) {
                        name = buildSampleDisplayName(sampleId);
                    }
                    bool first = !m_selectedSamples.contains(sampleId);
                    m_selectedSamples.insert(sampleId, name);
                    if (first) {
                        addSampleCurve(sampleId, name);
                    }
                    updateSelectedSamplesList();
                    scheduleRedraw();
                } else {
                    // 从“被选中样本”集合移除；同时移除可见集合与曲线
                    m_selectedSamples.remove(sampleId);
                    if (m_visibleSamples.contains(sampleId)) {
                        m_visibleSamples.remove(sampleId);
                        removeSampleCurve(sampleId);
                    }
                    updateSelectedSamplesList();
                    if (origin == QStringLiteral("BatchSelect") || origin == QStringLiteral("Dialog-UnselectAll")) {
                        scheduleRedraw();
                    } else {
                        scheduleRedraw();
                    }
                }
            });
}

void ChromatographDataProcessDialog::onClearCurvesClicked()
{
    // 清除所有绘图区域的曲线，并重置本界面选中状态
    if (m_chartView1) m_chartView1->clearGraphs();
    if (m_chartView2) m_chartView2->clearGraphs();
    if (m_chartView3) m_chartView3->clearGraphs();
    if (m_chartView4) m_chartView4->clearGraphs();

    m_visibleSamples.clear();
    updateLegendPanel();
    updateSelectedSamplesList();
}

void ChromatographDataProcessDialog::onDrawAllSelectedCurvesClicked()
{
    // 从全局选择管理器获取“色谱”类型已选样本，全部设为可见并绘制
    QSet<int> ids = SampleSelectionManager::instance()->selectedIdsByType(QStringLiteral("色谱"));
    m_visibleSamples = ids;
    // 确保 m_selectedSamples 包含名称（统一名称格式）
    for (int sampleId : ids) {
        if (!m_selectedSamples.contains(sampleId)) {
            QString fullName = buildSampleDisplayName(sampleId);
            m_selectedSamples.insert(sampleId, fullName);
        }
    }
    scheduleRedraw();
    updateSelectedSamplesList();
}

void ChromatographDataProcessDialog::onUnselectAllSamplesClicked()
{
    // 清空全局选择管理器中“色谱”类型的所有样本，同时清空本界面的可见与选中集合
    const QString type = QStringLiteral("色谱");
    QSet<int> ids = SampleSelectionManager::instance()->selectedIdsByType(type);
    for (int sampleId : ids) {
        SampleSelectionManager::instance()->setSelectedWithType(sampleId, type, false, QStringLiteral("Dialog-UnselectAll"));
    }

    m_selectedSamples.clear();
    m_visibleSamples.clear();
    updateSelectedSamplesList();
}

// 显示/隐藏左侧标签页（包含样本导航与选中样本列表）
void ChromatographDataProcessDialog::onToggleNavigatorClicked()
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
void ChromatographDataProcessDialog::updateSelectedSamplesList()
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

void ChromatographDataProcessDialog::updateSelectedStatsInfo()
{
    if (!m_selectedStatsLabel) return;
    int selectedCount = m_selectedSamples.size();
    int visibleCount = m_visibleSamples.size();
    QString t = tr("已选中样本: %1 / 绘图样本: %2").arg(selectedCount).arg(visibleCount);
    if (m_recalcInProgress)
        t += tr("  · 计算中…");
    m_selectedStatsLabel->setText(t);
}

// 统一调度一次重绘与图例刷新，避免同一事件内重复调用造成卡顿
void ChromatographDataProcessDialog::scheduleRedraw()
{
    if (!m_drawScheduled) {
        m_drawScheduled = true;
        QTimer::singleShot(0, this, [this]{
            m_drawScheduled = false;
            recalculateAndUpdatePlot();
            updateSelectedStatsInfo();
        });
    }
}

void ChromatographDataProcessDialog::ensureSelectedListItem(int sampleId, const QString& displayName)
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

void ChromatographDataProcessDialog::removeSelectedListItem(int sampleId)
{
    QListWidgetItem* item = m_selectedItemMap.take(sampleId);
    if (item) delete item;
}

void ChromatographDataProcessDialog::prefetchCurveIfNeeded(int sampleId)
{
    if (m_curveCache.contains(sampleId)) return;
    QString dataType = QStringLiteral("色谱");
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
void ChromatographDataProcessDialog::onSelectedSamplesListItemChanged(QListWidgetItem* item)
{
    if (!item) return;
    const bool isChecked = (item->checkState() == Qt::Checked);
    const int sampleId = item->data(Qt::UserRole).toInt();

    int oldVisibleCount = m_visibleSamples.size();

    // 这里只做“可见集合 + 一次重绘”控制，避免重复触发导致二次/三次重算
    if (isChecked) {
        m_visibleSamples.insert(sampleId);
    } else {
        m_visibleSamples.remove(sampleId);
    }

    const int newVisibleCount = m_visibleSamples.size();

    if (isChecked && newVisibleCount >= 2 && newVisibleCount > oldVisibleCount)
        promptChromatographReferenceSampleIfNeeded(oldVisibleCount);

    scheduleRedraw();
    updateSelectedStatsInfo();
}

// // 显示/隐藏导航树
// void ChromatographDataProcessDialog::onToggleNavigatorClicked()
// {
//     if (m_leftPanel->isVisible()) {
//         m_leftPanel->hide();
//     } else {
//         m_leftPanel->show();
//     }
    
//     // 调整分割器大小
//     if (m_leftPanel->isVisible()) {
//         QList<int> sizes;
//         sizes << 200 << 700 << 20;
//         m_mainSplitter->setSizes(sizes);
//     } else {
//         QList<int> sizes;
//         sizes << 0 << 900 << 20;
//         m_mainSplitter->setSizes(sizes);
//     }
// }

// 处理左侧导航器的右键菜单
void ChromatographDataProcessDialog::onLeftNavigatorContextMenu(const QPoint &pos)
{
    // // 获取点击位置的项
    // QTreeWidgetItem *item = m_leftNavigator->itemAt(pos);
    // if (!item) return;
    
    // // 检查是否为批次节点
    // bool isBatchNode = false;
    
    // // 方法1：通过层级判断（第二层为批次节点）
    // int level = 0;
    // QTreeWidgetItem *parent = item->parent();
    // while (parent) {
    //     level++;
    //     parent = parent->parent();
    // }
    // if (level == 1) {
    //     isBatchNode = true;
    // }
    
    // // 方法2：通过节点数据判断
    // QVariant itemData = item->data(0, Qt::UserRole);
    // if (itemData.isValid() && itemData.canConvert<NavigatorNodeInfo>()) {
    //     NavigatorNodeInfo nodeInfo = itemData.value<NavigatorNodeInfo>();
    //     if (nodeInfo.type == NavigatorNodeInfo::Batch) {
    //         isBatchNode = true;
    //     }
    // }
    
    // // 如果是批次节点，显示右键菜单
    // if (isBatchNode) {
    //     // 保存当前批次节点
    //     m_currentBatchItem = item;
        
    //     // 创建右键菜单
    //     QMenu contextMenu(this);
    //     QAction *selectAllAction = contextMenu.addAction(tr("选择该批次下所有样本"));
        
    //     // 连接动作
    //     connect(selectAllAction, &QAction::triggered, this, &ChromatographDataProcessDialog::onSelectAllSamplesInBatch);
        
    //     // 显示菜单
    //     contextMenu.exec(m_leftNavigator->mapToGlobal(pos));
    // }
}

// // 批量选择批次下所有样本
// void ChromatographDataProcessDialog::onSelectAllSamplesInBatch()
// {
//     DEBUG_LOG << "开始批量添加样本";
    
//     // 获取当前选中的项
//     QTreeWidgetItem* currentItem = m_leftNavigator->currentItem();
//     DEBUG_LOG << "当前选中项:" << currentItem->text(0);
    
//     // 重要：直接使用右键菜单触发时的批次项
//     // 这是最可靠的方式，因为右键菜单只会在批次节点上显示"选择该批次下所有样本"选项
//     if (m_currentBatchItem) {
//         DEBUG_LOG << "使用右键菜单设置的批次节点:" << m_currentBatchItem->text(0);
//     }
//     // 如果m_currentBatchItem未设置，尝试从当前选中项识别批次
//     else if (currentItem) {
//         // 直接将当前选中项设为批次项
//         m_currentBatchItem = currentItem;
//         DEBUG_LOG << "将当前选中项设为批次项:" << currentItem->text(0);
        
//         // 记录节点信息用于调试
//         QString itemText = currentItem->text(0);
//         DEBUG_LOG << "节点文本:" << itemText;
        
//         // 记录节点类型
//         QVariant itemData = currentItem->data(0, Qt::UserRole);
//         if (itemData.isValid() && itemData.canConvert<NavigatorNodeInfo>()) {
//             NavigatorNodeInfo nodeInfo = itemData.value<NavigatorNodeInfo>();
//             DEBUG_LOG << "节点类型:" << nodeInfo.type;
//         }
        
//         // 记录子节点信息
//         int childCount = currentItem->childCount();
//         DEBUG_LOG << "子节点数量:" << childCount;
//         for (int i = 0; i < childCount && i < 5; i++) { // 最多显示5个子节点
//             QTreeWidgetItem* childItem = currentItem->child(i);
//             if (childItem) {
//                 DEBUG_LOG << "子节点" << i << "文本:" << childItem->text(0);
//             }
//         }
        
//         // 记录父节点信息
//         QTreeWidgetItem* parentItem = currentItem->parent();
//         if (parentItem) {
//             DEBUG_LOG << "父节点文本:" << parentItem->text(0);
//         }
//     }
//     // 如果没有选中项也没有m_currentBatchItem，尝试查找第一个批次节点
//     else {
//         DEBUG_LOG << "尝试查找第一个批次节点";
        
//         // 遍历所有顶级项
//         for (int i = 0; i < m_leftNavigator->topLevelItemCount(); i++) {
//             QTreeWidgetItem* topItem = m_leftNavigator->topLevelItem(i);
//             if (!topItem) continue;
            
//             // 如果是项目节点，查找其下的批次节点
//             if (topItem->text(0).contains("项目")) {
//                 for (int j = 0; j < topItem->childCount(); j++) {
//                     QTreeWidgetItem* batchCandidate = topItem->child(j);
//                     if (batchCandidate) {
//                         // 检查是否为批次节点
//                         bool isBatch = false;
                        
//                         // 通过文本判断
//                         if (batchCandidate->text(0).contains("批次")) {
//                             isBatch = true;
//                         }
                        
//                         // 通过类型判断
//                         QVariant batchData = batchCandidate->data(0, Qt::UserRole);
//                         if (batchData.isValid() && batchData.canConvert<NavigatorNodeInfo>()) {
//                             NavigatorNodeInfo batchInfo = batchData.value<NavigatorNodeInfo>();
//                             if (batchInfo.type == NavigatorNodeInfo::Batch) {
//                                 isBatch = true;
//                             }
//                         }
                        
//                         if (isBatch) {
//                             m_currentBatchItem = batchCandidate;
//                             DEBUG_LOG << "找到批次节点:" << batchCandidate->text(0);
//                             break;
//                         }
//                     }
//                 }
//             }
            
//             if (m_currentBatchItem) break;
//         }
//     }
    
//     // 如果仍然没有找到批次节点，报错并返回
//     if (!m_currentBatchItem) {
//         DEBUG_LOG << "未找到有效批次节点";
//         QMessageBox::warning(this, tr("错误"), tr("未选中有效的批次"));
//         return;
//     }
    
//     DEBUG_LOG << "成功识别批次节点:" << m_currentBatchItem->text(0);

    
//     try {
//         // 获取当前选中的样本ID集合，用于避免重复选择
//         QMap<int, QString> selectedSamples;
//         try {
//             selectedSamples = getSelectedSamples();
//             DEBUG_LOG << "当前已选中样本数量:" << selectedSamples.size();
//         } catch (const std::exception& e) {
//             DEBUG_LOG << "获取已选样本时发生异常:" << e.what();
//             selectedSamples.clear();
//         } catch (...) {
//             DEBUG_LOG << "获取已选样本时发生未知异常";
//             selectedSamples.clear();
//         }
        
//         QSet<int> selectedIds;
//         if (!selectedSamples.isEmpty()) {
//             selectedIds = QSet<int>(selectedSamples.keys().begin(), selectedSamples.keys().end());
//         }
        
//         // 遍历批次下的所有样本
//         int addedCount = 0;
//         int totalSamples = 0;
//         int childCount = 0;
        
//         try {
//             childCount = m_currentBatchItem->childCount();
//             DEBUG_LOG << "批次下样本数量:" << childCount;
//         } catch (const std::exception& e) {
//             DEBUG_LOG << "获取批次子项数量时发生异常:" << e.what();
//             QMessageBox::warning(this, tr("错误"), tr("获取批次样本信息失败"));
//             return;
//         } catch (...) {
//             DEBUG_LOG << "获取批次子项数量时发生未知异常";
//             QMessageBox::warning(this, tr("错误"), tr("获取批次样本信息失败"));
//             return;
//         }
        
//         for (int i = 0; i < childCount; i++) {
//             QTreeWidgetItem *sampleItem = nullptr;
            
//             try {
//                 sampleItem = m_currentBatchItem->child(i);
//             } catch (const std::exception& e) {
//                 DEBUG_LOG << "获取样本项时发生异常:" << e.what();
//                 continue;
//             } catch (...) {
//                 DEBUG_LOG << "获取样本项时发生未知异常";
//                 continue;
//             }
            
//             if (!sampleItem) {
//                 DEBUG_LOG << "样本项为空，跳过";
//                 continue;
//             }
            
//             // 获取样本ID和数据类型
//             int sampleId = -1;
//             QString sampleName;
//             QString dataType;
            
//             try {
//                 bool ok = false;
//                 QVariant idVariant = sampleItem->data(0, Qt::UserRole);
//                 if (idVariant.isValid()) {
//                     sampleId = idVariant.toInt(&ok);
//                 }
                
//                 if (!ok || sampleId <= 0) {
//                     DEBUG_LOG << "样本ID无效:" << idVariant;
//                     continue;
//                 }
                
//                 sampleName = sampleItem->text(0);
                
//                 QVariant typeVariant = sampleItem->data(0, Qt::UserRole + 1);
//                 if (typeVariant.isValid()) {
//                     dataType = typeVariant.toString();
//                 }
                
//                 DEBUG_LOG << "处理样本:" << sampleId << sampleName << dataType;
//             } catch (const std::exception& e) {
//                 DEBUG_LOG << "获取样本数据时发生异常:" << e.what();
//                 continue;
//             } catch (...) {
//                 DEBUG_LOG << "获取样本数据时发生未知异常";
//                 continue;
//             }
            
//             // 处理样本类型判断
//             bool isValidSampleType = false;
            
//             // 检查是否为色谱类型样本或未指定类型
//             if (dataType.isEmpty()) {
//                 // 未指定类型的样本默认视为有效
//                 isValidSampleType = true;
//                 DEBUG_LOG << "样本类型为空，默认视为有效样本";
//             } else if (dataType.contains("色谱", Qt::CaseInsensitive)) {
//                 // 包含"色谱"字样的样本类型视为有效
//                 isValidSampleType = true;
//                 DEBUG_LOG << "样本类型包含'色谱'，视为有效样本";
//             } else if (dataType == "TG" || dataType == "tg" || dataType == "Tg") {
//                 // TG类型样本也视为有效
//                 isValidSampleType = true;
//                 DEBUG_LOG << "样本类型为TG，视为有效样本";
//             }
            
//             if (isValidSampleType) {
//                 totalSamples++;
                
//                 // 避免重复选择
//                 if (!selectedIds.contains(sampleId)) {
//                     try {
//                         // 添加样本曲线
//                         DEBUG_LOG << "添加样本曲线:" << sampleId << sampleName;
                        
//                         // 检查样本ID是否有效
//                         if (sampleId <= 0) {
//                             DEBUG_LOG << "样本ID无效:" << sampleId;
//                             continue;
//                         }
                        
//                         addSampleCurve(sampleId, sampleName);
                        
//                         // 更新样本项的选中状态
//                         if (sampleItem) {
//                             sampleItem->setCheckState(0, Qt::Checked);
//                         }
                        
//                         addedCount++;
//                     } catch (const std::exception& e) {
//                         DEBUG_LOG << "添加样本曲线时发生异常:" << e.what();
//                         continue;
//                     } catch (...) {
//                         DEBUG_LOG << "添加样本曲线时发生未知异常";
//                         continue;
//                     }
//                 } else {
//                     DEBUG_LOG << "样本已添加，跳过:" << sampleId;
//                 }
//             } else {
//                 DEBUG_LOG << "样本类型不匹配，跳过:" << dataType;
//             }
//         }
        
//         // 更新图表显示
//         try {
//             DEBUG_LOG << "更新图表显示";
//             updatePlot();
//             updateLegendPanel();
//         } catch (const std::exception& e) {
//             DEBUG_LOG << "更新图表显示时发生异常:" << e.what();
//         } catch (...) {
//             DEBUG_LOG << "更新图表显示时发生未知异常";
//         }
        
//         // 显示添加结果
//         DEBUG_LOG << "添加结果: 总样本=" << totalSamples << " 添加数量=" << addedCount;
//         if (addedCount > 0) {
//             QMessageBox::information(this, tr("批量添加成功"), 
//                 tr("成功添加 %1 个样本\n\n共处理 %2 个有效样本").arg(addedCount).arg(totalSamples));
//         } else if (totalSamples == 0) {
//             QMessageBox::warning(this, tr("未找到有效样本"), 
//                 tr("批次下没有可添加的色谱类型样本\n\n请确认批次中包含色谱类型的样本"));
//         } else {
//             QMessageBox::information(this, tr("无新样本可添加"), 
//                 tr("批次中的所有有效样本(%1个)已经添加\n\n没有新的样本可添加").arg(totalSamples));
//         }
//     } catch (const std::exception& e) {
//         DEBUG_LOG << "批量添加样本时发生异常:" << e.what();
//         QMessageBox::warning(this, tr("错误"), tr("批量添加样本时发生错误: %1").arg(e.what()));
//     } catch (...) {
//         DEBUG_LOG << "批量添加样本时发生未知异常";
//         QMessageBox::warning(this, tr("错误"), tr("批量添加样本时发生未知错误"));
//     }
    
//     DEBUG_LOG << "批量添加样本完成";
// }


void ChromatographDataProcessDialog::onSelectAllSamplesInBatch(const QString& projectName, const QString& batchCode)
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
            // 添加到 m_selectedSamples：使用完整样本名称
            QVariantMap info = m_sampleDao.getSampleById(sample.sampleId);
            QString fullName = info.value("sample_name").toString();
            if (fullName.trimmed().isEmpty()) fullName = QString("样本 %1").arg(sample.sampleId);
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
                            DEBUG_LOG << "已勾选色谱样本复选框:" << sample.sampleId;
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
            DEBUG_LOG << "添加样本:" << sample.sampleId << sample.shortCode;
        } else {
            DEBUG_LOG << "样本已存在，跳过:" << sample.sampleId;
        }
    }

    DEBUG_LOG << "批量添加完成，总样本数:" << totalSamples << " 新增数:" << addedCount;
    // 批次添加后合并触发一次自动重算，完成后在 onCalculationFinished 中调用 updatePlot()
    scheduleRedraw();
    // 先更新图例，让用户即时看到已选样本列表，曲线将在计算完成后显示
    updateLegendPanel();

    // 显示提示
    if (addedCount > 0) {
        QMessageBox::information(this, tr("批量添加成功"),
                                 tr("成功添加 %1 个样本\n共处理 %2 个有效样本ss")
                                 .arg(addedCount)
                                 .arg(totalSamples));
    } else if (totalSamples == 0) {
        QMessageBox::warning(this, tr("未找到有效样本"),
                             tr("批次下没有可添加的色谱类型样本"));
    } else {
        QMessageBox::information(this, tr("无新样本可添加"),
                                 tr("批次中的所有有效样本(%1 个)已经添加")
                                 .arg(totalSamples));
    }

    // InfoAutoClose* dlg = new InfoAutoClose(this); // this为父窗口

    // if (addedCount > 0) {
    //     dlg->showMessage(
    //         tr("批量添加成功"), // 标题
    //         tr("成功添加 %1 个样本\n共处理 %2 个有效样本")
    //         .arg(addedCount)
    //         .arg(totalSamples),
    //         3000 // 自动关闭2秒
    //     );
    //     // dlg->setWindowTitle(tr("批量添加成功"));
    // } 
    // else if (totalSamples == 0) {
    //     dlg->showMessage(
    //         tr("未找到有效样本"), // 标题
    //         tr("批次下没有可添加的类型样本"),
    //         3000
    //     );
    //     // dlg->setWindowTitle(tr("未找到有效样本"));
    // } 
    // else {
    //     dlg->showMessage(
    //         tr("无新样本可添加"), // 标题
    //         tr("批次中的所有有效样本(%1 个)已经添加")
    //         .arg(totalSamples),
    //         3000
    //     );
    //     // dlg->setWindowTitle(tr("无新样本可添加"));
    // }


    DEBUG_LOG << "批量添加完成，总样本数:" << totalSamples << " 新增数:" << addedCount;
}


void ChromatographDataProcessDialog::loadNavigatorData()
{
    try {
        // 清空现有项目
        m_leftNavigator->clear();
        
        // 检查是否有主界面导航树引用
        if (!m_mainNavigator) {
            // 没有主界面导航树引用，从数据库加载色谱数据
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

void ChromatographDataProcessDialog::loadNavigatorDataFromDatabase()
{
    // 获取色谱样本数据
    m_chromatographTgSamples = m_sampleDao.getSamplesByDataType("色谱");
    
    // 按项目分组
    QMap<QString, QTreeWidgetItem*> projectItems;
    QMap<QString, QMap<QString, QTreeWidgetItem*>> batchItems;
    
    for (const auto &sample : m_chromatographTgSamples) {
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

void ChromatographDataProcessDialog::loadNavigatorDataFromMainNavigator()
{
    // 获取主界面导航树的数据源根节点
    QTreeWidgetItem* dataSourceRoot = m_mainNavigator->getDataSourceRoot();
    if (!dataSourceRoot) {
        DEBUG_LOG << "ChromatographDataProcessDialog: 无法获取主界面导航树数据源根节点";
        return;
    }
    
    // 按项目分组
    QMap<QString, QTreeWidgetItem*> projectItems;
    QMap<QString, QMap<QString, QTreeWidgetItem*>> batchItems;
    
    // 遍历主界面导航树，筛选色谱数据（仅复制主导航“已选中”的样本）
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
                    
                    // 检查是否为色谱数据类型
                    QVariant nodeData = dataTypeItem->data(0, Qt::UserRole);
                    if (nodeData.canConvert<NavigatorNodeInfo>()) {
                        NavigatorNodeInfo nodeInfo = nodeData.value<NavigatorNodeInfo>();
                        
                        if (nodeInfo.type == NavigatorNodeInfo::DataType && 
                             nodeInfo.dataType == "色谱") {
                            
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
                                        m_suppressItemChanged = true;
                                        newSampleItem->setCheckState(0, Qt::Checked);
                                        m_suppressItemChanged = false;

                                        // 同步到本界面的已选/可见集合，避免“节点已勾选但图像不显示”
                                        m_selectedSamples[sampleInfo.id] = buildSampleDisplayName(sampleInfo.id);
                                        m_visibleSamples.insert(sampleInfo.id);
                                    }
                                }
                            }
                        }
                    }
                 }
             }
         }
     }

    // 初次加载导航后刷新“选中样本”列表并触发一次绘图
    updateSelectedSamplesList();
    scheduleRedraw();
}

void ChromatographDataProcessDialog::loadSampleCurve(int sampleId)
{
    try {
        DEBUG_LOG << "ChromatographDataProcessDialog::loadSampleCurve - Loading curve for sampleId:" << sampleId;
        
        // 不再单独加载一个样本曲线，统一走自动重算流程
        scheduleRedraw();
        return;
        
    } catch (const std::exception& e) {
        DEBUG_LOG << "ChromatographDataProcessDialog::loadSampleCurve - 加载样本曲线失败:" << e.what();
        QMessageBox::warning(this, tr("警告"), tr("加载样本曲线失败: %1").arg(e.what()));
    }
}

void ChromatographDataProcessDialog::onLeftItemClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column)
    
    if (!item) return;
    
    // updateRightPanel(item);
}

void ChromatographDataProcessDialog::onSampleItemClicked(QTreeWidgetItem *item, int column)
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
        
        // 确保导航树标题显示为"色谱 - 样本导航"
        m_leftNavigator->setHeaderLabel(tr("色谱 - 样本导航"));
        
        DEBUG_LOG << "色谱样本已选中:" << sampleId << sampleFullName;
    }
}



void ChromatographDataProcessDialog::onCancelButtonClicked()
{
    // 在MDI模式下，close()会关闭MDI子窗口
    close();
}


void ChromatographDataProcessDialog::onParameterSettingsClicked()
{


    // // 1. 创建对话框实例
    // //    m_currentParams 是 Workbench 的一个 ProcessingParameters 成员变量
    // ChromatographParameterSettingsDialog *dialog = new ChromatographParameterSettingsDialog(m_currentParams, this);

    // // 2. 【关键】连接对话框的 `parametersApplied` 信号到 Workbench 的槽函数
    // connect(dialog, &ChromatographParameterSettingsDialog::parametersApplied, this, &ChromatographDataProcessDialog::onParametersApplied);

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
void ChromatographDataProcessDialog::onParametersApplied(const ProcessingParameters &newParams)
{
    DEBUG_LOG << "ChromatographDataProcessDialog::onParametersApplied - Processing parameters:" << newParams.toString();
    // 1. 更新 Workbench 内部缓存的参数
    m_currentParams = newParams;

    DEBUG_LOG << "ChromatographDataProcessDialog::onParametersApplied - Current parameters:" << m_currentParams.toString();

    // 2. 触发重新计算和绘图的流程
    recalculateAndUpdatePlot();
}


// void ChromatographDataProcessDialog::recalculateAndUpdatePlot()
// {
//     // --- 1. UI 反馈 ---
//     QApplication::setOverrideCursor(Qt::WaitCursor);
//     m_parameterButton->setEnabled(false);

//     // --- 2. 异步调用 Service 层 ---
    
//     // 【升级】获取所有需要处理的样本ID列表
//     const QList<int> sampleIds = m_selectedSamples.keys();
//     // DEBUG_LOG << "ChromatographDataProcessDialog::recalculateAndUpdatePlot() - Processing samples:" << sampleIds;
//     QStringList idStrList;
//     for (int id : sampleIds)
//         idStrList << QString::number(id);

//     DEBUG_LOG << "ChromatographDataProcessDialog::recalculateAndUpdatePlot() - Processing samples:"
//             << idStrList.join(", ");

//     // 【升级】调用新的批量处理方法
//     QFuture<BatchMultiStageData> future = QtConcurrent::run(
//         m_processingService,
//         &DataProcessingService::runChromatographPipelineForMultiple,
//         sampleIds,       // 传递 ID 列表
//         m_currentParams
//     );
//     DEBUG_LOG << "ChromatographDataProcessDialog::recalculateAndUpdatePlot() - Future created";
    
//     // --- 3. 监控后台任务 ---
//     QFutureWatcher<BatchMultiStageData>* watcher = new QFutureWatcher<BatchMultiStageData>(this);
//     connect(watcher, &QFutureWatcher<BatchMultiStageData>::finished, this, &ChromatographDataProcessDialog::onCalculationFinished);
//     watcher->setFuture(future);

//     DEBUG_LOG << "ChromatographDataProcessDialog::recalculateAndUpdatePlot() - Watcher created";
// }

// void ChromatographDataProcessDialog::onCalculationFinished()
// {
//     DEBUG_LOG << "ChromatographDataProcessDialog::onCalculationFinished()";
//     // QFutureWatcher<BatchMultiStageData>* watcher = qobject_cast<QFutureWatcher<BatchMultiStageData>*>(sender());
//     QFutureWatcher<BatchMultiStageData>* watcher = static_cast<QFutureWatcher<BatchMultiStageData>*>(sender());

//     if (!watcher) { /* ... 恢复UI ... */ return; }

//     // a. 【升级】获取并缓存批量结果
//     m_stageDataCache.clear(); // 需要为 BatchMultiStageData 实现 clear
//     m_stageDataCache = watcher->result();
//     watcher->deleteLater();

//     // 【新】当数据处理完成后，使“计算差异度”按钮可用
//     m_startComparisonButton->setEnabled(m_selectedSamples.count() >= 2);

//     // // b. 调用绘图更新
//     // updatePlot(); 
    
//     // c. 恢复 UI
//     QApplication::restoreOverrideCursor();
//     m_parameterButton->setEnabled(true);

//     DEBUG_LOG << "ChromatographDataProcessDialog::onCalculationFinished() - Finished";
// }


void ChromatographDataProcessDialog::recalculateAndUpdatePlot()
{
    if (m_recalcInProgress) {
        m_recalcPending = true;
        return;
    }

    // --- 1. UI 反馈 ---
    QApplication::setOverrideCursor(Qt::WaitCursor);
    m_parameterButton->setEnabled(false);
    m_recalcInProgress = true;
    updateSelectedStatsInfo();

    // --- 2. 获取所有需要处理的样本ID列表 ---
    // 仅处理左侧“选中样本”列表中被勾选（可见）的样本
    QList<int> sampleIds;
    for (int id : m_visibleSamples) {
        sampleIds.append(id);
    }
    std::sort(sampleIds.begin(), sampleIds.end());
    if (sampleIds.isEmpty()) {
        DEBUG_LOG << "recalculateAndUpdatePlot - 可见样本为空，跳过处理";
        if (m_chartView1) m_chartView1->clearGraphs();
        if (m_chartView2) m_chartView2->clearGraphs();
        if (m_chartView3) m_chartView3->clearGraphs();
        if (m_chartView4) m_chartView4->clearGraphs();
        updateLegendPanel();
        QApplication::restoreOverrideCursor();
        m_parameterButton->setEnabled(true);
        m_recalcInProgress = false;
        updateSelectedStatsInfo();

        if (m_recalcPending) {
            m_recalcPending = false;
            QTimer::singleShot(0, this, [this]{ recalculateAndUpdatePlot(); });
        }
        return;
    }

    if (m_currentParams.referenceSampleId <= 0 || !sampleIds.contains(m_currentParams.referenceSampleId)) {
        m_currentParams.referenceSampleId = sampleIds.first();
        DEBUG_LOG << "Chromatograph alignment reference sample auto-set to:" << m_currentParams.referenceSampleId;
    }

    // 对齐阶段必须基于“裁剪后曲线”进行；若参考样本没有裁剪曲线，则自动切换到首个有裁剪曲线的样本
    if (m_currentParams.alignmentEnabled) {
        auto hasClipStage = [this](int sid) -> bool {
            for (auto it = m_stageDataCache.constBegin(); it != m_stageDataCache.constEnd(); ++it) {
                const SampleGroup& group = it.value();
                for (const SampleDataFlexible& s : group.sampleDatas) {
                    if (s.sampleId != sid) continue;
                    for (const StageData& st : s.stages) {
                        if (st.stageName == StageName::Clip && st.curve) return true;
                    }
                }
            }
            return false;
        };

        if (!hasClipStage(m_currentParams.referenceSampleId)) {
            for (int sid : sampleIds) {
                if (hasClipStage(sid)) {
                    m_currentParams.referenceSampleId = sid;
                    DEBUG_LOG << "Chromatograph alignment reference auto-switched to clip-ready sample:" << sid;
                    break;
                }
            }
        }
    }

    QStringList idStrList;
    for (int id : sampleIds)
        idStrList << QString::number(id);

    DEBUG_LOG << "ChromatographDataProcessDialog::recalculateAndUpdatePlot() - Processing samples:"
              << idStrList.join(", ");

    // --- 3. 异步调用 Service 层 (使用新数据结构 BatchGroupData) ---
    QFuture<BatchGroupData> future = QtConcurrent::run(
        m_processingService,
        &DataProcessingService::runChromatographPipelineForMultiple,
        sampleIds,
        m_currentParams
    );

    DEBUG_LOG << "ChromatographDataProcessDialog::recalculateAndUpdatePlot() - Future created";

    // --- 4. 监控后台任务 ---
    QFutureWatcher<BatchGroupData>* watcher = new QFutureWatcher<BatchGroupData>(this);
    connect(watcher, &QFutureWatcher<BatchGroupData>::finished,
            this, &ChromatographDataProcessDialog::onCalculationFinished);
    watcher->setFuture(future);

    DEBUG_LOG << "ChromatographDataProcessDialog::recalculateAndUpdatePlot() - Watcher created";
}


void ChromatographDataProcessDialog::onCalculationFinished()
{
    DEBUG_LOG << "ChromatographDataProcessDialog::onCalculationFinished()";

    QFutureWatcher<BatchGroupData>* watcher = static_cast<QFutureWatcher<BatchGroupData>*>(sender());
    if (!watcher) {
        QApplication::restoreOverrideCursor();
        m_parameterButton->setEnabled(true);
        m_recalcInProgress = false;
        updateSelectedStatsInfo();
        return;
    }

    // --- 1. 获取并缓存批量结果 ---
    m_stageDataCache.clear(); // m_stageDataCache 类型需要为 BatchGroupData
    m_stageDataCache = watcher->result();
    watcher->deleteLater();

    DEBUG_LOG << "BatchGroupData size:" << m_stageDataCache.size();

    // b. 调用绘图更新（异常时仍恢复光标，避免一直显示等待）
    try {
        updatePlot();
    } catch (const std::exception& e) {
        WARNING_LOG << "ChromatographDataProcessDialog::updatePlot exception:" << e.what();
    } catch (...) {
        WARNING_LOG << "ChromatographDataProcessDialog::updatePlot unknown exception";
    }

    // --- 2. 启用“计算差异度”按钮（至少两条可比较曲线） ---
    int comparableCurveCount = 0;
    for (auto it = m_stageDataCache.constBegin(); it != m_stageDataCache.constEnd(); ++it) {
        const SampleGroup& group = it.value();
        for (const SampleDataFlexible& sample : group.sampleDatas) {
            for (const StageData& stage : sample.stages) {
                if (stage.useForPlot && !stage.curve.isNull()) {
                    ++comparableCurveCount;
                }
            }
        }
    }
    m_startComparisonButton->setEnabled(comparableCurveCount >= 2);

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
    m_recalcInProgress = false;
    updateSelectedStatsInfo();

    if (m_recalcPending) {
        m_recalcPending = false;
        QTimer::singleShot(0, this, [this]{ recalculateAndUpdatePlot(); });
    }

    DEBUG_LOG << "ChromatographDataProcessDialog::onCalculationFinished() - Finished";
}



// 确保包含了 ChartView.h 和 core/entities/Curve.h
#include "gui/views/ChartView.h"
#include "core/entities/Curve.h"

// ... (其他函数的实现)




// ...


void ChromatographDataProcessDialog::updatePlot()
{
    // --- 0. 安全检查 ---
    if (m_stageDataCache.isEmpty()) {
        qWarning() << "updatePlot called with empty data cache. Clearing all charts.";
        // 清空所有图表
        m_chartView1->clearGraphs();
        m_chartView2->clearGraphs();
        m_chartView3->clearGraphs();
        m_chartView4->clearGraphs();
        return;
    }

    printBatchGroupData(m_stageDataCache);

    // 统一颜色映射：同一样本在原始/基线/峰标记三图保持一致颜色
    QList<int> orderedSampleIds = m_visibleSamples.values();
    std::sort(orderedSampleIds.begin(), orderedSampleIds.end());
    QHash<int, QColor> sampleColorMap;
    sampleColorMap.reserve(orderedSampleIds.size());
    for (int i = 0; i < orderedSampleIds.size(); ++i) {
        sampleColorMap.insert(orderedSampleIds[i], ColorUtils::setCurveColor(i));
    }

    // --- 1. 更新【原始数据】图表 (m_chartView1) ---
    m_chartView1->clearGraphs();
    m_chartView1->setLabels(tr("响应时间"), tr("强度"));
    m_chartView1->setPlotTitle(QStringLiteral("原始数据"));

    for (auto groupIt = m_stageDataCache.constBegin(); groupIt != m_stageDataCache.constEnd(); ++groupIt) {
        const SampleGroup &group = groupIt.value();
        for (const auto &sample : group.sampleDatas) {
            const int sampleId = sample.sampleId;
            if (!m_visibleSamples.contains(sampleId)) continue;

            for (const auto &stage : sample.stages) {
                if (stage.stageName == StageName::RawData && stage.curve) {
                    QSharedPointer<Curve> rawCurve = stage.curve;
                    SingleTobaccoSampleDAO dao;
                    SampleIdentifier sid = dao.getSampleIdentifierById(sampleId);
                    const QString legendName = QString("%1-%2-%3-%4")
                                                .arg(sid.projectName)
                                                .arg(sid.batchCode)
                                                .arg(sid.shortCode)
                                                .arg(sid.parallelNo)
                                                + chromatographLegendExtraTags(sid);
                    rawCurve->setName(legendName);
                    rawCurve->setColor(sampleColorMap.value(sampleId, QColor(200, 30, 30)));
                    m_chartView1->addCurve(rawCurve);
                    break;
                }
            }
        }
    }
    m_chartView1->setLegendVisible(false);
    m_chartView1->replot();


    // --- 2. 更新【基线校正】图表 (m_chartView2) ---
    m_chartView2->clearGraphs();
    m_chartView2->setLabels(tr("响应时间"), tr("强度"));
    m_chartView2->setPlotTitle(QStringLiteral("基线校正"));

    // --- 3. 更新【裁剪数据】图表 (m_chartView3) ---
    m_chartView3->clearGraphs();
    m_chartView3->setLabels(tr("响应时间"), tr("强度"));
    m_chartView3->setPlotTitle(QStringLiteral("裁剪数据"));

    // --- 4. 更新【色谱对齐】图表 (m_chartView4) ---
    m_chartView4->clearGraphs();
    m_chartView4->setLabels(tr("响应时间"), tr("强度"));
    m_chartView4->setPlotTitle(QStringLiteral("色谱对齐"));

    for (auto groupIt = m_stageDataCache.constBegin(); groupIt != m_stageDataCache.constEnd(); ++groupIt) {
        const SampleGroup& group = groupIt.value();

        for (const auto& sample : group.sampleDatas) {
            const int sampleId = sample.sampleId;
            if (!m_visibleSamples.contains(sampleId)) continue;

            SingleTobaccoSampleDAO dao;
            SampleIdentifier sid = dao.getSampleIdentifierById(sampleId);
            const QString legendName = QString("%1-%2-%3-%4")
                                          .arg(sid.projectName)
                                          .arg(sid.batchCode)
                                          .arg(sid.shortCode)
                                          .arg(sid.parallelNo)
                                          + chromatographLegendExtraTags(sid);
            const QColor color = sampleColorMap.value(sampleId, QColor(200, 30, 30));

            for (const auto& stage : sample.stages) {
                if (!stage.curve) continue;

                if (stage.stageName == StageName::BaselineCorrection) {
                    QSharedPointer<Curve> curve = stage.curve;
                    curve->setName(legendName);
                    curve->setColor(color);
                    m_chartView2->addCurve(curve);

                    if (m_currentParams.baselineOverlayRaw) {
                        for (const auto& rawStage : sample.stages) {
                            if (rawStage.stageName == StageName::RawData && rawStage.curve) {
                                QSharedPointer<Curve> rawCurve = rawStage.curve;
                                rawCurve->setName(legendName + QStringLiteral(" (原始叠加)"));
                                rawCurve->setColor(color);
                                rawCurve->setLineStyle(Qt::DashLine);
                                m_chartView2->addCurve(rawCurve);
                                break;
                            }
                        }
                    }
                } else if (stage.stageName == StageName::Clip) {
                    QSharedPointer<Curve> curve = stage.curve;
                    curve->setName(legendName);
                    curve->setColor(color);
                    m_chartView3->addCurve(curve);
                } else if (stage.stageName == StageName::PeakAlignment) {
                    QSharedPointer<Curve> curve = stage.curve;
                    curve->setName(legendName);
                    curve->setColor(color);
                    m_chartView4->addCurve(curve);
                }
            }

            if (sample.sampleId == m_currentParams.referenceSampleId) {
                bool hasAligned = false;
                for (const auto& stage : sample.stages) {
                    if (stage.stageName == StageName::PeakAlignment && stage.curve) {
                        hasAligned = true;
                        break;
                    }
                }
                if (!hasAligned) {
                    // 对齐图中的参考曲线优先使用“裁剪数据”，与 MATLAB 流程一致
                    QSharedPointer<Curve> refCurve;
                    for (const auto& stage : sample.stages) {
                        if (stage.stageName == StageName::Clip && stage.curve) {
                            refCurve = stage.curve;
                            break;
                        }
                    }
                    // 仅在无裁剪阶段时兜底到基线校正
                    if (refCurve.isNull()) {
                        for (const auto& stage : sample.stages) {
                            if (stage.stageName == StageName::BaselineCorrection && stage.curve) {
                                refCurve = stage.curve;
                                break;
                            }
                        }
                    }

                    if (!refCurve.isNull()) {
                        refCurve->setName(legendName + QStringLiteral(" (参考)"));
                        // 参考曲线在对齐图中保持与其它图一致的样本颜色
                        refCurve->setColor(color);
                        refCurve->setLineWidth(3);
                        m_chartView4->addCurve(refCurve);
                    }
                }
            }
        }
    }

    m_chartView2->setLegendVisible(false);
    m_chartView2->replot();
    m_chartView3->setLegendVisible(false);
    m_chartView3->replot();
    m_chartView4->setLegendVisible(false);
    m_chartView4->replot();


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

    updateLegendPanel();
}

void ChromatographDataProcessDialog::updateLegendPanel()
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
        SingleTobaccoSampleDAO sidDao;
        const SampleIdentifier sid = sidDao.getSampleIdentifierById(sampleId);
        QString legendText = buildSampleDisplayName(sampleId) + chromatographLegendExtraTags(sid);

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




void ChromatographDataProcessDialog::onParameterChanged()
{
    // 更新右侧面板显示
    QTreeWidgetItem* currentItem = m_leftNavigator->currentItem();
    if (currentItem) {
        // updateRightPanel(currentItem);
    }
}



void ChromatographDataProcessDialog::onItemChanged(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column)

    if (m_suppressItemChanged) return;
    if (!item || !item->parent() || !item->parent()->parent()) return; // 仅样本节点

    const int sampleId = item->data(0, Qt::UserRole).toInt();
    const bool isChecked = (item->checkState(0) == Qt::Checked);
    const int oldVisibleCount = m_visibleSamples.size();

    if (isChecked) {
        QString sampleFullName = item->data(0, Qt::UserRole + 1).toString();
        if (sampleFullName.trimmed().isEmpty()) {
            sampleFullName = buildSampleDisplayName(sampleId);
        }
        m_selectedSamples[sampleId] = sampleFullName;
        m_visibleSamples.insert(sampleId);
    } else {
        m_selectedSamples.remove(sampleId);
        m_visibleSamples.remove(sampleId);
    }

    updateSelectedSamplesList();

    const int newVisibleCount = m_visibleSamples.size();
    if (isChecked && newVisibleCount >= 2 && newVisibleCount > oldVisibleCount)
        promptChromatographReferenceSampleIfNeeded(oldVisibleCount);

    scheduleRedraw();
    updateSelectedStatsInfo();
}

// 在左侧导航树中查找样本节点（通过样本ID），若不存在返回nullptr
QTreeWidgetItem* ChromatographDataProcessDialog::findSampleItemById(int sampleId) const
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

// 处理主导航样本选中状态变化，仅对 dataType=="色谱" 生效，动态增删导航树节点与曲线
void ChromatographDataProcessDialog::onMainNavigatorSampleSelectionChanged(int sampleId, const QString& sampleName, const QString& dataType, bool selected)
{
    try {
        if (dataType != QStringLiteral("色谱")) {
            return;
        }

        if (selected) {
            QTreeWidgetItem* existing = findSampleItemById(sampleId);
            if (existing) {
                m_suppressItemChanged = true;
                existing->setCheckState(0, Qt::Checked);
                m_suppressItemChanged = false;
                const QString dn = buildSampleDisplayName(sampleId);
                m_selectedSamples.insert(sampleId, dn);
                addSampleCurve(sampleId, dn);
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

            // 创建样本节点（使用统一显示名称）
            QString sampleFullName = buildSampleDisplayName(sampleId);
            QTreeWidgetItem* sampleItem = new QTreeWidgetItem(batchItem, QStringList() << sampleFullName);
            
            sampleItem->setFlags(sampleItem->flags() | Qt::ItemIsUserCheckable);
            sampleItem->setData(0, Qt::UserRole, sampleId);
            m_suppressItemChanged = true;
            sampleItem->setCheckState(0, Qt::Checked);
            m_suppressItemChanged = false;

            // 同步选中样本集合并加入可见曲线（与全局选择、addSampleCurve 逻辑一致）
            m_selectedSamples.insert(sampleId, sampleFullName);
            addSampleCurve(sampleId, sampleFullName);

            loadSampleCurve(sampleId);
        } else {
            QTreeWidgetItem* sampleItem = findSampleItemById(sampleId);
            if (!sampleItem) return;
            removeSampleCurve(sampleId);
            QTreeWidgetItem* batchItem = sampleItem->parent();
            QTreeWidgetItem* projectItem = batchItem ? batchItem->parent() : nullptr;
            delete sampleItem;
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
// void ChromatographDataProcessDialog::onStartComparison()
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
// void ChromatographDataProcessDialog::onStartComparison()
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

// void ChromatographDataProcessDialog::onStartComparison()
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

void ChromatographDataProcessDialog::onStartComparison()
{
    DEBUG_LOG << "ChromatographDataProcessDialog::onStartComparison";
    if (m_stageDataCache.isEmpty()) {
        QMessageBox::warning(this, "提示", "暂无可用于对比的数据。");
        return;
    }

    int comparableCurveCount = 0;
    for (auto it = m_stageDataCache.constBegin(); it != m_stageDataCache.constEnd(); ++it) {
        const SampleGroup& group = it.value();
        for (const SampleDataFlexible& sample : group.sampleDatas) {
            for (const StageData& stage : sample.stages) {
                if (stage.useForPlot && !stage.curve.isNull()) {
                    ++comparableCurveCount;
                }
            }
        }
    }
    if (comparableCurveCount < 2) {
        QMessageBox::warning(this, "提示", "可用于对比的曲线不足（需要至少2条）。");
        return;
    }

    // --- 1. 【关键修正】准备用于显示的样本名称列表 ---
    
    // a. 获取所有选中样本的 ID
    QList<int> selectedIds = m_selectedSamples.keys();
    
    // b. 使用统一显示名称构建 ID 到名称的映射
    QMap<QString, int> nameToIdMap; // Key: 统一显示名称, Value: sampleId
    QStringList displayNames;

    for (int id : selectedIds) {
        QString fullName = buildSampleDisplayName(id);
        if (fullName.trimmed().isEmpty()) {
            fullName = QStringLiteral("样本 %1").arg(id);
        }
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
    emit requestNewChromatographDifferenceWorkbench(referenceSampleId, m_stageDataCache, m_currentParams);
    DEBUG_LOG << "0000000";
}
