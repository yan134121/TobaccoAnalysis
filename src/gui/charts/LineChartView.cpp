#include "LineChartView.h"
#include "Logger.h"

LineChartView::LineChartView(QWidget *parent)
    : QChartView(new QChart(), parent) // QChartView 构造函数需要一个 QChart 对象
{
    m_chart = chart(); // 获取内部的 QChart 对象
    setupChart();
}

void LineChartView::setupChart()
{
    m_chart->legend()->hide(); // 隐藏图例
    m_chart->setTitle("数据曲线图");

    // 初始化 X 轴
    m_xAxis = new QValueAxis();
    m_xAxis->setTitleText("X轴");
    m_chart->addAxis(m_xAxis, Qt::AlignBottom);

    // 初始化 Y 轴
    m_yAxis = new QValueAxis();
    m_yAxis->setTitleText("Y轴");
    m_chart->addAxis(m_yAxis, Qt::AlignLeft);
}

void LineChartView::setChartData(const QList<double>& xValues, const QList<double>& yValues,
                                 const QString& xLabel, const QString& yLabel,
                                 const QString& seriesName)
{
    clearChart(); // 清除旧数据

    if (xValues.isEmpty() || yValues.isEmpty() || xValues.size() != yValues.size()) {
        WARNING_LOG << "LineChartView: 无效的图表数据。";
        m_chart->setTitle("无数据");
        return;
    }

    m_series = new QLineSeries();
    m_series->setName(seriesName.isEmpty() ? "数据系列" : seriesName);

    for (int i = 0; i < xValues.size(); ++i) {
        m_series->append(xValues.at(i), yValues.at(i));
    }

    m_chart->addSeries(m_series);

    // 将系列关联到轴
    m_series->attachAxis(m_xAxis);
    m_series->attachAxis(m_yAxis);

    // 更新轴标签
    m_xAxis->setTitleText(xLabel);
    m_yAxis->setTitleText(yLabel);

    // 根据数据范围调整轴的显示范围
    // 可以根据数据自动计算或手动设置
    double minX = *std::min_element(xValues.begin(), xValues.end());
    double maxX = *std::max_element(xValues.begin(), xValues.end());
    double minY = *std::min_element(yValues.begin(), yValues.end());
    double maxY = *std::max_element(yValues.begin(), yValues.end());

    m_xAxis->setRange(minX - (maxX - minX) * 0.05, maxX + (maxX - minX) * 0.05);
    m_yAxis->setRange(minY - (maxY - minY) * 0.05, maxY + (maxY - minY) * 0.05);

    m_chart->setTitle(seriesName.isEmpty() ? "数据曲线图" : seriesName);
}

void LineChartView::clearChart()
{
    if (m_series) {
        m_chart->removeSeries(m_series);
        delete m_series;
        m_series = nullptr;
    }
    // 重置轴标题
    m_xAxis->setTitleText("X轴");
    m_yAxis->setTitleText("Y轴");
    m_chart->setTitle("数据曲线图");
}
