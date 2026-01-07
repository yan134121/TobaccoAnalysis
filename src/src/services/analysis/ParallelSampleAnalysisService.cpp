#include "ParallelSampleAnalysisService.h"
#include "core/AppInitializer.h"
#include "services/analysis/SampleComparisonService.h"
#include "core/entities/Curve.h"
#include "Logger.h"
#include <algorithm>
#include <limits>
#include <cmath>

ParallelSampleAnalysisService::ParallelSampleAnalysisService(AppInitializer* appInit, QObject* parent)
    : QObject(parent), m_appInitializer(appInit)
{
}

// 复制各工作台的常用工具方法：根据阶段取曲线
QSharedPointer<Curve> ParallelSampleAnalysisService::getCurveFromStage(const SampleDataFlexible& sample, StageName stage)
{
    for (const StageData& s : sample.stages) {
        if (s.stageName == stage) {
            return s.curve;
        }
    }
    return QSharedPointer<Curve>();
}

void ParallelSampleAnalysisService::selectRepresentativesInBatch(
    BatchGroupData& batchData,
    const ProcessingParameters& params,
    StageName stage) const
{
    for (auto it = batchData.begin(); it != batchData.end(); ++it) {
        SampleGroup& group = it.value();
        processReplicateGroup(group, params, stage);
    }
}
bool ParallelSampleAnalysisService::processReplicateGroup(
    SampleGroup& group,
    const ProcessingParameters& params,
    StageName stage) const
{
    // —— 安全检查：确保可获取差异度对比服务 ——
    if (!m_appInitializer) {
        WARNING_LOG << "ParallelSampleAnalysisService::processReplicateGroup: AppInitializer is null";
        return false;
    }
    SampleComparisonService* comparer = m_appInitializer->getSampleComparisonService();
    if (!comparer) {
        WARNING_LOG << "ParallelSampleAnalysisService::processReplicateGroup: SampleComparisonService unavailable";
        return false;
    }

    if (group.sampleDatas.isEmpty()) {
        return false;
    }

    // —— 重置组内标记 ——
    for (auto& s : group.sampleDatas) {
        s.bestInGroup = false;
    }

    // —— 根据指定阶段收集曲线（含 ROI 裁剪） ——
    struct Item { int sampleId; QSharedPointer<Curve> curve; };
    QVector<Item> items;
    items.reserve(group.sampleDatas.size());
    // ROI裁剪函数，支持使用 ProcessingParameters 的 comparisonStart/comparisonEnd
    auto getRoiCurve = [&](QSharedPointer<Curve> original) -> QSharedPointer<Curve> {
        if (original.isNull()) return QSharedPointer<Curve>();
        double startX = params.comparisonStart;
        double endX   = params.comparisonEnd;
        // -1 表示不裁剪，直接返回原曲线
        if (startX < 0 && endX < 0) return original;
        // 构造裁剪范围，支持仅指定一端
        double minX = (startX >= 0) ? startX : -std::numeric_limits<double>::infinity();
        double maxX = (endX   >= 0) ? endX   :  std::numeric_limits<double>::infinity();

        const QVector<QPointF>& data = original->data();
        QVector<QPointF> newData; newData.reserve(data.size());
        for (const auto& p : data) {
            if (p.x() >= minX && p.x() <= maxX) {
                newData.append(p);
            }
        }
        if (newData.size() < 2) return original; // 裁剪过小则回退
        QSharedPointer<Curve> c(new Curve());
        c->setData(newData);
        c->setSampleId(original->sampleId());
        c->setName(original->name());
        return c;
    };

    for (const SampleDataFlexible& sample : group.sampleDatas) {
        QSharedPointer<Curve> c = getCurveFromStage(sample, stage);
        QSharedPointer<Curve> roiC = getRoiCurve(c);
        if (!roiC.isNull()) {
            items.push_back({sample.sampleId, roiC});
        }
    }

    // —— 边界处理：无曲线/单曲线 ——
    if (items.isEmpty()) {
        // 无可对比曲线，默认第一个样本为代表
        group.sampleDatas[0].bestInGroup = true;
        return true;
    }
    if (items.size() == 1) {
        int onlyId = items.front().sampleId;
        for (auto& s : group.sampleDatas) {
            if (s.sampleId == onlyId) { s.bestInGroup = true; break; }
        }
        return true;
    }

    // —— 统一长度截断到组内最短长度（对齐 V2.2.1_origin）——
    int minLen = std::numeric_limits<int>::max();
    for (const auto& item : items) {
        if (!item.curve.isNull()) {
            minLen = std::min(minLen, item.curve->pointCount());
        }
    }
    if (minLen > 0 && minLen < std::numeric_limits<int>::max()) {
        for (auto& item : items) {
            if (!item.curve.isNull() && item.curve->pointCount() > minLen) {
                QVector<QPointF> oldData = item.curve->data();
                QVector<QPointF> newData = oldData.mid(0, minLen);
                QSharedPointer<Curve> newCurve(new Curve());
                newCurve->setData(newData);
                newCurve->setSampleId(item.curve->sampleId());
                newCurve->setName(item.curve->name());
                item.curve = newCurve;
            }
        }
    }

    // —— 预计算所有两两差异的原始值（含 NRMSE），用于后续按样本平均与归一化 ——
    struct PairScore { int i; int j; double nrmse; double rmse; double pearson; double euclid; };
    QVector<PairScore> pairScores;
    for (int i = 0; i < items.size(); ++i) {
        for (int j = i + 1; j < items.size(); ++j) {
            // 手动计算 RMSE/NRMSE（NRMSE 以第二条曲线的振幅作为归一化分母）
            const QVector<QPointF>& da = items[i].curve->data();
            const QVector<QPointF>& db = items[j].curve->data();
            int n = std::min(da.size(), db.size());
            double sumSq = 0.0;
            double minB = std::numeric_limits<double>::infinity();
            double maxB = -std::numeric_limits<double>::infinity();
            for (int k = 0; k < n; ++k) {
                double ya = da[k].y();
                double yb = db[k].y();
                double diff = ya - yb;
                sumSq += diff * diff;
                if (yb < minB) minB = yb;
                if (yb > maxB) maxB = yb;
            }
            double rmse = (n > 0) ? std::sqrt(sumSq / n) : 0.0;
            double rangeB = std::max(maxB - minB, std::numeric_limits<double>::epsilon());
            double nrmse = rmse / rangeB;

            double pear  = comparer->calculatePearsonCorrelation(items[i].curve, items[j].curve);
            double eucl  = comparer->calculateEuclideanDistance(items[i].curve, items[j].curve);
            pairScores.push_back({i, j, nrmse, rmse, pear, eucl});
        }
    }

    const double wNRMSE  = params.weightNRMSE;
    const double wRMSE   = params.weightRMSE;
    const double wPearson= params.weightPearson;
    const double wEuclid = params.weightEuclidean;
    double totalW = wNRMSE + wRMSE + wPearson + wEuclid;
    if (totalW <= 0.0) totalW = 1.0;

    QVector<double> sumNrmse(items.size(), 0.0), sumRmse(items.size(), 0.0), sumPear(items.size(), 0.0), sumEucl(items.size(), 0.0);
    QVector<int> counts(items.size(), 0);
    for (const auto& ps : pairScores) {
        sumNrmse[ps.i] += ps.nrmse; sumNrmse[ps.j] += ps.nrmse;
        sumRmse[ps.i]  += ps.rmse;  sumRmse[ps.j]  += ps.rmse;
        sumPear[ps.i]  += ps.pearson; sumPear[ps.j] += ps.pearson;
        sumEucl[ps.i]  += ps.euclid;  sumEucl[ps.j] += ps.euclid;
        counts[ps.i]++; counts[ps.j]++;
    }

    QVector<double> rawNrmse(items.size(), 0.0), rawRmse(items.size(), 0.0), rawPear(items.size(), 0.0), rawEucl(items.size(), 0.0);
    for (int i = 0; i < items.size(); ++i) {
        const double denom = (counts[i] > 0) ? static_cast<double>(counts[i]) : 1.0;
        rawNrmse[i] = sumNrmse[i] / denom;
        rawRmse[i]  = sumRmse[i]  / denom;
        rawPear[i]  = sumPear[i]  / denom;
        rawEucl[i]  = sumEucl[i]  / denom;
    }

    auto normalizeVector = [](const QVector<double>& x, bool inverse) {
        QVector<double> y; y.resize(x.size());
        if (x.isEmpty()) return y;
        double xmin = *std::min_element(x.begin(), x.end());
        double xmax = *std::max_element(x.begin(), x.end());
        if (xmax > xmin) {
            for (int i = 0; i < x.size(); ++i) {
                double v = (x[i] - xmin) / (xmax - xmin);
                y[i] = inverse ? (1.0 - v) : v;
            }
        } else {
            for (int i = 0; i < x.size(); ++i) y[i] = 1.0;
        }
        return y;
    };

    QVector<double> scoreNrmse = normalizeVector(rawNrmse, true);
    QVector<double> scoreRmse  = normalizeVector(rawRmse,  true);
    QVector<double> scoreEucl  = normalizeVector(rawEucl,  true);
    QVector<double> scorePear  = normalizeVector(rawPear,  false);

    int bestIndex = 0;
    double bestScore = -std::numeric_limits<double>::infinity();
    for (int i = 0; i < items.size(); ++i) {
        double score = (wNRMSE * scoreNrmse[i] + wRMSE * scoreRmse[i] + wPearson * scorePear[i] + wEuclid * scoreEucl[i]) / totalW;
        if (score > bestScore) {
            bestScore = score;
            bestIndex = i;
        }
    }

    // // 得分最低的样本作为最优
    // int bestIndex = 0;
    // double bestScore = std::numeric_limits<double>::max(); // 初始为最大值
    // for (int i = 0; i < items.size(); ++i) {
    //     double avg = counts[i] > 0 ? (avgScores[i] / counts[i]) : 0.0;
    //     if (avg < bestScore) {
    //         bestScore = avg;
    //         bestIndex = i;
    //     }
    // }



    int bestSampleId = items[bestIndex].sampleId;
    for (auto& s : group.sampleDatas) {
        s.bestInGroup = (s.sampleId == bestSampleId);
    }

    DEBUG_LOG << "ReplicateGroup best sampleId=" << bestSampleId << ", score=" << bestScore;
    return true;
}
