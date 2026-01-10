
#ifndef DATANAVIGATOR_H
#define DATANAVIGATOR_H

#include <QTreeWidget>
#include "data_access/NavigatorDAO.h"
#include <QMdiSubWindow>
#include <QMdiArea>
#include <QStyledItemDelegate> // 
#include <QSet>
#include <QStringList>
#include "../../core/common.h"


class QTreeWidgetItem; 

class DataNavigator : public QTreeWidget
{
    Q_OBJECT
public:
    explicit DataNavigator(QWidget* parent = nullptr);
    ~DataNavigator();

    // 公共接口
    void refreshNavigator();
    
    // 刷新指定节点下的内容
    void refreshNode(QTreeWidgetItem* item);


    void applySearchFilter(const QString& queryText);

    // 清除过滤，恢复导航树全部显示
    void clearSearchFilter();
    
signals:
    // 发送打开样本窗口的信号
    void requestOpenSampleViewWindow(int id,
        const QString &projectName, 
                            const QString &batchCode, const QString &shortCode,
                            int parallelNo, const QString &dataType);
    
    // 发送样本选择状态变化的信号
    void sampleSelectionChanged(int sampleId, const QString &sampleName, 
                               const QString &dataType, bool selected);
    
    // 请求选择批次中的所有样本
    void requestSelectAllSamplesInBatch(const NavigatorNodeInfo& batchInfo);

public:

    void addOpenView(QMdiSubWindow* window);
    void removeOpenView(QMdiSubWindow* window);
    void setActiveView(QMdiSubWindow* window);

    void setShowEmptyProjectAndBatch(bool show) { m_showEmptyProjectAndBatchNodes = show; }
    bool showEmptyProjectAndBatch() const { return m_showEmptyProjectAndBatchNodes; }


    void setHideNodesWithoutSamples(bool hide) { m_hideNodesWithoutSamples = hide; }
    bool hideNodesWithoutSamples() const { return m_hideNodesWithoutSamples; }
    
    void refreshDataSource();
    void refreshProcessData();
    void setupTree02();
    // 获取工作区根节点
    QTreeWidgetItem* getWorkspaceRoot() const { return m_workspaceRoot; }
    
    // 获取数据源根节点
    QTreeWidgetItem* getDataSourceRoot() const { return m_dataSourceRoot; }
    QTreeWidgetItem* getProcessDataRoot() const { return m_processDataRoot; }

private slots:
    // 用于懒加载的槽函数
    void onItemExpanded(QTreeWidgetItem* item);


    void onOpenViewItemClicked(QTreeWidgetItem* item, int column);
    
    // 处理双击样本事件
    void onItemDoubleClicked(QTreeWidgetItem* item, int column);
    
    // 处理选择框状态变化事件
    void onItemChanged(QTreeWidgetItem* item, int column);

protected:
    // 重写 contextMenuEvent 来处理右键点击
    void contextMenuEvent(QContextMenuEvent *event) override;

private:

    void setupTree();

    void loadBatches(QTreeWidgetItem* modelItem);
    void loadShortCodes(QTreeWidgetItem* batchItem);
    void loadDataTypes(QTreeWidgetItem* shortCodeItem);
    void loadParallelSamples(QTreeWidgetItem* dataTypeItem);
    void loadSamplesForBatch(QTreeWidgetItem* batchItem, const QString& projectName, const QString& batchCode, int batchId);


    void loadShortCodesForType(QTreeWidgetItem* typeItem, const QString& dataType);
    void loadParallelSamplesForShortCode(QTreeWidgetItem* shortCodeItem);
    void loadSamplesForProcessBatch(QTreeWidgetItem* batchItem);
    void loadBatchesForProcessProject(QTreeWidgetItem* projectItem);
    
    // 右键菜单功能
    void showSampleProperties(const struct NavigatorNodeInfo& info);
    void showSampleDataTable(const struct NavigatorNodeInfo& info);

 
    void parseQuery(const QString& queryText, QMap<QString, QString>& conds, QStringList& tokens) const;

    bool updateVisibilityRecursive(QTreeWidgetItem* item, const QMap<QString, QString>& conds, const QStringList& tokens);
    
   
    bool revealSamplesByDatabaseSearch(const QString& keyword);
    

    QTreeWidgetItem* ensureModelExpandedAndGetItem(QTreeWidgetItem* root, int modelId);
    QTreeWidgetItem* ensureBatchExpandedAndGetItem(QTreeWidgetItem* modelItem, int batchId);
    QTreeWidgetItem* ensureShortCodeExpandedAndGetItem(QTreeWidgetItem* batchItem, const QString& shortCode);
    QTreeWidgetItem* ensureDataTypeExpandedAndGetItem(QTreeWidgetItem* shortCodeItem, const QString& dataType);
    QTreeWidgetItem* findSampleItemUnderDataType(QTreeWidgetItem* dataTypeItem, int sampleId, int parallelNo);

public:
    // 根据数据类型启用/禁用样本选择框
    void enableSampleCheckboxesByDataType(const QString& dataType, bool enable);
    
    // 设置特定样本的选择框状态
    void setSampleCheckState(int sampleId, bool checked);
    // 按数据类型设置特定样本的复选框状态（自动展开路径）
    void setSampleCheckStateForType(int sampleId, const QString& dataType, bool checked);

    NavigatorDAO m_dao;


    QTreeWidgetItem* m_bigTgRoot;
    QTreeWidgetItem* m_smallTgRoot;
    QTreeWidgetItem* m_smallTgRawRoot;
    QTreeWidgetItem* m_chromRoot;


    QTreeWidgetItem* m_dataSourceRoot; // 保留以兼容旧代码，但不再作为根节点显示
    QTreeWidgetItem* m_workspaceRoot;
    QTreeWidgetItem* m_processDataRoot;
    QTreeWidgetItem* m_openSamplesRoot; // 打开的窗口节点
    

    QMap<QMdiSubWindow*, QTreeWidgetItem*> m_windowToItemMap;
    QMap<QTreeWidgetItem*, QMdiSubWindow*> m_itemToWindowMap;
    

    QMap<int, QList<QTreeWidgetItem*>> m_sampleToWindowItemsMap; // 样本ID到窗口项列表的映射


    QStringList m_currentTokens;               //
    QMap<QString, QString> m_currentConds;     // 当前键值条件（保留以便扩展）
    bool m_hasExpandedSnapshot = false;        // 是否已记录搜索前的展开状态
    QSet<QTreeWidgetItem*> m_expandedBeforeSearch; // 搜索前处于展开状态的节点集合
    QStyledItemDelegate* m_highlightDelegate = nullptr; // 高亮绘制委托

    bool m_showEmptyProjectAndBatchNodes = false;

    bool m_hideNodesWithoutSamples = true;

    bool m_inProgrammaticUpdate = false;
    QSet<QString> m_enabledSampleCheckboxTypes;
};

#endif // DATANAVIGATOR_H
