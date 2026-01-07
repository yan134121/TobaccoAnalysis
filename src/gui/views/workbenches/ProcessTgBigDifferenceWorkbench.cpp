#include "ProcessTgBigDifferenceWorkbench.h"
#include "gui/views/ChartView.h"
#include "core/models/DifferenceResultModel.h"
#include "services/analysis/SampleComparisonService.h"
#include "services/analysis/ParallelSampleAnalysisService.h"
#include "core/AppInitializer.h"
// #include "gui/mainwindow.h"  // 根据你项目的实际路径
#include  "Logger.h"
#include "data_access/SingleTobaccoSampleDAO.h" // 修改: 包含对应的头文件
#include "common.h"
#include "ColorUtils.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QTableView>
#include <QHeaderView>
#include <QCloseEvent>
#include <QPushButton>
#include <QSplitter>
#include <QMessageBox>
#include <QFileDialog>
#include <QTableWidget>
#include <QDialog>
#include <QApplication>
#include <QDesktopWidget>
#include "gui/delegates/WidthAwareNumberDelegate.h"
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include "xlsxdocument.h"

class MainWindow; // 前向声明，不包含头文件

// 使用公共头文件中的委托类

ProcessTgBigDifferenceWorkbench::ProcessTgBigDifferenceWorkbench(int referenceSampleId,
                                 const BatchGroupData& allProcessedData,
                                         AppInitializer* appInit,
                                         const ProcessingParameters& params,
                                         QWidget* parent)
    : QMdiSubWindow(parent),
      m_referenceSampleId(referenceSampleId),
      m_processedData(allProcessedData),
      m_appInitializer(appInit),
      m_processingParams(params)
      {
    setupUi();              //  先初始化界面
    calculateAndDisplay();  //  再填充数据
}



void ProcessTgBigDifferenceWorkbench::setupUi()
{
    // --- 创建主布局 ---
    QWidget* container = new QWidget;
    QVBoxLayout* mainLayout = new QVBoxLayout(container);
    
    // --- 创建分割器 ---
    QSplitter* splitter = new QSplitter(Qt::Horizontal);

    m_chartView = new ChartView;
    m_tableView = new QTableView;
    m_model = new DifferenceResultModel(m_appInitializer, this);

    m_tableView->setModel(m_model);
    m_tableView->horizontalHeader()->setStretchLastSection(true);
    m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableView->setSelectionMode(QAbstractItemView::SingleSelection);

    // 添加到分割器
    splitter->addWidget(m_chartView);
    splitter->addWidget(m_tableView);

    m_tableView->setMinimumWidth(120);  // 设置右侧表格最小宽度

    //  设置伸缩因子
    splitter->setStretchFactor(0, 1); // 左侧 chart
    splitter->setStretchFactor(1, 1); // 右侧 table
    // 初始化大小
    splitter->setSizes({1,2});

    // --- 给 splitter 添加边距 ---
    splitter->setContentsMargins(10, 8, 10, 8);  // 左、上、右、下各 8px

    // 或者用样式表加边框
    splitter->setStyleSheet(
        "QSplitter {"
        "  border: 1px solid #cccccc;"
        "  border-radius: 4px;"
        "  background-color: #f9f9f9;"
        "}"
    );
    
    // 添加按钮
    m_bestSampleRankingButton = new QPushButton("最佳样品差异排序表");
    m_bestSampleRankingButton->setMinimumHeight(30);
    m_bestSampleRankingButton->setStyleSheet(
        "QPushButton {"
        "  background-color: #4a86e8;"
        "  color: white;"
        "  border: none;"
        "  border-radius: 4px;"
        "  padding: 5px 10px;"
        "  font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "  background-color: #3a76d8;"
        "}"
        "QPushButton:pressed {"
        "  background-color: #2a66c8;"
        "}"
    );
    
    // 添加组件到主布局
    mainLayout->addWidget(splitter);
    mainLayout->addWidget(m_bestSampleRankingButton);
    
    setWidget(container); // QMdiSubWindow 设置子窗口内容

    // 信号连接
    connect(m_tableView->selectionModel(), &QItemSelectionModel::currentRowChanged,
            this, &ProcessTgBigDifferenceWorkbench::onResultTableRowClicked);
    connect(m_bestSampleRankingButton, &QPushButton::clicked,
            this, &ProcessTgBigDifferenceWorkbench::onShowBestSampleRankingTable);
}



