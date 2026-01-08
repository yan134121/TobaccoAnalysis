
#include "Logger.h"
#include "core/singletons/StringManager.h"
#include "DataNavigator.h"
#include "core/singletons/SampleSelectionManager.h" // 统一选中管理器，用于维护全局样本选中状态
#include "core/common.h"
#include <QMessageBox>
#include "core/singletons/StringManager.h"

#include <QMenu>                //  QMenu
#include <QContextMenuEvent>    //  QContextMenuEvent
#include <QAction>              //  QAction (虽然QMenu会包含它，但显式包含更清晰)
#include <QTimer>
#include <QMainWindow>          //  QMainWindow
#include <QMdiArea>             //  QMdiArea
#include <QMdiSubWindow>        //  QMdiSubWindow
#include "gui/dialogs/SamplePropertiesDialog.h"
#include "gui/dialogs/SampleDataTableDialog.h"

#include "StackTraceUtils.h"

// 搜索过滤相关头文件
#include <QRegularExpression>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QTextLayout>
#include <QApplication>
#include <QCursor>
#include <QSignalBlocker>



DataNavigator::DataNavigator(QWidget *parent) : QTreeWidget(parent)
{
    setHeaderLabel(tr("数据集"));
    setupTree();
    connect(this, &QTreeWidget::itemExpanded, this, &DataNavigator::onItemExpanded);
    // 【新】连接 itemClicked 信号，用于激活窗口
    connect(this, &QTreeWidget::itemClicked, this, &DataNavigator::onOpenViewItemClicked);
    // 连接双击信号，用于打开样本窗口
    connect(this, &QTreeWidget::itemDoubleClicked, this, &DataNavigator::onItemDoubleClicked);
    // 连接选择框状态变化信号
    connect(this, &QTreeWidget::itemChanged, this, &DataNavigator::onItemChanged);

    // expandAll();
    // // 使用单次定时器，确保所有子节点都已创建
    // QTimer::singleShot(0, this, [this]() {
    //     expandAll(); // 这会递归展开所有节点（包括子节点）
    // });

    DEBUG_LOG;

    // 设置高亮绘制委托（中文注释）
    class NavigatorHighlightDelegate : public QStyledItemDelegate {
    public:
        explicit NavigatorHighlightDelegate(DataNavigator* nav, QObject* parent = nullptr)
            : QStyledItemDelegate(parent), m_nav(nav) {}

        void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
            QStyleOptionViewItem opt(option);
            initStyleOption(&opt, index);

            // 使用默认样式绘制背景/选择态，但不绘制文本
            QStyleOptionViewItem optNoText = opt;
            optNoText.text = QString();
            const QWidget* widget = opt.widget;
            QStyle* style = widget ? widget->style() : QApplication::style();
            style->drawControl(QStyle::CE_ItemViewItem, &optNoText, painter, widget);

            // 文本内容
            const QString text = opt.text;
            const QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &opt, widget);

            // 若无搜索关键词，直接绘制文本
            if (!m_nav || m_nav->m_currentTokens.isEmpty()) {
                painter->save();
                painter->setFont(opt.font);
                painter->setPen(opt.palette.color(QPalette::Text));
                painter->drawText(textRect.adjusted(2, 0, -2, 0), opt.displayAlignment, text);
                painter->restore();
                return;
            }

            // 构造高亮格式区间（大小写不敏感）
            QList<QTextLayout::FormatRange> ranges;
            for (const QString& token : m_nav->m_currentTokens) {
                if (token.isEmpty()) continue;
                int from = 0;
                while (true) {
                    // 大小写不敏感查找
                    int idx = text.mid(from).indexOf(token, 0, Qt::CaseInsensitive);
                    if (idx < 0) break;
                    idx += from;
                    QTextCharFormat fmt;
                    fmt.setForeground(QBrush(Qt::black));
                    fmt.setBackground(QBrush(QColor(255, 235, 59))); // 亮黄色高亮
                    QTextLayout::FormatRange fr;
                    fr.start = idx;
                    fr.length = token.length();
                    fr.format = fmt;
                    ranges.append(fr);
                    from = idx + token.length();
                }
            }

            // 使用 QTextLayout 绘制带高亮的文本
            painter->save();
            painter->setFont(opt.font);
            // QTextLayout layout(text, opt.font);
            // layout.setFormats(ranges);
            // QList → QVector 转换（或者你直接把 ranges 定义成 QVector）
            QVector<QTextLayout::FormatRange> vecRanges = QVector<QTextLayout::FormatRange>::fromList(ranges);

            QTextLayout layout(text, opt.font);
            layout.setFormats(vecRanges);

            layout.beginLayout();
            QTextLine line = layout.createLine();
            layout.endLayout();
            if (line.isValid()) {
                line.setLineWidth(textRect.width() - 4);
                const QPointF pos(textRect.left() + 2, textRect.center().y() - line.height() / 2);
                layout.draw(painter, pos);
            } else {
                painter->drawText(textRect.adjusted(2, 0, -2, 0), opt.displayAlignment, text);
            }
            painter->restore();
        }
    private:
        DataNavigator* m_nav;
    };
    m_highlightDelegate = new NavigatorHighlightDelegate(this, this);
    this->setItemDelegateForColumn(0, m_highlightDelegate);

    // 【新增】初始化空节点显示开关（中文注释）
    // 默认 false：当 project_name 或 batch_code 为空时不创建对应节点
    m_showEmptyProjectAndBatchNodes = false;
    // 【新增】初始化“隐藏无样本数据的节点”开关（中文注释）
    // 默认 true：如果节点最终没有任何样本，则隐藏该节点
    m_hideNodesWithoutSamples = true;
}

DataNavigator::~DataNavigator()
{
    // 清理资源
    // QTreeWidget会自动删除所有的QTreeWidgetItem，所以这里不需要手动删除
}

void DataNavigator::setupTree()
{
    // 设置为两列，第二列可以用来显示文件路径或状态
    setColumnCount(1); 
    setHeaderHidden(true); // 隐藏顶部的 "数据集" 表头

    // 创建并添加三个顶级根节点
    m_workspaceRoot = new QTreeWidgetItem(this, {STR("nav.workspace.root")}); // e.g., "工作区"
    
    // 【新】不再使用原来的 m_dataSourceRoot，而是创建三个具体的数据类型根节点
    m_dataSourceRoot = new QTreeWidgetItem(); // 保留对象防止空指针，但不添加到树中
    
    m_bigTgRoot = new QTreeWidgetItem(this, {QStringLiteral("大热重")});
    m_smallTgRoot = new QTreeWidgetItem(this, {QStringLiteral("小热重")});
    m_chromRoot = new QTreeWidgetItem(this, {QStringLiteral("色谱")});
    
    // 【新】工序数据源根节点重命名为"工序大热重"，作为该类数据的容器
    m_processDataRoot = new QTreeWidgetItem(this, {QStringLiteral("工序大热重")}); // 原 STR("nav.processdata.root")
    
    m_workspaceRoot->setIcon(0, style()->standardIcon(QStyle::SP_DirOpenIcon));
    // m_bigTgRoot->setIcon(0, style()->standardIcon(QStyle::SP_DirHomeIcon));
    // m_smallTgRoot->setIcon(0, style()->standardIcon(QStyle::SP_DirHomeIcon));
    // m_chromRoot->setIcon(0, style()->standardIcon(QStyle::SP_DirHomeIcon));
    m_bigTgRoot->setIcon(0, style()->standardIcon(QStyle::SP_DirOpenIcon));
    m_smallTgRoot->setIcon(0, style()->standardIcon(QStyle::SP_DirOpenIcon));
    m_chromRoot->setIcon(0, style()->standardIcon(QStyle::SP_DirOpenIcon));
    m_processDataRoot->setIcon(0, style()->standardIcon(QStyle::SP_DirOpenIcon));
    
    // 为新的根节点设置元数据，以便 onItemExpanded 识别
    auto initRootInfo = [](QTreeWidgetItem* item, const QString& dataType) {
        NavigatorNodeInfo info;
        info.type = NavigatorNodeInfo::DataType; // 根节点作为 DataType 类型
        info.dataType = dataType;
        item->setData(0, Qt::UserRole, QVariant::fromValue(info));
        item->addChild(new QTreeWidgetItem()); // 添加占位符以支持展开
    };

    initRootInfo(m_bigTgRoot, "大热重");
    initRootInfo(m_smallTgRoot, "小热重");
    initRootInfo(m_chromRoot, "色谱");

    // 创建"打开的窗口"节点
    m_openSamplesRoot = new QTreeWidgetItem(m_workspaceRoot, {"打开的窗口"});
    m_openSamplesRoot->setIcon(0, style()->standardIcon(QStyle::SP_FileDialogListView));
    
    addTopLevelItem(m_workspaceRoot);
    addTopLevelItem(m_bigTgRoot);
    addTopLevelItem(m_smallTgRoot);
    addTopLevelItem(m_chromRoot);
    addTopLevelItem(m_processDataRoot);
    
    // 刷新数据源和工序数据源
    refreshDataSource();
    refreshProcessData();
}


