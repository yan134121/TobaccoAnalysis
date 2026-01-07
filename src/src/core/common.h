
#ifndef COMMON_H
#define COMMON_H

#include <QString>
#include <QVariant> // 需要包含 QVariant
#include <qDebug>
#include <QAbstractTableModel>
#include <QSharedPointer>
#include <QMap>

#include "core/entities/Curve.h" // 包含 Curve 定义
// #include "TgBigParameterSettingsDialog.h"



// === “是否全优”判断阈值（宏定义），四类数据统一集中管理 ===
// 大热重
#define TG_BIG_OPTIMAL_PEARSON_RAW_THRESHOLD         1.0   // 原始皮尔逊阈值
#define TG_BIG_OPTIMAL_PEARSON_NORMALIZED_THRESHOLD  1.0   // 归一化皮尔逊阈值
// 小热重
#define TG_SMALL_OPTIMAL_PEARSON_RAW_THRESHOLD       1.0
#define TG_SMALL_OPTIMAL_PEARSON_NORMALIZED_THRESHOLD 1.0
// 色谱
#define CHROMATOGRAPH_OPTIMAL_PEARSON_RAW_THRESHOLD  1.0
#define CHROMATOGRAPH_OPTIMAL_PEARSON_NORMALIZED_THRESHOLD 1.0
// 工序大热重
#define PROCESS_TG_BIG_OPTIMAL_PEARSON_RAW_THRESHOLD 1.0
#define PROCESS_TG_BIG_OPTIMAL_PEARSON_NORMALIZED_THRESHOLD 1.0

struct NavigatorNodeInfo
{
    enum NodeType { Root, Model, Batch, ShortCode, DataType, Sample,  };

    NodeType type = Root;
    int id = -1;              //  / sample_id
    QString projectName;      // 新增：项目名
    QString batchCode;        // 新增：批次号
    QString shortCode;        // 样本编码
    int parallelNo = -1;      // 新增：平行号
    QString dataType;         // 数据类型
};


// 确保 Q_DECLARE_METATYPE 在 struct 定义之后
Q_DECLARE_METATYPE(NavigatorNodeInfo)




/**
 * @brief The ProcessingParameters struct
 * 一个用于在对话框和外部世界之间传递所有参数的结构体。
 */
struct ProcessingParameters {
    // 剔除坏点参数
    bool outlierRemovalEnabled = true;
    double outlierThreshold = 3.0; // 例如，标准差的倍数

    // 【工序大热重】坏点修复与检测相关参数
    double invalidTokenFraction = 0.2;         // 最大非数值比例（导入阶段校验用）
    int outlierWindow = 15;                    // Hampel局部异常窗口（奇数）
    double outlierNSigma = 4.0;                // Hampel阈值 n_sigma
    double jumpDiffThreshold = 2.0;            // 跳变异常差分阈值
    double globalNSigma = 4.0;                 // 全局MAD阈值
    int badPointsToShow = 0;                   // UI显示坏点TopN（0=全部）
    QString fitType = "linear";               // 坏段拟合类型：linear/quad
    double gamma = 0.0;                        // 混合权重（预留，与MATLAB一致）
    int anchorWindow = 8;                      // 拟合锚点窗口
    int monoStart = 20;                        // 单调性区间起点（索引）
    int monoEnd = 120;                         // 单调性区间终点（索引，闭区间）
    double edgeBlend = 0.0;                    // 边缘平滑（预留）
    double epsScale = 1e-9;                    // 平台严格递减斜率尺度
    double slopeThreshold = 1e-9;              // 斜率阈值（预留）
    QString interpMethod = "pchip";           // 插值方法：pchip/linear

    // --- 绘图显示选项（工序大热重界面） ---
    bool showMeanCurve = false;               // 显示均值曲线
    bool plotInterpolation = false;           // 绘图时插值到统一X
    bool showRawOverlay = false;              // 显示原始（预修复）叠加
    bool showBadPoints = false;               // 显示坏点标记

