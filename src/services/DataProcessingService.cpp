#include "core/common.h"  // 这里面有 ProcessingParameters 和 operator<<
#include "DataProcessingService.h"
#include "core/AppInitializer.h"  // 引入 AppInitializer 以便获取代表样选择服务
#include "services/analysis/ParallelSampleAnalysisService.h" // 修复：需要完整类型以调用其成员方法
#include "data_access/SampleDAO.h" // 假设有这个DAO
#include "services/algorithm/processing/IProcessingStep.h" // 包含接口
#include "Logger.h"
// 包含所有具体的算法策略...
#include "services/algorithm/Loess.h"
#include "services/algorithm/SavitzkyGolay.h"
#include "services/algorithm/BadPointRepair.h"
// ...
#include "services/algorithm/Clipping.h"
#include "services/algorithm/Normalization.h"

#include "services/algorithm/BaselineCorrector.h"
#include "services/algorithm/FindPeaks.h"       // 峰检测算法
#include "services/algorithm/COWAlignment.h"    // COW峰对齐算法
// 工序大热重数据访问与实体
#include "data_access/ProcessTgBigDataDAO.h"
#include "core/entities/ProcessTgBigData.h"


#include "data_access/SingleTobaccoSampleDAO.h"
#include "data_access/DatabaseConnector.h" // 获取主数据库连接以克隆线程专用连接
#include <QSqlDatabase> // 线程内数据库连接
// #include "core/common.h"

#include <QThread>
#include <QDebug>
#include <QString>
#include <QElapsedTimer>

// 构造函数中注册所有算法
DataProcessingService::DataProcessingService(QObject *parent) : QObject(parent) {
    // 记录并保存 AppInitializer 指针（父对象即为 AppInitializer）
    m_appInitializer = qobject_cast<AppInitializer*>(parent);
    registerSteps();
}
DataProcessingService::~DataProcessingService() {
    qDeleteAll(m_registeredSteps);
}

void DataProcessingService::registerSteps() {
    DEBUG_LOG << "Registering steps...";

    // 这里 new 出了所有可用的算法策略
    m_registeredSteps["clipping"] = new Clipping();
    m_registeredSteps["normalization"] = new Normalization();
    // m_registeredSteps["smoothing_sg"] = new SmoothingStep();
    // m_registeredSteps["normalization_max_min"] = new NormalizationStep();
    // ...
    // 注册两种不同的平滑算法
    m_registeredSteps["smoothing_loess"] = new Loess();
    m_registeredSteps["smoothing_sg"] = new SavitzkyGolay();

    // 注册用于求导的 SG 策略
    // 注意：虽然是同一个类，但我们用不同的ID注册它，代表不同的“意图”
    m_registeredSteps["derivative_sg"] = new SavitzkyGolay(); 

    // 注册基线校正算法
    m_registeredSteps["baseline_correction"] = new BaselineCorrector();

    // 注册坏点修复算法（用于工序大热重流程的“坏点修复”阶段）
    m_registeredSteps["bad_point_repair"] = new BadPointRepair();

    // 注册峰检测与COW对齐（色谱算法）
    m_registeredSteps["peak_detection"] = new FindPeaks();
    m_registeredSteps["cow_alignment"] = new COWAlignment();
}

// 为当前线程确保一个独立的数据库连接，避免跨线程使用同一连接导致崩溃
static QSqlDatabase ensureThreadLocalDatabase()
{
    QString connName = QStringLiteral("ta_thread_conn_%1").arg(reinterpret_cast<quintptr>(QThread::currentThreadId()));
    if (QSqlDatabase::contains(connName)) {
        QSqlDatabase db = QSqlDatabase::database(connName);
        if (!db.isOpen()) db.open();
        return db;
    }
    QSqlDatabase base = DatabaseConnector::getInstance().getDatabase();
    QSqlDatabase db = QSqlDatabase::cloneDatabase(base, connName);
    db.open();
    return db;
}

