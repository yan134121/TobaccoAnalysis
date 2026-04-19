/**
 * PeakSeg-COW 峰对齐 —— 对应 色谱处理算法/03_色谱对齐/my_cow_align_peakseg_V5_auto.m
 * - movmean 平滑参考、多区间 my_findpeaks 式峰检测、峰簇聚类与谷值分段
 * - 段内 DP：代价 1−ρ，my_linear_resample 与 my_corr 与 MATLAB 实现一致（见本文件内注释）
 * 批处理脚本 Sepu_align_batch.m 中的 opts（ranges/rangeProm）在应用层由 peakSegMatlabSepuAlignBatchParams() 注入（见 DataProcessingService.cpp）
 *
 * 性能：movmean 用前缀和 O(n)；DP 内层复用 warping 缓冲区，避免每步 QVector 分配与拷贝（与 MATLAB 数值路径一致）。
 */
#include "PeakSegCOWAlignment.h"

#include "core/entities/Curve.h"

#include <QCoreApplication>
#include <QtMath>
#include <algorithm>
#include <limits>

namespace {

static QVector<double> movmean(const QVector<double>& x, int span)
{
    if (span <= 1 || x.isEmpty()) return x;
    const int n = x.size();
    const int half = span / 2;
    QVector<double> out;
    out.resize(n);

    // 前缀和：窗口求和 O(1)，总体 O(n)（原实现为 O(n·span)）
    QVector<double> cum(n + 1);
    cum[0] = 0.0;
    for (int i = 0; i < n; ++i)
        cum[i + 1] = cum[i] + x[i];

    for (int i = 0; i < n; ++i) {
        const int l = qMax(0, i - half);
        const int r = qMin(n - 1, i + half);
        out[i] = (cum[r + 1] - cum[l]) / double(r - l + 1);
    }
    return out;
}

/** 与历史 QVector 版数值一致；写入 dst[0..newLen-1]，可原地 src==dst 仅当 origLen==newLen */
static void linearResampleInPlace(const double* src, int origLen, double* dst, int newLen)
{
    if (!src || !dst || origLen <= 0 || newLen <= 0)
        return;
    if (origLen == newLen) {
        if (src != dst) {
            for (int i = 0; i < newLen; ++i)
                dst[i] = src[i];
        }
        return;
    }

    // MATLAB：orig_x=linspace(1,orig_len,orig_len); new_x=linspace(1,orig_len,new_len);
    for (int i = 0; i < newLen; ++i) {
        const double t = (newLen == 1) ? 0.0 : (double(i) / double(newLen - 1));
        const double newX = 1.0 + t * double(origLen - 1);

        if (newX <= 1.0) {
            dst[i] = src[0];
        } else if (newX >= double(origLen)) {
            dst[i] = src[origLen - 1];
        } else {
            int leftIdx1 = int(qFloor(newX));
            leftIdx1 = qMax(1, qMin(leftIdx1, origLen - 1));
            const double alpha = newX - double(leftIdx1);
            const int left0 = leftIdx1 - 1;
            dst[i] = (1.0 - alpha) * src[left0] + alpha * src[left0 + 1];
        }
    }
}

static double myCorrPtr(const double* x, const double* y, int n)
{
    if (!x || !y || n <= 0)
        return std::numeric_limits<double>::quiet_NaN();

    double meanX = 0.0, meanY = 0.0;
    for (int i = 0; i < n; ++i) {
        meanX += x[i];
        meanY += y[i];
    }
    meanX /= double(n);
    meanY /= double(n);

    double num = 0.0;
    double normX = 0.0;
    double normY = 0.0;
    bool allZeroX = true;
    bool allZeroY = true;
    bool equalXY = true;

    for (int i = 0; i < n; ++i) {
        const double xc = x[i] - meanX;
        const double yc = y[i] - meanY;
        num += xc * yc;
        normX += xc * xc;
        normY += yc * yc;
        if (x[i] != 0.0) allZeroX = false;
        if (y[i] != 0.0) allZeroY = false;
        if (x[i] != y[i]) equalXY = false;
    }

    normX = qSqrt(normX);
    normY = qSqrt(normY);
    if (normX < std::numeric_limits<double>::epsilon() || normY < std::numeric_limits<double>::epsilon()) {
        if (allZeroX && allZeroY) return std::numeric_limits<double>::quiet_NaN();
        if (equalXY && !(allZeroX && allZeroY)) return 1.0;
        return std::numeric_limits<double>::quiet_NaN();
    }

    return num / (normX * normY);
}

struct Peak {
    int loc1;     // 1-based index in segment
    double value; // peak height
    double prom;  // prominence
};

static QVector<Peak> myFindPeaksProminence(const QVector<double>& y, double minProm)
{
    QVector<Peak> peaks;
    if (y.size() < 3) return peaks;

    const int N = y.size();

    // 1) 找局部极大值（平顶峰取中心）
    QVector<int> locs1;
    locs1.reserve(N / 10);

    int k = 1; // 0-based, start at 2nd element
    while (k <= N - 2) {
        if (y[k] > y[k - 1] && y[k] > y[k + 1]) {
            locs1.push_back(k + 1); // 1-based
        } else if (y[k] > y[k - 1] && y[k] == y[k + 1]) {
            int j = k;
            while (j < N - 1 && y[j] == y[j + 1]) j++;
            int loc0 = int(qRound((k + j) / 2.0));
            locs1.push_back(loc0 + 1); // 1-based
            k = j; // 跳过平顶
        }
        ++k;
    }

    if (locs1.isEmpty()) return peaks;

    // 2) prominence 精确计算 + 阈值过滤（该算法只需要 MinPeakProminence）
    for (int idx = 0; idx < locs1.size(); ++idx) {
        int loc1 = locs1[idx];
        int loc0 = loc1 - 1;

        // 向左搜索直到遇到更高峰或开头：while L>=1 && y(L)<=y(k) L--
        int L0 = loc0 - 1;
        while (L0 >= 0 && y[L0] <= y[loc0]) L0--;
        double leftMin = y[loc0];
        for (int t = L0 + 1; t <= loc0; ++t) leftMin = qMin(leftMin, y[t]);

        // 向右搜索直到遇到更高峰或结尾：while R<=N && y(R)<=y(k) R++
        int R0 = loc0 + 1;
        while (R0 < N && y[R0] <= y[loc0]) R0++;
        double rightMin = y[loc0];
        for (int t = loc0; t <= R0 - 1; ++t) rightMin = qMin(rightMin, y[t]);

        double base = qMax(leftMin, rightMin);
        double prom = y[loc0] - base;
        if (prom < minProm) continue;

        peaks.push_back(Peak{loc1, y[loc0], prom});
    }

    return peaks;
}

static QVector<int> buildUniformRangeStarts0(int n, int rangeCount)
{
    // MATLAB 默认：均分 3 段（ranges 用 1-based），这里用 0-based 生成等价分段
    QVector<int> starts0;
    if (n <= 0) return starts0;
    rangeCount = qMax(1, rangeCount);
    starts0.reserve(rangeCount);
    for (int i = 0; i < rangeCount; ++i) {
        int s = int(qRound(double(i) * double(n) / double(rangeCount)));
        s = qMin(qMax(0, s), n - 1);
        if (starts0.isEmpty() || s > starts0.last()) starts0.push_back(s);
    }
    if (starts0.isEmpty()) starts0.push_back(0);
    return starts0;
}

/** 解析 params["ranges"]（每行 1-based 闭区间）与 range_prominences；为空则按 rangeCount 均分 */
static void resolveMultiRanges(
    const QVariantMap& params,
    int n1,
    double minProm,
    int rangeCount,
    QVector<int>& outRangeStarts0,
    QVector<int>& outRangeEnds0,
    QVector<double>& outPromPerRange)
{
    outRangeStarts0.clear();
    outRangeEnds0.clear();
    outPromPerRange.clear();

    const QVariantList rangesVar = params.value(QStringLiteral("ranges")).toList();
    if (!rangesVar.isEmpty()) {
        for (const QVariant& rv : rangesVar) {
            const QVariantList row = rv.toList();
            if (row.size() < 2) continue;
            const int a1 = row[0].toInt();
            const int b1 = row[1].toInt();
            const int s0 = qMax(0, a1 - 1);
            const int e0 = qMin(n1 - 1, b1 - 1);
            if (s0 > e0) continue;
            outRangeStarts0.push_back(s0);
            outRangeEnds0.push_back(e0);
        }
        if (outRangeStarts0.isEmpty()) {
            QVector<int> starts0 = buildUniformRangeStarts0(n1, rangeCount);
            for (int i = 0; i < starts0.size(); ++i) {
                const int s0 = starts0[i];
                int e0 = (i + 1 < starts0.size()) ? (starts0[i + 1] - 1) : (n1 - 1);
                if (e0 < s0) e0 = s0;
                outRangeStarts0.push_back(s0);
                outRangeEnds0.push_back(e0);
                outPromPerRange.push_back(minProm);
            }
            return;
        }
        const QVariantList promVar = params.value(QStringLiteral("range_prominences")).toList();
        for (int i = 0; i < outRangeStarts0.size(); ++i) {
            const double p = (i < promVar.size()) ? promVar[i].toDouble() : minProm;
            outPromPerRange.push_back(p);
        }
        return;
    }

    const QVector<int> starts0 = buildUniformRangeStarts0(n1, rangeCount);
    for (int i = 0; i < starts0.size(); ++i) {
        const int s0 = starts0[i];
        int e0 = (i + 1 < starts0.size()) ? (starts0[i + 1] - 1) : (n1 - 1);
        if (e0 < s0) e0 = s0;
        outRangeStarts0.push_back(s0);
        outRangeEnds0.push_back(e0);
        outPromPerRange.push_back(minProm);
    }
}

/** 峰检测 + 聚类分段，与 process 内逻辑一致 */
static bool computePeakSegSegments(
    const QVector<double>& chrom1,
    const QVector<double>& refSmooth,
    const QVariantMap& params,
    double minProm,
    int maxClusterGap,
    int n1,
    int rangeCount,
    QVector<int>& segStarts0,
    QVector<int>& segEnds0,
    QString& error)
{
    QVector<int> rangeStarts0;
    QVector<int> rangeEnds0;
    QVector<double> rangeProms;
    resolveMultiRanges(params, n1, minProm, rangeCount, rangeStarts0, rangeEnds0, rangeProms);

    QVector<int> allPeakLocs0;
    for (int ri = 0; ri < rangeStarts0.size(); ++ri) {
        const int rStart0 = qMax(0, rangeStarts0[ri]);
        const int rEnd0 = qMin(n1 - 1, rangeEnds0[ri]);
        if (rStart0 >= rEnd0) continue;

        QVector<double> seg;
        seg.reserve(rEnd0 - rStart0 + 1);
        double segMax = 0.0;
        for (int i = rStart0; i <= rEnd0; ++i) {
            seg.push_back(refSmooth[i]);
            segMax = qMax(segMax, refSmooth[i]);
        }

        const double prom_i = rangeProms.value(ri, minProm);
        double baseProm = prom_i;
        if (prom_i <= 1.0) {
            baseProm = prom_i * segMax;
            if (baseProm < std::numeric_limits<double>::epsilon()) baseProm = prom_i;
        }

        const QVector<Peak> pks = myFindPeaksProminence(seg, baseProm);
        for (const Peak& pk : pks) {
            const int loc0 = (pk.loc1 - 1) + rStart0;
            if (loc0 >= 0 && loc0 < n1) allPeakLocs0.push_back(loc0);
        }
    }

    std::sort(allPeakLocs0.begin(), allPeakLocs0.end());
    allPeakLocs0.erase(std::unique(allPeakLocs0.begin(), allPeakLocs0.end()), allPeakLocs0.end());

    segStarts0.clear();
    segEnds0.clear();

    if (allPeakLocs0.isEmpty()) {
        int step = int(qCeil(double(n1) / 20.0));
        if (step <= 0) step = n1;
        for (int s0 = 0; s0 < n1; s0 += step) segStarts0.push_back(s0);
        for (int i = 0; i < segStarts0.size(); ++i) {
            const int e0 = (i + 1 < segStarts0.size()) ? (segStarts0[i + 1] - 1) : (n1 - 1);
            segEnds0.push_back(e0);
        }
    } else {
        QVector<QVector<int>> clusters;
        QVector<int> current;
        current.push_back(allPeakLocs0[0]);
        for (int i = 1; i < allPeakLocs0.size(); ++i) {
            if (allPeakLocs0[i] - allPeakLocs0[i - 1] <= maxClusterGap) {
                current.push_back(allPeakLocs0[i]);
            } else {
                clusters.push_back(current);
                current.clear();
                current.push_back(allPeakLocs0[i]);
            }
        }
        clusters.push_back(current);

        QVector<int> centers0;
        centers0.reserve(clusters.size());
        for (const auto& c : clusters) {
            if (c.isEmpty()) continue;
            QVector<int> sorted = c;
            std::sort(sorted.begin(), sorted.end());
            double med = 0.0;
            const int mid = sorted.size() / 2;
            if (sorted.size() % 2 == 1) med = sorted[mid];
            else med = 0.5 * (sorted[mid - 1] + sorted[mid]);
            centers0.push_back(int(qRound(med)));
        }
        std::sort(centers0.begin(), centers0.end());

        QVector<int> ends0;
        for (int k = 0; k < centers0.size() - 1; ++k) {
            const int left0 = centers0[k];
            const int right0 = centers0[k + 1];
            int split0 = left0;
            if (right0 - left0 <= 1) {
                split0 = left0;
            } else {
                int bestIdx = left0;
                double bestVal = refSmooth[left0];
                for (int i = left0; i <= right0; ++i) {
                    if (refSmooth[i] < bestVal) {
                        bestVal = refSmooth[i];
                        bestIdx = i;
                    }
                }
                split0 = bestIdx;
            }
            ends0.push_back(split0);
        }

        segStarts0.push_back(0);
        for (int e : ends0) segStarts0.push_back(qMin(n1 - 1, e + 1));

        segEnds0 = ends0;
        segEnds0.push_back(n1 - 1);

        QVector<int> cleanStarts, cleanEnds;
        for (int i = 0; i < segStarts0.size(); ++i) {
            int s0 = segStarts0[i];
            int e0 = segEnds0.value(i, n1 - 1);
            if (i > 0 && s0 <= cleanStarts.last()) continue;
            if (e0 < s0) e0 = s0;
            cleanStarts.push_back(s0);
            cleanEnds.push_back(e0);
        }
        segStarts0 = cleanStarts;
        segEnds0 = cleanEnds;
    }

    if (segStarts0.isEmpty()) {
        error = QCoreApplication::translate("PeakSegCOWAlignment", "分段失败，无法执行对齐");
        return false;
    }
    return true;
}

} // namespace

