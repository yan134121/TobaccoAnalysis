#include "TgBigDifferenceWorkbench.h"
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

class MainWindow; // 前向声明，不包含头文件

// 使用公共头文件中的委托类

TgBigDifferenceWorkbench::TgBigDifferenceWorkbench(int referenceSampleId,
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



void TgBigDifferenceWorkbench::setupUi()
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
            this, &TgBigDifferenceWorkbench::onResultTableRowClicked);
    connect(m_bestSampleRankingButton, &QPushButton::clicked,
            this, &TgBigDifferenceWorkbench::onShowBestSampleRankingTable);
}



// ===================================================================
// ==          calculateAndDisplay 的完整实现 (使用新参数)        ==
// ===================================================================
void TgBigDifferenceWorkbench::calculateAndDisplay()
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
            m_processedData, m_processingParams, StageName::Derivative);
    }

    for (auto it = m_processedData.constBegin(); it != m_processedData.constEnd(); ++it) {
        const SampleGroup& group = it.value();

        // 遍历组内每个样本
        for (const SampleDataFlexible& sample : group.sampleDatas) {
            // 仅加入代表样或参考样本
            if (!sample.bestInGroup && sample.sampleId != m_referenceSampleId) continue;
            // 获取 Derivative 阶段曲线
            QSharedPointer<Curve> curve = getCurveFromStage(sample, StageName::Derivative);
            if (!curve.isNull()) {
                allDerivativeCurves.append(curve);

                // 如果是参考样本
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
    
    setWindowTitle(tr("大热重差异度分析 - 参考样本ID: %1").arg(m_referenceSampleId));

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

        // 仅绘制代表样或参考样本
        if (!sample.bestInGroup && sample.sampleId != m_referenceSampleId) continue;
        QSharedPointer<Curve> curve = getCurveFromStage(sample, StageName::Derivative);
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



void TgBigDifferenceWorkbench::onResultTableRowClicked(const QModelIndex &index)
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




// TgBigDifferenceWorkbench.cpp
void TgBigDifferenceWorkbench::closeEvent(QCloseEvent* event)
{
    QMdiSubWindow::closeEvent(event);
    DEBUG_LOG << "TgBigDifferenceWorkbench::closeEvent() emitted";
    emit closed(this);  // 通知外部
}


// 计算综合评分（使用界面权重参数进行加权）
double TgBigDifferenceWorkbench::calculateComprehensiveScore(const QMap<QString, double>& scores)
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
void TgBigDifferenceWorkbench::exportToExcel(const QList<QMap<QString, QVariant>>& data, const QString& filePath)
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
void TgBigDifferenceWorkbench::onShowBestSampleRankingTable()
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

        // 遍历组内每个样本
        for (const SampleDataFlexible& sample : group.sampleDatas) {
            // 仅加入代表样或参考样本
            if (!sample.bestInGroup && sample.sampleId != m_referenceSampleId) continue;
            // 获取 Derivative 阶段曲线
            QSharedPointer<Curve> curve = getCurveFromStage(sample, StageName::Derivative);
            if (!curve.isNull()) {
                allCurves.append(curve);

                // 如果是参考样本
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
        bool isOptimal = (pearsonRaw >= TG_BIG_OPTIMAL_PEARSON_RAW_THRESHOLD
                          && pearsonNormalized >= TG_BIG_OPTIMAL_PEARSON_NORMALIZED_THRESHOLD);
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
    layout->addWidget(exportButton);

    connect(exportButton, &QPushButton::clicked, [this, resultData]() {
        QString filePath = QFileDialog::getSaveFileName(this, "保存Excel文件",
                                                      QDir::homePath() + "/大热重最佳样品差异排序表.xlsx",
                                                      "Excel文件 (*.xlsx);;CSV文件 (*.csv)");
        if (!filePath.isEmpty()) {
            exportToExcel(resultData, filePath);
        }
    });

    dialog.show();
    dialog.exec();
}

