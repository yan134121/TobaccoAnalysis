#ifndef TGSMALLDATATABLEMODEL_H
#define TGSMALLDATATABLEMODEL_H

#include <QAbstractTableModel>
#include <QList>
#include "TgSmallData.h" // 引入 TgSmallData 实体

class TgSmallDataTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit TgSmallDataTableModel(QObject *parent = nullptr);
    ~TgSmallDataTableModel() override = default;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    void setTgSmallData(const QList<TgSmallData>& data);
    QList<TgSmallData> getTgSmallData() const { return m_data; }

    enum Column {
        Id = 0,
        SampleId,
        SerialNo,
        Temperature, // <-- 字段名不同
        TgValue,
        DtgValue,
        SourceName,
        CreatedAt,
        ColumnCount
    };

private:
    QList<TgSmallData> m_data;
    QStringList m_headers;
};

#endif // TGSMALLDATATABLEMODEL_H
