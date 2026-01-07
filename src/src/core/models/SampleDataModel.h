
#ifndef SAMPLEDATAMODEL_H
#define SAMPLEDATAMODEL_H

#include <QAbstractTableModel>
#include "core/entities/SingleTobaccoSample.h"

class SampleDataModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit SampleDataModel(QObject *parent = nullptr);
    ~SampleDataModel();

    // QAbstractTableModel 的纯虚函数实现
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    // 公共接口，用于填充或刷新模型的数据
    void populate(const QList<SingleTobaccoSample*>& sampleList);

private:
    QList<SingleTobaccoSample*> m_samples;
    QStringList m_headers;
};

#endif // SAMPLEDATAMODEL_H