    // 裁剪参数
    bool clippingEnabled = true;
    double clipMinX = 60.0;
    double clipMaxX = 400.0;
    // 为“大热重”和“工序大热重”分别提供独立的裁剪参数，便于分别设置
    bool clippingEnabled_TgBig = true;
    double clipMinX_TgBig = 60.0;
    double clipMaxX_TgBig = 400.0;
    bool clippingEnabled_ProcessTgBig = true;
    double clipMinX_ProcessTgBig = 60.0;
    double clipMaxX_ProcessTgBig = 400.0;

    // 归一化参数
    bool normalizationEnabled = true;
    // 已更新：默认采用 MATLAB 风格的 absmax 归一化；范围参数由算法默认提供为 [0,1]
    QString normalizationMethod = "absmax"; // 当前支持 "absmax"；旧值如 "max_min"/"z_score" 已弃用

    // 平滑参数
    bool smoothingEnabled = true;
    QString smoothingMethod = "loess"; // 默认使用 Loess 平滑
    int sgWindowSize = 11;
    int sgPolyOrder = 2;
    double loessSpan = 0.2;                    // Loess平滑窗口比例（0..1）

    // 微分参数
    bool derivativeEnabled = true;
    QString derivativeMethod = "derivative_sg";  // DTG导数阶段方法
    int derivSgWindowSize = 13;                   // DTG导数窗口
    int derivSgPolyOrder = 2;                     // DTG导数阶数
    QString derivative2Method = "derivative_sg"; // 一阶导数阶段方法（对DTG再求导）
    int deriv2SgWindowSize = 11;                  // 一阶导数窗口
    int deriv2SgPolyOrder = 2;                    // 一阶导数阶数
    QString derivativeBase = "derivative_sg";  // SG/LOESS/first_diff 选择（与 derivativeMethod 对齐）
    QString defaultSmoothDisplay = "smoothed"; // 默认平滑显示类型（UI预留）

    // --- 基线校正 ---
    bool baselineEnabled = false;
    double lambda = 1e5;
    int order = 2;
    double p = 0.05;
    double wep = 0.0; // 权重例外比例，用于保护起始和结束区域（0~0.5）
    int itermax = 50;
    bool baselineOverlayRaw = false;      // 在基线校正图表中叠加显示原始曲线
    bool baselineDisplayEnabled = false;  // 在基线校正图表中绘制基线（原点样式）

    // --- 峰检测（新增色谱算法） ---
    bool peakDetectionEnabled = false;     // 是否执行峰检测
    double peakMinHeight = 0.0;            // 最小峰高度
    double peakMinProminence = 0.0;        // 最小峰显著性
    int    peakMinDistance = 5;            // 峰间最小点距
    double peakSnrThreshold = 0.0;         // 最小信噪比阈值

    // --- COW峰对齐（新增色谱算法，当前用于批量/参考对齐场景，预留参数） ---
    bool alignmentEnabled = false;         // 是否执行峰对齐（需要参考样本）
    int cowWindowSize = 100;               // 相关性窗口大小（预留）
    int cowMaxWarp = 50;                   // 最大滞后搜索范围（点）
    int cowSegmentCount = 1;               // 分段数（预留）
    double cowResampleStep = 0.0;          // 重采样步长（0 表示按参考x插值）
    int referenceSampleId = -1;            // 参考样本ID（-1 表示未设置）

    // 权重参数
    double weightNRMSE = 0.333;
    double weightPearson = 0.333;
    double weightEuclidean = 0.333;
    double weightRMSE = 0.0; // 新增：RMSE权重
    double totalWeight = weightNRMSE + weightPearson + weightEuclidean + weightRMSE;

    // 对比ROI参数 (新增)
    double comparisonStart = -1.0; // -1 表示使用全范围
    double comparisonEnd = -1.0;   // -1 表示使用全范围