void DataNavigator::refreshDataSource()
{
    DEBUG_LOG;
    
    // 重置三个数据类型根节点
    auto resetRoot = [](QTreeWidgetItem* root) {
        if (!root) return;
        qDeleteAll(root->takeChildren());
        root->addChild(new QTreeWidgetItem()); // 添加占位符
        root->setExpanded(false);
    };

    resetRoot(m_bigTgRoot);
    resetRoot(m_smallTgRoot);
    resetRoot(m_chromRoot);
    
    // 确保UI更新
    this->update();
    DEBUG_LOG;
}

// 【新】公共接口实现
void DataNavigator::addOpenView(QMdiSubWindow *window)
{
    if (!window || m_windowToItemMap.contains(window)) return;
    
    // 获取窗口关联的样本信息
    int sampleId = window->property("sampleId").toInt();
    
    // 所有窗口都添加到"打开的窗口"节点下
    QTreeWidgetItem* parentItem = m_openSamplesRoot;
    
    QTreeWidgetItem* newItem = new QTreeWidgetItem(parentItem, {window->windowTitle()});
    newItem->setIcon(0, window->windowIcon());
    
    m_windowToItemMap[window] = newItem;
    m_itemToWindowMap[newItem] = window;
    
    // 如果是样本节点下的窗口，记录到映射中
    if (sampleId > 0) {
        m_sampleToWindowItemsMap[sampleId].append(newItem);
    }
    
    // 当窗口标题变化时，同步更新树节点文本
    connect(window, &QWidget::windowTitleChanged, this, [=](const QString& title){
        newItem->setText(0, title);
    });
    
    // 确保父节点展开
    parentItem->setExpanded(true);
    
    // 确保父节点的所有父节点也展开，这样用户可以看到新添加的项
    QTreeWidgetItem* current = parentItem;
    while (current && current != invisibleRootItem()) {
        current->setExpanded(true);
        current = current->parent();
    }
}

void DataNavigator::removeOpenView(QMdiSubWindow *window)
{
    if (!window || !m_windowToItemMap.contains(window)) return;
    
    QTreeWidgetItem* item = m_windowToItemMap.take(window);
    
    // 从样本映射中移除
    int sampleId = window->property("sampleId").toInt();
    if (sampleId > 0 && m_sampleToWindowItemsMap.contains(sampleId)) {
        m_sampleToWindowItemsMap[sampleId].removeAll(item);
    }
    
    m_itemToWindowMap.remove(item);
    delete item;
}

void DataNavigator::setActiveView(QMdiSubWindow *window)
{
    if (!window || !m_windowToItemMap.contains(window)) return;
    
    QTreeWidgetItem* item = m_windowToItemMap[window];
    setCurrentItem(item);
    
    for (QTreeWidgetItem* item : m_windowToItemMap.values()) {
        QFont font = item->font(0);
        font.setBold(false);
        item->setFont(0, font);
    }
    
    if (window && m_windowToItemMap.contains(window)) {
        QTreeWidgetItem* item = m_windowToItemMap.value(window);
        QFont font = item->font(0);
        font.setBold(true); // 将当前激活的条目加粗
        item->setFont(0, font);
        setCurrentItem(item);
        
        // 根据窗口类型启用对应数据类型的样本选择框
        QString windowClassName = window->widget()->metaObject()->className();
        
        // 不再禁用所有数据类型的选择框，允许多种数据类型同时被选中
        // 只根据当前窗口类型启用对应数据类型的样本选择框
        
        if (windowClassName == "TgBigDataProcessDialog") {
            enableSampleCheckboxesByDataType(QStringLiteral("大热重"), true);
        } else if (windowClassName == "TgSmallDataProcessDialog") {
            enableSampleCheckboxesByDataType(QStringLiteral("小热重"), true);
        } else if (windowClassName == "ChromatographDataProcessDialog") {
            enableSampleCheckboxesByDataType(QStringLiteral("色谱"), true);
        } else if (windowClassName == "ProcessTgBigDataProcessDialog") {
            enableSampleCheckboxesByDataType(QStringLiteral("工序大热重"), true);
        }
    }
}

void DataNavigator::onOpenViewItemClicked(QTreeWidgetItem *item, int column)
{
    if (!item || column != 0) return;

    if (m_itemToWindowMap.contains(item)) {
        QMdiSubWindow* window = m_itemToWindowMap.value(item);
        if (window && window->mdiArea()) {
            window->mdiArea()->setActiveSubWindow(window);
        }
    } else {
        // 获取节点信息
        QVariant data = item->data(0, Qt::UserRole);
        if (data.canConvert<NavigatorNodeInfo>()) {
            NavigatorNodeInfo info = data.value<NavigatorNodeInfo>();
            
            // 如果是样本节点，切换选择框状态
            if (info.type == NavigatorNodeInfo::Sample) {
                if (!(item->flags() & Qt::ItemIsUserCheckable)) return;
                if (item->isDisabled()) return;

                const QPoint viewPos = viewport()->mapFromGlobal(QCursor::pos());
                const QRect itemRect = visualItemRect(item);

                QStyleOptionViewItem option;
                option.initFrom(this);
                option.rect = itemRect;
                option.features |= QStyleOptionViewItem::HasCheckIndicator;
                option.checkState = item->checkState(0);
                const QRect checkRect = style()->subElementRect(QStyle::SE_ItemViewItemCheckIndicator, &option, this);

                if (checkRect.contains(viewPos)) {
                    return;
                }

                // 切换选择框状态
                Qt::CheckState newState = (item->checkState(0) == Qt::Checked) ? Qt::Unchecked : Qt::Checked;
                item->setCheckState(0, newState);
            }
        }
    }
}

// 解析查询文本为条件映射与自由关键词列表（中文注释）
// 规则：
// - 支持以空格或分号分隔的多个片段
// - 片段中含有 ':' 或 '=' 视为键值对条件，例如："牌号:红塔山"、"batch=2023"
// - 其他片段为自由关键词，做模糊匹配（大小写不敏感）
void DataNavigator::parseQuery(const QString& queryText, QMap<QString, QString>& conds, QStringList& tokens) const
{
    conds.clear();
    tokens.clear();

    // 统一分隔符为空格
    QString normalized = queryText;
    normalized.replace(';', ' ');
    const QStringList parts = normalized.split(' ', Qt::SkipEmptyParts);

    auto normKey = [](const QString& k) {
        QString key = k.trimmed().toLower();
        // 将中文/英文别名归一化到统一键
        if (key == "牌号" || key == "brand" || key == "project" || key == "projectname") return QString("projectName");
        if (key == "批次" || key == "批次号" || key == "batch" || key == "batchcode") return QString("batchCode");
        if (key == "样本" || key == "样本号" || key == "短码" || key == "short" || key == "sample" || key == "shortcode") return QString("shortCode");
        if (key == "并行" || key == "并行号" || key == "parallel" || key == "parallelno") return QString("parallelNo");
        if (key == "数据类型" || key == "datatype" || key == "类型") return QString("dataType");
        if (key == "id" || key == "样本id") return QString("id");
        return k; // 默认保持原样
    };

    for (const QString& raw : parts) {
        const QString part = raw.trimmed();
        if (part.isEmpty()) continue;
        int sepIdx = part.indexOf(':');
        if (sepIdx < 0) sepIdx = part.indexOf('=');
        if (sepIdx > 0) {
            QString k = normKey(part.left(sepIdx));
            QString v = part.mid(sepIdx + 1).trimmed();
            if (!v.isEmpty()) conds[k] = v;
        } else {
            tokens << part;
        }
    }
}