SampleDataFlexible DataProcessingService::runTgBigPipeline(int sampleId, const ProcessingParameters &params)
{
    DEBUG_LOG << "Pipeline running in thread:" << QThread::currentThread();
    // DEBUG_LOG << "Processing parameters:" << params << "sampleId" << sampleId;

    SampleDataFlexible sampleData;
    sampleData.sampleId = sampleId;
    sampleData.dataType = DataType::TG_BIG;
    QString error;
    SampleDAO dao;

    
    // 构造阶段数据
    StageData stage;
    
    
    //  = QSharedPointer<Curve>::create(x, y, "原始微分数据0");
    // stage.curve->setSampleId(sampleId);
    

    DEBUG_LOG << "Starting pipeline for sampleId:" << sampleId;

    QElapsedTimer timer;  //  先声明
    timer.restart();

    // --- 1. 获取原始数据 ---
    QVector<QPointF> rawPoints = dao.fetchChartDataForSample(sampleId, DataType::TG_BIG, error);
    if (rawPoints.isEmpty()) {
        WARNING_LOG << "Pipeline failed: No raw data for sample" << sampleId;
        return sampleData;
    }

    // 对齐 V2.2.1_origin 的固定裁剪策略（索引起点=60，最大长度=341）
    int fixedStartIndex = 60;            // MATLAB: fixedStartPoint=60
    int fixedMaxLength  = 341;           // MATLAB: MaxLength=341
    int n = rawPoints.size();
    int startIdx = qMin(qMax(0, fixedStartIndex), n);
    int maxLen   = qMin(fixedMaxLength, n - startIdx);
    if (maxLen < 0) maxLen = 0;
    QVector<QPointF> fixedSegment;
    fixedSegment.reserve(maxLen);
    for (int i = 0; i < maxLen; ++i) {
        fixedSegment.append(rawPoints[startIdx + i]);
    }

    QVector<double> x, y;
    for(const auto& p : fixedSegment) { x.append(p.x()); y.append(p.y()); }
    stage.stageName = StageName::RawData;

    DEBUG_LOG << "CURVE前：" ;
    stage.curve = QSharedPointer<Curve>::create(x, y, "原始数据");
    stage.curve->setSampleId(sampleId);

    DEBUG_LOG << "CURVE后:";

    stage.algorithm = AlgorithmType::None;
    stage.isSegmented = false;
    stage.numSegments = 1;

    sampleData.stages.append(stage);

    // DEBUG_LOG << "大热重单样本原始数据用时：" << timer.elapsed() << "ms";
    timer.restart();

    // --- 2. 流水线处理 ---
    // 【修正】接力棒现在是智能指针，保证所有权清晰
    QSharedPointer<Curve> currentCurve = stage.curve;

    // --- 阶段1.5: 坏点修复 (对齐 Copy_of_V2.2) ---
    // 在裁剪和归一化之前进行坏点修复，以避免异常值影响后续处理
    // 允许通过 `params.outlierRemovalEnabled` 开启/关闭坏点修复
    if (params.outlierRemovalEnabled && m_registeredSteps.contains("bad_point_repair")) {
        // DEBUG_LOG << "BadPointRepair step registered";
        IProcessingStep* step = m_registeredSteps.value("bad_point_repair");
        // 获取默认参数（Hampel filter, MAD, PCHIP插值等）
        QVariantMap repairParams = step->defaultParameters();
        
        // 根据界面/ProcessingParameters 覆盖默认参数，实现与 Copy_of_V2.2 一致的行为
        repairParams["window"]         = params.outlierWindow;       // Hampel局部异常窗口（奇数）
        repairParams["n_sigma"]        = params.outlierNSigma;       // Hampel阈值倍数
        repairParams["jump_threshold"] = params.jumpDiffThreshold;   // 跳变异常差分阈值
        repairParams["global_n_sigma"] = params.globalNSigma;        // 全局MAD阈值
        repairParams["mono_start"]     = params.monoStart;           // 单调性校正起点
        repairParams["mono_end"]       = params.monoEnd;             // 单调性校正终点
        repairParams["anchor_win"]     = params.anchorWindow;        // 拟合锚点窗口
        repairParams["fit_type"]       = params.fitType;             // 拟合类型：linear/quad
        repairParams["eps_scale"]      = params.epsScale;            // 平台严格递减斜率尺度
        repairParams["interp_method"]  = params.interpMethod;        // 插值方法：pchip/linear
        
        ProcessingResult res = step->process({currentCurve.data()}, repairParams, error);
        
        if (!res.namedCurves.isEmpty() && res.namedCurves.contains("repaired")) {
            // DEBUG_LOG << "BadPointRepair succeeded";
            
            stage.stageName = StageName::BadPointRepair;
            stage.curve = QSharedPointer<Curve>(res.namedCurves.value("repaired").first());
            stage.curve->setSampleId(sampleId);
            stage.algorithm = AlgorithmType::BadPointRepair;
            stage.isSegmented = false;
            stage.numSegments = 1;
            
            // 记录该阶段，但不一定需要在 UI 上单独展示，除非用户想看
            sampleData.stages.append(stage);
            
            // 【关键】更新接力棒
            currentCurve = stage.curve;
        } else {
            // 即使没有坏点被修复，通常也会返回一条曲线（可能与输入相同），
            // 如果返回为空则说明出错或无坏点（视实现而定）。
            // 若实现是“无坏点则不返回 repaired”，则保持 currentCurve 不变。
            // 假设 BadPointRepair 总是返回 repaired
        }
    }

    // --- 阶段2: 裁剪 ---
    // 大热重使用独立的裁剪参数组（clippingEnabled_TgBig / clipMinX_TgBig / clipMaxX_TgBig）
    if (params.clippingEnabled_TgBig) {
        // DEBUG_LOG << "Clipping enabled";
        if (m_registeredSteps.contains("clipping")) {
            // DEBUG_LOG << "Clipping step registered";
            IProcessingStep* step = m_registeredSteps.value("clipping");
            QVariantMap clipParams;
            clipParams["min_x"] = params.clipMinX_TgBig;
            clipParams["max_x"] = params.clipMaxX_TgBig;

            // DEBUG_LOG << "Clipping parameters:" << clipParams;

            // 【修正】使用 -> 操作符，并通过 .data() 获取裸指针传递
            ProcessingResult res = step->process({currentCurve.data()}, clipParams, error);

            // DEBUG_LOG << "Clipping result:" ;

            if (!res.namedCurves.isEmpty() && res.namedCurves.contains("clipped")) {
                // DEBUG_LOG << "Clipping succeeded";
                // 接管返回的裸指针，并更新接力棒
                // results.cleaned = QSharedPointer<Curve>(res.namedCurves.value("clipped").first());
                // results.cleaned->setSampleId(sampleId);

                stage.stageName = StageName::Clip;
                stage.curve = QSharedPointer<Curve>(res.namedCurves.value("clipped").first());
                stage.curve->setSampleId(sampleId);
                stage.algorithm = AlgorithmType::Clip;
                stage.isSegmented = false;
                stage.numSegments = 1;

                // currentCurve = results.cleaned;
                sampleData.stages.append(stage);

                // 【关键】更新接力棒：后续归一化/平滑/微分均应基于裁剪后的曲线
                // 中文说明：如果不在此更新，后续阶段会继续沿用原始曲线，导致 X 轴范围回退到原始数据范围。
                currentCurve = stage.curve;
            }
        }
    }

    // DEBUG_LOG << "大热重单样本裁剪数据用时：" << timer.elapsed() << "ms";
    timer.restart();

    // --- 阶段3: 归一化 ---
    if (params.normalizationEnabled) {
        if (m_registeredSteps.contains("normalization")) {
            IProcessingStep* step = m_registeredSteps.value("normalization");

            // 
            // - Normalization.cpp 已更新为 MATLAB 风格的 absmax 归一化，并 rangeMin/rangeMax 参数。
            // - 这里主动获取算法的默认参数，确保传入 rangeMin/rangeMax（默认为 [0,1]）。
            // - 若 UI/ProcessingParameters 未来提供自定义范围，可在此覆盖。
            QVariantMap normParams = step->defaultParameters(); // 包含 method=absmax, rangeMin=0.0, rangeMax=1.0
            // 兼容旧配置字段：仅当方法为支持的 "absmax" 时才覆盖，避免传入已移除的方法导致错误
            if (params.normalizationMethod == "absmax") {
                normParams["method"] = "absmax";
            }
            // 对齐 MATLAB，将归一化范围改为 [0,100]
            normParams["rangeMin"] = 0.0;
            normParams["rangeMax"] = 100.0;
            // TODO(后续可选): 若增加 ProcessingParameters.normalizationRangeMin/Max，则在此覆盖：
            // normParams["rangeMin"] = params.normalizationRangeMin;
            // normParams["rangeMax"] = params.normalizationRangeMax;

            // 【修正】使用 -> 操作符，并通过 .data() 获取裸指针传递
            ProcessingResult res = step->process({currentCurve.data()}, normParams, error);

            if (!res.namedCurves.isEmpty() && res.namedCurves.contains("normalized")) {
                // 接管返回的裸指针，并更新接力棒
                // 归一化阶段完成后，将接力棒 currentCurve 更新为归一化结果，
                // 以确保后续平滑、微分等阶段基于 [0,1] 范围的数据处理。
                stage.stageName = StageName::Normalize;
                stage.curve = QSharedPointer<Curve>(res.namedCurves.value("normalized").first());
                stage.curve->setSampleId(sampleId);
                stage.algorithm = AlgorithmType::Normalize;
                stage.isSegmented = false;
                stage.numSegments = 1;

                sampleData.stages.append(stage);

                // 【关键】更新接力棒用于后续阶段
                currentCurve = stage.curve;
            }
        }
    }

// DEBUG_LOG << "大热重单样本归一化数据用时：" << timer.elapsed() << "ms";
    timer.restart();

    // --- 阶段4: 平滑 ---
    if (params.smoothingEnabled) {
        // 根据参数选择平滑算法，默认使用 Loess
        QString sm = QStringLiteral("loess"); // 强制使用 Loess 以对齐 MATLAB
        QString methodId = (sm == "loess") ? QStringLiteral("smoothing_loess") : QStringLiteral("smoothing_sg");
        if (m_registeredSteps.contains(methodId)) {
            IProcessingStep* step = m_registeredSteps.value(methodId);
            QVariantMap smoothParams;
            // Loess 平滑使用参数 fraction（窗口比例），若未配置则使用算法默认参数
            smoothParams["fraction"] = params.loessSpan;
            ProcessingResult res = step->process({currentCurve.data()}, smoothParams, error);
            if (!res.namedCurves.isEmpty() && res.namedCurves.contains("smoothed")) {
                stage.stageName = StageName::Smooth;
                stage.curve = QSharedPointer<Curve>(res.namedCurves.value("smoothed").first());
                stage.curve->setSampleId(sampleId);
                stage.algorithm = (sm == "loess") ? AlgorithmType::Smooth_Loess : AlgorithmType::Smooth_SG;
                stage.isSegmented = false;
                stage.numSegments = 1;
                sampleData.stages.append(stage);
                currentCurve = stage.curve;
            } else {
                WARNING_LOG << "平滑阶段无结果：未返回 smoothed 曲线";
            }
        } else {
            WARNING_LOG << "未注册的平滑算法:" << methodId;
        }
    }

    // DEBUG_LOG << "大热重单样本平滑数据用时：" << timer.elapsed() << "ms";
    timer.restart();

    // --- 阶段5: 微分 ---
    if (params.derivativeEnabled) {
        const QString& derivativeMethodId = params.derivativeMethod; // e.g., "derivative_sg"

        if (m_registeredSteps.contains(derivativeMethodId)) {
            IProcessingStep* step = m_registeredSteps.value(derivativeMethodId);
            
            QVariantMap derivParams;
            if (derivativeMethodId == "derivative_sg") {
                derivParams["window_size"] = params.derivSgWindowSize;
                derivParams["poly_order"] = params.derivSgPolyOrder;
                // 【关键】明确告诉 SG 算法，这次是做一阶求导
                derivParams["derivative_order"] = 1; 
            }
            
            // 注意：微分的输入，通常是平滑后的数据 (currentCurve)
            ProcessingResult res = step->process({currentCurve.data()}, derivParams, error);

            if (res.namedCurves.contains("derivative1")) {
                //  results.derivative = QSharedPointer<Curve>(res.namedCurves.value("derivative1").first());
                //  results.derivative->setSampleId(sampleId);
                stage.stageName = StageName::Derivative;
                stage.curve = QSharedPointer<Curve>(res.namedCurves.value("derivative1").first());
                stage.curve->setSampleId(sampleId);
                stage.algorithm = AlgorithmType::Derivative_SG;
                stage.isSegmented = false;
                stage.numSegments = 1;

                sampleData.stages.append(stage);

            }
        }
    }

    // DEBUG_LOG << "大热重单样本微分数据用时：" << timer.elapsed() << "ms";
    timer.restart();

//     // ===== 调试输出 =====
// if (results.original) {
//     DEBUG_LOG << "Original pointCount:" << results.original->pointCount();
// }
// if (results.cleaned) {
//     DEBUG_LOG << "Cleaned pointCount:"  << results.cleaned->pointCount();
// }
// if (results.normalized) {
//     DEBUG_LOG << "Normalized pointCount:" << results.normalized->pointCount();
// }
// if (results.smoothed) {
//     DEBUG_LOG << "Smoothed pointCount:" << results.smoothed->pointCount();
// }
// if (results.derivative) {
//     DEBUG_LOG << "Derivative pointCount:" << results.derivative->pointCount();
// }

    DEBUG_LOG << "Pipeline completed:" ;
    return sampleData;
}