// ===================================================================
// ==          calculateAndDisplay 的完整实现 (使用新参数)        ==
// ===================================================================
void ProcessTgBigDifferenceWorkbench::calculateAndDisplay()
{
    // --- 1. 从传入的 m_processedData 中提取出用于计算的“微分”曲线 ---
    QSharedPointer<Curve> referenceCurve;
    QList<QSharedPointer<Curve>> allDerivativeCurves;
    
    // // 辅助函数 (可以放在 .cpp 顶部或一个工具类中)
    // auto getCurveFromStage = [](const MultiStageData& stages, const QString& stage) -> QSharedPointer<Curve> {
    //     if (stage == "derivative") return stages.derivative;
    //     if (stage == "smoothed") return stages.smoothed;
    //     // ...
    //     return nullptr;
    // };
    // 辅助函数：根据阶段枚举获取 Curve
    auto getCurveFromStage = [](const SampleDataFlexible& sample, StageName stage) -> QSharedPointer<Curve> {
        for (const StageData& s : sample.stages) {
            if (s.stageName == stage) {
                return s.curve;
            }
        }
        return nullptr;
    };
    
    // for (auto it = m_processedData.constBegin(); it != m_processedData.constEnd(); ++it) {
    //     // 将 SampleGroup 转换为 MultiStageData
    //     MultiStageData stageData;
    //     stageData.derivative = it.value().derivative;
    //     stageData.smoothed = it.value().smoothed;
    //     stageData.original = it.value().original;
    //     stageData.normalized = it.value().normalized;
        
    //     QSharedPointer<Curve> curve = getCurveFromStage(stageData, "derivative");
    //     if (!curve.isNull()) {
    //         allDerivativeCurves.append(curve);
    //         if (it.key().toInt() == m_referenceSampleId) {
    //             referenceCurve = curve;
    //         }
    //     }
    // }

    printBatchGroupData(m_processedData); // 调试输出整个批量组数据

    // 选择各组代表样，仅用于后续比较与绘图（保留参考样本）
    if (m_appInitializer && m_appInitializer->getParallelSampleAnalysisService()) {
        m_appInitializer->getParallelSampleAnalysisService()->selectRepresentativesInBatch(
            m_processedData, m_processingParams, StageName::RawData);
    }

    for (auto it = m_processedData.constBegin(); it != m_processedData.constEnd(); ++it) {
        const SampleGroup& group = it.value();

        // 遍历组内样本：只加入代表样或参考样本
        for (const SampleDataFlexible& sample : group.sampleDatas) {
            // 仅加入代表样或参考样本
            if (!sample.bestInGroup && sample.sampleId != m_referenceSampleId) continue;
            // 获取 RawData 阶段曲线
            QSharedPointer<Curve> curve = getCurveFromStage(sample, StageName::RawData);
            if (!curve.isNull()) {
                allDerivativeCurves.append(curve);
                if (sample.sampleId == m_referenceSampleId) {
                    referenceCurve = curve;
                }
            }
        }
    }

    if (referenceCurve.isNull()) {
        QMessageBox::critical(this, tr("错误"), tr("未能找到参考样本的有效微分数据。"));
        return;
    }
    
    setWindowTitle(tr("工序大热重差异度分析 - 参考样本ID: %1").arg(m_referenceSampleId));

    // --- 2. 调用 Service 进行差异度计算 ---
    if (!m_appInitializer || !m_appInitializer->getSampleComparisonService()) {
        QMessageBox::critical(this, tr("严重错误"), tr("差异度对比服务不可用。"));
        return;
    }
    SampleComparisonService* comparer = m_appInitializer->getSampleComparisonService();
    
    // 这个调用是同步的，因为所有数据都已在内存中
    m_resultCache = comparer->calculateRankingFromCurves(referenceCurve, allDerivativeCurves);

    // --- 3. 填充结果表格 ---
    m_model->populate(m_resultCache);
    if (m_model->columnCount() > 1) {
        // m_tableView->sortByColumn(1, Qt::AscendingOrder);
        m_tableView->sortByColumn(1, Qt::DescendingOrder);
    }

    // --- 4. 绘制所有微分曲线到图表，并设置详细的图例名称 ---
    m_chartView->clearGraphs();
    SingleTobaccoSampleDAO sampleDao; // 创建 DAO 用于查询样本详情

    int colorIndex = 0;
    // for (QSharedPointer<Curve> curve : allDerivativeCurves) {
    for (auto it = m_processedData.constBegin(); it != m_processedData.constEnd(); ++it) {

        const SampleGroup& group = it.value();

        // 遍历组内每个样本
        for (const SampleDataFlexible& sample : group.sampleDatas) {

        QSharedPointer<Curve> curve = getCurveFromStage(sample, StageName::RawData);
        // 使用 DAO 获取详细的样本标识符
        SampleIdentifier sid = sampleDao.getSampleIdentifierById(sample.sampleId);
        // DEBUG_LOG << " curve->sampleId():"<< it.key().toInt();
        QString legendName = QString("%1-%2-%3-%4")
                                .arg(sid.projectName)
                                .arg(sid.batchCode)
                                .arg(sid.shortCode)
                                .arg(sid.parallelNo);
        DEBUG_LOG << "legendName:legendName " << legendName;

        
        // DEBUG_LOG << "curve->sampleId():curve->sampleId " << m_processedData.keys();
        // for (auto id : m_processedData.keys()) {
        //     DEBUG_LOG << "sampleId:sampleId:" << id ;
        // }
        curve->setName(legendName);

        // 设置参考曲线的样式
        if (curve == referenceCurve) {
            curve->setColor(Qt::black);
            curve->setLineWidth(static_cast<int>(2.5));
        } else {
            // curve->setColor(QColor::fromHsv(qrand() % 360, 200, 200));
            QColor curveColor = ColorUtils::setCurveColor(colorIndex++);
            curve->setColor(curveColor);
        }
        
        m_chartView->addCurve(curve);
    }
    }
    m_chartView->replot();
}



