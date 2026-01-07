#ifndef CHROMATOGRAPHYDATATABLEMODEL_H
#define CHROMATOGRAPHYDATATABLEMODEL_H

#include <QAbstractTableModel>
#include <QList>
#include "ChromatographyData.h" // 引入 ChromatographyData 实体

class ChromatographyDataTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit ChromatographyDataTableModel(QObject *parent = nullptr);
    ~ChromatographyDataTableModel() override = default;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    void setChromatographyData(const QList<ChromatographyData>& data);
    QList<ChromatographyData> getChromatographyData() const { return m_data; }

    enum Column {
        Id = 0,
        SampleId,
        ReplicateNo,
        RetentionTime, // <-- 字段名不同
        ResponseValue, // <-- 字段名不同
        SourceName,
        CreatedAt,
        ColumnCount
    };

private:
    QList<ChromatographyData> m_data;
    QStringList m_headers;
};

#endif // CHROMATOGRAPHYDATATABLEMODEL_H