// ---------------- 批量样本流水线 ----------------
BatchGroupData DataProcessingService::runTgBigPipelineForMultiple(
    const QList<int> &sampleIds,
    const ProcessingParameters &params)
{
    BatchGroupData batchResults;
    SingleTobaccoSampleDAO dao;

    DEBUG_LOG << "Processing small TG samples:" << sampleIds;

    QElapsedTimer timer;  //先声明
    timer.restart();


    for (int sampleId : sampleIds) {
        // 单样本处理
        SampleDataFlexible singleSample = runTgBigPipeline(sampleId, params);
        SampleIdentifier identifier = dao.getSampleIdentifierById(sampleId);
        QString projectName = identifier.projectName;
        QString batchCode = identifier.batchCode;
        QString shortCode = identifier.shortCode;
        QString groupKey  = QString("%1-%2-%3").arg(projectName).arg(batchCode).arg(shortCode);

        // // 插入批量结果
        // batchResults.insert(name, singleSample);

        // 构造或获取已有的 SampleGroup
        SampleGroup &group = batchResults[groupKey];
        group.projectName = projectName;
        group.batchCode   = batchCode;
        group.shortCode   = shortCode;

        
        // // 添加单样本到平行样本列表
        // group.parallelSamples.append(singleSample);

        // // 用 SampleDataFlexible 构造 ParallelSampleData
        // ParallelSampleData parallelSample;
        // parallelSample.sampleData = singleSample;
        // parallelSample.parallelNo = identifier.parallelNo;  // 直接使用 identifier 中的 parallel_no

        // 添加到组内样本列表（不在此处打 bestInGroup 标记，由代表样选择服务统一处理）
        group.sampleDatas.append(singleSample);

    }

    DEBUG_LOG << "大热重批量样本处理用时：" << timer.elapsed() << "ms";

    // // 调试输出每个样本的数据点数
    // for (auto it = batchResults.constBegin(); it != batchResults.constEnd(); ++it) {
    //     int sampleId = it.key();
    //     const SampleDataFlexible &data = it.value();

    //     if (!data.stages.isEmpty() && data.stages.first().curve) {
    //         int pointCount = data.stages.first().curve->pointCount();
    //         DEBUG_LOG << QString("Sample %1: derivative exists, point count = %2")
    //                      .arg(sampleId).arg(pointCount);
    //     } else {
    //         DEBUG_LOG << QString("Sample %1: derivative is null!").arg(sampleId);
    //     }
    // }

    // 【关键】在完成批量组构建后，调用代表样选择服务，按 Derivative 阶段进行组内最优选择
    if (m_appInitializer && m_appInitializer->getParallelSampleAnalysisService()) {
        // 使用参数权重进行综合评分，选择每组 bestInGroup
        m_appInitializer->getParallelSampleAnalysisService()->selectRepresentativesInBatch(
            batchResults, params, StageName::Derivative
        );
    } else {
        WARNING_LOG << "DataProcessingService: 未能调用代表样选择服务 (AppInitializer 或服务为空)";
    }

    return batchResults;
}




