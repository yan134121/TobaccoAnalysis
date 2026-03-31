#include "ChromatogramDifferenceMetrics.h"

#include <QtMath>
#include <limits>

namespace ChromatogramDifferenceMetrics {

double cosineSimilarity(const QVector<double>& a, const QVector<double>& b)
{
    if (a.size() != b.size() || a.isEmpty()) return 0.0;
    double dot = 0.0;
    double na = 0.0;
    double nb = 0.0;
    for (int i = 0; i < a.size(); ++i) {
        dot += a[i] * b[i];
        na += a[i] * a[i];
        nb += b[i] * b[i];
    }
    if (na <= std::numeric_limits<double>::epsilon() || nb <= std::numeric_limits<double>::epsilon())
        return 0.0;
    return dot / (qSqrt(na) * qSqrt(nb));
}

double cosineDifference(const QVector<double>& a, const QVector<double>& b)
{
    return 1.0 - cosineSimilarity(a, b);
}

double meanSpc(const QVector<double>& a, const QVector<double>& b)
{
    if (a.size() != b.size() || a.isEmpty()) return 0.0;
    const double eps = std::numeric_limits<double>::epsilon();
    double sum = 0.0;
    const int n = a.size();
    for (int i = 0; i < n; ++i) {
        const double q = qAbs(a[i] - b[i]);
        double m = a[i] + b[i];
        if (m == 0.0) m = eps;
        sum += q / m;
    }
    return sum / double(n);
}

double newMetric(const QVector<double>& a, const QVector<double>& b)
{
    if (a.size() != b.size() || a.isEmpty()) return 0.0;
    const int n = a.size();
    double sumSq = 0.0;
    for (int i = 0; i < n; ++i) {
        if (qAbs(b[i]) < std::numeric_limits<double>::epsilon()) continue;
        const double q = a[i] / b[i];
        const double oneMinus = 1.0 - q;
        sumSq += oneMinus * oneMinus;
    }
    return qSqrt(sumSq / double(n));
}

double meanSpcCorrected(double meanSpc, double calibCosine, double gain)
{
    return meanSpc * (1.0 - gain * calibCosine);
}

QVariantList pairwiseDifferenceTable(const QVector<int>& sampleIds,
                                    const QVector<QVector<double>>& areaRowsFinetuned,
                                    double gain,
                                    const QVector<double>& calibCosinePerPair)
{
    QVariantList rows;
    const int numSamples = sampleIds.size();
    if (numSamples < 2 || areaRowsFinetuned.size() != numSamples) return rows;

    int pairIdx = 0; // 与 calibCosinePerPair 对齐时使用；缺省则全 0
    for (int i = 0; i < numSamples - 1; ++i) {
        for (int j = i + 1; j < numSamples; ++j) {
            const QVector<double>& x = areaRowsFinetuned[i];
            const QVector<double>& y = areaRowsFinetuned[j];
            if (x.size() != y.size()) continue;

            const double cosDiff = cosineDifference(x, y);
            const double mSpc = meanSpc(x, y);
            const double calibC = (pairIdx < calibCosinePerPair.size()) ? calibCosinePerPair[pairIdx] : 0.0;
            const double mSpcCorr = meanSpcCorrected(mSpc, calibC, gain);
            const double newM = newMetric(x, y);

            QVariantMap row;
            row.insert(QStringLiteral("pairId"), QStringLiteral("%1_vs_%2").arg(sampleIds[i]).arg(sampleIds[j]));
            row.insert(QStringLiteral("refSampleId"), sampleIds[i]);
            row.insert(QStringLiteral("cmpSampleId"), sampleIds[j]);
            row.insert(QStringLiteral("cosineDiff"), cosDiff);
            row.insert(QStringLiteral("meanSPC"), mSpc);
            row.insert(QStringLiteral("meanSPC_corrected"), mSpcCorr);
            row.insert(QStringLiteral("newMetric"), newM);
            row.insert(QStringLiteral("calibCosine"), calibC);
            rows.append(row);
            ++pairIdx;
        }
    }
    return rows;
}

} // namespace ChromatogramDifferenceMetrics
