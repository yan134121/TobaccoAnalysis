#ifndef PROCESSTGBIGDATATABLEMODEL_H
#define PROCESSTGBIGDATATABLEMODEL_H

#include <QAbstractTableModel>
#include <QList>
#include "ProcessTgBigData.h"

class ProcessTgBigDataTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit ProcessTgBigDataTableModel(QObject *parent = nullptr);
    ~ProcessTgBigDataTableModel() override = default;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    void setProcessTgBigData(const QList<ProcessTgBigData>& data);
    QList<ProcessTgBigData> getProcessTgBigData() const { return m_data; }

    enum Column {
        Id = 0,
        SampleId,
        SerialNo,
        Temperature,
        Weight,
        TgValue,
        DtgValue,
        SourceName,
        CreatedAt,
        ColumnCount
    };

private:
    QList<ProcessTgBigData> m_data;
    QStringList m_headers;
};

#endif // PROCESSTGBIGDATATABLEMODEL_H