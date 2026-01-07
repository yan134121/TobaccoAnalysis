#ifndef TGBIGDATATABLEMODEL_H
#define TGBIGDATATABLEMODEL_H

#include <QAbstractTableModel>
#include <QList>
#include "TgBigData.h"

class TgBigDataTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit TgBigDataTableModel(QObject *parent = nullptr);
    ~TgBigDataTableModel() override = default;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    void setTgBigData(const QList<TgBigData>& data);
    QList<TgBigData> getTgBigData() const { return m_data; }

    enum Column {
        Id = 0,
        SampleId,
        SerialNo,
        Temperature,
        TgValue,
        DtgValue,
        SourceName,
        CreatedAt,
        ColumnCount
    };

private:
    QList<TgBigData> m_data;
    QStringList m_headers;
};

#endif // TGBIGDATATABLEMODEL_H
