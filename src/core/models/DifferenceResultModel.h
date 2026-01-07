#ifndef DIFFERENCERESULTMODEL_H
#define DIFFERENCERESULTMODEL_H

#include <QAbstractTableModel>
#include "core/common.h" // 确保 common.h 中包含了 DifferenceResultRow 的定义
#include "core/AppInitializer.h"

class DifferenceResultModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    // explicit DifferenceResultModel(QObject *parent = nullptr);
    explicit DifferenceResultModel(AppInitializer* appInit, QObject* parent = nullptr);
    
    // --- QAbstractTableModel 必须实现的虚函数 ---

    /**
     * @brief 返回表格的行数。
     */
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    /**
     * @brief 返回表格的列数。
     */
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    /**
     * @brief 获取指定索引处单元格的数据。
     * @param index 单元格的索引 (行和列)。
     * @param role 请求的数据角色 (我们主要处理 DisplayRole)。
     * @return 返回要显示的数据。
     */
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    /**
     * @brief 获取表头的数据。
     * @param section 行号或列号。
     * @param orientation 水平表头还是垂直表头。
     * @param role 请求的数据角色。
     * @return 返回表头标题。
     */
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    
    // --- 用于支持 QTableView 的点击列头排序功能 ---
    
    /**
     * @brief 当用户点击表头时，由视图调用，对数据进行排序。
     * @param column 用户点击的列。
     * @param order 升序或降序。
     */
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

    // --- 公共接口 ---

    /**
     * @brief 用新的计算结果填充或刷新模型。
     * @param results 从 Service 层获取的差异度计算结果列表。
     */
    void populate(const QList<DifferenceResultRow>& results);

    /**
     * @brief 根据行号获取对应的样本ID。
     * @param row 行号。
     * @return 样本ID，如果行号无效则返回-1。
     */
    int getSampleIdForRow(int row) const;

private:
    QList<DifferenceResultRow> m_results; // 存储原始的、未排序的数据
    // 用于按列索引算法ID，以保证列的顺序在刷新和排序时是稳定的
    QStringList m_algorithmIds; 
    AppInitializer* m_appInit;  // 保存指针
};

#endif // DIFFERENCERESULTMODEL_H