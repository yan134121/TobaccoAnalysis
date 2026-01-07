#ifndef XLSXPARSER_H
#define XLSXPARSER_H

#include "AbstractFileParser.h"
#include <QDebug>
// #include <QXlsx/xlsxdocument.h> // 引入 QXlsx 库
#include "xlsxdocument.h"
#include "xlsxformat.h"
#include "common.h"

class XlsxParser : public AbstractFileParser
{
    Q_OBJECT
public:
    explicit XlsxParser(QObject *parent = nullptr) : AbstractFileParser(parent) {}
    ~XlsxParser() override = default;

    // QList<QVariantMap> parseFile(const QString& filePath, QString& errorMessage) override;
    QList<QVariantList> parseFile(const QString& filePath, QString& errorMessage, int startDataRow = 1, int startDataCol = 1) override;

    // QMap<QString, QList<QVariantMap>> parseMultiSheetFile(const QString& filePath, QString& errorMessage); // <-- 确保有此声明
     QMap<QString, QList<QVariantMap>> parseMultiSheetFile(const QString& filePath, QString& errorMessage, int startDataRow = 1);

private:
    // 辅助函数，用于从单个 Worksheet 读取数据
    QList<QVariantList> readWorksheet(QXlsx::Worksheet* worksheet, int startDataRow, int startDataCol, QString& errorMessage);

    QList<QVariantMap> readWorksheetWithHeaders(QXlsx::Worksheet* worksheet, int startDataRow, QString& errorMessage);
};

#endif // XLSXPARSER_H
