

// Suggested Filename: src/model/SingleTobaccoSampleTableModel.cpp
#include "SingleTobaccoSampleTableModel.h" // Modified: Include corresponding header
#include <QDebug>

// Modified: Class name
SingleTobaccoSampleTableModel::SingleTobaccoSampleTableModel(QObject *parent)
    : QAbstractTableModel(parent)
{
    // Modified: Initialize headers to match the new Column enum
    // m_headers << "ID" << "批次ID" << "香烟型号" << "批次代码" << "样本编码" << "平行样号"
    //           << "样本名称" << "年份" << "产地" << "部位" << "等级" << "打料方式"
    //           << "备注" << "采集日期" << "检测日期" << "创建时间";
    m_headers << "ID" << "批次ID" << "香烟型号" << "批次代码" << "样本编码" << "平行样号"
            << "样本名称" ;
}

int SingleTobaccoSampleTableModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    // Modified: Use m_samples
    return m_samples.count();
}

int SingleTobaccoSampleTableModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return ColumnCount;
}

QVariant SingleTobaccoSampleTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_samples.size() || index.row() < 0)
        return QVariant();

    // Modified: Use SingleTobaccoSampleData and m_samples
    const SingleTobaccoSampleData &sample = m_samples.at(index.row());

    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        // Modified: The switch statement is completely updated
        switch (index.column()) {
            case Id: return sample.getId();
            case BatchId: return sample.getBatchId();
            case ProjectName: return sample.getProjectName();
            case BatchCode: return sample.getBatchCode();
            case ShortCode: return sample.getShortCode();
            case ParallelNo: return sample.getParallelNo();
            case SampleName: return sample.getSampleName();
            case Year: return sample.getYear();
            case Origin: return sample.getOrigin();
            case Part: return sample.getPart();
            case Grade: return sample.getGrade();
            case Type: return sample.getType();
            case Note: return sample.getNote();
            case CollectDate: return sample.getCollectDate().toString(Qt::ISODate);
            case DetectDate: return sample.getDetectDate().toString(Qt::ISODate);
            case CreatedAt: return sample.getCreatedAt().toString("yyyy-MM-dd hh:mm:ss"); // More readable format
            default: break;
        }
    }
    return QVariant();
}

QVariant SingleTobaccoSampleTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
        if (section >= 0 && section < m_headers.size()) {
            return m_headers.at(section);
        }
    }
    return QAbstractTableModel::headerData(section, orientation, role);
}

Qt::ItemFlags SingleTobaccoSampleTableModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;

    Qt::ItemFlags defaultFlags = Qt::ItemIsSelectable | Qt::ItemIsEnabled;

    // Modified: Update the list of editable columns
    switch (index.column()) {
        case ProjectName:
        case BatchId:
        case BatchCode:
        case ShortCode:
        case ParallelNo:
        case SampleName:
        case Year:
        case Origin:
        case Part:
        case Grade:
        case Type:
        case Note:
        case CollectDate:
        case DetectDate:
            return defaultFlags | Qt::ItemIsEditable; // These columns can be edited
        default:
            return defaultFlags; // Others like ID, CreatedAt are not editable
    }
}

bool SingleTobaccoSampleTableModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || role != Qt::EditRole || index.row() >= m_samples.size() || index.row() < 0)
        return false;

    // Modified: Use SingleTobaccoSampleData and m_samples
    SingleTobaccoSampleData &sample = m_samples[index.row()];
    bool changed = false;

    // Modified: The switch statement is completely updated with new setters
    switch (index.column()) {
        case ProjectName: if (sample.getProjectName() != value.toString()) { sample.setProjectName(value.toString()); changed = true; } break;
        case BatchId: if (sample.getBatchId() != value.toInt()) { sample.setBatchId(value.toInt()); changed = true; } break;
        case BatchCode: if (sample.getBatchCode() != value.toString()) { sample.setBatchCode(value.toString()); changed = true; } break;
        case ShortCode: if (sample.getShortCode() != value.toString()) { sample.setShortCode(value.toString()); changed = true; } break;
        case ParallelNo: if (sample.getParallelNo() != value.toInt()) { sample.setParallelNo(value.toInt()); changed = true; } break;
        case SampleName: if (sample.getSampleName() != value.toString()) { sample.setSampleName(value.toString()); changed = true; } break;
        case Year: if (sample.getYear() != value.toInt()) { sample.setYear(value.toInt()); changed = true; } break;
        case Origin: if (sample.getOrigin() != value.toString()) { sample.setOrigin(value.toString()); changed = true; } break;
        case Part: if (sample.getPart() != value.toString()) { sample.setPart(value.toString()); changed = true; } break;
        case Grade: if (sample.getGrade() != value.toString()) { sample.setGrade(value.toString()); changed = true; } break;
        case Type: if (sample.getType() != value.toString()) { sample.setType(value.toString()); changed = true; } break;
        case Note: if (sample.getNote() != value.toString()) { sample.setNote(value.toString()); changed = true; } break;
        case CollectDate: if (sample.getCollectDate() != value.toDate()) { sample.setCollectDate(value.toDate()); changed = true; } break;
        case DetectDate: if (sample.getDetectDate() != value.toDate()) { sample.setDetectDate(value.toDate()); changed = true; } break;
        default: break;
    }

    if (changed) {
        emit dataChanged(index, index, {role});
        return true;
    }
    return false;
}

// Modified: Method names and types
void SingleTobaccoSampleTableModel::setSamples(const QList<SingleTobaccoSampleData>& samples)
{
    beginResetModel();
    m_samples = samples;
    endResetModel();
}

QList<SingleTobaccoSampleData> SingleTobaccoSampleTableModel::getSamples() const
{
    return m_samples;
}

SingleTobaccoSampleData SingleTobaccoSampleTableModel::getSampleAt(int row) const
{
    if (row >= 0 && row < m_samples.size()) {
        return m_samples.at(row);
    }
    return SingleTobaccoSampleData();
}

bool SingleTobaccoSampleTableModel::updateSampleAt(int row, const SingleTobaccoSampleData& updatedSample)
{
    if (row >= 0 && row < m_samples.size()) {
        m_samples[row] = updatedSample;
        emit dataChanged(index(row, 0), index(row, columnCount() - 1), {Qt::DisplayRole, Qt::EditRole});
        return true;
    }
    return false;
}
