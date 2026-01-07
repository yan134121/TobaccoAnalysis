
#ifndef SINGLETOBACCOSAMPLETABLEMODEL_H // 
#define SINGLETOBACCOSAMPLETABLEMODEL_H

#include <QAbstractTableModel>
#include <QList>
#include "core/entities/SingleTobaccoSampleData.h" // 

// 
class SingleTobaccoSampleTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit SingleTobaccoSampleTableModel(QObject *parent = nullptr);
    ~SingleTobaccoSampleTableModel() override = default;

    // --- QAbstractTableModel Overrides ---
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

    // --- Custom Methods ---
    //
    void setSamples(const QList<SingleTobaccoSampleData>& samples);
    QList<SingleTobaccoSampleData> getSamples() const;
    SingleTobaccoSampleData getSampleAt(int row) const;
    bool updateSampleAt(int row, const SingleTobaccoSampleData& updatedSample);

    // 
    enum Column {
        Id = 0,
        BatchId,
        ProjectName,
        BatchCode,
        ShortCode,
        ParallelNo,
        SampleName,
        Year,
        Origin,
        Part,
        Grade,
        Type,
        Note,
        CollectDate,
        DetectDate,
        CreatedAt,
        ColumnCount // 
    };
    Q_ENUM(Column)

private:
    // 
    QList<SingleTobaccoSampleData> m_samples;
    QStringList m_headers;
};

#endif // SingleTobaccoSampleTableModel_H