// 递归判断并更新节点可见性；返回该节点或其后代是否匹配
bool DataNavigator::updateVisibilityRecursive(QTreeWidgetItem* item, const QMap<QString, QString>& conds, const QStringList& tokens)
{
    if (!item) return false;

    // 取节点绑定的业务信息（样本/批次/型号等），没有则用文本匹配
    NavigatorNodeInfo info;
    QVariant v = item->data(0, Qt::UserRole);
    if (v.isValid()) {
        info = v.value<NavigatorNodeInfo>();
    }

    auto containsCi = [](const QString& hay, const QString& needle) {
        return hay.contains(needle, Qt::CaseInsensitive);
    };

    // 自由文本匹配：在节点文本及关键字段上做模糊匹配
    bool tokenMatch = tokens.isEmpty();
    if (!tokens.isEmpty()) {
        const QString agg = item->text(0) + "|" + info.projectName + "|" + info.batchCode + "|" + info.shortCode + "|" + info.dataType + "|" + QString::number(info.parallelNo) + "|" + QString::number(info.id);
        if (tokens.size() == 2 && conds.isEmpty()) {
            // 两个关键词的“三种匹配方式”组合（OR）：
            // 1) 牌号→批次；2) 牌号→样本（短码/名称）；3) 批次→样本（短码/名称）
            const QString t0 = tokens[0];
            const QString t1 = tokens[1];
            bool strat1 = containsCi(info.projectName, t0) && containsCi(info.batchCode, t1);
            bool strat2 = containsCi(info.projectName, t0) && containsCi(agg, t1);
            bool strat3 = containsCi(info.batchCode, t0) && containsCi(agg, t1);
            tokenMatch = strat1 || strat2 || strat3;
        } else {
            // 默认：所有自由词都需命中聚合文本（AND）
            tokenMatch = true;
            for (const QString& t : tokens) {
                if (!containsCi(agg, t)) { tokenMatch = false; break; }
            }
        }
    }

    // 键值条件匹配：逐项匹配（并行号与ID支持数值字符串比对）
    bool condMatch = true;
    for (auto it = conds.constBegin(); it != conds.constEnd(); ++it) {
        const QString key = it.key();
        const QString val = it.value();
        if (key == "projectName") {
            if (!containsCi(info.projectName, val)) { condMatch = false; break; }
        } else if (key == "batchCode") {
            if (!containsCi(info.batchCode, val)) { condMatch = false; break; }
        } else if (key == "shortCode") {
            if (!containsCi(info.shortCode, val)) { condMatch = false; break; }
        } else if (key == "parallelNo") {
            if (!QString::number(info.parallelNo).contains(val, Qt::CaseInsensitive)) { condMatch = false; break; }
        } else if (key == "dataType") {
            if (!containsCi(info.dataType, val)) { condMatch = false; break; }
        } else if (key == "id") {
            if (!QString::number(info.id).contains(val, Qt::CaseInsensitive)) { condMatch = false; break; }
        } else {
            // 未知键：回退到节点文本匹配
            if (!containsCi(item->text(0), val)) { condMatch = false; break; }
        }
    }

    // 先递归子节点
    bool childVisible = false;
    for (int i = 0; i < item->childCount(); ++i) {
        QTreeWidgetItem* ch = item->child(i);
        childVisible = updateVisibilityRecursive(ch, conds, tokens) || childVisible;
    }

    // 自身是否匹配
    bool selfVisible = tokenMatch && condMatch;
    bool visible = selfVisible || childVisible;
    item->setHidden(!visible);

    // 若后代可见，则展开到该节点，提升可见性
    if (visible && childVisible) {
        item->setExpanded(true);
    }
    return visible;
}

// 对外接口：应用搜索过滤
void DataNavigator::applySearchFilter(const QString& queryText)
{
    // 解析查询文本，递归隐藏不匹配的节点，仅保留匹配链路
    QMap<QString, QString> conds; QStringList tokens;
    parseQuery(queryText, conds, tokens);

    // 层级搜索规则
    // 1) 若仅输入一个词，视为“样本”全局搜索（短码/样本名），直接调用数据库路径展开
    // 2) 若输入两个词：优先判断第一个是否命中任何烟牌（项目名）；命中则作为 牌号 + 批次；否则作为 批次 + 样本
    // 3) 若输入≥3个词：第一个为牌号，第二个为批次，其余聚合作为样本关键字（短码优先）
    if (conds.isEmpty()) {
        const QStringList t = tokens;

        if (t.size() == 1) {
            // 一个关键词——保持本地树模糊过滤，不在此处触发数据库展开（避免覆盖过滤）
            // 全局数据库展开作为后置兜底逻辑，见函数末尾的“数据库先查、路径定向展开”。
        } else if (t.size() == 2) {
            // 两词场景采用“三种匹配方式”的组合逻辑，由 updateVisibilityRecursive 内部处理：
            // 1) 牌号→批次；2) 牌号→样本；3) 批次→样本
            // 这里保留两个自由词，交由递归匹配进行 OR 组合判定
            const QString p = t[0].trimmed();
            const QString b_or_s = t[1].trimmed();
            tokens = QStringList{p, b_or_s};
        } else if (t.size() >= 3) {
            const QString p = t[0].trimmed();
            const QString b = t[1].trimmed();
            conds["projectName"] = p;
            conds["batchCode"]   = b;
            // 其余词直接作为样本关键字列表（短码/样本名模糊）
            QStringList rest;
            for (int i = 2; i < t.size(); ++i) { const QString piece = t[i].trimmed(); if (!piece.isEmpty()) rest << piece; }
            tokens = rest;
        }
    }

    // 同步当前高亮关键词与条件（用于委托绘制与后续操作）
    m_currentTokens = tokens;
    m_currentConds = conds;

    // 记录当前关键词用于高亮
    m_currentTokens = tokens;
    m_currentConds = conds;

    // 首次进入搜索时，记录展开状态快照（中文注释）
    if (!queryText.trimmed().isEmpty() && !m_hasExpandedSnapshot) {
        m_expandedBeforeSearch.clear();
        auto captureExpanded = [&](QTreeWidgetItem* root){
            if (!root) return;
            QList<QTreeWidgetItem*> stack; stack.push_back(root);
            while (!stack.isEmpty()) {
                QTreeWidgetItem* cur = stack.takeLast();
                if (cur->isExpanded()) m_expandedBeforeSearch.insert(cur);
                for (int i = 0; i < cur->childCount(); ++i) stack.push_back(cur->child(i));
            }
        };
        captureExpanded(m_workspaceRoot);
        captureExpanded(m_dataSourceRoot);
        captureExpanded(m_processDataRoot);
        m_hasExpandedSnapshot = true;
    }

    // 顶级根：工作区通常不参与数据匹配，保留显示
    if (m_workspaceRoot) {
        // 工作区保持显示，不过滤其子项（打开的窗口列表）
        m_workspaceRoot->setHidden(false);
    }

    if (m_dataSourceRoot) {
        updateVisibilityRecursive(m_dataSourceRoot, conds, tokens);
        m_dataSourceRoot->setHidden(false);
        if (!queryText.trimmed().isEmpty()) m_dataSourceRoot->setExpanded(true);
    }
    if (m_processDataRoot) {
        updateVisibilityRecursive(m_processDataRoot, conds, tokens);
        m_processDataRoot->setHidden(false);
        if (!queryText.trimmed().isEmpty()) m_processDataRoot->setExpanded(true);
    }
    // 触发视图重绘以更新高亮效果（中文注释）
    if (this->viewport()) this->viewport()->update();

    // 【新增】数据库先查、路径定向展开：
    // 当本地树未能匹配任何“样本”节点时，触发数据库全局搜索并按路径展开到样本。
    if (!queryText.trimmed().isEmpty()) {
        auto hasVisibleSample = [&](QTreeWidgetItem* root){
            if (!root) return false;
            QList<QTreeWidgetItem*> stack; stack.push_back(root);
            while (!stack.isEmpty()) {
                QTreeWidgetItem* cur = stack.takeLast();
                if (!cur->isHidden()) {
                    QVariant v = cur->data(0, Qt::UserRole);
                    if (v.isValid()) {
                        NavigatorNodeInfo info = v.value<NavigatorNodeInfo>();
                        if (info.type == NavigatorNodeInfo::Sample) return true;
                    }
                }
                for (int i = 0; i < cur->childCount(); ++i) stack.push_back(cur->child(i));
            }
            return false;
        };

        bool anyVisibleSample = hasVisibleSample(m_dataSourceRoot) || hasVisibleSample(m_processDataRoot);
        if (!anyVisibleSample) {
            bool revealed = revealSamplesByDatabaseSearch(queryText.trimmed());
            if (revealed) {
                // 为确保用户能看到展开结果，这里清除过滤并滚动到第一个匹配样本
                clearSearchFilter();
            }
        }
    }
}

// 对外接口：清除搜索过滤，恢复所有节点显示
void DataNavigator::clearSearchFilter()
{
    // 清除高亮关键词
    m_currentTokens.clear();
    m_currentConds.clear();

    // 恢复所有节点显示，并根据搜索前快照恢复展开状态（中文注释）
    auto restoreAll = [&](QTreeWidgetItem* root){
        if (!root) return;
        QList<QTreeWidgetItem*> stack; stack.push_back(root);
        while (!stack.isEmpty()) {
            QTreeWidgetItem* cur = stack.takeLast();
            cur->setHidden(false);
            // 如果该节点在快照中，展开；否则收起
            cur->setExpanded(m_expandedBeforeSearch.contains(cur));
            for (int i = 0; i < cur->childCount(); ++i) stack.push_back(cur->child(i));
        }
    };
    restoreAll(m_workspaceRoot);
    restoreAll(m_dataSourceRoot);
    restoreAll(m_processDataRoot);

    // 清空快照标记
    m_expandedBeforeSearch.clear();
    m_hasExpandedSnapshot = false;

    // 触发视图重绘以取消高亮（中文注释）
    if (this->viewport()) this->viewport()->update();
}