QString PeakSegCOWAlignment::stepName() const
{
    return "peakseg_cow_alignment";
}

QString PeakSegCOWAlignment::userVisibleName() const
{
    return QObject::tr("PeakSeg-COW峰对齐");
}

QVariantMap PeakSegCOWAlignment::defaultParameters() const
{
    QVariantMap p;
    p["min_prominence"] = 0.05;
    p["max_cluster_gap"] = 5;
    p["t"] = 50;
    p["smooth_span"] = 5;
    p["range_count"] = 3; // 未提供 ranges 时的默认均分段数
    p["ranges"] = QVariantList(); // 非空时：每元素为 [start1,end1]（1-based 闭区间）
    p["range_prominences"] = QVariantList(); // 与 ranges 行数对应；可省略则用 min_prominence
    return p;
}

QVector<int> PeakSegCOWAlignment::referenceSegmentStarts1Based(const Curve* ref,
                                                               const QVariantMap& params,
                                                               QString& error)
{
    QVector<int> starts1;
    if (!ref) {
        error = QCoreApplication::translate("PeakSegCOWAlignment", "参考曲线为空");
        return starts1;
    }
    const QVector<QPointF> refData = ref->data();
    QVector<double> chrom1;
    chrom1.reserve(refData.size());
    for (const auto& p : refData) chrom1.append(p.y());
    const int n1 = chrom1.size();
    if (n1 < 2) {
        error = QCoreApplication::translate("PeakSegCOWAlignment", "参考曲线数据点过少");
        return starts1;
    }
    const double minProm = params.value(QStringLiteral("min_prominence"), 0.05).toDouble();
    const int maxClusterGap = params.value(QStringLiteral("max_cluster_gap"), 5).toInt();
    const int smoothSpan = qMax(1, params.value(QStringLiteral("smooth_span"), 5).toInt());
    const int rangeCount = qMax(1, params.value(QStringLiteral("range_count"), 3).toInt());
    const QVector<double> refSmooth = movmean(chrom1, smoothSpan);
    QVector<int> segStarts0;
    QVector<int> segEnds0;
    if (!computePeakSegSegments(chrom1, refSmooth, params, minProm, maxClusterGap, n1, rangeCount, segStarts0, segEnds0, error))
        return starts1;
    for (int s0 : segStarts0) starts1.append(s0 + 1);
    return starts1;
}