// ---------------- 单样本流水线 ----------------
SampleDataFlexible DataProcessingService::runTgSmallPipeline(int sampleId, const ProcessingParameters &params)
{
    DEBUG_LOG << "Small pipeline running in thread:" << QThread::currentThread();

    SampleDataFlexible sampleData;
    sampleData.sampleId = sampleId;
    sampleData.dataType = DataType::TG_SMALL;
    QString error;
    SampleDAO dao;

    // --- 1. 获取原始数据 ---
    QVector<QPointF> rawPoints = dao.fetchSmallRawWeightData(sampleId, error);
    if (rawPoints.isEmpty()) {
        WARNING_LOG << "Pipeline failed: No raw data for sample" << sampleId;
        return sampleData;
    }

    QVector<double> x, y;
    for (const auto &p : rawPoints) {
        x.append(p.x());
        y.append(p.y());
    }

    // 构造阶段数据
    StageData stage;
    stage.stageName = StageName::RawData;
    stage.curve = QSharedPointer<Curve>::create(x, y, "原始数据");
    stage.curve->setSampleId(sampleId);
    stage.algorithm = AlgorithmType::None;
    stage.isSegmented = false;
    stage.numSegments = 1;

    sampleData.stages.append(stage);

    QSharedPointer<Curve> currentCurve = stage.curve;

    // --- 阶段1.5: 坏点修复 ---
    if (params.outlierRemovalEnabled && m_registeredSteps.contains("bad_point_repair")) {
        IProcessingStep* step = m_registeredSteps.value("bad_point_repair");
        QVariantMap repairParams = step->defaultParameters();

        repairParams["window"]         = params.outlierWindow;
        repairParams["n_sigma"]        = params.outlierNSigma;
        repairParams["jump_threshold"] = params.jumpDiffThreshold;
        repairParams["global_n_sigma"] = params.globalNSigma;
        repairParams["mono_start"]     = params.monoStart;
        repairParams["mono_end"]       = params.monoEnd;
        repairParams["anchor_win"]     = params.anchorWindow;
        repairParams["fit_type"]       = params.fitType;
        repairParams["eps_scale"]      = params.epsScale;
        repairParams["interp_method"]  = params.interpMethod;

        ProcessingResult res = step->process({currentCurve.data()}, repairParams, error);

        if (!res.namedCurves.isEmpty() && res.namedCurves.contains("repaired")) {
            stage.stageName = StageName::BadPointRepair;
            stage.curve = QSharedPointer<Curve>(res.namedCurves.value("repaired").first());
            stage.curve->setSampleId(sampleId);
            stage.algorithm = AlgorithmType::BadPointRepair;
            stage.isSegmented = false;
            stage.numSegments = 1;

            sampleData.stages.append(stage);
            currentCurve = stage.curve;
        }
    }

    // --- 阶段2: 裁剪 ---
    if (params.clippingEnabled) {
        if (m_registeredSteps.contains("clipping")) {
            IProcessingStep* step = m_registeredSteps.value("clipping");
            QVariantMap clipParams;
            clipParams["min_x"] = params.clipMinX;
            clipParams["max_x"] = params.clipMaxX;

            ProcessingResult res = step->process({currentCurve.data()}, clipParams, error);

            if (!res.namedCurves.isEmpty() && res.namedCurves.contains("clipped")) {
                stage.stageName = StageName::Clip;
                stage.curve = QSharedPointer<Curve>(res.namedCurves.value("clipped").first());
                stage.curve->setSampleId(sampleId);
                stage.algorithm = AlgorithmType::Clip;
                stage.isSegmented = false;
                stage.numSegments = 1;

                sampleData.stages.append(stage);
                currentCurve = stage.curve;
            }
        }
    }

    // --- 阶段3: 归一化 ---
    if (params.normalizationEnabled) {
        if (m_registeredSteps.contains("normalization")) {
            IProcessingStep* step = m_registeredSteps.value("normalization");

            QVariantMap normParams = step->defaultParameters();
            if (params.normalizationMethod == "absmax") {
                normParams["method"] = "absmax";
            }
            normParams["rangeMin"] = 0.0;
            normParams["rangeMax"] = 100.0;

            ProcessingResult res = step->process({currentCurve.data()}, normParams, error);

            if (!res.namedCurves.isEmpty() && res.namedCurves.contains("normalized")) {
                stage.stageName = StageName::Normalize;
                stage.curve = QSharedPointer<Curve>(res.namedCurves.value("normalized").first());
                stage.curve->setSampleId(sampleId);
                stage.algorithm = AlgorithmType::Normalize;
                stage.isSegmented = false;
                stage.numSegments = 1;

                sampleData.stages.append(stage);
                currentCurve = stage.curve;
            }
        }
    }

    // --- 阶段4: 平滑 ---
    if (params.smoothingEnabled) {
        QString sm = QStringLiteral("loess");
        QString methodId = (sm == "loess") ? QStringLiteral("smoothing_loess") : QStringLiteral("smoothing_sg");
        if (m_registeredSteps.contains(methodId)) {
            IProcessingStep* step = m_registeredSteps.value(methodId);
            QVariantMap smoothParams;
            smoothParams["fraction"] = params.loessSpan;
            ProcessingResult res = step->process({currentCurve.data()}, smoothParams, error);
            if (!res.namedCurves.isEmpty() && res.namedCurves.contains("smoothed")) {
                stage.stageName = StageName::Smooth;
                stage.curve = QSharedPointer<Curve>(res.namedCurves.value("smoothed").first());
                stage.curve->setSampleId(sampleId);
                stage.algorithm = (sm == "loess") ? AlgorithmType::Smooth_Loess : AlgorithmType::Smooth_SG;
                stage.isSegmented = false;
                stage.numSegments = 1;
                sampleData.stages.append(stage);
                currentCurve = stage.curve;
            } else {
                WARNING_LOG << "平滑阶段无结果：未返回 smoothed 曲线";
            }
        } else {
            WARNING_LOG << "未注册的平滑算法:" << methodId;
        }
    }

    // --- 阶段5: 微分 ---
    if (params.derivativeEnabled) {
        const QString& derivativeMethodId = params.derivativeMethod;

        if (m_registeredSteps.contains(derivativeMethodId)) {
            IProcessingStep* step = m_registeredSteps.value(derivativeMethodId);

            QVariantMap derivParams;
            if (derivativeMethodId == "derivative_sg") {
                derivParams["window_size"] = params.derivSgWindowSize;
                derivParams["poly_order"] = params.derivSgPolyOrder;
                derivParams["derivative_order"] = 1;
            }

            ProcessingResult res = step->process({currentCurve.data()}, derivParams, error);

            if (res.namedCurves.contains("derivative1")) {
                stage.stageName = StageName::Derivative;
                stage.curve = QSharedPointer<Curve>(res.namedCurves.value("derivative1").first());
                stage.curve->setSampleId(sampleId);
                stage.algorithm = AlgorithmType::Derivative_SG;
                stage.isSegmented = false;
                stage.numSegments = 1;

                sampleData.stages.append(stage);
            }
        }
    }

    return sampleData;
}

// ---------------- 批量样本流水线 ----------------
BatchGroupData DataProcessingService::runTgSmallPipelineForMultiple(
    const QList<int> &sampleIds,
    const ProcessingParameters &params)
{
    BatchGroupData batchResults;
    SingleTobaccoSampleDAO dao;

    DEBUG_LOG << "Processing small TG samples:" << sampleIds;

    for (int sampleId : sampleIds) {
        // 单样本处理
        SampleDataFlexible singleSample = runTgSmallPipeline(sampleId, params);
        SampleIdentifier identifier = dao.getSampleIdentifierById(sampleId);
        QString projectName = identifier.projectName;
        QString batchCode = identifier.batchCode;
        QString shortCode = identifier.shortCode;
        QString groupKey  = QString("%1-%2-%3").arg(projectName).arg(batchCode).arg(shortCode);

        // // 插入批量结果
        // batchResults.insert(name, singleSample);

        // 构造或获取已有的 SampleGroup
        SampleGroup &group = batchResults[groupKey];
        group.projectName = projectName;
        group.batchCode   = batchCode;
        group.shortCode   = shortCode;

        
        // // 添加单样本到平行样本列表
        // group.parallelSamples.append(singleSample);

        // // 用 SampleDataFlexible 构造 ParallelSampleData
        // ParallelSampleData parallelSample;
        // parallelSample.sampleData = singleSample;
        // parallelSample.parallelNo = identifier.parallelNo;  // 直接使用 identifier 中的 parallel_no

        // 添加到平行样本列表
        group.sampleDatas.append(singleSample);

    }

    // 【关键】在完成批量组构建后，调用代表样选择服务，按 Derivative 阶段进行组内最优选择
    if (m_appInitializer && m_appInitializer->getParallelSampleAnalysisService()) {
        m_appInitializer->getParallelSampleAnalysisService()->selectRepresentativesInBatch(
            batchResults, params, StageName::Derivative
        );
    } else {
        WARNING_LOG << "DataProcessingService: 未能调用代表样选择服务 (AppInitializer 或服务为空)";
    }

    return batchResults;
}