    QString toString() const {
        return QString("ProcessingParameters(clippingEnabled=%1, clipMinX=%2, clipMaxX=%3, "
                       "normalizationEnabled=%4, normalizationMethod=%5, "
                       "smoothingEnabled=%6, smoothingMethod=%7, sgWindowSize=%8, sgPolyOrder=%9, "
                       "derivativeEnabled=%10, derivativeMethod=%11, derivSgWindowSize=%12, derivSgPolyOrder=%13"
                       "weightNRMSE=%14, weightPearson=%15, weightEuclidean=%16, weightRMSE=%17, comparisonStart=%18, comparisonEnd=%19)")
                .arg(clippingEnabled)
                .arg(clipMinX)
                .arg(clipMaxX)
                .arg(normalizationEnabled)
                .arg(normalizationMethod)
                .arg(smoothingEnabled)
                .arg(smoothingMethod)
                .arg(sgWindowSize)
                .arg(sgPolyOrder)
                .arg(derivativeEnabled)
                .arg(derivativeMethod)
                .arg(derivSgWindowSize)
                .arg(derivSgPolyOrder)
                .arg(weightNRMSE)
                .arg(weightPearson)
                .arg(weightEuclidean)
                .arg(weightRMSE)
                .arg(comparisonStart)
                .arg(comparisonEnd);

    }

};

Q_DECLARE_METATYPE(ProcessingParameters)

// inline QDebug operator<<(QDebug dbg, const ProcessingParameters& params)
// {
//     // QDebugStateSaver 确保我们的修改（如 nospace()）只在函数内部有效
//     QDebugStateSaver saver(dbg);
//     dbg.nospace() << "ProcessingParameters("
//                   << "clipEnabled: " << params.clippingEnabled
//                   << ", clipMin: " << params.clipMinX
//                   << ", clipMax: " << params.clipMaxX
//                   << ", normEnabled: " << params.normalizationEnabled
//                   << ", normMethod: " << params.normalizationMethod
//                   << ", smoothEnabled: " << params.smoothingEnabled
//                   << ", smoothMethod: " << params.smoothingMethod
//                   << ", sgWindow: " << params.sgWindowSize
//                   << ", sgPoly: " << params.sgPolyOrder
//                   << ", derivEnabled: " << params.derivativeEnabled
//                   // ... (添加所有其他参数的打印)
//                   << ")";
//     return dbg;
// }


// 用于从 Service 返回多阶段处理结果的结构体
// struct MultiStageData
// {
//     Curve* original = nullptr;
//     Curve* cleaned = nullptr; // 剔除坏点后
//     Curve* smoothed = nullptr;
//     Curve* normalized = nullptr;
//     Curve* derivative = nullptr;

//     // 析构函数，负责释放所有 Curve 对象的内存
//     ~MultiStageData() {
//         clear();
//     }
    
//     // 清理函数
//     void clear() {
//         delete original; original = nullptr;
//         delete cleaned; cleaned = nullptr;
//         delete smoothed; smoothed = nullptr;
//         delete normalized; normalized = nullptr;
//         delete derivative; derivative = nullptr;
//     }
// };

// 可以给大小热重和工序热重使用，色谱的单独使用一个结构体
struct MultiStageData
{
    QSharedPointer<Curve> original;
    QSharedPointer<Curve> cleaned;
    QSharedPointer<Curve> smoothed;
    QSharedPointer<Curve> normalized;
    QSharedPointer<Curve> derivative;

    MultiStageData() = default;
};


// **重要**：我们需要让这个自定义类型能在信号槽中传递
Q_DECLARE_METATYPE(MultiStageData)

// 【新】多个样本的批量多阶段处理结果
struct BatchMultiStageData : public QMap<int, MultiStageData>
{
    // C++11 using 关键字，让我们可以使用基类的构造函数
    using QMap<int, MultiStageData>::QMap;

    void clear() {
        // QMap 的 value 是对象而不是指针，所以 clear() 会自动调用 MultiStageData 的析构函数
        // 如果 MultiStageData 里是 Curve*，那么 MultiStageData 的析构函数必须正确 delete
        QMap<int, MultiStageData>::clear();
    }
};
Q_DECLARE_METATYPE(BatchMultiStageData)

/**
 * @brief AnalysisTaskDescriptor 结构体
 * 描述一个需要后台服务执行的分析或处理任务。
 */
struct AnalysisTaskDescriptor
{
    // 任务类型，用于服务层决定调用哪个具体逻辑
    QString taskType; // e.g., "tg_big_multistage", "process_comparison"

    // 输入数据标识符
    QList<int> sampleIds;      // 适用于需要多个样本ID的场景
    int primarySampleId = -1;  // 适用于“单一样本处理”场景
    int batchId = -1;          // 适用于“整个批次分析”场景

