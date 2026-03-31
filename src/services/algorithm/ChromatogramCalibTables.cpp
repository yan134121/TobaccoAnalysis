#include "ChromatogramCalibTables.h"

#include "third_party/QXlsx/header/xlsxdocument.h"

#include <QCoreApplication>
#include <QFile>
#include <QtMath>
#include <algorithm>
#include <limits>

namespace {

static QString normalizeKey(const QVariant& v)
{
    if (v.isNull() || !v.isValid())
        return QString();
    if (v.type() == QVariant::Double) {
        double d = v.toDouble();
        if (qIsNaN(d) || qIsInf(d))
            return QString();
        if (d == std::floor(d))
            return QString::number(static_cast<qint64>(d));
        return QString::number(d, 'g', 12);
    }
    QString s = v.toString().trimmed();
    return s;
}

} // namespace

namespace ChromatogramCalibTables {

bool loadSampleToCalibMapWide(const QString& xlsxPath, QMap<QString, QString>& sampleKeyToCalibKey, QString& error)
{
    sampleKeyToCalibKey.clear();
    if (xlsxPath.trimmed().isEmpty()) {
        error = QCoreApplication::translate("ChromatogramCalibTables", "标定映射表路径为空");
        return false;
    }
    QXlsx::Document doc(xlsxPath);
    if (!doc.load()) {
        error = QCoreApplication::translate("ChromatogramCalibTables", "无法加载 Excel：%1").arg(xlsxPath);
        return false;
    }
    const QXlsx::CellRange dim = doc.dimension();
    const int lastRow = dim.lastRow();
    const int lastCol = dim.lastColumn();
    if (lastRow < 1 || lastCol < 1) {
        error = QCoreApplication::translate("ChromatogramCalibTables", "标定映射表为空");
        return false;
    }

    for (int c = 1; c <= lastCol; ++c) {
        const QString calibKey = normalizeKey(doc.read(1, c));
        if (calibKey.isEmpty())
            continue;
        sampleKeyToCalibKey.insert(calibKey, calibKey);
        for (int r = 2; r <= lastRow; ++r) {
            const QString follower = normalizeKey(doc.read(r, c));
            if (follower.isEmpty())
                continue;
            sampleKeyToCalibKey.insert(follower, calibKey);
        }
    }
    return !sampleKeyToCalibKey.isEmpty();
}

bool loadCalibPairwiseCosine(const QString& xlsxPath,
                             QMap<QPair<QString, QString>, double>& outUndirected,
                             QString& error)
{
    outUndirected.clear();
    if (xlsxPath.trimmed().isEmpty()) {
        error = QCoreApplication::translate("ChromatogramCalibTables", "标定 cosine 表路径为空");
        return false;
    }
    QXlsx::Document doc(xlsxPath);
    if (!doc.load()) {
        error = QCoreApplication::translate("ChromatogramCalibTables", "无法加载 Excel：%1").arg(xlsxPath);
        return false;
    }
    const QXlsx::CellRange dim = doc.dimension();
    const int lastRow = dim.lastRow();
    const int lastCol = dim.lastColumn();
    int colRef = -1, colCmp = -1, colCos = -1;
    for (int c = 1; c <= lastCol; ++c) {
        const QString h = normalizeKey(doc.read(1, c)).toLower();
        if (h == QStringLiteral("refsample") || h == QStringLiteral("ref_sample"))
            colRef = c;
        else if (h == QStringLiteral("cmpsample") || h == QStringLiteral("cmp_sample"))
            colCmp = c;
        else if (h == QStringLiteral("cosine") || h == QStringLiteral("calib_cosine"))
            colCos = c;
    }
    if (colRef < 1 || colCmp < 1 || colCos < 1) {
        error = QCoreApplication::translate("ChromatogramCalibTables", "标定 cosine 表缺少列 RefSample / CmpSample / cosine");
        return false;
    }
    for (int r = 2; r <= lastRow; ++r) {
        const QString a = normalizeKey(doc.read(r, colRef));
        const QString b = normalizeKey(doc.read(r, colCmp));
        if (a.isEmpty() || b.isEmpty())
            continue;
        bool ok = false;
        const double cosv = doc.read(r, colCos).toDouble(&ok);
        if (!ok || qIsNaN(cosv))
            continue;
        outUndirected.insert(qMakePair(a, b), cosv);
        outUndirected.insert(qMakePair(b, a), cosv);
    }
    return true;
}

double calibCosineForPair(const QMap<QString, QString>& sampleToCalib,
                          const QMap<QPair<QString, QString>, double>& pairCosine,
                          const QString& refKey,
                          const QString& cmpKey)
{
    const QString rc = sampleToCalib.value(refKey, refKey);
    const QString cc = sampleToCalib.value(cmpKey, cmpKey);
    if (rc == cc)
        return 0.0;
    const double v = pairCosine.value(qMakePair(rc, cc), std::numeric_limits<double>::quiet_NaN());
    return v;
}

} // namespace ChromatogramCalibTables

namespace ChromatogramSegmentStartsIo {

QVector<int> loadSegmentStartsOneBasedFromTextFile(const QString& path, QString& error)
{
    QVector<int> out;
    error.clear();
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        error = QCoreApplication::translate("ChromatogramSegmentStartsIo", "无法打开分段文件：%1").arg(path);
        return out;
    }
    while (!f.atEnd()) {
        const QByteArray line = f.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#'))
            continue;
        bool ok = false;
        const int v = line.toInt(&ok);
        if (ok && v >= 1)
            out.push_back(v);
    }
    f.close();
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    if (out.isEmpty())
        error = QCoreApplication::translate("ChromatogramSegmentStartsIo", "分段文件未解析到有效起点（每行一个 1-based 整数）");
    return out;
}

} // namespace ChromatogramSegmentStartsIo