void ProcessTgBigDifferenceWorkbench::onResultTableRowClicked(const QModelIndex &index)
{
    if (!index.isValid()) return;

    // 获取选中行对应的样本名称
    QString sampleName = m_model->data(m_model->index(index.row(), 0)).toString();
    DEBUG_LOG << "[onResultTableRowClicked] row:" << index.row() << "sampleName:" << sampleName;

    // --- 延迟高亮，避免最小化/布局问题 ---
    QTimer::singleShot(0, this, [this, sampleName]() {
        // 打印当前对象状态，便于调试
        DEBUG_LOG << "[delayed highlight] m_chartView:" << m_chartView
                  << " m_referenceCurve:" << m_referenceCurve.data();

        if (m_chartView) {
            // 高亮参考曲线
            if (!m_referenceCurve.isNull()) {
                DEBUG_LOG << "[delayed highlight] highlighting reference curve:" << m_referenceCurve->name();
                m_chartView->highlightGraph(m_referenceCurve->name());
            }
            // 高亮选中曲线
            DEBUG_LOG << "[delayed highlight] highlighting selected curve:" << sampleName;
            m_chartView->highlightGraph(sampleName);
        }
    });
}




// ProcessTgBigDifferenceWorkbench.cpp
void ProcessTgBigDifferenceWorkbench::closeEvent(QCloseEvent* event)
{
    QMdiSubWindow::closeEvent(event);
    DEBUG_LOG << "ProcessTgBigDifferenceWorkbench::closeEvent() emitted";
    emit closed(this);  // 通知外部
}


// 计算综合评分（使用界面权重参数进行加权）
double ProcessTgBigDifferenceWorkbench::calculateComprehensiveScore(const QMap<QString, double>& scores)
{
    double rmseNormalized      = scores.value("rmse_normalized", 0.0);
    double pearsonNormalized   = scores.value("pearson_normalized", 0.0);
    double euclideanNormalized = scores.value("euclidean_normalized", 0.0);

    double wRmse     = m_processingParams.weightNRMSE;
    double wPearson  = m_processingParams.weightPearson;
    double wEuclidean= m_processingParams.weightEuclidean;
    double totalWeight = wRmse + wPearson + wEuclidean;

    // DEBUG_LOG
    //     << "Calculating comprehensive score with weights:"
    //     << "RMSE weight:" << wRmse
    //     << "Pearson weight:" << wPearson
    //     << "Euclidean weight:" << wEuclidean
    //     << "Total weight:" << totalWeight
    //     << "Scores:"
    //     << "RMSE normalized:" << rmseNormalized
    //     << "Pearson normalized:" << pearsonNormalized
    //     << "Euclidean normalized:" << euclideanNormalized;

    double weighted = rmseNormalized * wRmse
                    + pearsonNormalized * wPearson
                    + euclideanNormalized * wEuclidean;

    // DEBUG_LOG << "Weighted sum:" << weighted;
    // DEBUG_LOG << "Weighted comprehensive score:" << (totalWeight > 0 ? (weighted / totalWeight) : 0.0);
    return totalWeight > 0 ? (weighted / totalWeight) : 0.0;
}


