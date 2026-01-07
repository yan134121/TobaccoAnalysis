#include "ChromatographyDataTableModel.h"
#include <QDebug>

ChromatographyDataTableModel::ChromatographyDataTableModel(QObject *parent)
    : QAbstractTableModel(parent)
{
    m_headers << "ID" << "样本ID" << "平行样号" << "保留时间" << "响应值" << "源文件" << "创建时间";
}

int ChromatographyDataTableModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return m_data.count();
}

int ChromatographyDataTableModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return ColumnCount;
}

QVariant ChromatographyDataTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_data.size() || index.row() < 0)
        return QVariant();

    const ChromatographyData &item = m_data.at(index.row());

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
            case Id: return item.getId();
            case SampleId: return item.getSampleId();
            case ReplicateNo: return item.getReplicateNo();
            case RetentionTime: return item.getRetentionTime(); // <-- 字段名不同
            case ResponseValue: return item.getResponseValue(); // <-- 字段名不同
            case SourceName: return item.getSourceName();
            case CreatedAt: return item.getCreatedAt().toString(Qt::ISODate);
            default: break;
        }
    }
    return QVariant();
}

QVariant ChromatographyDataTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
        if (section >= 0 && section < m_headers.size()) {
            return m_headers.at(section);
        }
    }
    return QAbstractTableModel::headerData(section, orientation, role);
}

void ChromatographyDataTableModel::setChromatographyData(const QList<ChromatographyData>& data)
{
    beginResetModel();
    m_data = data;
    endResetModel();
}