void DataNavigator::refreshNavigator()
{
    // 保存当前展开状态
    QMap<QString, bool> rootExpandedStates;
    rootExpandedStates["workspaceRoot"] = m_workspaceRoot ? m_workspaceRoot->isExpanded() : false;
    rootExpandedStates["dataSourceRoot"] = m_dataSourceRoot ? m_dataSourceRoot->isExpanded() : false;
    rootExpandedStates["processDataRoot"] = m_processDataRoot ? m_processDataRoot->isExpanded() : false;
    
    // 保存工作区窗口项
    QMap<QMdiSubWindow*, QString> windowTitles;
    if (m_workspaceRoot) {
        for (auto it = m_windowToItemMap.begin(); it != m_windowToItemMap.end(); ++it) {
            windowTitles[it.key()] = it.value()->text(0);
        }
    }
    
    // 清空树并重新设置
    this->clear();
    setupTree();
    
    // 恢复展开状态
    if (m_workspaceRoot && rootExpandedStates["workspaceRoot"]) {
        m_workspaceRoot->setExpanded(true);
    }
    if (m_dataSourceRoot && rootExpandedStates["dataSourceRoot"]) {
        m_dataSourceRoot->setExpanded(true);
    }
    if (m_processDataRoot && rootExpandedStates["processDataRoot"]) {
        m_processDataRoot->setExpanded(true);
    }
    
    // 恢复工作区窗口项
    for (auto it = windowTitles.begin(); it != windowTitles.end(); ++it) {
        addOpenView(it.key());
    }
}


void DataNavigator::onItemExpanded(QTreeWidgetItem *item)
{
    // 如果没有子节点，或者子节点不是占位符，则说明已经加载过
    if (!item || item->childCount() == 0 || !item->child(0)->text(0).isEmpty()) {
        return;
    }
    
    // 移除占位符
    item->takeChild(0);

    NavigatorNodeInfo info = item->data(0, Qt::UserRole).value<NavigatorNodeInfo>();
    
    // 根据当前节点的类型，决定下一步要加载什么
    switch (info.type) {
        case NavigatorNodeInfo::DataType:
            // 如果是大热重/小热重/色谱的根节点，加载 ShortCode
            if (info.dataType == "大热重" || info.dataType == "小热重" || info.dataType == "色谱") {
                loadShortCodesForType(item, info.dataType);
            }
            break;
        case NavigatorNodeInfo::ShortCode:
            // 如果是 ShortCode 节点，且父节点是 DataType (大/小/色)，加载平行样
            // 注意：工序大热重下的 ShortCode 已经在 loadSamplesForProcessBatch 中直接作为 Sample 加载了，不会有 ShortCode 类型的中间节点
            // 除非我们在工序中也保留了 ShortCode 层级。根据用户需求：工序大热重 -> 烟牌 -> 批次 -> 样本(short_code(parallel_no))
            // 所以工序大热重没有 ShortCode 这一层作为容器。
            // 只有大/小/色有 ShortCode 层级。
            if (!info.dataType.isEmpty()) {
                 loadParallelSamplesForShortCode(item);
            }
            break;
        case NavigatorNodeInfo::Model:
             // 工序大热重：加载批次
             loadBatchesForProcessProject(item);
             break;
        case NavigatorNodeInfo::Batch:
             // 工序大热重：加载样本
             loadSamplesForProcessBatch(item);
             break;
        default:
            break;
    }
}

void DataNavigator::loadShortCodesForType(QTreeWidgetItem* typeItem, const QString& dataType)
{
    QString error;
    QList<QString> shortCodes = m_dao.fetchShortCodesForDataType(dataType, error);
    if (!error.isEmpty()) {
        DEBUG_LOG << "Error loading short codes for type " << dataType << ": " << error;
        return;
    }

    for (const QString& code : shortCodes) {
        QTreeWidgetItem* item = new QTreeWidgetItem(typeItem, {code});
        NavigatorNodeInfo info;
        info.type = NavigatorNodeInfo::ShortCode;
        info.shortCode = code;
        info.dataType = dataType; // 传递数据类型
        item->setData(0, Qt::UserRole, QVariant::fromValue(info));
        item->addChild(new QTreeWidgetItem()); // 占位符
    }
}

void DataNavigator::loadParallelSamplesForShortCode(QTreeWidgetItem* shortCodeItem)
{
    NavigatorNodeInfo parentInfo = shortCodeItem->data(0, Qt::UserRole).value<NavigatorNodeInfo>();
    QString error;
    // 获取平行样信息
    auto samples = m_dao.fetchParallelSamplesForShortCodeAndType(parentInfo.shortCode, parentInfo.dataType, error);
    
    if (!error.isEmpty()) {
        DEBUG_LOG << "Error loading parallel samples: " << error;
        return;
    }

    QSet<int> selectedForType = SampleSelectionManager::instance()->selectedIdsByType(parentInfo.dataType);

    for (const auto& sample : samples) {
        // 格式：short_code(parallel_no)-时间戳
        QString text = QString("%1(%2)-%3")
                       .arg(parentInfo.shortCode)
                       .arg(sample.parallelNo)
                       .arg(sample.timestamp);
        
        QTreeWidgetItem* item = new QTreeWidgetItem(shortCodeItem, {text});
        NavigatorNodeInfo info = parentInfo;
        info.type = NavigatorNodeInfo::Sample;
        info.id = sample.id;
        info.parallelNo = sample.parallelNo;
        
        // 填充项目与批次信息，确保右键属性和搜索功能正常工作
        info.projectName = sample.projectName;
        info.batchCode = sample.batchCode;
        
        item->setData(0, Qt::UserRole, QVariant::fromValue(info));
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        
        {
            QSignalBlocker blocker(this);
            item->setCheckState(0, selectedForType.contains(sample.id) ? Qt::Checked : Qt::Unchecked);
        }
    }
}

void DataNavigator::loadBatchesForProcessProject(QTreeWidgetItem* projectItem)
{
    NavigatorNodeInfo parentInfo = projectItem->data(0, Qt::UserRole).value<NavigatorNodeInfo>();
    QString error;
    auto batches = m_dao.fetchBatchesForProcessProject(parentInfo.projectName, error);
    if (!error.isEmpty()) {
        DEBUG_LOG << "Error loading batches for process project: " << error;
        return;
    }

    for (const auto& batch : batches) {
         // batch is pair<batchCode, batchId>
         QTreeWidgetItem* item = new QTreeWidgetItem(projectItem, {batch.first});
         NavigatorNodeInfo info = parentInfo;
         info.type = NavigatorNodeInfo::Batch;
         info.batchCode = batch.first;
         info.id = batch.second; 
         
         item->setData(0, Qt::UserRole, QVariant::fromValue(info));
         item->addChild(new QTreeWidgetItem()); // 占位符
    }
}

void DataNavigator::loadSamplesForProcessBatch(QTreeWidgetItem* batchItem)
{
    NavigatorNodeInfo parentInfo = batchItem->data(0, Qt::UserRole).value<NavigatorNodeInfo>();
    QString error;
    // fetchSamplesForProcessBatch 只需要 batchCode
    auto samples = m_dao.fetchSamplesForProcessBatch(parentInfo.batchCode, error);
    
    if (!error.isEmpty()) {
        DEBUG_LOG << "Error loading samples for process batch: " << error;
        return;
    }

    QSet<int> selectedForType = SampleSelectionManager::instance()->selectedIdsByType("工序大热重");

    for (const auto& sample : samples) {
        // 格式：short_code(parallel_no)
        QString text = QString("%1(%2)")
                       .arg(sample.shortCode) // SampleLeafInfo 需包含 shortCode
                       .arg(sample.parallelNo);
        
        QTreeWidgetItem* item = new QTreeWidgetItem(batchItem, {text});
        NavigatorNodeInfo info = parentInfo;
        info.type = NavigatorNodeInfo::Sample;
        info.id = sample.id;
        info.shortCode = sample.shortCode;
        info.parallelNo = sample.parallelNo;
        info.dataType = "工序大热重";
        
        // 填充项目与批次信息，确保右键属性和搜索功能正常工作
        info.projectName = sample.projectName;
        info.batchCode = sample.batchCode;
        
        item->setData(0, Qt::UserRole, QVariant::fromValue(info));
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        
        {
            QSignalBlocker blocker(this);
            item->setCheckState(0, selectedForType.contains(sample.id) ? Qt::Checked : Qt::Unchecked);
        }
    }
}