// 导出到Excel
void ProcessTgBigDifferenceWorkbench::exportToExcel(const QList<QMap<QString, QVariant>>& data, const QString& filePath)
{
    if (filePath.endsWith(".xlsx", Qt::CaseInsensitive)) {
        // 使用QXlsx库导出为真正的Excel文件
        QXlsx::Document xlsx;
        
        // 设置表头
        xlsx.write(1, 1, "样品前缀");
        xlsx.write(1, 2, "综合评分");
        xlsx.write(1, 3, "标准化均方根(原始)");
        xlsx.write(1, 4, "标准化均方根(归一化)");
        xlsx.write(1, 5, "皮尔逊相关系数(原始)");
        xlsx.write(1, 6, "皮尔逊相关系数(归一化)");
        xlsx.write(1, 7, "欧氏距离(原始)");
        xlsx.write(1, 8, "欧氏距离(归一化)");
        xlsx.write(1, 9, "排名");
        xlsx.write(1, 10, "是否全优");
        
        // 写入数据行
        int row = 2;
        for (const auto& rowData : data) {
            xlsx.write(row, 1, rowData["sample_prefix"].toString());
            xlsx.write(row, 2, rowData["comprehensive_score"].toString());
            xlsx.write(row, 3, rowData["rmse_raw"].toString());
            xlsx.write(row, 4, rowData["rmse_normalized"].toString());
            xlsx.write(row, 5, rowData["pearson_raw"].toString());
            xlsx.write(row, 6, rowData["pearson_normalized"].toString());
            xlsx.write(row, 7, rowData["euclidean_raw"].toString());
            xlsx.write(row, 8, rowData["euclidean_normalized"].toString());
            xlsx.write(row, 9, rowData["rank"].toString());
            xlsx.write(row, 10, rowData["is_optimal"].toBool() ? "是" : "否");
            row++;
        }
        
        // 保存Excel文件
        if (!xlsx.saveAs(filePath)) {
            QMessageBox::critical(this, "错误", "无法保存Excel文件: " + filePath);
        }
    } else {
        // 导出为CSV格式
        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QMessageBox::critical(this, "错误", "无法创建文件: " + filePath);
            return;
        }

        // 创建CSV格式的文本，使用UTF-8编码
        QTextStream out(&file);
        out.setCodec("UTF-8");
        
        // 写入BOM标记，帮助Excel识别UTF-8编码
        out.setGenerateByteOrderMark(true);
        
        // 写入表头
        out << "样品前缀\t综合评分\t标准化均方根(原始)\t标准化均方根(归一化)\t皮尔逊相关系数(原始)\t皮尔逊相关系数(归一化)\t欧氏距离(原始)\t欧氏距离(归一化)\t排名\t是否全优\n";
        
        // 写入数据行，使用制表符分隔
        for (const auto& row : data) {
            out << row["sample_prefix"].toString() << "\t"
                << row["comprehensive_score"].toString() << "\t"
                << row["rmse_raw"].toString() << "\t"
                << row["rmse_normalized"].toString() << "\t"
                << row["pearson_raw"].toString() << "\t"
                << row["pearson_normalized"].toString() << "\t"
                << row["euclidean_raw"].toString() << "\t"
                << row["euclidean_normalized"].toString() << "\t"
                << row["rank"].toString() << "\t"
                << (row["is_optimal"].toBool() ? "是" : "否") << "\n";
        }
        
        // 关闭文件
        file.close();
    }
    
    QMessageBox::information(this, "导出成功", "数据已成功导出到: " + filePath);
}


