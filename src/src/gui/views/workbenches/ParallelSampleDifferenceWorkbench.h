#ifndef PARALLELSAMPLEDIFFERENCEWORKBENCH_H
#define PARALLELSAMPLEDIFFERENCEWORKBENCH_H

#include <QMdiSubWindow>
#include <QString>
#include <QTableView>
#include <QStandardItemModel>
#include <QSharedPointer>

class AppInitializer;
class ChartView;
class DifferenceResultModel;

// 使用 BatchGroupData 作为批量分组数据结构
#include "core/common.h"

class ParallelSampleDifferenceWorkbench : public QMdiSubWindow
{
    Q_OBJECT

public:
    explicit ParallelSampleDifferenceWorkbench(
        const QString& baseIdentifier,
        BatchGroupData allProcessedData,
        AppInitializer* appInit,
        const ProcessingParameters& params = ProcessingParameters{},
        StageName stage = StageName::RawData,
        QWidget* parent = nullptr
    );

private:
    void setupUi();
    void calculateAndDisplay();

private:
    QString m_baseIdentifier;
    BatchGroupData m_processedData;
    AppInitializer* m_appInitializer;
    ProcessingParameters m_params;
    StageName m_stage;

    // UI组件
    ChartView* m_chartView = nullptr;
    QTableView* m_tableView = nullptr;
    QStandardItemModel* m_tableModel = nullptr; // 展示每组代表样
};

#endif // PARALLELSAMPLEDIFFERENCEWORKBENCH_H