// 新增方法：根据数据类型启用/禁用样本选择框
void DataNavigator::enableSampleCheckboxesByDataType(const QString& dataType, bool enable)
{
    DEBUG_LOG << "DataNavigator::enableSampleCheckboxesByDataType - "
        << "data type:" << dataType << "enable:" << enable;

    if (enable) {
        m_enabledSampleCheckboxTypes.insert(dataType);
    } else {
        m_enabledSampleCheckboxTypes.remove(dataType);
    }
        
    // 遍历所有数据类型节点
    for (int i = 0; i < m_dataSourceRoot->childCount(); ++i) {
        QTreeWidgetItem* projectItem = m_dataSourceRoot->child(i);
        for (int j = 0; j < projectItem->childCount(); ++j) {
            QTreeWidgetItem* batchItem = projectItem->child(j);
            for (int k = 0; k < batchItem->childCount(); ++k) {
                QTreeWidgetItem* shortCodeItem = batchItem->child(k);
                for (int l = 0; l < shortCodeItem->childCount(); ++l) {
                    QTreeWidgetItem* dataTypeItem = shortCodeItem->child(l);
                    
                    // 获取节点信息
                    NavigatorNodeInfo info = dataTypeItem->data(0, Qt::UserRole).value<NavigatorNodeInfo>();

                    // 如果是目标数据类型，启用/禁用其下所有样本的选择框
                    if (info.type == NavigatorNodeInfo::DataType && info.dataType == dataType) {
                        DEBUG_LOG << "Found matching data type node: " << info.dataType;
                        
                        for (int m = 0; m < dataTypeItem->childCount(); ++m) {
                            QTreeWidgetItem* sampleItem = dataTypeItem->child(m);
                            NavigatorNodeInfo sampleInfo = sampleItem->data(0, Qt::UserRole).value<NavigatorNodeInfo>();
                            
                            if (sampleInfo.type == NavigatorNodeInfo::Sample) {
                                sampleItem->setDisabled(!enable);
                                DEBUG_LOG << "Set sample " << sampleInfo.id << " disabled state to " << !enable;
                            }
                        }
                    }
                }
            }
        }
    }
}


// 刷新指定节点下的内容
void DataNavigator::refreshNode(QTreeWidgetItem* item)
{
    if (!item) return;
    
    // 保存当前展开状态
    bool wasExpanded = item->isExpanded();
    
    // 获取节点信息
    QVariant data = item->data(0, Qt::UserRole);
    if (!data.canConvert<NavigatorNodeInfo>()) return;
    
    NavigatorNodeInfo info = data.value<NavigatorNodeInfo>();
    
    // 根据节点类型进行刷新
    switch (info.type) {
        case NavigatorNodeInfo::Model:
            // 工序大热重：项目 -> 批次
            qDeleteAll(item->takeChildren());
            loadBatchesForProcessProject(item);
            break;
        case NavigatorNodeInfo::Batch:
            // 工序大热重：批次 -> 样本
            qDeleteAll(item->takeChildren());
            loadSamplesForProcessBatch(item);
            break;
        case NavigatorNodeInfo::ShortCode:
            // 大热重/小热重/色谱：ShortCode -> 平行样
            qDeleteAll(item->takeChildren());
            loadParallelSamplesForShortCode(item);
            break;
        case NavigatorNodeInfo::DataType:
            // 大热重/小热重/色谱：根节点 -> ShortCode
            qDeleteAll(item->takeChildren());
            loadShortCodesForType(item, info.dataType);
            break;
        case NavigatorNodeInfo::Sample:
            // 样本节点没有子节点，不需要刷新
            break;
        default:
            // 如果是根节点，则根据类型刷新
            if (item == m_dataSourceRoot) {
                // 保存展开状态
                QMap<QString, bool> expandedStates;
                
                for (int i = 0; i < m_dataSourceRoot->childCount(); i++) {
                    QTreeWidgetItem* child = m_dataSourceRoot->child(i);
                    expandedStates[child->text(0)] = child->isExpanded();
                }
                
                // 刷新数据源
                refreshDataSource();
                
                // 恢复展开状态
                for (int i = 0; i < m_dataSourceRoot->childCount(); i++) {
                    QTreeWidgetItem* child = m_dataSourceRoot->child(i);
                    if (expandedStates.contains(child->text(0)) && expandedStates[child->text(0)]) {
                        child->setExpanded(true);
                    }
                }
            } else if (item == m_workspaceRoot) {
                // 刷新工作区节点
                // 保留现有的窗口项
            } else if (item == m_processDataRoot) {
                // 保存展开状态
                QMap<QString, bool> expandedStates;
                for (int i = 0; i < m_processDataRoot->childCount(); i++) {
                    QTreeWidgetItem* child = m_processDataRoot->child(i);
                    expandedStates[child->text(0)] = child->isExpanded();
                }
                
                // 刷新工序数据源
                refreshProcessData();
                
                // 恢复展开状态
                for (int i = 0; i < m_processDataRoot->childCount(); i++) {
                    QTreeWidgetItem* child = m_processDataRoot->child(i);
                    if (expandedStates.contains(child->text(0)) && expandedStates[child->text(0)]) {
                        child->setExpanded(true);
                    }
                }
            }
            break;
    }
    
    // 恢复展开状态
    if (wasExpanded) {
        item->setExpanded(true);
    }
}



void DataNavigator::contextMenuEvent(QContextMenuEvent *event)
{
    QTreeWidgetItem* item = itemAt(event->pos());
    QMenu menu(this);

    // 通用菜单
    QAction* expandAllAction = menu.addAction("展开所有");
    QAction* collapseAllAction = menu.addAction("折叠所有");
    QAction* refreshAction = menu.addAction("刷新");

    connect(expandAllAction, &QAction::triggered, this, &QTreeWidget::expandAll);
    connect(collapseAllAction, &QAction::triggered, this, &QTreeWidget::collapseAll);

    if (item) {
        QVariant var = item->data(0, Qt::UserRole);
        if (var.isValid()) {
            NavigatorNodeInfo info = var.value<NavigatorNodeInfo>();
            DEBUG_LOG << "节点类型:" << info.type
                      << " batchCode:" << info.batchCode
                      << " dataType:" << info.dataType;

            // 判断节点所在分支（原料数据源/工序数据源）
            auto getRootOf = [&](QTreeWidgetItem* it){
                QTreeWidgetItem* cur = it;
                while (cur && cur->parent()) cur = cur->parent();
                return cur;
            };
            QTreeWidgetItem* root = getRootOf(item);
            bool processBranch = (root == m_processDataRoot);

            // 删除操作菜单
            if (info.type == NavigatorNodeInfo::Model) {
                menu.addSeparator();
                QAction* delProject = menu.addAction(tr("删除项目"));
                connect(delProject, &QAction::triggered, this, [this, info, processBranch]() {
                    QString tip = processBranch ? tr("将删除【工序数据源】下项目 '%1' 的所有批次与样本。").arg(info.projectName)
                                                : tr("将删除【原料数据源】下项目 '%1' 的所有批次与样本。").arg(info.projectName);
                    if (QMessageBox::question(this, tr("确认删除"), tip + tr("此操作不可撤销，是否继续？")) == QMessageBox::Yes) {
                        QString err;
                        if (m_dao.deleteProjectCascade(info.projectName, processBranch, err)) {
                            if (processBranch) refreshProcessData(); else refreshDataSource();
                        } else {
                            QMessageBox::warning(this, tr("删除失败"), err);
                        }
                    }
                });
            }

            if (info.type == NavigatorNodeInfo::Batch) {
                menu.addSeparator();
                QAction* addBatchAction = menu.addAction(tr("按批次添加样本"));
                connect(addBatchAction, &QAction::triggered, this, [this, info]() {
                    emit requestSelectAllSamplesInBatch(info);
                    DEBUG_LOG << "按批次添加样本：" << info.batchCode;
                });

                QAction* delBatch = menu.addAction(tr("删除批次"));
                connect(delBatch, &QAction::triggered, this, [this, info, processBranch, item]() {
                    QString tip = processBranch ? tr("将删除【工序数据源】下批次 '%1' 的所有样本与数据。")
                                                .arg(info.batchCode)
                                                : tr("将删除【原料数据源】下批次 '%1' 的所有样本与数据。")
                                                .arg(info.batchCode);
                    if (QMessageBox::question(this, tr("确认删除"), tip + tr("此操作不可撤销，是否继续？")) == QMessageBox::Yes) {
                        QString err;
                        if (m_dao.deleteBatchCascade(info.projectName, info.batchCode, processBranch, err)) {
                            // 刷新父型号节点或分支
                            QTreeWidgetItem* parent = item->parent();
                            if (parent) refreshNode(parent); else { if (processBranch) refreshProcessData(); else refreshDataSource(); }
                        } else {
                            QMessageBox::warning(this, tr("删除失败"), err);
                        }
                    }
                });
            }

            if (info.type == NavigatorNodeInfo::Sample) {
                menu.addSeparator();
                QAction* delSample = menu.addAction(tr("删除样本"));
                connect(delSample, &QAction::triggered, this, [this, info, processBranch, item]() {
                    QString tip = processBranch ? tr("将删除【工序数据源】下该样本及其工序大热重数据。")
                                                : tr("将删除【原料数据源】下该样本及其大热重/小热重/色谱数据。");
                    if (QMessageBox::question(this, tr("确认删除"), tip + tr("此操作不可撤销，是否继续？")) == QMessageBox::Yes) {
                        QString err;
                        if (m_dao.deleteSampleCascade(info.id, processBranch, err)) {
                            // 刷新数据类型节点
                            QTreeWidgetItem* parent = item->parent();
                            if (parent) refreshNode(parent); else { if (processBranch) refreshProcessData(); else refreshDataSource(); }
                        } else {
                            QMessageBox::warning(this, tr("删除失败"), err);
                        }
                    }
                });
            }
        }

        // 刷新单个节点
        connect(refreshAction, &QAction::triggered, this, [this, item]() {
            this->refreshNode(item);
        });

    } else {
        DEBUG_LOG << "点击空白区域刷新整个导航树";
        connect(refreshAction, &QAction::triggered, this, &DataNavigator::refreshNavigator);
    }

    menu.exec(event->globalPos());
}



