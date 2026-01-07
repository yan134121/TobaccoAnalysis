#include "DifferenceResultModel.h"
#include <algorithm> // for std::sort
#include "services/analysis/SampleComparisonService.h" // 用于获取算法的友好名称
#include "core/AppInitializer.h"
#include "services/SingleTobaccoSampleService.h"

// DifferenceResultModel::DifferenceResultModel(QObject *parent) : QAbstractTableModel(parent) {}
DifferenceResultModel::DifferenceResultModel(AppInitializer* appInit, QObject* parent)
    : QAbstractTableModel(parent),
      m_appInit(appInit) {}

void DifferenceResultModel::populate(const QList<DifferenceResultRow> &results)
{
    // beginResetModel/endResetModel 会通知所有连接的视图（如 QTableView）
    beginResetModel();
    
    m_results = results;
    
    // 从第一条结果中动态地生成表头（算法列）
    m_algorithmIds.clear();
    if (!m_results.isEmpty()) {
        m_algorithmIds = m_results.first().scores.keys();
        // 对算法ID进行排序，以保证每次显示的列顺序都一样
        std::sort(m_algorithmIds.begin(), m_algorithmIds.end());
    }
    
    endResetModel();
}

int DifferenceResultModel::rowCount(const QModelIndex &parent) const
{
    // 如果 parent 是有效的，说明请求的是子项的行数，对于表格我们没有子项
    return parent.isValid() ? 0 : m_results.count();
}

int DifferenceResultModel::columnCount(const QModelIndex &parent) const
{
    // 列数 = 样本名列(1) + 算法数量
    return parent.isValid() ? 0 : m_algorithmIds.count() + 1;
}

QVariant DifferenceResultModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal) {
        return QVariant(); // 只处理水平表头的显示请求
    }
    
    if (section == 0) {
        return tr("样本名称");
    }
    
    // section 从1开始是算法列
    if (section - 1 < m_algorithmIds.count()) {
        QString algId = m_algorithmIds.at(section - 1);
        
        // // 从 Service 获取用户可见的友好名称，如果获取失败，则直接显示ID
        // if (AppContext::instance() && AppContext::instance()->m_sampleComparisonService()) {
        //     return AppContext::instance()->m_sampleComparisonService()->availableAlgorithms().value(algId, algId.toUpper());
        // }
        // 使用 m_appInit 替代 AppContext
        if (m_appInit && m_appInit->getSampleComparisonService()) {
            return m_appInit->getSampleComparisonService()->availableAlgorithms().value(algId, algId.toUpper());
        }
        return algId.toUpper(); // 备用显示
    }
    
    return QVariant();
}

QVariant DifferenceResultModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_results.count()) {
        return QVariant();
    }
    
    // 我们只处理 DisplayRole (显示) 和 ToolTipRole (鼠标悬停提示)
    if (role == Qt::DisplayRole || role == Qt::ToolTipRole) {
        const DifferenceResultRow& rowData = m_results.at(index.row());
        
        if (index.column() == 0) {
            // return rowData.sampleName;
            return SingleTobaccoSampleService::buildSampleDisplayName(rowData.sampleId);
        }
        
        if (index.column() - 1 < m_algorithmIds.count()) {
            QString algId = m_algorithmIds.at(index.column() - 1);
            // 返回对应算法的得分，格式化为小数点后4位
            return QString::number(rowData.scores.value(algId, 0.0), 'f', 2);
        }
    }
    
    return QVariant();
}

void DifferenceResultModel::sort(int column, Qt::SortOrder order)
{
    // 通知视图
    emit layoutAboutToBeChanged();
    
    // 根据点击的列进行排序
    if (column == 0) { // 按样本名称的字母顺序排序
        std::sort(m_results.begin(), m_results.end(), [&](const auto& a, const auto& b){
            return order == Qt::AscendingOrder ? a.sampleName < b.sampleName : a.sampleName > b.sampleName;
        });
    } else if (column - 1 < m_algorithmIds.count()) { // 按某个算法的得分排序
        QString algId = m_algorithmIds.at(column - 1);
        std::sort(m_results.begin(), m_results.end(), [&](const auto& a, const auto& b){
            double scoreA = a.scores.value(algId, 0.0);
            double scoreB = b.scores.value(algId, 0.0);
            return order == Qt::AscendingOrder ? scoreA < scoreB : scoreA > scoreB;
        });
    }
    
    // 通知视图，布局已经改变，请刷新
    emit layoutChanged();
}

int DifferenceResultModel::getSampleIdForRow(int row) const
{
    if (row >= 0 && row < m_results.count()) {
        return m_results.at(row).sampleId;
    }
    return -1;
}
