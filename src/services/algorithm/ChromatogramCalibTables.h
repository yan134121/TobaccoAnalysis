#pragma once

#include <QMap>
#include <QString>
#include <QVector>

/**
 * MATLAB all_norm.m / calculate_all_difference.m 配套：标定样映射表与标定对 cosine 表（XLSX）。
 */
namespace ChromatogramCalibTables {

/** MATLAB 宽表：第 1 行为各列标定样编号，第 2 行起为该标定组后跟样。 */
bool loadSampleToCalibMapWide(const QString& xlsxPath, QMap<QString, QString>& sampleKeyToCalibKey, QString& error);

/**
 * 标定样两两 cosine 表：需含列 RefSample、CmpSample、cosine（列名不区分大小写）。
 * 存入对称键 (a,b) 与 (b,a)。
 */
bool loadCalibPairwiseCosine(const QString& xlsxPath,
                             QMap<QPair<QString, QString>, double>& outUndirected,
                             QString& error);

/** 与 MATLAB calculate_all_difference 一致：同标定组为 0，否则查表，缺失为 NaN。 */
double calibCosineForPair(const QMap<QString, QString>& sampleToCalib,
                          const QMap<QPair<QString, QString>, double>& pairCosine,
                          const QString& refKey,
                          const QString& cmpKey);

} // namespace ChromatogramCalibTables

namespace ChromatogramSegmentStartsIo {

/** 文本文件：每行一个分段起点索引（1-based），与 PeakSeg 输出顺序一致。 */
QVector<int> loadSegmentStartsOneBasedFromTextFile(const QString& path, QString& error);

} // namespace ChromatogramSegmentStartsIo