ProcessingResult PeakSegCOWAlignment::process(const QList<Curve*>& inputCurves,
                                             const QVariantMap& params,
                                             QString& error)
{
    ProcessingResult result;
    if (inputCurves.size() < 2) {
        error = QObject::tr("PeakSeg-COW峰对齐需要至少两条输入曲线（参考 + 待对齐）");
        return result;
    }
    const Curve* ref = inputCurves[0];
    const Curve* tgt = inputCurves[1];
    if (!ref || !tgt) {
        error = QObject::tr("输入曲线为空，无法执行对齐");
        return result;
    }

    QVector<QPointF> refData = ref->data();
    QVector<QPointF> tgtData = tgt->data();
    if (refData.isEmpty() || tgtData.isEmpty()) {
        error = QObject::tr("参考或目标曲线数据为空");
        return result;
    }

    QVector<double> refX, chrom1, chrom2;
    refX.reserve(refData.size());
    chrom1.reserve(refData.size());
    for (const auto& p : refData) { refX.append(p.x()); chrom1.append(p.y()); }
    chrom2.reserve(tgtData.size());
    for (const auto& p : tgtData) { chrom2.append(p.y()); }

    const int n1 = chrom1.size();
    const int n2 = chrom2.size();
    if (n1 < 2 || n2 < 2) {
        error = QObject::tr("数据点过少，无法执行 PeakSeg-COW 对齐");
        return result;
    }

    // DP 内层每步原分配 QVector 并重采样；复用缓冲区避免海量小分配（主要性能来源）
    QVector<double> warpWork;
    warpWork.resize(n1);

    // 参数（与 MATLAB opts 对齐）
    const double minProm = params.value("min_prominence", 0.05).toDouble();
    const int maxClusterGap = params.value("max_cluster_gap", 5).toInt();
    const int tWarp = qMax(0, params.value("t", 50).toInt());
    const int smoothSpan = qMax(1, params.value("smooth_span", 5).toInt());
    int rangeCount = qMax(1, params.value("range_count", 3).toInt());

    // 参考平滑
    const QVector<double> refSmooth = movmean(chrom1, smoothSpan);

    QVector<int> segStarts0;
    QVector<int> segEnds0;
    if (!computePeakSegSegments(chrom1, refSmooth, params, minProm, maxClusterGap, n1, rangeCount, segStarts0, segEnds0, error))
        return result;

    const int m = segStarts0.size();
    if (m <= 0) {
        error = QCoreApplication::translate("PeakSegCOWAlignment", "分段失败，无法执行对齐");
        return result;
    }

    QVector<int> segLen;
    segLen.reserve(m);
    for (int i = 0; i < m; ++i) segLen.push_back(segEnds0[i] - segStarts0[i] + 1);

    // === DP（严格复刻 MATLAB 的 1-based j/k 语义）===
    const double INF = std::numeric_limits<double>::infinity();
    QVector<QVector<double>> DP(m, QVector<double>(n2, INF));
    QVector<QVector<int>> path(m, QVector<int>(n2, -1)); // 存 1-based k

    // 7.1 第一段初始化
    int firstLen = segLen[0];
    int jmin = qMax(firstLen - tWarp, 1);
    int jmax = qMin(firstLen + tWarp, n2);

    QVector<double> refSeg0;
    refSeg0.reserve(firstLen);
    for (int i = segStarts0[0]; i <= segEnds0[0]; ++i) refSeg0.push_back(chrom1[i]);

    for (int j = jmin; j <= jmax; ++j) {
        linearResampleInPlace(chrom2.constData(), j, warpWork.data(), firstLen);
        const double r = myCorrPtr(refSeg0.constData(), warpWork.constData(), firstLen);
        double cost = 1.0 - r;
        if (!qIsFinite(cost)) continue;
        DP[0][j - 1] = cost;
        path[0][j - 1] = 1; // MATLAB 里 path(1,j)=1
    }

    // 7.2 主循环
    int prefixSum = segLen[0];
    for (int iSeg = 2; iSeg <= m; ++iSeg) { // 1-based
        prefixSum += segLen[iSeg - 1];

        int refStart0 = segStarts0[iSeg - 1];
        int refEnd0 = segEnds0[iSeg - 1];
        int currLen = refEnd0 - refStart0 + 1;

        QVector<double> refSeg;
        refSeg.reserve(currLen);
        for (int i = refStart0; i <= refEnd0; ++i) refSeg.push_back(chrom1[i]);

        int theoreticalSum = 0;
        for (int k = 0; k < iSeg; ++k) theoreticalSum += segLen[k];
        int jLo = qMax(1, theoreticalSum - iSeg * tWarp);
        int jHi = qMin(n2, theoreticalSum + iSeg * tWarp);

        for (int j = jLo; j <= jHi; ++j) {
            int minK = qMax(1, j - currLen - tWarp);
            int maxK = qMin(n2, j - currLen + tWarp);

            double bestCost = INF;
            int bestK = -1;

            for (int k = minK; k <= maxK; ++k) {
                if (k >= j) continue;
                double prev = DP[iSeg - 2][k - 1];
                if (!qIsFinite(prev)) continue;

                int s0 = k;      // MATLAB: k+1, so 1-based start = k+1 => 0-based = k
                int e0 = j - 1;  // inclusive 0-based
                if (s0 < 0) s0 = 0;
                if (e0 >= n2) e0 = n2 - 1;
                if (s0 > e0) continue;

                const int segLenTgt = e0 - s0 + 1;
                linearResampleInPlace(chrom2.constData() + s0, segLenTgt, warpWork.data(), currLen);
                const double r = myCorrPtr(refSeg.constData(), warpWork.constData(), currLen);
                double cost = 1.0 - r;
                if (!qIsFinite(cost)) continue;
                double total = prev + cost;
                if (total < bestCost) {
                    bestCost = total;
                    bestK = k;
                }
            }

            if (qIsFinite(bestCost) && bestK >= 1) {
                DP[iSeg - 1][j - 1] = bestCost;
                path[iSeg - 1][j - 1] = bestK; // 1-based
            }
        }
    }

    // 8) 回溯 bounds（1-based）
    QVector<int> bounds1;
    bounds1.resize(m);
    int endPos1 = 1;
    double best = INF;
    for (int j = 1; j <= n2; ++j) {
        double v = DP[m - 1][j - 1];
        if (qIsFinite(v) && v < best) { best = v; endPos1 = j; }
    }
    bounds1[m - 1] = endPos1;
    for (int i = m - 2; i >= 0; --i) {
        int nextJ1 = bounds1[i + 1];
        int k1 = (nextJ1 >= 1 && nextJ1 <= n2) ? path[i + 1][nextJ1 - 1] : -1;
        if (k1 < 1) {
            // 无法回溯时，退化为“理论长度前缀”（尽量不崩溃）
            int tsum = 0;
            for (int t = 0; t <= i; ++t) tsum += segLen[t];
            k1 = qMin(qMax(1, tsum), n2);
        }
        bounds1[i] = k1;
    }

    // 9) 根据 bounds 重采样拼回 aligned（长度 n1）
    QVector<double> aligned;
    aligned.resize(n1);
    std::fill(aligned.begin(), aligned.end(), 0.0);

    int prevEnd1 = 0;
    for (int i = 0; i < m; ++i) {
        int startIdx1 = prevEnd1 + 1;
        int endIdx1 = bounds1[i];
        int refStart0 = segStarts0[i];
        int refEnd0 = segEnds0[i];
        int targetLen = refEnd0 - refStart0 + 1;

        if (endIdx1 >= startIdx1 && startIdx1 <= n2) {
            const int s0 = startIdx1 - 1;
            const int e0 = qMin(endIdx1, n2) - 1;
            const int segLenOrig = e0 - s0 + 1;
            if (i == m - 1 && segLenOrig == targetLen) {
                for (int u = 0; u < targetLen; ++u)
                    aligned[refStart0 + u] = chrom2[s0 + u];
            } else {
                linearResampleInPlace(chrom2.constData() + s0, segLenOrig, warpWork.data(), targetLen);
                for (int u = 0; u < targetLen; ++u)
                    aligned[refStart0 + u] = warpWork[u];
            }
        } else {
            // 保持为 0
        }

        prevEnd1 = endIdx1;
    }

    // 输出曲线：对齐到参考 x
    QVector<QPointF> alignedPoints;
    alignedPoints.reserve(n1);
    for (int i = 0; i < n1; ++i) alignedPoints.push_back(QPointF(refX[i], aligned[i]));

    Curve* alignedCurve = new Curve(alignedPoints, tgt->name() + QObject::tr(" (PeakSeg-COW对齐到参考)"));
    alignedCurve->setSampleId(tgt->sampleId());
    result.namedCurves["aligned"].append(alignedCurve);

    QVariantList metaStarts;
    for (int s0 : segStarts0) metaStarts.append(s0 + 1);
    result.metadata.insert(QStringLiteral("segment_starts_1based"), metaStarts);
    result.metadata.insert(QStringLiteral("segment_count"), m);
    return result;
}