// 显示样本属性对话框
void DataNavigator::showSampleProperties(const struct NavigatorNodeInfo& info)
{
    // 创建并显示属性对话框
    SamplePropertiesDialog dialog(this);
    dialog.setSampleInfo(info);
    dialog.exec();
}

// 显示样本数据表格对话框
void DataNavigator::showSampleDataTable(const struct NavigatorNodeInfo& info)
{
    // 创建数据表格窗口
    SampleDataTableDialog* dataTableWidget = new SampleDataTableDialog();
    dataTableWidget->loadSampleData(info);
    
    // 获取主窗口
    QMainWindow* mainWindow = qobject_cast<QMainWindow*>(window());
    if (mainWindow) {
        // 获取MDI区域
        QMdiArea* mdiArea = mainWindow->findChild<QMdiArea*>();
        if (mdiArea) {
            // 添加到MDI区域
            QMdiSubWindow* subWindow = mdiArea->addSubWindow(dataTableWidget);
            subWindow->setAttribute(Qt::WA_DeleteOnClose);
            
            // 设置样本ID属性，用于在导航树中关联
            subWindow->setProperty("sampleId", info.id);
            
            // 添加到导航树的打开视图
            addOpenView(subWindow);
            
            subWindow->show();
        } else {
            // 如果找不到MDI区域，则作为独立窗口显示
            dataTableWidget->setAttribute(Qt::WA_DeleteOnClose);
            dataTableWidget->show();
        }
    } else {
        // 如果找不到主窗口，则作为独立窗口显示
        dataTableWidget->setAttribute(Qt::WA_DeleteOnClose);
        dataTableWidget->show();
    }
}


// 处理双击样本事件
void DataNavigator::onItemDoubleClicked(QTreeWidgetItem* item, int column)
{
    if (!item) return;
    
    // 检查是否是窗口项
    if (m_itemToWindowMap.contains(item)) {
        QMdiSubWindow* window = m_itemToWindowMap[item];
        if (window) {
            // 激活窗口并显示到最前面，并最大化
            window->setFocus();
            window->raise();
            window->showMaximized(); // 改为最大化窗口
            return;
        }
    }
    
    // 获取节点信息
    NavigatorNodeInfo info = item->data(0, Qt::UserRole).value<NavigatorNodeInfo>();
    
    // 只处理样本节点
    if (info.type != NavigatorNodeInfo::Sample) return;
    
    DEBUG_LOG << "DataNavigator::onItemDoubleClicked - Sample:" << item->text(0)
             << "ID:" << info.id 
             << "DataType:" << info.dataType;
    
    // 发出信号给主窗口处理
    emit requestOpenSampleViewWindow(
        info.id, 
        info.projectName, 
                                   info.batchCode, info.shortCode,
                                   info.parallelNo, info.dataType);
}

void DataNavigator::setupTree02()
{
    // 此方法已不再需要，因为setupTree已经创建了所有必要的根节点
    // 工序数据源将通过正常的数据库查询加载，不再使用模拟数据
}

void DataNavigator::loadSamplesForBatch(QTreeWidgetItem* batchItem, const QString& projectName, const QString& batchCode, int batchId)
{
    // 获取该批次下的所有样本编码（短码）
    QString error;
    auto shortCodes = m_dao.fetchShortCodesForBatch(batchId, error);
    if (!error.isEmpty()) {
        LOG_WARNING(QString("Failed to fetch samples for batch %1: %2").arg(batchCode).arg(error));
        return;
    }

    // 若无任何短码，根据开关决定是否显示占位或删除批次节点（中文注释）
    if (shortCodes.isEmpty()) {
        if (m_hideNodesWithoutSamples) {
            // 开启隐藏时，直接在树中移除该批次节点
            QTreeWidgetItem* parent = batchItem->parent();
            if (parent) {
                parent->removeChild(batchItem);
            }
            delete batchItem;
        } else {
            // 未开启隐藏时，展示一个提示占位节点
            QTreeWidgetItem* emptyItem = new QTreeWidgetItem(batchItem);
            emptyItem->setText(0, tr("无样本数据"));
            emptyItem->setDisabled(true);
        }
        return;
    }

    int addedShortCodeCount = 0; // 记录实际添加到树的短码数量

    // 遍历短码，按需创建节点
    for (const auto& shortCode : shortCodes) {
        // 预取该短码下的平行样列表（一次查询复用到数据类型构建）
        QString perr;
        QList<ParallelSampleInfo> parallelSamples = m_dao.fetchParallelSamplesForBatch(batchId, shortCode, perr);
        if (!perr.isEmpty()) {
            LOG_WARNING(QString("Failed to fetch parallel samples for %1-%2: %3").arg(batchCode).arg(shortCode).arg(perr));
        }

        // 开启隐藏且没有任何平行样时，跳过该短码节点
        if (m_hideNodesWithoutSamples && parallelSamples.isEmpty()) {
            continue;
        }

        // 创建短码节点
        QTreeWidgetItem* shortCodeItem = new QTreeWidgetItem(batchItem);
        shortCodeItem->setText(0, shortCode);

        NavigatorNodeInfo shortCodeInfo;
        shortCodeInfo.type = NavigatorNodeInfo::ShortCode;
        shortCodeInfo.id = batchId;
        shortCodeInfo.projectName = projectName;
        shortCodeInfo.batchCode = batchCode;
        shortCodeInfo.shortCode = shortCode;
        shortCodeInfo.parallelNo = -1;
        shortCodeInfo.dataType = "";

        shortCodeItem->setData(0, Qt::UserRole, QVariant::fromValue(shortCodeInfo));

        // 工序数据固定类型：仅“工序大热重”
        QStringList dataTypes = {"工序大热重"};
        for (const auto& dataType : dataTypes) {
            // 开启隐藏且无样本时，不创建数据类型节点
            if (m_hideNodesWithoutSamples && parallelSamples.isEmpty()) {
                continue;
            }

            QTreeWidgetItem* dataTypeItem = new QTreeWidgetItem(shortCodeItem);
            dataTypeItem->setText(0, dataType);

            NavigatorNodeInfo dataTypeInfo;
            dataTypeInfo.type = NavigatorNodeInfo::DataType;
            dataTypeInfo.id = batchId;
            dataTypeInfo.projectName = projectName;
            dataTypeInfo.batchCode = batchCode;
            dataTypeInfo.shortCode = shortCode;
            dataTypeInfo.parallelNo = -1;
            dataTypeInfo.dataType = dataType;

            dataTypeItem->setData(0, Qt::UserRole, QVariant::fromValue(dataTypeInfo));

            if (parallelSamples.isEmpty()) {
                // 无样本时：未开启隐藏则展示一个占位样本
                if (!m_hideNodesWithoutSamples) {
                    QTreeWidgetItem* parallelItem = new QTreeWidgetItem(dataTypeItem);
                    parallelItem->setText(0, QString("平行样1"));

                    NavigatorNodeInfo parallelInfo;
                    parallelInfo.type = NavigatorNodeInfo::Sample;
                    parallelInfo.id = -1;
                    parallelInfo.projectName = projectName;
                    parallelInfo.batchCode = batchCode;
                    parallelInfo.shortCode = shortCode;
                    parallelInfo.parallelNo = 1;
                    parallelInfo.dataType = dataType;

                    parallelItem->setData(0, Qt::UserRole, QVariant::fromValue(parallelInfo));

                    // 为工序数据源中的样本节点显式添加复选框（首次展开即可见）
                    parallelItem->setFlags(parallelItem->flags() | Qt::ItemIsUserCheckable);
                    {
                        QSignalBlocker blocker(this);
                        parallelItem->setCheckState(0, Qt::Unchecked);
                    }
                }
            } else {
                const QSet<int> selectedForType = SampleSelectionManager::instance()->selectedIdsByType(dataType);
                // 根据实际平行样数量添加节点
                for (const ParallelSampleInfo& sample : parallelSamples) {
                    QTreeWidgetItem* parallelItem = new QTreeWidgetItem(dataTypeItem);
                    parallelItem->setText(0, QString("平行样%1").arg(sample.parallelNo));

                    NavigatorNodeInfo parallelInfo;
                    parallelInfo.type = NavigatorNodeInfo::Sample;
                    parallelInfo.id = sample.id;
                    parallelInfo.projectName = projectName;
                    parallelInfo.batchCode = batchCode;
                    parallelInfo.shortCode = shortCode;
                    parallelInfo.parallelNo = sample.parallelNo;
                    parallelInfo.dataType = dataType;

                    parallelItem->setData(0, Qt::UserRole, QVariant::fromValue(parallelInfo));

                    // 为工序数据源中的样本节点显式添加复选框（首次展开即可见）
                    parallelItem->setFlags(parallelItem->flags() | Qt::ItemIsUserCheckable);
                    {
                        QSignalBlocker blocker(this);
                        parallelItem->setCheckState(0, selectedForType.contains(sample.id) ? Qt::Checked : Qt::Unchecked);
                    }
                }
            }
        }

        // 统计成功添加的短码
        addedShortCodeCount++;
    }

    // 若未添加任何短码且开启隐藏，则删除批次节点
    if (m_hideNodesWithoutSamples && addedShortCodeCount == 0) {
        QTreeWidgetItem* parent = batchItem->parent();
        if (parent) {
            parent->removeChild(batchItem);
        }
        delete batchItem;
    }
}

