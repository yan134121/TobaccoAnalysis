
#include "SampleViewWindow.h"
#include "data_access/SampleDAO.h"
#include "core/models/SampleDataModel.h"
#include "gui/views/ChartView.h" // 在 cpp 中包含
#include <QTableView>
#include <QMessageBox>
#include <QHeaderView>
#include <QSplitter>
#include <QItemSelectionModel>
#include "Logger.h" 
#include "AddCurveDialog.h"
#include <QRandomGenerator>
#include "common.h"


SampleViewWindow::SampleViewWindow(
    const QString &projectName,
                     const QString &batchCode,
                     const QString &shortCode,
                     int parallelNo, const QString &dataType, QWidget *parent)
    // : QMdiSubWindow(parent), 
     : MyWidget("实验视图", parent, "SampleViewWindow"),   //  初始化 MyWidget，而不是 QMdiSubWindow
  m_projectName(projectName),
  m_batchCode(batchCode),
  m_shortCode(shortCode),
  m_parallelNo(parallelNo),
  m_dataType(dataType)
{
    setAttribute(Qt::WA_DeleteOnClose);
    m_dao = new SampleDAO();
    m_model = new SampleDataModel(this);
    
    setupUi();
    
    connect(m_tableView->selectionModel(), &QItemSelectionModel::currentRowChanged, this, &SampleViewWindow::onTableRowClicked);

    // connect(m_chartView, &ChartView::requestAddMultipleCurves,
    //     this, &SampleViewWindow::onRequestAddMultipleCurves);
    //     connect(m_chartView, &ChartView::requestAddCurve,
    //     this, &SampleViewWindow::onRequestAddMultipleCurves);

    // this->showMaximized();


    loadData();
}

SampleViewWindow::~SampleViewWindow()
{
    delete m_dao;
}

void SampleViewWindow::setupUi()
{
    m_mainSplitter = new QSplitter(Qt::Vertical, this);
    m_tableView = new QTableView(m_mainSplitter);
    m_chartView = new ChartView(m_mainSplitter); // **现在只有一个 ChartView**

    m_tableView->setModel(m_model);
    // ... (表格 UI 设置不变)
    m_tableView->setAlternatingRowColors(true);
    m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableView->verticalHeader()->hide();
    m_tableView->horizontalHeader()->setStretchLastSection(true);
    
    m_mainSplitter->addWidget(m_chartView);
    m_mainSplitter->addWidget(m_tableView);
    // m_mainSplitter->addWidget(m_chartView);
    m_mainSplitter->setStretchFactor(0, 30);
    m_mainSplitter->setStretchFactor(1, 2);
    
    setWidget(m_mainSplitter);

    // 设置窗口初始大小（推荐添加在这里）
    // resize(600, 400); // 设置合适的初始大小
    resize(600, 400); // 备用：设置一个默认大小
    showMaximized();  // 打开时最大化

}

void SampleViewWindow::loadData()
{
    QString error;
    // 1. 加载表格数据：获取所有平行样的信息
    QList<SingleTobaccoSample*> samples = m_dao->fetchParallelSamplesInfo(m_projectName, m_batchCode, m_shortCode, m_parallelNo, error);
    if (!error.isEmpty()) { /* ...错误处理... */ return; }
    
    m_model->populate(samples);
    // setWindowTitle(tr("%1 - %2").arg(m_shortCode).arg(m_dataType));
    // 设置窗口标题，包含项目名、批次号、shortCode、平行样号和数据类型
    setWindowTitle(tr("%1-%2-%3-%4-%5")
                   .arg(m_projectName)
                   .arg(m_batchCode)
                   .arg(m_shortCode)
                   .arg(m_parallelNo)
                   .arg(m_dataType));

    // 2. 加载图表数据：遍历所有平行样，获取并绘制其特定类型的图表数据
    m_chartView->clearGraphs();
    error.clear();

    DataType dataType;
    if(m_dataType == "大热重"){
        dataType = DataType::TG_BIG;
    }else if(m_dataType == "小热重"){
        dataType = DataType::TG_SMALL;
    }else if(m_dataType == "色谱"){
        dataType = DataType::CHROMATOGRAM;
    }else{
        WARNING_LOG << "未知的数据类型:" << m_dataType;
        return;
    }
    
    for (SingleTobaccoSample* sample : samples) {
        QVector<QPointF> chartData = m_dao->fetchChartDataForSample(sample->id(), dataType, error);
        if (!error.isEmpty()) { /* ...错误处理... */ continue; }

        // int sampleId = m_currentSample["sample_id"].toInt();
        int sampleId = sample->id(); // <-- 改成这里

        QVector<double> xData, yData;
        for(const auto& p : chartData) { xData.append(p.x()); yData.append(p.y()); }
        
        QString graphName = tr("平行样 %1").arg(sample->parallelNo());
        m_chartView->addGraph(xData, yData, graphName, QColor::fromHsv(QRandomGenerator::global()->bounded(360), 200, 200), sampleId);
    }
    
    // 3. 设置坐标轴标签并重绘
    if (m_dataType == "大热重") {
        m_chartView->setLabels(tr("数据序号"), tr("热重微分值 (DTG)"));
    } else if (m_dataType == "小热重") {
        m_chartView->setLabels(tr("温度 (°C)"), tr("热重微分值 (DTG)"));
    } else if (m_dataType == "色谱") {
        m_chartView->setLabels(tr("保留时间 (min)"), tr("响应值"));
    }
    m_chartView->replot();
}

void SampleViewWindow::onTableRowClicked(const QModelIndex &current, const QModelIndex &previous)
{
    Q_UNUSED(previous);
    if (!current.isValid()) return;
    
    int parallelNo = m_model->data(m_model->index(current.row(), 3), Qt::DisplayRole).toInt();
    QString graphName = tr("平行样 %1").arg(parallelNo);
    m_chartView->highlightGraph(graphName);
}

ChartView* SampleViewWindow::activeChartView() const {
    // if (!m_chartTabs) return nullptr;
    // return qobject_cast<ChartView*>(m_chartTabs->currentWidget());
    // 【修正】直接返回唯一的图表视图指针
    return m_chartView;
}


// 实现接口方法，将命令转发给内部的 ChartView
void SampleViewWindow::setCurrentTool(const QString &toolId)
{
    if (m_chartView) {
        m_chartView->setToolMode(toolId);
    }
}

void SampleViewWindow::zoomIn()
{
    if (m_chartView) {
        // m_chartView->zoomIn();
        // 切换到框选放大模式
        m_chartView->setToolMode("zoom_rect");
    }
}

void SampleViewWindow::zoomOut()
{
    DEBUG_LOG;
    if (m_chartView) {
        m_chartView->zoomOutFixed();
    }
}

void SampleViewWindow::zoomReset()
{
    if (m_chartView) {
        m_chartView->zoomReset();
    }
}

// SampleViewWindow.cpp
void SampleViewWindow::onZoomRectToolClicked()
{
    if (m_chartView) {
        m_chartView->setToolMode("zoom_rect");
    }
}

void SampleViewWindow::selectArrow()
{
    if (m_chartView) {
        m_chartView->setToolMode("select");  // 或 "pan();
    }
}

#include "SampleDAO.h"
