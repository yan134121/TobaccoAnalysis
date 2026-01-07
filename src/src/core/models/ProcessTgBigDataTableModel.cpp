#include "ProcessTgBigDataTableModel.h"
#include <QDebug>

ProcessTgBigDataTableModel::ProcessTgBigDataTableModel(QObject *parent)
    : QAbstractTableModel(parent)
{
    m_headers << "ID" << "样本ID" << "平行样号" << "序号" << "重量" << "热重值" << "热重微分" << "源文件" << "创建时间";
}

int ProcessTgBigDataTableModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return m_data.count();
}

int ProcessTgBigDataTableModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return ColumnCount;
}

QVariant ProcessTgBigDataTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_data.size() || index.row() < 0)
        return QVariant();

    const ProcessTgBigData &item = m_data.at(index.row());

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
            case Id: return item.getId();
            case SampleId: return item.getSampleId();
            case SerialNo: return item.getSerialNo();
            case Temperature: return item.getTemperature();
            case Weight: return item.getWeight();
            case TgValue: return item.getTgValue();
            case DtgValue: return item.getDtgValue();
            case SourceName: return item.getSourceName();
            case CreatedAt: return item.getCreatedAt().toString(Qt::ISODate);
            default: break;
        }
    }
    return QVariant();
}

QVariant ProcessTgBigDataTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
        if (section >= 0 && section < m_headers.size()) {
            return m_headers.at(section);
        }
    }
    return QAbstractTableModel::headerData(section, orientation, role);
}

void ProcessTgBigDataTableModel::setProcessTgBigData(const QList<ProcessTgBigData>& data)
{
    beginResetModel();
    m_data = data;
    endResetModel();
}