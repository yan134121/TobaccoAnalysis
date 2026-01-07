#include "SampleDataModel.h"

SampleDataModel::SampleDataModel(QObject *parent)
    : QAbstractTableModel(parent)
{
    // 定义表格的列标题
    m_headers << tr("ID") << tr("香烟型号") << tr("批次号") << tr("样本编码") << tr("平行样编号")
              << tr("产地") << tr("等级") << tr("样本名称");
}

SampleDataModel::~SampleDataModel()
{
    // 模型析构时，释放其持有的所有 Sample 对象的内存
    qDeleteAll(m_samples);
}

int SampleDataModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return m_samples.count();
}

int SampleDataModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return m_headers.count();
}

QVariant SampleDataModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_samples.count())
        return QVariant();

    // 只处理显示角色
    if (role == Qt::DisplayRole) {
        SingleTobaccoSample* sample = m_samples.at(index.row());
        switch (index.column()) {
            case 0: return sample->id();
            case 1: return sample->projectName();
            case 2: return sample->batchCode();
            case 3: return sample->shortCode();
            case 4: return sample->parallelNo();
            case 5: return sample->origin();
            case 6: return sample->grade();
            case 7: return sample->sampleName();
            default: return QVariant();
        }
    }
    return QVariant();
}

QVariant SampleDataModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
        if (section >= 0 && section < m_headers.count())
            return m_headers.at(section);
    }
    return QAbstractTableModel::headerData(section, orientation, role);
}

void SampleDataModel::populate(const QList<SingleTobaccoSample *> &sampleList)
{
    beginResetModel();
    qDeleteAll(m_samples); // 清理旧数据
    m_samples = sampleList;
    endResetModel();
}