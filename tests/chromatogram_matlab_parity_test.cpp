/**
 * PeakSeg-COW / 微调 / 差异度 自洽与回归校验（不依赖 MATLAB 运行时）。
 * 构建：见 tests/CMakeLists.txt
 */
#include <QCoreApplication>
#include <QtMath>
#include <cmath>
#include <cstdio>

#include "core/entities/Curve.h"
#include "services/algorithm/ChromatogramSegmentFinetune.h"
#include "services/algorithm/ChromatogramDifferenceMetrics.h"
#include "services/algorithm/PeakSegCOWAlignment.h"
#include "services/algorithm/processing/IProcessingStep.h"

static double pearsonCorr(const QVector<double>& a, const QVector<double>& b)
{
    if (a.size() != b.size() || a.isEmpty())
        return 0.0;
    double mx = 0, my = 0;
    for (int i = 0; i < a.size(); ++i) {
        mx += a[i];
        my += b[i];
    }
    mx /= a.size();
    my /= b.size();
    double num = 0, dx = 0, dy = 0;
    for (int i = 0; i < a.size(); ++i) {
        const double vx = a[i] - mx;
        const double vy = b[i] - my;
        num += vx * vy;
        dx += vx * vx;
        dy += vy * vy;
    }
    if (dx <= 0 || dy <= 0)
        return 0.0;
    return num / std::sqrt(dx * dy);
}

static double rmse(const QVector<double>& a, const QVector<double>& b)
{
    if (a.size() != b.size() || a.isEmpty())
        return 1e100;
    double s = 0;
    for (int i = 0; i < a.size(); ++i) {
        const double d = a[i] - b[i];
        s += d * d;
    }
    return std::sqrt(s / a.size());
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    const int n = 256;
    QVector<double> x, yRef, yTgt;
    x.reserve(n);
    yRef.reserve(n);
    yTgt.reserve(n);
    for (int i = 0; i < n; ++i) {
        const double t = double(i);
        x.append(t);
        yRef.append(std::sin(t * 0.02) + 0.1 * std::sin(t * 0.15));
        yTgt.append(yRef[i]); // 与参考相同 → 对齐后应高度一致
    }

    Curve ref(x, yRef, QStringLiteral("ref"));
    Curve tgt(x, yTgt, QStringLiteral("tgt"));

    PeakSegCOWAlignment aligner;
    QVariantMap p;
    p.insert(QStringLiteral("min_prominence"), 0.05);
    p.insert(QStringLiteral("max_cluster_gap"), 5);
    p.insert(QStringLiteral("t"), 35);
    p.insert(QStringLiteral("smooth_span"), 5);
    p.insert(QStringLiteral("range_count"), 4);

    QString err;
    ProcessingResult pr = aligner.process({&ref, &tgt}, p, err);
    if (!err.isEmpty() || !pr.namedCurves.contains(QStringLiteral("aligned"))
        || pr.namedCurves[QStringLiteral("aligned")].isEmpty()) {
        std::fprintf(stderr, "FAIL: PeakSeg-COW %s\n", qPrintable(err));
        return 1;
    }
    Curve* al = pr.namedCurves[QStringLiteral("aligned")].first();
    QVector<double> ya;
    for (const QPointF& pt : al->data())
        ya.append(pt.y());

    const double r = pearsonCorr(yRef, ya);
    const double e = rmse(yRef, ya);
    if (r < 0.99 || e > 0.15) {
        std::fprintf(stderr, "FAIL: identity PeakSeg sanity r=%.6f rmse=%.6f\n", r, e);
        return 2;
    }

    QVector<int> segStarts = PeakSegCOWAlignment::referenceSegmentStarts1Based(&ref, p, err);
    if (segStarts.isEmpty()) {
        std::fprintf(stderr, "FAIL: segment starts %s\n", qPrintable(err));
        return 3;
    }
    ChromatogramFinetuneResult ft = finetuneSegmentBoundaries(x, ya, segStarts, 3);
    if (ft.segmentAreasFinetuned.isEmpty()) {
        std::fprintf(stderr, "FAIL: finetune produced no areas\n");
        return 4;
    }

    QVector<double> a1 = ft.segmentAreasFinetuned;
    QVector<double> a2 = a1;
    for (double& v : a2)
        v *= 1.02;
    const double cd = ChromatogramDifferenceMetrics::cosineDifference(a1, a2);
    if (cd < 1e-6 || cd > 1.0) {
        std::fprintf(stderr, "FAIL: cosineDifference unexpected %g\n", cd);
        return 5;
    }

    QVector<int> ids{1, 2};
    QVector<QVector<double>> rows{a1, a2};
    QVariantList tbl = ChromatogramDifferenceMetrics::pairwiseDifferenceTable(ids, rows, 15.0, {});
    if (tbl.isEmpty()) {
        std::fprintf(stderr, "FAIL: pairwise table empty\n");
        return 6;
    }

    std::printf("OK chromatogram_matlab_parity_test PeakSeg r=%.6f rmse=%.6f segs=%d pairs=%d\n",
                r, e, ft.segStartsFinetuned1Based.size(), tbl.size());
    return 0;
}