// 显示最佳样品差异排序表（含归一化）
void ProcessTgBigDifferenceWorkbench::onShowBestSampleRankingTable()
{
    // 创建对话框
    QDialog dialog(this);
    dialog.setWindowTitle("最佳样品差异排序表");
    dialog.setMinimumSize(900, 600);

    dialog.setWindowFlags(Qt::Window | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);
    
    // 居中显示
    QRect screenGeometry = QGuiApplication::primaryScreen()->geometry();
    dialog.move((screenGeometry.width() - dialog.width()) / 2,
                (screenGeometry.height() - dialog.height()) / 2);

    // 创建表格
    QTableWidget* tableWidget = new QTableWidget(&dialog);
    tableWidget->setColumnCount(10);
    QStringList headers;
    headers << "样品前缀" << "综合评分"
            << "标准化均方根(原始)" << "标准化均方根(归一化)"
            << "皮尔逊相关系数(原始)" << "皮尔逊相关系数(归一化)"
            << "欧氏距离(原始)" << "欧氏距离(归一化)"
            << "排名" << "是否全优";
    tableWidget->setHorizontalHeaderLabels(headers);
    tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    // 表头样式与交互设置
    tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive); // 允许拖拽调整列宽
    tableWidget->horizontalHeader()->setSectionsMovable(true);                       // 允许左右拖动调整列顺序

    // 安装宽度自适应数字显示委托（数值列：1..8）
    auto* widthDelegate = new WidthAwareNumberDelegate(tableWidget);
    for (int c = 1; c <= 8; ++c) {
        tableWidget->setItemDelegateForColumn(c, widthDelegate);
    }

    // 获取参考曲线
    QSharedPointer<Curve> referenceCurve;
    QList<QSharedPointer<Curve>> allCurves;
    // auto getCurveFromStage = [](const MultiStageData& stages, const QString& stage) -> QSharedPointer<Curve> {
    //     if (stage == "derivative") return stages.derivative;
    //     if (stage == "smoothed") return stages.smoothed;
    //     if (stage == "normalized") return stages.normalized;
    //     if (stage == "raw") return stages.original;
    //     return nullptr;
    // };

    auto getCurveFromStage = [](const SampleDataFlexible& sample, StageName stage) -> QSharedPointer<Curve> {
        for (const StageData& s : sample.stages) {
            if (s.stageName == stage) {
                return s.curve;
            }
        }
        return nullptr;
    };

    // for (auto it = m_processedData.constBegin(); it != m_processedData.constEnd(); ++it) {
    //     // 将 SampleGroup 转换为 MultiStageData
    //     MultiStageData stageData;
    //     stageData.derivative = it.value().derivative;
    //     stageData.smoothed = it.value().smoothed;
    //     stageData.original = it.value().original;
    //     stageData.normalized = it.value().normalized;
        
    //     QSharedPointer<Curve> derivativeCurve = getCurveFromStage(stageData, "derivative");
    //     if (!derivativeCurve.isNull()) {
    //         allCurves.append(derivativeCurve);
    //         if (it.key().toInt() == m_referenceSampleId)
    //             referenceCurve = derivativeCurve;
    //     }
    // }

    for (auto it = m_processedData.constBegin(); it != m_processedData.constEnd(); ++it) {
        const SampleGroup& group = it.value();

        // 遍历组内样本：只加入代表样或参考样本
        for (const SampleDataFlexible& sample : group.sampleDatas) {
            if (!sample.bestInGroup && sample.sampleId != m_referenceSampleId) continue;
            // 获取 RawData 阶段曲线
            QSharedPointer<Curve> curve = getCurveFromStage(sample, StageName::RawData);
            if (!curve.isNull()) {
                allCurves.append(curve);
                if (sample.sampleId == m_referenceSampleId) {
                    referenceCurve = curve;
                }
            }
        }
    }


    if (referenceCurve.isNull()) {
        QMessageBox::critical(this, "错误", "未能找到参考样本的有效数据");
        return;
    }

    // === 第一次遍历：计算原始结果，收集最大最小值 ===
    struct DiffResult {
        int sampleId;
        QString samplePrefix;
        double rmseRaw;
        double pearsonRaw;
        double euclideanRaw;
    };

    QList<DiffResult> diffResults;
    double rmseMax = 0.0, euclideanMax = 0.0;
    double pearsonMin = 999.0, pearsonMax = -999.0;

    SampleComparisonService* comparer = m_appInitializer->getSampleComparisonService();
    SingleTobaccoSampleDAO sampleDao;

    for (int i = 0; i < allCurves.size(); ++i) {
        QSharedPointer<Curve> curve = allCurves[i];
        if (curve.isNull()) continue;

        int sampleId = curve->sampleId();
        SampleIdentifier sid = sampleDao.getSampleIdentifierById(sampleId);
        QString samplePrefix = QString("%1-%2-%3")
                                    .arg(sid.projectName)
                                    .arg(sid.batchCode)
                                    .arg(sid.shortCode);

        double rmseRaw = comparer->calculateRMSE(referenceCurve, curve);
        double pearsonRaw = comparer->calculatePearsonCorrelation(referenceCurve, curve);
        double euclideanRaw = comparer->calculateEuclideanDistance(referenceCurve, curve);

        rmseMax = qMax(rmseMax, rmseRaw);
        euclideanMax = qMax(euclideanMax, euclideanRaw);
        pearsonMin = qMin(pearsonMin, pearsonRaw);
        pearsonMax = qMax(pearsonMax, pearsonRaw);

        diffResults.append({sampleId, samplePrefix, rmseRaw, pearsonRaw, euclideanRaw});
    }

    if (diffResults.isEmpty()) {
        QMessageBox::warning(this, "警告", "未能计算出任何差异数据。");
        return;
    }

    // === 第二次遍历：执行归一化与综合评分 ===
    QList<QMap<QString, QVariant>> resultData;

    for (const auto& res : diffResults) {
        double rmseNormalized = (rmseMax > 0) ? (1.0 - (res.rmseRaw / rmseMax)) : 0.0;
        double euclideanNormalized = (euclideanMax > 0) ? (1.0 - (res.euclideanRaw / euclideanMax)) : 0.0;
        double pearsonNormalized = (pearsonMax - pearsonMin > 1e-12)
                                   ? ((res.pearsonRaw - pearsonMin) / (pearsonMax - pearsonMin))
                                   : 0.0;

        QMap<QString, double> scores;
        scores["rmse_raw"] = res.rmseRaw;
        scores["rmse_normalized"] = rmseNormalized;
        scores["pearson_raw"] = res.pearsonRaw;
        scores["pearson_normalized"] = pearsonNormalized;
        scores["euclidean_raw"] = res.euclideanRaw;
        scores["euclidean_normalized"] = euclideanNormalized;

        double comprehensiveScore = calculateComprehensiveScore(scores);

        QMap<QString, QVariant> rowData;
        rowData["sample_id"] = res.sampleId;
        rowData["sample_prefix"] = res.samplePrefix;
        rowData["comprehensive_score"] = QString::number(comprehensiveScore, 'f', 16);
        rowData["rmse_raw"] = QString::number(res.rmseRaw, 'f', 16);
        rowData["rmse_normalized"] = QString::number(rmseNormalized, 'f', 16);
        rowData["pearson_raw"] = QString::number(res.pearsonRaw, 'f', 16);
        rowData["pearson_normalized"] = QString::number(pearsonNormalized, 'f', 16);
        rowData["euclidean_raw"] = QString::number(res.euclideanRaw, 'f', 16);
        rowData["euclidean_normalized"] = QString::number(euclideanNormalized, 'f', 16);
        rowData["score_value"] = comprehensiveScore;
        resultData.append(rowData);
    }

    // === 排序并添加排名与“全优”标记 ===
    std::sort(resultData.begin(), resultData.end(),
              [](const QMap<QString, QVariant>& a, const QMap<QString, QVariant>& b) {
                  return a["score_value"].toDouble() > b["score_value"].toDouble();
              });

    for (int i = 0; i < resultData.size(); ++i) {
        resultData[i]["rank"] = i + 1;
        double pearsonRaw = resultData[i]["pearson_raw"].toString().toDouble();
        double pearsonNormalized = resultData[i]["pearson_normalized"].toString().toDouble();
        bool isOptimal = (pearsonRaw >= PROCESS_TG_BIG_OPTIMAL_PEARSON_RAW_THRESHOLD
                          && pearsonNormalized >= PROCESS_TG_BIG_OPTIMAL_PEARSON_NORMALIZED_THRESHOLD);
        resultData[i]["is_optimal"] = isOptimal;
    }

    // === 填充表格 ===
    tableWidget->setRowCount(resultData.size());
    for (int i = 0; i < resultData.size(); ++i) {
        tableWidget->setItem(i, 0, new QTableWidgetItem(resultData[i]["sample_prefix"].toString()));
        tableWidget->setItem(i, 1, new QTableWidgetItem(resultData[i]["comprehensive_score"].toString()));
        tableWidget->setItem(i, 2, new QTableWidgetItem(resultData[i]["rmse_raw"].toString()));
        tableWidget->setItem(i, 3, new QTableWidgetItem(resultData[i]["rmse_normalized"].toString()));
        tableWidget->setItem(i, 4, new QTableWidgetItem(resultData[i]["pearson_raw"].toString()));
        tableWidget->setItem(i, 5, new QTableWidgetItem(resultData[i]["pearson_normalized"].toString()));
        tableWidget->setItem(i, 6, new QTableWidgetItem(resultData[i]["euclidean_raw"].toString()));
        tableWidget->setItem(i, 7, new QTableWidgetItem(resultData[i]["euclidean_normalized"].toString()));
        tableWidget->setItem(i, 8, new QTableWidgetItem(QString::number(resultData[i]["rank"].toInt())));
        tableWidget->setItem(i, 9, new QTableWidgetItem(resultData[i]["is_optimal"].toBool() ? "是" : "否"));

        if (resultData[i]["is_optimal"].toBool()) {
            for (int j = 0; j < tableWidget->columnCount(); ++j)
                tableWidget->item(i, j)->setBackground(QColor(200, 255, 200));
        }
    }

    // === 导出按钮 ===
    QPushButton* exportButton = new QPushButton("导出到Excel", &dialog);
    exportButton->setMinimumHeight(30);
    exportButton->setStyleSheet(
        "QPushButton { background-color: #4a86e8; color: white; border: none; border-radius: 4px; padding: 5px 10px; font-weight: bold; }"
        "QPushButton:hover { background-color: #3a76d8; }"
        "QPushButton:pressed { background-color: #2a66c8; }");

    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    layout->addWidget(tableWidget);
    // 新增“导出工艺内差异度详细表”按钮
    QPushButton* exportIntraButton = new QPushButton("导出工艺内差异度详细表", &dialog);
    exportIntraButton->setMinimumHeight(30);
    exportIntraButton->setStyleSheet(
        "QPushButton { background-color: #6aa84f; color: white; border: none; border-radius: 4px; padding: 5px 10px; font-weight: bold; }"
        "QPushButton:hover { background-color: #5a983f; }"
        "QPushButton:pressed { background-color: #4a882f; }");
    QHBoxLayout* btnRow = new QHBoxLayout();
    btnRow->addWidget(exportButton);
    btnRow->addWidget(exportIntraButton);
    layout->addLayout(btnRow);

    connect(exportButton, &QPushButton::clicked, [this, resultData]() {
        QString filePath = QFileDialog::getSaveFileName(this, "保存Excel文件",
                                                      QDir::homePath() + "/工序大热重最佳样品差异排序表.xlsx",
                                                      "Excel文件 (*.xlsx);;CSV文件 (*.csv)");
        if (!filePath.isEmpty()) {
            exportToExcel(resultData, filePath);
        }
    });
    // 连接工艺内差异度详细表导出
    connect(exportIntraButton, &QPushButton::clicked, [this]() {
        exportProcessIntraDifferenceDetailTable();
    });

    dialog.show();
    dialog.exec();
}


