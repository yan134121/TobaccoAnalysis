#ifndef CSVPARSER_H
#define CSVPARSER_H

#include "AbstractFileParser.h" // 继承自抽象基类
#include <QFile>
#include <QTextStream>

class CsvParser : public AbstractFileParser
{
    Q_OBJECT
public:
    explicit CsvParser(QObject *parent = nullptr) : AbstractFileParser(parent) {}
    ~CsvParser() override = default;

    // QList<QVariantMap> parseFile(const QString& filePath, QString& errorMessage) override;
    QList<QVariantList> parseFile(const QString& filePath, QString& errorMessage, int startDataRow = 1, int startDataCol = 1) override; // <-- 添加参数

private:
    // 辅助函数，如果需要
    // QStringList parseCsvLine(const QString& line);
};

#endif // CSVPARSER_H