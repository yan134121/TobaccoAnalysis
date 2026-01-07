#ifndef LINECHARTVIEW_H
#define LINECHARTVIEW_H

#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QDebug>

QT_CHARTS_USE_NAMESPACE // 使用 QtCharts 命名空间

class LineChartView : public QChartView
{
    Q_OBJECT
public:
    explicit LineChartView(QWidget *parent = nullptr);
    ~LineChartView() override = default;

    // 设置图表数据
    // xValues: x轴数据，yValues: y轴数据
    // xLabel: x轴标签，yLabel: y轴标签
    // seriesName: 曲线名称
    void setChartData(const QList<double>& xValues, const QList<double>& yValues,
                      const QString& xLabel = QString(), const QString& yLabel = QString(),
                      const QString& seriesName = QString());

    // 清除图表
    void clearChart();

private:
    QChart* m_chart = nullptr;
    QLineSeries* m_series = nullptr;
    QValueAxis* m_xAxis = nullptr;
    QValueAxis* m_yAxis = nullptr;

    void setupChart(); // 初始化图表
};

#endif // LINECHARTVIEW_H