// 实现“工序-大热重工艺内差异度详细表”的计算与导出
void ProcessTgBigDifferenceWorkbench::exportProcessIntraDifferenceDetailTable()
{
    try {
        // 校验缓存批次数据是否有效
        if (m_processedData.isEmpty()) {
            QMessageBox::warning(this, "提示", "当前无可用的批次处理数据，无法导出工艺内差异度详细表。");
            return;
        }

        // 选择保存路径
        QString filePath = QFileDialog::getSaveFileName(
            this,
            "保存Excel文件",
            QDir::homePath() + "/工序-大热重工艺内差异度详细表.xlsx",
            "Excel文件 (*.xlsx)");
        if (filePath.isEmpty()) return;

        // 构造“工艺基础”分组（去除短码末尾阶段字母Q/Z/H）
        QMap<QString, QVector<int>> baseToSampleIds;
        QMap<QString, QVector<QString>> baseToSamplePrefixes;
        QMap<QString, QVector<QSharedPointer<Curve>>> baseToCurves;
        QMap<QString, QVector<QString>> baseToStages;

        QRegularExpression reSuffix(QStringLiteral("-(Q|Z|H)(?:-|$)"), QRegularExpression::CaseInsensitiveOption);

        for (auto it = m_processedData.constBegin(); it != m_processedData.constEnd(); ++it) {
            const SampleGroup& group = it.value();
            QString shortCodeBase = group.shortCode;
            QRegularExpressionMatch m = reSuffix.match(shortCodeBase);
            QString stageChar = "ALL";
            if (m.hasMatch()) {
                QString cap = m.captured(1);
                if (!cap.isEmpty()) {
                    stageChar = cap.toUpper();
                    // 移除末尾阶段标识，形成工艺基础短码
                    QString pattern = QStringLiteral("-(?:") + QRegularExpression::escape(cap) + QStringLiteral(")$");
                    shortCodeBase.replace(QRegularExpression(pattern, QRegularExpression::CaseInsensitiveOption), QString());
                }
            }
            // 工艺基础键：项目-批次-短码（无Q/Z/H后缀）
            QString baseKey = QString("%1-%2-%3").arg(group.projectName).arg(group.batchCode).arg(shortCodeBase);

            for (const SampleDataFlexible& sample : group.sampleDatas) {
                // 优先使用微分阶段（与 V2.2.1_origin 的 dtg_final 对齐），若无则退回原始阶段
                QSharedPointer<Curve> curve;
                // 直接复制工作台用法：从阶段列表中查找
                for (const StageData& s : sample.stages) {
                    if (s.stageName == StageName::Derivative && !s.curve.isNull()) {
                        curve = s.curve; break;
                    }
                }
                if (curve.isNull()) {
                    for (const StageData& s : sample.stages) {
                        if (s.stageName == StageName::RawData && !s.curve.isNull()) {
                            curve = s.curve; break;
                        }
                    }
                }
                if (curve.isNull()) continue;

                // 记录分组
                baseToSampleIds[baseKey].append(curve->sampleId());
                baseToCurves[baseKey].append(curve);
                baseToStages[baseKey].append(stageChar);
                SingleTobaccoSampleDAO dao;
                SampleIdentifier sid = dao.getSampleIdentifierById(curve->sampleId());
                QString samplePrefix = QString("%1-%2-%3").arg(sid.projectName).arg(sid.batchCode).arg(sid.shortCode);
                baseToSamplePrefixes[baseKey].append(samplePrefix);
            }
        }

        // 创建Excel文档
        QXlsx::Document xlsx;
        xlsx.addSheet("工艺内差异度详细表");
        xlsx.selectSheet("工艺内差异度详细表");

        // 写入表头
        int row = 1;
        xlsx.write(row, 1, "工艺基础");
        xlsx.write(row, 2, "阶段");
        xlsx.write(row, 3, "样品前缀");
        xlsx.write(row, 4, "归一化均方根误差");
        xlsx.write(row, 5, "皮尔逊相关系数");
        xlsx.write(row, 6, "欧几里得距离");
        xlsx.write(row, 7, "组合得分");
        xlsx.write(row, 8, "组内排名");
        row++;

        // 遍历每个工艺基础分组，计算均值曲线与组内差异度
        for (auto it = baseToCurves.constBegin(); it != baseToCurves.constEnd(); ++it) {
            const QString& baseKey = it.key();
            const QVector<QSharedPointer<Curve>>& curves = it.value();
            const QVector<QString>& stages = baseToStages[baseKey];
            const QVector<QString>& prefixes = baseToSamplePrefixes[baseKey];
            if (curves.size() == 0) continue;

            // 统一长度为该组内最短长度
            int minLen = std::numeric_limits<int>::max();
            for (const auto& c : curves) {
                minLen = std::min(minLen, c->pointCount());
            }
            if (minLen <= 0 || minLen == std::numeric_limits<int>::max()) continue;

            // 构造矩阵并计算均值曲线
            QVector<QVector<double>> Y;
            Y.reserve(curves.size());
            for (const auto& c : curves) {
                QVector<double> y; y.reserve(minLen);
                const QVector<QPointF>& data = c->data();
                for (int k = 0; k < minLen; ++k) y.append(data[k].y());
                Y.append(y);
            }
            QVector<double> meanCurve(minLen, 0.0);
            for (int k = 0; k < minLen; ++k) {
                double s = 0.0;
                for (int i = 0; i < Y.size(); ++i) s += Y[i][k];
                meanCurve[k] = s / Y.size();
            }

            // 计算组内归一化所需范围（稳健范围近似）
            double minMean = *std::min_element(meanCurve.begin(), meanCurve.end());
            double maxMean = *std::max_element(meanCurve.begin(), meanCurve.end());
            double localRefRange = std::max(maxMean - minMean, 1e-6);

            // 先收集原始指标以便做组内归一化
            QVector<double> allNR, allP, allE;
            allNR.reserve(curves.size());
            allP.reserve(curves.size());
            allE.reserve(curves.size());

            auto calcPearson = [&](const QVector<double>& a, const QVector<double>& b) -> double {
                int n = std::min(a.size(), b.size());
                if (n <= 1) return 1.0;
                double meanA = 0.0, meanB = 0.0;
                for (int i = 0; i < n; ++i) { meanA += a[i]; meanB += b[i]; }
                meanA /= n; meanB /= n;
                double num = 0.0, denA = 0.0, denB = 0.0;
                for (int i = 0; i < n; ++i) {
                    double da = a[i] - meanA;
                    double db = b[i] - meanB;
                    num += da * db;
                    denA += da * da;
                    denB += db * db;
                }
                double denom = std::sqrt(denA) * std::sqrt(denB);
                if (denom <= 1e-12) return 1.0;
                double r = num / denom;
                // 限制到[-1,1]
                if (r > 1.0) r = 1.0;
                if (r < -1.0) r = -1.0;
                return r;
            };

            QVector<double> sampleNR(curves.size(), 0.0);
            QVector<double> sampleP(curves.size(), 1.0);
            QVector<double> sampleE(curves.size(), 0.0);

            for (int i = 0; i < curves.size(); ++i) {
                const QVector<double>& a = Y[i];
                // MSE / 局部参考范围
                double mse = 0.0;
                for (int k = 0; k < minLen; ++k) {
                    double d = a[k] - meanCurve[k];
                    mse += d * d;
                }
                mse /= minLen;
                double rmse = std::sqrt(mse);
                double nrmse = rmse / localRefRange;
                sampleNR[i] = nrmse;
                double p = calcPearson(a, meanCurve);
                sampleP[i] = p;
                // 欧氏距离
                double e = 0.0;
                for (int k = 0; k < minLen; ++k) {
                    double d = a[k] - meanCurve[k];
                    e += d * d;
                }
                sampleE[i] = std::sqrt(e);
                allNR.append(nrmse);
                allP.append(p);
                allE.append(sampleE[i]);
            }

            auto vmin = [](const QVector<double>& v){ return v.isEmpty()? 0.0 : *std::min_element(v.begin(), v.end()); };
            auto vmax = [](const QVector<double>& v){ return v.isEmpty()? 1.0 : *std::max_element(v.begin(), v.end()); };
            double nrMin = vmin(allNR), nrMax = vmax(allNR);
            double pMin  = vmin(allP),  pMax  = vmax(allP);
            double eMin  = vmin(allE),  eMax  = vmax(allE);

            auto norm01 = [](double v, double vmin, double vmax){
                if (vmax <= vmin) return 0.0;
                return (v - vmin) / (vmax - vmin);
            };

            // 按权重计算组合得分（差异度越小越好）
            struct Row { QString stage; QString prefix; double nr; double p; double e; double comp; };
            QVector<Row> rowsForRank; rowsForRank.reserve(curves.size());
            for (int i = 0; i < curves.size(); ++i) {
                double nNR = norm01(sampleNR[i], nrMin, nrMax);         // 差异度指标
                double nP  = norm01(sampleP[i],  pMin,  pMax);          // 相关性（[0,1]越大越好）
                double nE  = norm01(sampleE[i],  eMin,  eMax);          // 差异度指标
                double comp = m_processingParams.weightNRMSE * nNR
                              + m_processingParams.weightEuclidean * nE
                              + m_processingParams.weightPearson * (1.0 - nP); // 相关性转为差异度
                rowsForRank.append({stages[i], prefixes[i], sampleNR[i], sampleP[i], sampleE[i], comp});
            }

            // 组内排序（差异度越小排名越靠前）
            std::sort(rowsForRank.begin(), rowsForRank.end(),
                      [](const Row& a, const Row& b){ return a.comp < b.comp; });

            // 写入Excel
            for (int i = 0; i < rowsForRank.size(); ++i) {
                const Row& r = rowsForRank[i];
                xlsx.write(row, 1, baseKey);
                xlsx.write(row, 2, r.stage);
                xlsx.write(row, 3, r.prefix);
                xlsx.write(row, 4, r.nr);
                xlsx.write(row, 5, r.p);
                xlsx.write(row, 6, r.e);
                xlsx.write(row, 7, r.comp);
                xlsx.write(row, 8, i + 1);
                row++;
            }
        }

        // 保存Excel
        if (!xlsx.saveAs(filePath)) {
            QMessageBox::warning(this, "提示", "保存Excel文件失败，请确认路径与权限。");
            return;
        }
        QMessageBox::information(this, "成功", "已导出工序-大热重工艺内差异度详细表。");
    } catch (...) {
        QMessageBox::critical(this, "错误", "导出工艺内差异度详细表的过程中出现异常。");
    }
}