SampleDataFlexible DataProcessingService::runChromatographPipeline(int sampleId, const ProcessingParameters& params)
{
    DEBUG_LOG << "Chromatograph pipeline running in thread:" << QThread::currentThread();
    SampleDataFlexible sampleData;
    sampleData.sampleId = sampleId;
    sampleData.dataType = DataType::TG_SMALL;
    QString error;
    SampleDAO dao;

    
    // 构造阶段数据
    StageData stage;
    
    //  = QSharedPointer<Curve>::create(x, y, "原始微分数据0");
    // stage.curve->setSampleId(sampleId);
    

    DEBUG_LOG << "Starting pipeline for sampleId:" << sampleId;

    QElapsedTimer timer;  //  先声明
    timer.restart();

    // --- 1. 获取原始数据 ---
    QVector<QPointF> rawPoints = dao.fetchChartDataForSample(sampleId, DataType::CHROMATOGRAM, error);
    if (rawPoints.isEmpty()) {
        WARNING_LOG << "Pipeline failed: No raw data for sample" << sampleId;
        return sampleData;
    }

    QVector<double> x, y;
    for(const auto& p : rawPoints) { x.append(p.x()); y.append(p.y()); }
    stage.stageName = StageName::RawData;
    stage.curve = QSharedPointer<Curve>::create(x, y, "原始数据");
    stage.curve->setSampleId(sampleId);
    stage.algorithm = AlgorithmType::None;
    stage.isSegmented = false;
    stage.numSegments = 1;

    sampleData.stages.append(stage);

    // DEBUG_LOG << "大热重单样本原始数据用时：" << timer.elapsed() << "ms";
    timer.restart();

    // --- 2. 流水线处理 ---
    // 【修正】接力棒现在是智能指针，保证所有权清晰
    QSharedPointer<Curve> currentCurve = stage.curve;

    

    DEBUG_LOG << "色谱单样本0000";
    // --- 阶段2: 基线校正 ---
    if (params.baselineEnabled) {
        if (m_registeredSteps.contains("baseline_correction")) {
            IProcessingStep* step = m_registeredSteps.value("baseline_correction");

            // === 构造基线校正参数 ===
            QVariantMap baselineParams;
            baselineParams["lambda"] = params.lambda;
            baselineParams["p"] = params.p;
            baselineParams["order"] = params.order;
            baselineParams["itermax"] = params.itermax;
            baselineParams["wep"] = params.wep;

            // === 调用算法 ===
            ProcessingResult res = step->process({currentCurve.data()}, baselineParams, error);

            if (!res.namedCurves.isEmpty() && res.namedCurves.contains("baseline_corrected")) {
                // 取出算法返回的基线校正结果
                stage.stageName = StageName::BaselineCorrection;
                stage.curve = QSharedPointer<Curve>(res.namedCurves.value("baseline_corrected").first());
                stage.curve->setSampleId(sampleId);
                stage.algorithm = AlgorithmType::BaselineCorrection;
                stage.isSegmented = false;
                stage.numSegments = 1;

                // 加入阶段结果
                sampleData.stages.append(stage);
                // 更新接力棒为基线校正后的曲线，便于后续峰检测等阶段
                currentCurve = stage.curve;
            }
        }
    }


    // DEBUG_LOG << "大热重单样本裁剪数据用时：" << timer.elapsed() << "ms";
    timer.restart();


    // --- 阶段3: 峰检测（） ---
    if (m_registeredSteps.contains("peak_detection") && params.peakDetectionEnabled) {
        IProcessingStep* step = m_registeredSteps.value("peak_detection");
        QVariantMap peakParams;
        peakParams["min_height"] = params.peakMinHeight;
        peakParams["min_prominence"] = params.peakMinProminence;
        peakParams["min_distance"] = params.peakMinDistance;
        peakParams["snr_threshold"] = params.peakSnrThreshold;
        peakParams["window"] = 20; // 默认窗口，用于局部显著性与噪声估计

        ProcessingResult res = step->process({currentCurve.data()}, peakParams, error);
        if (res.namedCurves.contains("peaks") && !res.namedCurves["peaks"].isEmpty()) {
            stage.stageName = StageName::PeakDetection;
            stage.curve = QSharedPointer<Curve>(res.namedCurves.value("peaks").first());
            stage.curve->setSampleId(sampleId);
            stage.algorithm = AlgorithmType::PeakDetection;
            stage.isSegmented = false;
            stage.numSegments = 1;
            sampleData.stages.append(stage);
        }
    }


//     DEBUG_LOG << "色谱单样本0000";



timer.restart();




    DEBUG_LOG << "Pipeline completed:" ;
    return sampleData;
}



// ---------------- 批量样本流水线 ----------------
BatchGroupData DataProcessingService::runChromatographPipelineForMultiple(
    const QList<int> &sampleIds,
    const ProcessingParameters &params)
{
    BatchGroupData batchResults;
    SingleTobaccoSampleDAO dao;

    DEBUG_LOG << "Processing chromatograph samples:" << sampleIds;

    for (int sampleId : sampleIds) {
        // 单样本处理
        SampleDataFlexible singleSample = runChromatographPipeline(sampleId, params);
        SampleIdentifier identifier = dao.getSampleIdentifierById(sampleId);
        QString projectName = identifier.projectName;
        QString batchCode = identifier.batchCode;
        QString shortCode = identifier.shortCode;
        QString groupKey  = QString("%1-%2-%3").arg(projectName).arg(batchCode).arg(shortCode);

        // // 插入批量结果
        // batchResults.insert(name, singleSample);

        // 构造或获取已有的 SampleGroup
        SampleGroup &group = batchResults[groupKey];
        group.projectName = projectName;
        group.batchCode   = batchCode;
        group.shortCode   = shortCode;

        
        // // 添加单样本到平行样本列表
        // group.parallelSamples.append(singleSample);

        // // 用 SampleDataFlexible 构造 ParallelSampleData
        // ParallelSampleData parallelSample;
        // parallelSample.sampleData = singleSample;
        // parallelSample.parallelNo = identifier.parallelNo;  // 直接使用 identifier 中的 parallel_no

        // 添加到组内样本列表（不在此处打 bestInGroup 标记，由代表样选择服务统一处理）
        group.sampleDatas.append(singleSample);

    }

    // 若启用峰对齐，且指定了参考样本ID，则对组内样本执行COW对齐
    if (params.alignmentEnabled && params.referenceSampleId > 0 && m_registeredSteps.contains("cow_alignment")) {
        IProcessingStep* alignStep = m_registeredSteps.value("cow_alignment");
        QString error;
        // 查找参考样本的曲线（优先使用基线校正后的曲线，没有则用原始数据）
        QSharedPointer<Curve> refCurve;
        for (auto it = batchResults.begin(); it != batchResults.end(); ++it) {
            SampleGroup& group = it.value();
            for (const SampleDataFlexible& sample : group.sampleDatas) {
                if (sample.sampleId == params.referenceSampleId) {
                    // 查找参考曲线阶段
                    for (const StageData& st : sample.stages) {
                        if (st.stageName == StageName::BaselineCorrection && st.curve) {
                            refCurve = st.curve;
                            break;
                        }
                    }
                    if (refCurve.isNull()) {
                        for (const StageData& st : sample.stages) {
                            if (st.stageName == StageName::RawData && st.curve) {
                                refCurve = st.curve;
                                break;
                            }
                        }
                    }
                }
                if (!refCurve.isNull()) break;
            }
            if (!refCurve.isNull()) break;
        }

        // 若找到参考曲线，则对其他样本执行对齐并追加阶段数据
        if (!refCurve.isNull()) {
            for (auto it = batchResults.begin(); it != batchResults.end(); ++it) {
                SampleGroup& group = it.value();
                for (SampleDataFlexible& sample : group.sampleDatas) {
                    // 跳过参考样本自身
                    if (sample.sampleId == params.referenceSampleId) continue;
                    // 目标曲线同样优先使用基线校正后的曲线
                    QSharedPointer<Curve> tgtCurve;
                    for (const StageData& st : sample.stages) {
                        if (st.stageName == StageName::BaselineCorrection && st.curve) {
                            tgtCurve = st.curve;
                            break;
                        }
                    }
                    if (tgtCurve.isNull()) {
                        for (const StageData& st : sample.stages) {
                            if (st.stageName == StageName::RawData && st.curve) {
                                tgtCurve = st.curve;
                                break;
                            }
                        }
                    }
                    if (tgtCurve.isNull()) continue;

                    // 执行对齐
                    QVariantMap alignParams;
                    alignParams["window_size"] = params.cowWindowSize;
                    alignParams["max_warp"] = params.cowMaxWarp;
                    alignParams["segment_count"] = params.cowSegmentCount;
                    alignParams["resample_step"] = params.cowResampleStep;

                    ProcessingResult ar = alignStep->process({refCurve.data(), tgtCurve.data()}, alignParams, error);
                    if (ar.namedCurves.contains("aligned") && !ar.namedCurves["aligned"].isEmpty()) {
                        StageData stage;
                        stage.stageName = StageName::PeakAlignment;
                        stage.curve = QSharedPointer<Curve>(ar.namedCurves["aligned"].first());
                        stage.curve->setSampleId(sample.sampleId);
                        stage.algorithm = AlgorithmType::PeakAlignment;
                        stage.isSegmented = false;
                        stage.numSegments = 1;
                        sample.stages.append(stage);
                    }
                }
            }
        }
    }

    return batchResults;
}