    // 任务参数
    ProcessingParameters params; // 复用我们已有的参数结构体
};



// struct SampleIdentifier
// {
//     QString projectName;   // 项目/香烟型号
//     QString batchCode;     // 批次编码
//     QString shortCode;     // 样本编码
//     int parallelNo;        // 平行样编号
// };

struct SampleIdentifier
{
    int sampleId = -1;           // 样品ID，新增
    QString projectName;   // 项目/香烟型号
    QString batchCode;     // 批次编码
    QString shortCode;     // 样本编码
    int parallelNo = -1;   // 平行样编号
};



/**
 * @brief DifferenceResultRow 结构体
 * 
 * 用于存储一个对比样本相对于参考样本的所有差异度计算结果。
 * 这是 "样本对比工作台" 中结果表格每一行的数据模型。
 */
struct DifferenceResultRow
{
    int sampleId;                   // 对比样本的ID (来自 single_tobacco_sample.id)
    QString sampleName;             // 对比样本的名称 (用于UI显示)
    
    // Key: 算法的唯一ID (e.g., "nrmse", "pearson")
    // Value: 该算法计算出的差异度得分
    QMap<QString, double> scores;
};

// 【重要】注册这个自定义类型，以便它可以在信号槽中安全地传递
Q_DECLARE_METATYPE(DifferenceResultRow)




enum DataType {
    TG_BIG,
    TG_SMALL,
    CHROMATOGRAM,
    PROCESS_TG_BIG
};


enum class BatchType { NORMAL, PROCESS };

struct SegmentInfo {
    int segmentId;
    double startPoint;
    double endPoint;
    bool isManualSegment;
};

struct CurveData {
    SampleIdentifier sampleId;
    DataType dataType;

    // 保存不同阶段的数据
    std::map<int, std::vector<double>> stageYValues; // key: TgBigProcessStage/TgSmallProcessStage等
    std::vector<double> xValues;
    std::vector<SegmentInfo> segments;
};

struct DifferenceResult {
    SampleIdentifier sample1;
    SampleIdentifier sample2;
    DataType dataType;
    int stage;          // 对应阶段
    int segmentId;      // -1表示整体曲线
    double rmseValue;
    double pearsonValue;
    double euclideanValue;
};

struct BatchData {
    std::string batchCode;
    std::vector<CurveData> curves;
    std::map<std::string, SampleIdentifier> optimalSamples;
};


struct AnalysisResultPackage
{
    QMap<QString, QSharedPointer<Curve>> curves;           // 单条曲线
    QMap<QString, QList<QSharedPointer<Curve>>> curveLists; // 多条曲线，比如不同阶段
    QMap<QString, QSharedPointer<QAbstractTableModel>> tables; // 差异度、峰信息
    QMap<QString, double> values;                          // 数值结果
    bool success = true;
    QString errorMessage;
};


////////////////////////////////////////////////////////////////////////////////////////

// 阶段名称枚举
enum class StageName {
    RawData,
    Clip,
    Normalize,
    Smooth,
    Derivative,
    Difference,
    Segmentation,
    BaselineCorrection,
    PeakDetection,
    PeakAlignment,
    BadPointRepair,
    SegmentComparison
};

// 算法类型枚举
enum class AlgorithmType {
    None,
    Clip,
    Normalize,
    Smooth_SG,
    Smooth_Loess,
    Derivative_SG,
    CentralDifference,
    Difference,
    Segmentation,
    BaselineCorrection,
    PeakDetection,
    PeakAlignment,
    BadPointRepair
};

// // 数据类型枚举
// enum class DataType { TG_BIG, TG_SMALL, CHROMATOGRAPH, PROCESS_TG_BIG };

// ---- 阶段/算法状态 ----
enum class StageStatus { Pending, Calculating, Done, Error };
enum class SegmentStatus { Pending, Done, Skipped, Error };
//新数据结构主要结构开始/////////////////////////////////////////////////
// ---- 单样本的分段数据 ----
struct SegmentData {
    QString name;                       // 段名，例如 "Segment 1"
    QSharedPointer<Curve> curve;        // 该段曲线
    QVariantMap metrics;                 // 差异度、统计指标等

