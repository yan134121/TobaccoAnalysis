#include "common.h"
#include "Logger.h"

#include <QSharedPointer>
#include <QDebug>
#include <QString>
#include <QMap>


// ----------------- 函数实现 -----------------
QString stageNameToString(StageName name){
    switch(name){
        case StageName::RawData: return "原始数据";
        case StageName::Clip: return "裁剪";
        case StageName::Normalize: return "归一化";
        case StageName::Smooth: return "平滑";
        case StageName::Derivative: return "微分";
        case StageName::Difference: return "差异度";
        case StageName::Segmentation: return "分段";
        case StageName::BaselineCorrection: return "基线校正";
        case StageName::PeakDetection: return "峰识别";
        case StageName::PeakAlignment: return "峰对齐";
        case StageName::BadPointRepair: return "坏点修复";
        case StageName::SegmentComparison: return "分段对比";
        default: return "未知";
    }
}

QString algorithmTypeToString(AlgorithmType algo){
    switch(algo){
        case AlgorithmType::None: return "None";
        case AlgorithmType::Clip: return "Clip";
        case AlgorithmType::Normalize: return "Normalize";
        case AlgorithmType::Smooth_SG: return "Smooth_SavitzkyGolay";
        case AlgorithmType::Smooth_Loess: return "Smooth_Loess";
        case AlgorithmType::Derivative_SG: return "Derivative_SavitzkyGolay";
        case AlgorithmType::CentralDifference: return "CentralDifference";
        case AlgorithmType::Difference: return "Difference";
        case AlgorithmType::Segmentation: return "Segmentation";
        case AlgorithmType::BaselineCorrection: return "BaselineCorrection";
        case AlgorithmType::PeakDetection: return "PeakDetection";
        case AlgorithmType::PeakAlignment: return "PeakAlignment";
        case AlgorithmType::BadPointRepair: return "BadPointRepair";
        default: return "Unknown";
    }
}


// ----------------- 大热重模板 -----------------
QVector<StageTemplate> tgBigTemplates = {
    {StageName::RawData, AlgorithmType::None, {}, false, 1},
    {StageName::Clip, AlgorithmType::Clip, {{"start",0},{"end",1000}}, false, 1},
    {StageName::Normalize, AlgorithmType::Normalize, {}, false, 1},
    {StageName::Smooth, AlgorithmType::Smooth_SG, {{"window",5},{"order",3}}, false, 1},
    {StageName::Derivative, AlgorithmType::CentralDifference, {{"step",1}}, false, 1},
    {StageName::Difference, AlgorithmType::Difference, {}, false, 1},
    {StageName::Segmentation, AlgorithmType::Segmentation, {}, false, 1}
};

// ----------------- 小热重模板 -----------------
QVector<StageTemplate> tgSmallTemplates = {
    {StageName::RawData, AlgorithmType::None, {}, false, 1},
    {StageName::Difference, AlgorithmType::Difference, {}, false, 1},
    {StageName::Segmentation, AlgorithmType::Segmentation, {}, false, 1}
};

// ----------------- 色谱模板 -----------------
QVector<StageTemplate> chromaTemplates = {
    {StageName::RawData, AlgorithmType::None, {}, false, 1},
    {StageName::BaselineCorrection, AlgorithmType::BaselineCorrection, {}, false, 1},
    {StageName::Normalize, AlgorithmType::Normalize, {}, false, 1},
    {StageName::PeakDetection, AlgorithmType::PeakDetection, {{"threshold",0.01}}, false, 1},
    {StageName::PeakAlignment, AlgorithmType::PeakAlignment, {}, false, 1},
    {StageName::Difference, AlgorithmType::Difference, {}, false, 1},
    {StageName::SegmentComparison, AlgorithmType::Segmentation, {}, false, 1}
};

// ----------------- 工序大热重模板 -----------------
QVector<StageTemplate> tgBigProcessTemplates = {
    {StageName::RawData, AlgorithmType::None, {}, false, 1},
    {StageName::BadPointRepair, AlgorithmType::BadPointRepair, {{"window",15},{"n_sigma",4.0},{"jump_threshold",2.0}}, false, 1},
    {StageName::Clip, AlgorithmType::Clip, {{"start",0},{"end",1000}}, false, 1},
    {StageName::Normalize, AlgorithmType::Normalize, {}, false, 1},
    {StageName::Smooth, AlgorithmType::Smooth_SG, {{"window",5},{"order",3}}, false, 1},
    {StageName::Derivative, AlgorithmType::CentralDifference, {{"step",1}}, false, 1},
    {StageName::Difference, AlgorithmType::Difference, {}, false, 1},
    {StageName::Segmentation, AlgorithmType::Segmentation, {}, false, 1}
};


// // const 版本
// const ParallelSampleData* getFirstParallelSampleAsBest(const SampleGroup& group) {
//     if (group.parallelSamples.isEmpty()) {
//         return nullptr;
//     }
//     return &group.parallelSamples[0];
// }

// ========== 打印 StageData ==========
void printStage(const StageData &stage, int indent)
{
    QString prefix(indent, ' ');
    DEBUG_LOG << prefix
                       << "Stage:" << static_cast<int>(stage.stageName)
                       << "| Algorithm:" << static_cast<int>(stage.algorithm)
                       << "| useForPlot:" << stage.useForPlot
                       << "| isSegmented:" << stage.isSegmented
                       << "| curve:" << (stage.curve ? "Valid" : "Null")
                       << "| curvePointCount:" << stage.curve->pointCount()
                       << "| metrics.count:" << stage.metrics.size();
}

// ========== 打印 SampleDataFlexible ==========
void printSample(const SampleDataFlexible &sample, int indent)
{
    QString prefix(indent, ' ');
    DEBUG_LOG << prefix
                       << "SampleDataFlexible:"
                       << "ID=" << sample.sampleId
                       << ", bestInGroup=" << sample.bestInGroup
                       << ", stageCount=" << sample.stages.size();

    for (const auto &stage : sample.stages) {
        printStage(stage, indent + 2);
    }
}

// ========== 打印 SampleGroup ==========
void printGroup(const SampleGroup &group)
{
    DEBUG_LOG << "SampleGroup:"
                       << group.projectName
                       << group.batchCode
                       << group.shortCode
                       << "| sampleCount:" << group.sampleDatas.size();

    int index = 0;
    for (const auto &sample : group.sampleDatas) {
        DEBUG_LOG << "  [Sample" << index++ << "]";
        printSample(sample, 6);
    }
}

// ========== 打印 BatchGroupData ==========
void printBatchGroupData(const BatchGroupData &batchData)
{
    DEBUG_LOG << "==== BatchGroupData Debug Print ====";
    DEBUG_LOG << "Total groups:" << batchData.size();

    for (auto it = batchData.constBegin(); it != batchData.constEnd(); ++it) {
        DEBUG_LOG << "---------------------------------";
        DEBUG_LOG << "Group key:" << it.key();
        printGroup(it.value());
    }
}


QString indentString(int indent) {
    return QString(indent, ' ');
}