SampleDataFlexible DataProcessingService::runProcessTgBigPipeline(int sampleId, const ProcessingParameters& params)
{
    DEBUG_LOG << "ProcessTgBig pipeline running in thread:" << QThread::currentThread();
    SampleDataFlexible sampleData;
    sampleData.sampleId = sampleId;
    sampleData.dataType = DataType::PROCESS_TG_BIG; // 数据类型设置为工序大热重
    QString error;
    SampleDAO dao;
    // 工序数据无需 SampleDAO 曲线查询，转用 ProcessTgBigDataDAO

    // DEBUG_LOG << "Starting pipeline for sampleId:" << sampleId;
    
    // 构造阶段数据
    StageData stage;

    QVector<QPointF> rawPoints = dao.fetchChartDataForSample(sampleId, sampleData.dataType, error);
        if (rawPoints.isEmpty()) {
            WARNING_LOG << "Pipeline failed: No raw data for sample" << sampleId;
            return sampleData;
        }

        // DEBUG_LOG << "Sample" << sampleId << "original points:" << rawPoints.size();

    QVector<double> x, y;
    for(const auto& p : rawPoints) { x.append(p.x()); y.append(p.y()); }
    stage.stageName = StageName::RawData;

    stage.curve = QSharedPointer<Curve>::create(x, y, "原始数据");
    stage.curve->setSampleId(sampleId);
    stage.algorithm = AlgorithmType::None;
    stage.isSegmented = false;
    stage.numSegments = 1;
    // 为“原始数据”阶段开启绘图显示（与界面阶段选择保持一致）
    // 之前未设置 useForPlot 导致在选择“原始数据”阶段时不绘制，造成视图清空
    stage.useForPlot = true;

    sampleData.stages.append(stage);

    // DEBUG_LOG << "Starting pipeline for sampleId:" << sampleId;

    // 接力棒——后续阶段的输入
    QSharedPointer<Curve> currentCurve = stage.curve;

    // 阶段2 坏点修复（替换原“拟合数据”阶段）
    if (m_registeredSteps.contains("bad_point_repair")) {

        // DEBUG_LOG << "工序大热重单样本坏点修复";

        IProcessingStep* step = m_registeredSteps.value("bad_point_repair");
        // DEBUG_LOG << "工序大热重单样本坏点修复";

        // 坏点修复参数映射（来自 UI 设置），覆盖算法默认值
        QVariantMap bpParams = step->defaultParameters();
        bpParams["window"] = params.outlierWindow;
        bpParams["n_sigma"] = params.outlierNSigma;
        bpParams["jump_threshold"] = params.jumpDiffThreshold;
        bpParams["global_n_sigma"] = params.globalNSigma;
        bpParams["mono_start"] = params.monoStart;
        bpParams["mono_end"] = params.monoEnd;
        bpParams["anchor_win"] = params.anchorWindow;
        bpParams["fit_type"] = params.fitType;
        bpParams["eps_scale"] = params.epsScale;
        bpParams["interp_method"] = params.interpMethod;

        // DEBUG_LOG << "工序大热重单样本坏点修复";
        
        QString error;
        ProcessingResult res = step->process({currentCurve.data()}, bpParams, error);

        // DEBUG_LOG << "工序大热重单样本坏点修复";

        if (!res.namedCurves.isEmpty() && res.namedCurves.contains("repaired")) {
            stage.stageName = StageName::BadPointRepair; // 坏点修复阶段
            stage.curve = QSharedPointer<Curve>(res.namedCurves.value("repaired").first());
            stage.curve->setSampleId(sampleId);
            stage.algorithm = AlgorithmType::BadPointRepair;
            stage.isSegmented = false;
            stage.numSegments = 1;
            stage.useForPlot = true; // 允许在绘图区域显示该阶段
            // 将坏点坐标写入 metrics 供前端绘制
            if (res.namedCurves.contains("bad_points") && !res.namedCurves["bad_points"].isEmpty()) {
                Curve* badCurve = res.namedCurves["bad_points"].first();
                QVector<QPointF> pts = badCurve->data();
                QVector<double> bx, by;
                bx.reserve(pts.size());
                by.reserve(pts.size());
                for (const auto& p : pts) { bx.append(p.x()); by.append(p.y()); }
                stage.metrics.insert("bad_points_x", QVariant::fromValue(bx));
                stage.metrics.insert("bad_points_y", QVariant::fromValue(by));
            }
            sampleData.stages.append(stage);
            currentCurve = stage.curve; // 更新接力棒，后续裁剪/归一化/平滑/微分基于修复后数据
        }
    }

    DEBUG_LOG << "工序大热重单样本坏点修复";

    // 阶段3 裁剪
    // 工序大热重使用独立的裁剪参数组（clippingEnabled_ProcessTgBig / clipMinX_ProcessTgBig / clipMaxX_ProcessTgBig）
    if (params.clippingEnabled_ProcessTgBig && m_registeredSteps.contains("clipping")) {
        IProcessingStep* step = m_registeredSteps.value("clipping");
        QVariantMap clipParams; clipParams["min_x"] = params.clipMinX_ProcessTgBig; clipParams["max_x"] = params.clipMaxX_ProcessTgBig;
        QString error;
        ProcessingResult res = step->process({currentCurve.data()}, clipParams, error);
        if (!res.namedCurves.isEmpty() && res.namedCurves.contains("clipped")) {
            stage.stageName = StageName::Clip;
            stage.curve = QSharedPointer<Curve>(res.namedCurves.value("clipped").first());
            stage.curve->setSampleId(sampleId);
            stage.algorithm = AlgorithmType::Clip;
            stage.isSegmented = false;
            stage.numSegments = 1;
            stage.useForPlot = true; // 允许在绘图区域显示该阶段
            sampleData.stages.append(stage);
            currentCurve = stage.curve;
        }
    }

    
    // 阶段4 归一化（参考 V2.2.1_origin：绝对最大值归一化到 [0,100]）
    if (params.normalizationEnabled && m_registeredSteps.contains("normalization")) {
        IProcessingStep* step = m_registeredSteps.value("normalization");
        QVariantMap normParams = step->defaultParameters();
        // 工序大热重使用 absmax 且输出范围为 [0,100]；不影响“大热重”其他管线
        if (params.normalizationMethod == "absmax") {
            normParams["method"] = "absmax";
            normParams["rangeMin"] = 0.0;
            normParams["rangeMax"] = 100.0;
        }
        QString error;
        ProcessingResult res = step->process({currentCurve.data()}, normParams, error);
        if (!res.namedCurves.isEmpty() && res.namedCurves.contains("normalized")) {
            stage.stageName = StageName::Normalize;
            stage.curve = QSharedPointer<Curve>(res.namedCurves.value("normalized").first());
            stage.curve->setSampleId(sampleId);
            stage.algorithm = AlgorithmType::Normalize;
            stage.isSegmented = false;
            stage.numSegments = 1;
            stage.useForPlot = true; // 允许在绘图区域显示该阶段
            sampleData.stages.append(stage);
            currentCurve = stage.curve;
        }
    }

    // 阶段5 平滑（支持 SG 或 Loess，SG窗口强制奇数以贴合 V2.2.1_origin）
    if (params.smoothingEnabled) {
        QString smMethod = params.smoothingMethod;
        if (smMethod == "savitzky_golay" && m_registeredSteps.contains("smoothing_sg")) {
            IProcessingStep* step = m_registeredSteps.value("smoothing_sg");
            QVariantMap smoothParams;
            int sgWin = params.sgWindowSize;
            // 强制窗口为奇数，且不超过点数范围（至少为3）
            if (sgWin % 2 == 0) sgWin += 1;
            if (currentCurve && sgWin >= currentCurve->pointCount()) {
                sgWin = qMax(3, currentCurve->pointCount() - 1);
                if (sgWin % 2 == 0) sgWin -= 1;
            }
            smoothParams["window_size"] = sgWin;
            smoothParams["poly_order"] = params.sgPolyOrder;
            smoothParams["derivative_order"] = 0;
        // 防御性检查，避免窗口大小超过点数导致算法越界崩溃
            if (!currentCurve || currentCurve->pointCount() <= 0) {
            WARNING_LOG << "平滑阶段跳过：当前曲线为空或点数为0";
            } else if (smoothParams["window_size"].toInt() <= 2 || smoothParams["window_size"].toInt() > currentCurve->pointCount()) {
            WARNING_LOG << "平滑阶段跳过：window_size 无效或超过点数";
            } else {
            QString error;
            try {
                ProcessingResult res = step->process({currentCurve.data()}, smoothParams, error);
                if (!res.namedCurves.isEmpty() && res.namedCurves.contains("smoothed")) {
                    stage.stageName = StageName::Smooth;
                    stage.curve = QSharedPointer<Curve>(res.namedCurves.value("smoothed").first());
                    stage.curve->setSampleId(sampleId);
                    stage.algorithm = AlgorithmType::Smooth_SG;
                    stage.isSegmented = false;
                    stage.numSegments = 1;
                    stage.useForPlot = true; // 允许在绘图区域显示该阶段
                    sampleData.stages.append(stage);
                    currentCurve = stage.curve;
                } else {
                    WARNING_LOG << "平滑阶段无结果：未返回 smoothed 曲线";
                }
            } catch (const std::exception& e) {
                ERROR_LOG << QString("平滑阶段异常：%1").arg(e.what());
            } catch (...) {
                ERROR_LOG << "平滑阶段未知异常";
            }
            }
        } else if (smMethod == "loess" && m_registeredSteps.contains("smoothing_loess")) {
            IProcessingStep* step = m_registeredSteps.value("smoothing_loess");
            QVariantMap smoothParams; smoothParams["fraction"] = params.loessSpan;
            if (!currentCurve || currentCurve->pointCount() <= 2) {
                WARNING_LOG << "平滑阶段跳过：当前曲线为空或点数不足";
            } else {
                QString error;
                try {
                    ProcessingResult res = step->process({currentCurve.data()}, smoothParams, error);
                    if (!res.namedCurves.isEmpty() && res.namedCurves.contains("smoothed")) {
                        stage.stageName = StageName::Smooth;
                        stage.curve = QSharedPointer<Curve>(res.namedCurves.value("smoothed").first());
                        stage.curve->setSampleId(sampleId);
                        stage.algorithm = AlgorithmType::Smooth_Loess; // Loess 平滑标记
                        stage.isSegmented = false;
                        stage.numSegments = 1;
                        stage.useForPlot = true;
                        sampleData.stages.append(stage);
                        currentCurve = stage.curve;
                    } else {
                        WARNING_LOG << "Loess 平滑阶段无结果：未返回 smoothed 曲线";
                    }
                } catch (const std::exception& e) {
                    ERROR_LOG << QString("Loess 平滑阶段异常：%1").arg(e.what());
                } catch (...) {
                    ERROR_LOG << "Loess 平滑阶段未知异常";
                }
            }
        }
    }

    // 阶段6 DTG导数（根据基础方法：SG 或 First Difference；SG窗口强制奇数）
    if (params.derivativeEnabled) {
        QString dMethod = params.derivativeMethod;
        if (dMethod == "derivative_sg" && m_registeredSteps.contains("derivative_sg")) {
        IProcessingStep* step = m_registeredSteps.value("derivative_sg");
        QVariantMap derivParams;
        int dWin = params.derivSgWindowSize;
        // 强制窗口为奇数，且不超过点数范围（至少为3）
        if (dWin % 2 == 0) dWin += 1;
        if (currentCurve && dWin >= currentCurve->pointCount()) {
            dWin = qMax(3, currentCurve->pointCount() - 1);
            if (dWin % 2 == 0) dWin -= 1;
        }
        derivParams["window_size"] = dWin;
        derivParams["poly_order"] = params.derivSgPolyOrder;
        derivParams["derivative_order"] = 1;
        // 防御性检查，避免窗口大小超过点数导致算法越界崩溃
        if (!currentCurve || currentCurve->pointCount() <= 1) {
            WARNING_LOG << "DTG阶段跳过：当前曲线为空或点数不足";
        } else if (derivParams["window_size"].toInt() <= 2 || derivParams["window_size"].toInt() > currentCurve->pointCount()) {
            WARNING_LOG << "DTG阶段跳过：window_size 无效或超过点数";
        } else {
            QString error;
            try {
                ProcessingResult res = step->process({currentCurve.data()}, derivParams, error);
                if (res.namedCurves.contains("derivative1")) {
                    stage.stageName = StageName::Derivative;
                    stage.curve = QSharedPointer<Curve>(res.namedCurves.value("derivative1").first());
                    stage.curve->setSampleId(sampleId);
                    stage.algorithm = AlgorithmType::Derivative_SG;
                    stage.isSegmented = false;
                    stage.numSegments = 1;
                    stage.useForPlot = true; // 允许在绘图区域显示该阶段
                    sampleData.stages.append(stage);
                    // 更新接力棒为DTG曲线，供下一阶段再次做一阶导数
                    currentCurve = stage.curve;
                } else {
                    WARNING_LOG << "DTG阶段无结果：未返回 derivative1 曲线";
                }
            } catch (const std::exception& e) {
                ERROR_LOG << QString("DTG阶段异常：%1").arg(e.what());
            } catch (...) {
                ERROR_LOG << "DTG阶段未知异常";
            }
        }
        } else if (dMethod == "first_diff") {
            if (!currentCurve || currentCurve->pointCount() <= 1) {
                WARNING_LOG << "DTG阶段跳过：当前曲线为空或点数不足";
            } else {
                // 简易一阶差分实现（基于x间隔的前向差分）
                QVector<QPointF> src = currentCurve->data();
                QVector<QPointF> diff;
                diff.reserve(src.size());
                for (int i = 0; i < src.size(); ++i) {
                    if (i == 0) { diff.append(QPointF(src[i].x(), 0.0)); continue; }
                    double dx = src[i].x() - src[i-1].x(); if (qAbs(dx) < 1e-12) dx = 1.0;
                    double dy = src[i].y() - src[i-1].y();
                    diff.append(QPointF(src[i].x(), dy / dx));
                }
                Curve* dcurve = new Curve(diff, currentCurve->name() + QObject::tr(" (一阶差分)"));
                stage.stageName = StageName::Derivative;
                stage.curve = QSharedPointer<Curve>(dcurve);
                stage.curve->setSampleId(sampleId);
                stage.algorithm = AlgorithmType::Derivative_SG; // 占位
                stage.isSegmented = false;
                stage.numSegments = 1;
                stage.useForPlot = true;
                sampleData.stages.append(stage);
                currentCurve = stage.curve;
            }
        }
    }

    // 阶段7 一阶导数（对DTG再次做一阶微分，相当于TG二阶导；SG窗口强制奇数）
    if (params.derivativeEnabled) {
        QString dMethod2 = params.derivative2Method;
        if (dMethod2 == "derivative_sg" && m_registeredSteps.contains("derivative_sg")) {
            IProcessingStep* step = m_registeredSteps.value("derivative_sg");
            QVariantMap derivParams;
            int d2Win = params.deriv2SgWindowSize;
            // 强制窗口为奇数，且不超过点数范围（至少为3）
            if (d2Win % 2 == 0) d2Win += 1;
            if (currentCurve && d2Win >= currentCurve->pointCount()) {
                d2Win = qMax(3, currentCurve->pointCount() - 1);
                if (d2Win % 2 == 0) d2Win -= 1;
            }
            derivParams["window_size"] = d2Win;
            derivParams["poly_order"] = params.deriv2SgPolyOrder;
            derivParams["derivative_order"] = 1;
        // 防御性检查
            if (!currentCurve || currentCurve->pointCount() <= 1) {
            WARNING_LOG << "一阶导阶段跳过：当前曲线为空或点数不足";
            } else if (derivParams["window_size"].toInt() <= 2 || derivParams["window_size"].toInt() > currentCurve->pointCount()) {
            WARNING_LOG << "一阶导阶段跳过：window_size 无效或超过点数";
            } else {
            QString error;
            try {
                ProcessingResult res = step->process({currentCurve.data()}, derivParams, error);
                if (res.namedCurves.contains("derivative1")) {
                    stage.stageName = StageName::Derivative;
                    stage.curve = QSharedPointer<Curve>(res.namedCurves.value("derivative1").first());
                    stage.curve->setSampleId(sampleId);
                    stage.algorithm = AlgorithmType::Derivative_SG;
                    stage.isSegmented = false;
                    stage.numSegments = 1;
                    stage.useForPlot = true; // 允许在绘图区域显示该阶段
                    sampleData.stages.append(stage);
                } else {
                    WARNING_LOG << "一阶导阶段无结果：未返回 derivative1 曲线";
                }
            } catch (const std::exception& e) {
                ERROR_LOG << QString("一阶导阶段异常：%1").arg(e.what());
            } catch (...) {
                ERROR_LOG << "一阶导阶段未知异常";
            }
            }
        } else if (dMethod2 == "first_diff") {
            if (!currentCurve || currentCurve->pointCount() <= 1) {
                WARNING_LOG << "一阶导阶段跳过：当前曲线为空或点数不足";
            } else {
                QVector<QPointF> src = currentCurve->data();
                QVector<QPointF> diff;
                diff.reserve(src.size());
                for (int i = 0; i < src.size(); ++i) {
                    if (i == 0) { diff.append(QPointF(src[i].x(), 0.0)); continue; }
                    double dx = src[i].x() - src[i-1].x(); if (qAbs(dx) < 1e-12) dx = 1.0;
                    double dy = src[i].y() - src[i-1].y();
                    diff.append(QPointF(src[i].x(), dy / dx));
                }
                Curve* dcurve = new Curve(diff, currentCurve->name() + QObject::tr(" (一阶差分)"));
                stage.stageName = StageName::Derivative;
                stage.curve = QSharedPointer<Curve>(dcurve);
                stage.curve->setSampleId(sampleId);
                stage.algorithm = AlgorithmType::Derivative_SG; // 占位
                stage.isSegmented = false;
                stage.numSegments = 1;
                stage.useForPlot = true;
                sampleData.stages.append(stage);
            }
        }
    }

    DEBUG_LOG << "Pipeline completed:" ;
    return sampleData;
}



