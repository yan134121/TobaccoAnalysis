
#ifndef TGSMALLDIFFERENCEWORKBENCH_H
#define TGSMALLDIFFERENCEWORKBENCH_H

#include <QMdiSubWindow>
#include <QSharedPointer>
#include <QModelIndex>
#include <QList>

#include "core/common.h"
#include "core/AppInitializer.h"
// #include "gui/mainwindow.h"  // 根据你项目的实际路径

// 前向声明，避免重复包含
class ChartView;
class QTableView;
class QPushButton;
class DifferenceResultModel;
class Curve;
struct DifferenceResultRow;   //  添加这个或引入对应头文件

class TgSmallDifferenceWorkbench : public QMdiSubWindow
{
    Q_OBJECT
public:
    // 构造函数接收"参考曲线"和"所有对比曲线"
    explicit TgSmallDifferenceWorkbench(int referenceSampleId,
                                 const BatchGroupData& allProcessedData,
                                 AppInitializer* appInit,
                                 const ProcessingParameters& params,
                                 QWidget* parent = nullptr);
    // TgSmallDifferenceWorkbench(QSharedPointer<Curve> referenceCurve,
    //                     const QList<QSharedPointer<Curve>>& allCurves,
    //                     AppInitializer* appInit,
    //                     MainWindow* mainWindow,  //  传入 MainWindow 指针
    //                     QWidget* parent = nullptr);

signals:
    void closed(TgSmallDifferenceWorkbench* self);

private slots:
    // 当在结果表格中点击一行时
    void onResultTableRowClicked(const QModelIndex& index);
    // 显示最佳样品差异排序表
    void onShowBestSampleRankingTable();

protected:  //  必须是 protected 或 public
    void closeEvent(QCloseEvent* event) override;

private:
    void setupUi();
    void calculateAndDisplay();
    // void updateChartHighlight();  //  暂未实现时可删除
    
    // // 计算原始数据的差异度
    // double calculateRawRMSE(QSharedPointer<Curve> curve1, QSharedPointer<Curve> curve2);
    // double calculateRawPearson(QSharedPointer<Curve> curve1, QSharedPointer<Curve> curve2);
    // double calculateRawEuclidean(QSharedPointer<Curve> curve1, QSharedPointer<Curve> curve2);
    
    // 计算综合评分
    double calculateComprehensiveScore(const QMap<QString, double>& scores);
    
    // 导出到Excel
    void exportToExcel(const QList<QMap<QString, QVariant>>& data, const QString& filePath);

    QSharedPointer<Curve> m_referenceCurve;
    QList<QSharedPointer<Curve>> m_allCurves;
    QList<DifferenceResultRow> m_resultCache;

    int m_referenceSampleId;
    BatchGroupData m_processedData;

    // UI 控件
    ChartView* m_chartView = nullptr;
    QTableView* m_tableView = nullptr;
    DifferenceResultModel* m_model = nullptr;
    QPushButton* m_bestSampleRankingButton = nullptr;

    AppInitializer* m_appInitializer = nullptr;

    ProcessingParameters m_processingParams;
};

#endif // TGSMALLDIFFERENCEWORKBENCH_H