    int startIndex = 0;                  // 分段起点（在原曲线中的索引）
    int endIndex = 0;                    // 分段终点（在原曲线中的索引）
    double startX = 0.0;                 // 分段起点 X 值
    double endX = 0.0;                   // 分段终点 X 值
    bool useSegment = true;              // 是否使用分段数据
    SegmentStatus status = SegmentStatus::Pending;

    SegmentData() = default;
};
Q_DECLARE_METATYPE(SegmentData)

// ---- 单样本的阶段数据 ----
struct StageData {
    StageName stageName;  // 阶段名称，例如 "平滑"
    AlgorithmType algorithm;  // 算法类型，例如 "Savitzky-Golay"
    // QString name;                        // 阶段名称，例如 "平滑"
    QSharedPointer<Curve> curve;         // 整体曲线（未分段）
    // QString algorithm;                    // 算法类型，例如 "Savitzky-Golay"
    QVariantMap parameters;               // 算法参数，例如 {"window":5, "order":3}

    bool isSegmented = false;            // 是否对该阶段曲线进行分段
    int numSegments = 1;                 // 分段数量
    QVector<SegmentData> segments;       // 分段数据

    StageStatus status = StageStatus::Pending;
    bool useForPlot = true;              // UI 是否显示该阶段曲线

    QVariantMap metrics;                  // <-- 存储该阶段计算出的指标或差异度

    StageData() = default;
};
Q_DECLARE_METATYPE(StageData)

// ---- 单样本的多阶段数据 ----
struct SampleDataFlexible {
    int sampleId;                        // 样本 ID
    // QString dataType;                     // 数据类型：大热重、小热重、色谱等
    DataType dataType;
    QVector<StageData> stages;            // 阶段列表（按处理顺序）

    bool bestInGroup = false;              // 是否组内最优平行样
    
    SampleDataFlexible() = default;
};
Q_DECLARE_METATYPE(SampleDataFlexible)

// ------------------- 样本组 -------------------
struct SampleGroup {
    QString projectName;
    QString batchCode;
    QString shortCode;

    QVector<SampleDataFlexible> sampleDatas; // 同一组内的所有平行样本
    // int bestSampleIndex = -1;                     // 指向 parallelSamples 中最优代表

    SampleGroup() = default;
};
Q_DECLARE_METATYPE(SampleGroup)

// ------------------- 批量组 -------------------
struct BatchGroupData : public QMap<QString, SampleGroup> {
    // key 可以是 "projectName-batchCode-shortCode"
    using QMap<QString, SampleGroup>::QMap;
};
Q_DECLARE_METATYPE(BatchGroupData)

//新数据结构主要结构结束/////////////////////////////////////////////////


// 阶段模板，便于初始化
struct StageTemplate {
    StageName stageName;
    AlgorithmType algorithmType;
    QVariantMap defaultParameters;
    bool isSegmented = false;
    int defaultNumSegments = 1;

    StageTemplate(StageName s, AlgorithmType a, const QVariantMap& p = {}, bool seg = false, int n = 1)
        : stageName(s), algorithmType(a), defaultParameters(p), isSegmented(seg), defaultNumSegments(n) {}
};


extern QVector<StageTemplate> tgBigTemplates;
extern QVector<StageTemplate> tgSmallTemplates;
extern QVector<StageTemplate> chromaTemplates;
extern QVector<StageTemplate> tgBigProcessTemplates;
// extern const ParallelSampleData* getFirstParallelSampleAsBest(const SampleGroup& group);

// ================= 函数声明 =================
QString stageNameToString(StageName name);
QString algorithmTypeToString(AlgorithmType algo);

// 打印整个批量组
// 打印整个批量组（顶层入口）
void printBatchGroupData(const BatchGroupData &batchData);

// 打印单个组
void printGroup(const SampleGroup &group);

// 打印单个样本
void printSample(const SampleDataFlexible &sample, int indent = 0);

// 打印单个阶段
void printStage(const StageData &stage, int indent = 0);

// 打印辅助函数：返回缩进字符串
QString indentString(int indent);



#endif // COMMON_H