// ---------------- 批量样本流水线 ----------------
BatchGroupData DataProcessingService::runProcessTgBigPipelineForMultiple(
    const QList<int> &sampleIds,
    const ProcessingParameters &params)
{
    BatchGroupData batchResults;
    // QSqlDatabase threadDb = ensureThreadLocalDatabase(); // 批量处理也使用线程专用连接
    // SingleTobaccoSampleDAO dao(threadDb);
    SingleTobaccoSampleDAO dao;

    DEBUG_LOG << "Processing ProcessTgBig samples:" << sampleIds;

    for (int sampleId : sampleIds) {
        // 单样本处理
        SampleDataFlexible singleSample = runProcessTgBigPipeline(sampleId, params);
        SampleIdentifier identifier = dao.getSampleIdentifierById(sampleId);
        QString projectName = identifier.projectName;
        QString batchCode = identifier.batchCode;
        QString shortCode = identifier.shortCode;
        QString groupKey  = QString("%1-%2-%3").arg(projectName).arg(batchCode).arg(shortCode);

        // // 插入批量结果
        // batchResults.insert(name, singleSample);

        // 构造或获取已有的 SampleGroup
        SampleGroup &group = batchResults[groupKey];
        group.projectName = projectName;
        group.batchCode   = batchCode;
        group.shortCode   = shortCode;

        
        // // 添加单样本到平行样本列表
        // group.parallelSamples.append(singleSample);

        // // 用 SampleDataFlexible 构造 ParallelSampleData
        // ParallelSampleData parallelSample;
        // parallelSample.sampleData = singleSample;
        // parallelSample.parallelNo = identifier.parallelNo;  // 直接使用 identifier 中的 parallel_no

        // 添加到组内样本列表（不在此处打 bestInGroup 标记，由代表样选择服务统一处理）
        group.sampleDatas.append(singleSample);

    }

    // 【关键】在完成批量组构建后，调用代表样选择服务，按 RawData 阶段进行组内最优选择
    if (m_appInitializer && m_appInitializer->getParallelSampleAnalysisService()) {
        m_appInitializer->getParallelSampleAnalysisService()->selectRepresentativesInBatch(
            batchResults, params, StageName::RawData
        );
    } else {
        WARNING_LOG << "DataProcessingService: 未能调用代表样选择服务 (AppInitializer 或服务为空)";
    }

    return batchResults;
}