void DataNavigator::refreshProcessData()
{
    DEBUG_LOG << "开始刷新工序数据源...";
    
    // 保存当前展开状态
    QMap<QString, bool> expandedStates;
    for (int i = 0; i < m_processDataRoot->childCount(); i++) {
        QTreeWidgetItem* item = m_processDataRoot->child(i);
        expandedStates[item->text(0)] = item->isExpanded();
    }
    
    // 清空工序数据源节点下的所有子节点
    qDeleteAll(m_processDataRoot->takeChildren());

    // 获取工序数据下的所有项目名
    QString error;
    auto projects = m_dao.fetchProjectsForProcessData(error);
    if (!error.isEmpty()) { 
        LOG_WARNING(QString("Failed to fetch projects for process data: %1").arg(error));
        return; 
    }

    // 遍历所有项目
    for (const auto& project : projects) {
        // 【新增】如果项目名为空且开关为不显示，则跳过
        if (!m_showEmptyProjectAndBatchNodes && project.trimmed().isEmpty()) {
            DEBUG_LOG << "skip empty project_name under processDataRoot";
            continue;
        }
        
        // 创建项目节点
        QTreeWidgetItem* projectItem = new QTreeWidgetItem(m_processDataRoot, {project});
        
        NavigatorNodeInfo projectInfo;
        projectInfo.type = NavigatorNodeInfo::Model; // 复用 Model 类型表示项目
        projectInfo.projectName = project;
        projectInfo.batchCode = "";
        projectInfo.shortCode = "";
        projectInfo.parallelNo = -1;
        projectInfo.dataType = "工序大热重"; // 标记为工序数据
        
        projectItem->setData(0, Qt::UserRole, QVariant::fromValue(projectInfo));
        projectItem->addChild(new QTreeWidgetItem()); // 添加占位符，支持懒加载
        
        // 恢复展开状态
        if (expandedStates.contains(project) && expandedStates[project]) {
            projectItem->setExpanded(true);
        }
    }
    
    m_processDataRoot->setExpanded(true);
    
    // 确保UI更新
    this->update();
    DEBUG_LOG << "工序数据源刷新完成";
}



int cycle_num = 0;

void DataNavigator::onItemChanged(QTreeWidgetItem* item, int column)
{
    // PrintStackTrace();   // 仅需一行
    // 程序化更新期间直接早退，避免一致性回写导致的重复链路
    if (m_inProgrammaticUpdate) return;

    DEBUG_LOG << "onItemChanged循环次数 " << cycle_num++;

    if (!item || column != 0) return;
    
    // 获取节点信息
    QVariant data = item->data(0, Qt::UserRole);
    if (!data.canConvert<NavigatorNodeInfo>()) return;
    
    NavigatorNodeInfo info = data.value<NavigatorNodeInfo>();
    
    // 只处理样本节点的选择框状态变化
    if (info.type == NavigatorNodeInfo::Sample) {
        bool isChecked = (item->checkState(0) == Qt::Checked);
        DEBUG_LOG << "DataNavigator::onItemChanged - Sample:000" ;
        // 以“数据类型维度”的选中状态作为一致性判断，避免不同数据类型共享同一样本ID导致的交叉回写
        const bool prevSelected = SampleSelectionManager::instance()
                                      ->selectedIdsByType(info.dataType)
                                      .contains(info.id);
        if (prevSelected == isChecked) {
            DEBUG_LOG << "Skip broadcast: state unchanged for sample" << info.id;
            return;
        }
        // 按数据类型记录选中状态，并仅在状态变化时通知各界面
        SampleSelectionManager::instance()->setSelectedWithType(info.id, info.dataType, isChecked, "DataNavigator");
        // // 同步到 StringManager 的样本集合，以驱动处理界面导航的即时更新
        // if (isChecked) {
        //     StringManager::instance()->addSample(info.dataType, info.id);
        // } else {
        //     StringManager::instance()->removeSample(info.dataType, info.id);
        // }
        emit sampleSelectionChanged(info.id, item->text(0), info.dataType, isChecked);
        DEBUG_LOG << "Sample selection changed:" 
                 << "ID=" << info.id 
                 << "Name=" << item->text(0)
                 << "DataType=" << info.dataType 
                 << "Selected=" << isChecked;
    }
}

// =============================
// 数据库先查、路径定向展开实现
// =============================
bool DataNavigator::revealSamplesByDatabaseSearch(const QString& keyword)
{
    // 调用DAO按样本名（含短码、项目名）进行模糊搜索，逐条尝试在树中展开到对应样本节点。
    QString error;
    QList<QVariantMap> matches = m_dao.searchSamplesByName(keyword, error);
    if (!error.isEmpty()) {
        LOG_WARNING(QString("Global search failed: %1").arg(error));
        return false;
    }
    if (matches.isEmpty()) return false;

    bool anyRevealed = false;
    QTreeWidgetItem* firstMatchedItem = nullptr;

    for (const auto& m : matches) {
        int modelId    = m.value("model_id").toInt();
        int batchId    = m.value("batch_id").toInt();
        QString batchCode = m.value("batch_code").toString();
        QString shortCode = m.value("short_code").toString();
        int parallelNo    = m.value("parallel_no").toInt();
        int sampleId      = m.value("sample_id").toInt();

        QTreeWidgetItem* modelItem = ensureModelExpandedAndGetItem(m_dataSourceRoot, modelId);
        if (!modelItem) continue;
        QTreeWidgetItem* batchItem = ensureBatchExpandedAndGetItem(modelItem, batchId);
        if (!batchItem) continue;
        QTreeWidgetItem* shortCodeItem = ensureShortCodeExpandedAndGetItem(batchItem, shortCode);
        if (!shortCodeItem) continue;

        // 加载数据类型并逐个展开，查找目标平行样
        if (!shortCodeItem->isExpanded()) {
            shortCodeItem->setExpanded(true);
            onItemExpanded(shortCodeItem);
        }
        for (int i = 0; i < shortCodeItem->childCount(); ++i) {
            QTreeWidgetItem* dataTypeItem = shortCodeItem->child(i);
            if (!dataTypeItem->isExpanded()) {
                dataTypeItem->setExpanded(true);
                onItemExpanded(dataTypeItem);
            }
            QTreeWidgetItem* sampleItem = findSampleItemUnderDataType(dataTypeItem, sampleId, parallelNo);
            if (sampleItem) {
                anyRevealed = true;
                if (!firstMatchedItem) firstMatchedItem = sampleItem;
                this->setCurrentItem(sampleItem);
                this->scrollToItem(sampleItem);
            }
        }
    }

    return anyRevealed;
}

QTreeWidgetItem* DataNavigator::ensureModelExpandedAndGetItem(QTreeWidgetItem* root, int modelId)
{
    if (!root) return nullptr;
    // 若根节点尚未加载模型子节点，先展开以触发懒加载（中文注释）
    if (root->childCount() == 0 && !root->isExpanded()) {
        root->setExpanded(true);
        onItemExpanded(root);
    }
    for (int i = 0; i < root->childCount(); ++i) {
        QTreeWidgetItem* modelItem = root->child(i);
        QVariant v = modelItem->data(0, Qt::UserRole);
        if (!v.isValid()) continue;
        NavigatorNodeInfo info = v.value<NavigatorNodeInfo>();
        if (info.type == NavigatorNodeInfo::Model && info.id == modelId) {
            if (!modelItem->isExpanded()) {
                modelItem->setExpanded(true);
                onItemExpanded(modelItem);
            }
            return modelItem;
        }
    }
    return nullptr;
}

QTreeWidgetItem* DataNavigator::ensureBatchExpandedAndGetItem(QTreeWidgetItem* modelItem, int batchId)
{
    if (!modelItem) return nullptr;
    for (int i = 0; i < modelItem->childCount(); ++i) {
        QTreeWidgetItem* batchItem = modelItem->child(i);
        QVariant v = batchItem->data(0, Qt::UserRole);
        if (!v.isValid()) continue;
        NavigatorNodeInfo info = v.value<NavigatorNodeInfo>();
        if (info.type == NavigatorNodeInfo::Batch && info.id == batchId) {
            if (!batchItem->isExpanded()) {
                batchItem->setExpanded(true);
                onItemExpanded(batchItem);
            }
            return batchItem;
        }
    }
    return nullptr;
}

