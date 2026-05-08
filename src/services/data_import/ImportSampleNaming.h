#ifndef IMPORT_SAMPLE_NAMING_H
#define IMPORT_SAMPLE_NAMING_H

#include <QString>
#include <QDate>

namespace ImportSampleNaming {

/** 导入时生成的样本显示名：批次-短码-平行号-YYYYMMDD（不含烟牌号） */
inline QString makeImportedSampleName(const QString& batchCode,
                                      const QString& shortCode,
                                      int parallelNo,
                                      const QDate& detectDate)
{
    const QDate d = detectDate.isValid() ? detectDate : QDate::currentDate();
    return QStringLiteral("%1-%2-%3-%4")
        .arg(batchCode)
        .arg(shortCode)
        .arg(parallelNo)
        .arg(d.toString(QStringLiteral("yyyyMMdd")));
}

} // namespace ImportSampleNaming

#endif
