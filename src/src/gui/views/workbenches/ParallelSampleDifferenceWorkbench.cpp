#include "ParallelSampleDifferenceWorkbench.h"
#include "core/AppInitializer.h"
#include "gui/views/ChartView.h"
#include "services/analysis/ParallelSampleAnalysisService.h"
#include "data_access/SingleTobaccoSampleDAO.h"
#include "ColorUtils.h"
#include "Logger.h"

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QSplitter>

// 本地工具：从 SampleDataFlexible 根据 StageName 取曲线
static QSharedPointer<Curve> getCurveFromStageLocal(const SampleDataFlexible& sample, StageName stage)
{
    for (const StageData& s : sample.stages) {
        if (s.stageName == stage) return s.curve;
    }
    return QSharedPointer<Curve>();
}

ParallelSampleDifferenceWorkbench::ParallelSampleDifferenceWorkbench(
        const QString& baseIdentifier,
        BatchGroupData allProcessedData,
        AppInitializer* appInit,
        const ProcessingParameters& params,
        StageName stage,
        QWidget* parent)
    : QMdiSubWindow(parent)
    , m_baseIdentifier(baseIdentifier)
    , m_processedData(std::move(allProcessedData))
    , m_appInitializer(appInit)
    , m_params(params)
    , m_stage(stage)
{
    setupUi();
    calculateAndDisplay();
}

void ParallelSampleDifferenceWorkbench::setupUi()
{
    QWidget* container = new QWidget(this);
    QVBoxLayout* vbox = new QVBoxLayout(container);

    QSplitter* splitter = new QSplitter(Qt::Vertical, container);
    m_chartView = new ChartView(splitter);
    m_tableView = new QTableView(splitter);
    splitter->addWidget(m_chartView);
    splitter->addWidget(m_tableView);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);

    vbox->addWidget(splitter);
    container->setLayout(vbox);
    setWidget(container);

    m_tableModel = new QStandardItemModel(this);
    m_tableModel->setHorizontalHeaderLabels({"组键", "代表样ID", "代表样名称"});
    m_tableView->setModel(m_tableModel);
    m_tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    setWindowTitle(tr("平行样代表选择：%1").arg(m_baseIdentifier));
}

void ParallelSampleDifferenceWorkbench::calculateAndDisplay()
{
    if (!m_appInitializer) {
        WARNING_LOG << "ParallelSampleDifferenceWorkbench: AppInitializer is null";
        return;
    }

    // 1) 运行代表性选择
    ParallelSampleAnalysisService* svc = m_appInitializer->getParallelSampleAnalysisService();
    if (!svc) {
        WARNING_LOG << "ParallelSampleDifferenceWorkbench: Analysis service unavailable";
        return;
    }
    svc->selectRepresentativesInBatch(m_processedData, m_params, m_stage);

    // 2) 绘制代表样曲线，并填充代表样表
    m_chartView->clearGraphs();
    m_tableModel->removeRows(0, m_tableModel->rowCount());

    SingleTobaccoSampleDAO dao;
    int colorIndex = 0;
    for (auto it = m_processedData.constBegin(); it != m_processedData.constEnd(); ++it) {
        const QString& groupKey = it.key();
        const SampleGroup& group = it.value();

        // 找到代表样
        const SampleDataFlexible* repSample = nullptr;
        for (const SampleDataFlexible& s : group.sampleDatas) {
            if (s.bestInGroup) { repSample = &s; break; }
        }
        if (!repSample) {
            // 未能选出代表样，跳过
            continue;
        }

        // 2.1 图表添加代表样曲线
        QSharedPointer<Curve> curve = getCurveFromStageLocal(*repSample, m_stage);
        if (!curve.isNull()) {
            QColor curveColor = ColorUtils::setCurveColor(colorIndex++);
            curve->setColor(curveColor);
            curve->setName(groupKey);
            m_chartView->addCurve(curve);
        }

        // 2.2 表格展示代表样信息
        SampleIdentifier sid = dao.getSampleIdentifierById(repSample->sampleId);
        QString displayName = QString("%1-%2-%3-%4")
                                .arg(sid.projectName)
                                .arg(sid.batchCode)
                                .arg(sid.shortCode)
                                .arg(sid.parallelNo);
        QList<QStandardItem*> rowItems;
        rowItems << new QStandardItem(groupKey)
                 << new QStandardItem(QString::number(repSample->sampleId))
                 << new QStandardItem(displayName);
        m_tableModel->appendRow(rowItems);
    }

    m_chartView->setPlotTitle(tr("代表样曲线（阶段：%1）").arg(static_cast<int>(m_stage)));
}