QTreeWidgetItem* DataNavigator::ensureShortCodeExpandedAndGetItem(QTreeWidgetItem* batchItem, const QString& shortCode)
{
    if (!batchItem) return nullptr;
    for (int i = 0; i < batchItem->childCount(); ++i) {
        QTreeWidgetItem* shortItem = batchItem->child(i);
        QVariant v = shortItem->data(0, Qt::UserRole);
        if (!v.isValid()) continue;
        NavigatorNodeInfo info = v.value<NavigatorNodeInfo>();
        if (info.type == NavigatorNodeInfo::ShortCode && info.shortCode == shortCode) {
            if (!shortItem->isExpanded()) {
                shortItem->setExpanded(true);
                onItemExpanded(shortItem);
            }
            return shortItem;
        }
    }
    return nullptr;
}

QTreeWidgetItem* DataNavigator::ensureDataTypeExpandedAndGetItem(QTreeWidgetItem* shortCodeItem, const QString& dataType)
{
    if (!shortCodeItem) return nullptr;
    for (int i = 0; i < shortCodeItem->childCount(); ++i) {
        QTreeWidgetItem* typeItem = shortCodeItem->child(i);
        QVariant v = typeItem->data(0, Qt::UserRole);
        if (!v.isValid()) continue;
        NavigatorNodeInfo info = v.value<NavigatorNodeInfo>();
        if (info.type == NavigatorNodeInfo::DataType && info.dataType == dataType) {
            if (!typeItem->isExpanded()) {
                typeItem->setExpanded(true);
                onItemExpanded(typeItem);
            }
            return typeItem;
        }
    }
    return nullptr;
}

QTreeWidgetItem* DataNavigator::findSampleItemUnderDataType(QTreeWidgetItem* dataTypeItem, int sampleId, int parallelNo)
{
    if (!dataTypeItem) return nullptr;
    for (int i = 0; i < dataTypeItem->childCount(); ++i) {
        QTreeWidgetItem* sampleItem = dataTypeItem->child(i);
        QVariant v = sampleItem->data(0, Qt::UserRole);
        if (!v.isValid()) continue;
        NavigatorNodeInfo info = v.value<NavigatorNodeInfo>();
        if (info.type == NavigatorNodeInfo::Sample) {
            if ((sampleId > 0 && info.id == sampleId) || (parallelNo > 0 && info.parallelNo == parallelNo)) {
                return sampleItem;
            }
        }
    }
    return nullptr;
}

// 设置特定样本的选择框状态
void DataNavigator::setSampleCheckState(int sampleId, bool checked)
{
    // 设置程序化更新守卫，防止 onItemChanged 在回写期间触发重复链路
    m_inProgrammaticUpdate = true;

    // 遍历所有数据类型节点
    for (int i = 0; i < m_dataSourceRoot->childCount(); ++i) {
        DEBUG_LOG<<"遍历数据类型节点";
        QTreeWidgetItem* projectItem = m_dataSourceRoot->child(i);
        for (int j = 0; j < projectItem->childCount(); ++j) {
            DEBUG_LOG<<"遍历项目节点";
            QTreeWidgetItem* batchItem = projectItem->child(j);
            for (int k = 0; k < batchItem->childCount(); ++k) {
                DEBUG_LOG<<"遍历批次节点";
                QTreeWidgetItem* shortCodeItem = batchItem->child(k);
                for (int l = 0; l < shortCodeItem->childCount(); ++l) {
                    DEBUG_LOG<<"遍历短码节点";
                    QTreeWidgetItem* dataTypeItem = shortCodeItem->child(l);
                    for (int m = 0; m < dataTypeItem->childCount(); ++m) {
                        DEBUG_LOG<<"遍历数据类型节点";
                        QTreeWidgetItem* sampleItem = dataTypeItem->child(m);
                        
                        // 获取节点信息
                        QVariant data = sampleItem->data(0, Qt::UserRole);
                        if (data.canConvert<NavigatorNodeInfo>()) {
                            DEBUG_LOG<<"获取节点信息";
                            NavigatorNodeInfo info = data.value<NavigatorNodeInfo>();
                            
                            // 检查是否是目标样本
                            if (info.type == NavigatorNodeInfo::Sample && info.id == sampleId) {
                                // 临时断开信号连接，避免触发onItemChanged
                                disconnect(this, &QTreeWidget::itemChanged, this, &DataNavigator::onItemChanged);
                                DEBUG_LOG << "断开itemChanged信号连接，循环次数 " << cycle_num++;
                                DEBUG_LOG << "Temporarily disconnecting itemChanged signal";
                                
                                // 设置选择框状态
                                sampleItem->setCheckState(0, checked ? Qt::Checked : Qt::Unchecked);
                                
                                // 重新连接信号
                                connect(this, &QTreeWidget::itemChanged, this, &DataNavigator::onItemChanged);
                                
                                DEBUG_LOG << "Sample checkbox state set:" 
                                         << "ID=" << sampleId 
                                         << "Checked=" << checked;
                                // 程序化更新结束，恢复守卫
                                m_inProgrammaticUpdate = false;
                                return;
                            }
                        }
                    }
                }
            }
        }
    }
    
    DEBUG_LOG << "Sample not found for checkbox state setting:" << sampleId;
    // 不展开树结构。保持现有展开/折叠状态，等待用户展开时由 loadParallelSamples 同步勾选。
    m_inProgrammaticUpdate = false;
}

// 按数据类型设置特定样本的复选框状态（自动展开路径，仅影响指定类型）
void DataNavigator::setSampleCheckStateForType(int sampleId, const QString& dataType, bool checked)
{
    m_inProgrammaticUpdate = true;

    // 优先尝试直接找到并设置
    for (int i = 0; i < m_dataSourceRoot->childCount(); ++i) {
        QTreeWidgetItem* projectItem = m_dataSourceRoot->child(i);
        for (int j = 0; j < projectItem->childCount(); ++j) {
            QTreeWidgetItem* batchItem = projectItem->child(j);
            for (int k = 0; k < batchItem->childCount(); ++k) {
                QTreeWidgetItem* shortCodeItem = batchItem->child(k);
                for (int l = 0; l < shortCodeItem->childCount(); ++l) {
                    QTreeWidgetItem* dataTypeItem = shortCodeItem->child(l);
                    QVariant dtVar = dataTypeItem->data(0, Qt::UserRole);
                    if (dtVar.canConvert<NavigatorNodeInfo>()) {
                        NavigatorNodeInfo dtInfo = dtVar.value<NavigatorNodeInfo>();
                        if (dtInfo.type == NavigatorNodeInfo::DataType && dtInfo.dataType == dataType) {
                            for (int m = 0; m < dataTypeItem->childCount(); ++m) {
                                QTreeWidgetItem* sampleItem = dataTypeItem->child(m);
                                QVariant data = sampleItem->data(0, Qt::UserRole);
                                if (data.canConvert<NavigatorNodeInfo>()) {
                                    NavigatorNodeInfo info = data.value<NavigatorNodeInfo>();
                                    if (info.type == NavigatorNodeInfo::Sample && info.id == sampleId) {
                                        disconnect(this, &QTreeWidget::itemChanged, this, &DataNavigator::onItemChanged);
                                        sampleItem->setCheckState(0, checked ? Qt::Checked : Qt::Unchecked);
                                        connect(this, &QTreeWidget::itemChanged, this, &DataNavigator::onItemChanged);
                                        m_inProgrammaticUpdate = false;
                                        return;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // 未找到则直接结束，不更改树的展开状态。
    m_inProgrammaticUpdate = false;
}

void DataNavigator::clearSampleChecksForType(const QString& dataType)
{
    m_inProgrammaticUpdate = true;
    for (int i = 0; i < m_dataSourceRoot->childCount(); ++i) {
        QTreeWidgetItem* projectItem = m_dataSourceRoot->child(i);
        for (int j = 0; j < projectItem->childCount(); ++j) {
            QTreeWidgetItem* batchItem = projectItem->child(j);
            for (int k = 0; k < batchItem->childCount(); ++k) {
                QTreeWidgetItem* shortCodeItem = batchItem->child(k);
                for (int l = 0; l < shortCodeItem->childCount(); ++l) {
                    QTreeWidgetItem* dataTypeItem = shortCodeItem->child(l);
                    QVariant dtVar = dataTypeItem->data(0, Qt::UserRole);
                    if (!dtVar.canConvert<NavigatorNodeInfo>()) continue;
                    NavigatorNodeInfo dtInfo = dtVar.value<NavigatorNodeInfo>();
                    if (dtInfo.type != NavigatorNodeInfo::DataType || dtInfo.dataType != dataType) {
                        continue;
                    }
                    disconnect(this, &QTreeWidget::itemChanged, this, &DataNavigator::onItemChanged);
                    for (int m = 0; m < dataTypeItem->childCount(); ++m) {
                        QTreeWidgetItem* sampleItem = dataTypeItem->child(m);
                        if (!sampleItem) continue;
                        sampleItem->setCheckState(0, Qt::Unchecked);
                    }
                    connect(this, &QTreeWidget::itemChanged, this, &DataNavigator::onItemChanged);
                }
            }
        }
    }
    m_inProgrammaticUpdate = false;
}
