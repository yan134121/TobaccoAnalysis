#include "DifferenceResultModel.h"
#include <algorithm> // for std::sort
#include <QtMath>
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

    // 固定展示列顺序：欧氏距离、皮尔逊相关系数、NRMSE
    m_algorithmIds = QStringList() << "euclidean" << "pearson" << "nrmse";

    // 默认按“排名规则”排序：
    // 非参考样本优先（参考样本始终在最后，排名列显示“基准样本”）
    // 其余样本按 NRMSE 升序、Pearson 降序、Euclidean 升序
    std::sort(m_results.begin(), m_results.end(), [this](const DifferenceResultRow& a, const DifferenceResultRow& b) {
        const bool aIsRef = (a.sampleId == m_referenceSampleId);
        const bool bIsRef = (b.sampleId == m_referenceSampleId);
        if (aIsRef != bIsRef) {
            return !aIsRef;
        }

        const double aNrmse = a.scores.value("nrmse", 0.0);
        const double bNrmse = b.scores.value("nrmse", 0.0);
        if (!qFuzzyCompare(aNrmse + 1.0, bNrmse + 1.0)) {
            return aNrmse < bNrmse;
        }

        const double aPearson = a.scores.value("pearson", 0.0);
        const double bPearson = b.scores.value("pearson", 0.0);
        if (!qFuzzyCompare(aPearson + 1.0, bPearson + 1.0)) {
            return aPearson > bPearson;
        }

        const double aEuclidean = a.scores.value("euclidean", 0.0);
        const double bEuclidean = b.scores.value("euclidean", 0.0);
        return aEuclidean < bEuclidean;
    });

    endResetModel();
}

void DifferenceResultModel::setReferenceSampleId(int sampleId)
{
    m_referenceSampleId = sampleId;
}

int DifferenceResultModel::rowCount(const QModelIndex &parent) const
{
    // 如果 parent 是有效的，说明请求的是子项的行数，对于表格我们没有子项
    return parent.isValid() ? 0 : m_results.count();
}

int DifferenceResultModel::columnCount(const QModelIndex &parent) const
{
    // 列数 = 样本名列(1) + 指标列(3) + 排名列(1)
    return parent.isValid() ? 0 : m_algorithmIds.count() + 2;
}

QVariant DifferenceResultModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal) {
        return QVariant(); // 只处理水平表头的显示请求
    }

    if (section == 0) {
        return tr("样本名称");
    }

    // section 从1开始是指标列
    if (section - 1 < m_algorithmIds.count()) {
        QString algId = m_algorithmIds.at(section - 1);

        if (algId == QStringLiteral("euclidean")) return tr("欧氏距离");
        if (algId == QStringLiteral("pearson")) return tr("皮尔逊相关系数");
        if (algId == QStringLiteral("nrmse")) return tr("NRMSE");

        // 使用 m_appInit 替代 AppContext
        if (m_appInit && m_appInit->getSampleComparisonService()) {
            return m_appInit->getSampleComparisonService()->availableAlgorithms().value(algId, algId.toUpper());
        }
        return algId.toUpper(); // 备用显示
    }

    // 最后一列：排名
    if (section == m_algorithmIds.count() + 1) {
        return tr("排名");
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
            return SingleTobaccoSampleService::buildSampleDisplayName(rowData.sampleId);
        }

        if (index.column() - 1 < m_algorithmIds.count()) {
            QString algId = m_algorithmIds.at(index.column() - 1);
            // 显示原始值，保留4位小数
            return QString::number(rowData.scores.value(algId, 0.0), 'f', 4);
        }

        // 排名列（参考样本不参与排名）
        if (index.column() == m_algorithmIds.count() + 1) {
            if (rowData.sampleId == m_referenceSampleId) {
                return tr("基准样本");
            }

            int rank = 1;
            for (int i = 0; i < index.row(); ++i) {
                if (m_results.at(i).sampleId != m_referenceSampleId) {
                    ++rank;
                }
            }
            return rank;
        }
    }

    return QVariant();
}

void DifferenceResultModel::sort(int column, Qt::SortOrder order)
{
    // 通知视图
    emit layoutAboutToBeChanged();

    // 根据点击的列进行排序
    if (column == 0) { // 按样本名称排序
        std::sort(m_results.begin(), m_results.end(), [&](const auto& a, const auto& b){
            const QString aName = SingleTobaccoSampleService::buildSampleDisplayName(a.sampleId);
            const QString bName = SingleTobaccoSampleService::buildSampleDisplayName(b.sampleId);
            return order == Qt::AscendingOrder ? aName < bName : aName > bName;
        });
    } else if (column - 1 < m_algorithmIds.count()) { // 按某个指标排序
        QString algId = m_algorithmIds.at(column - 1);
        std::sort(m_results.begin(), m_results.end(), [&](const auto& a, const auto& b){
            double scoreA = a.scores.value(algId, 0.0);
            double scoreB = b.scores.value(algId, 0.0);
            return order == Qt::AscendingOrder ? scoreA < scoreB : scoreA > scoreB;
        });
    } else if (column == m_algorithmIds.count() + 1) {
        // 排名列由当前顺序自然生成，不额外处理
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
