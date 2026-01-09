#include "CsvParser.h"
#include <QDebug>
#include <QStringList>
#include <QFile>
#include <QTextStream>
#include "Logger.h"

namespace {
QChar detectDelimiter(const QString& line)
{
    const int commaCount = line.count(QLatin1Char(','));
    const int semicolonCount = line.count(QLatin1Char(';'));
    const int tabCount = line.count(QLatin1Char('\t'));

    if (semicolonCount >= commaCount && semicolonCount >= tabCount && semicolonCount > 0) {
        return QLatin1Char(';');
    }

    if (tabCount > commaCount && tabCount >= semicolonCount) {
        return QLatin1Char('\t');
    }

    return QLatin1Char(',');
}

QStringList splitCsvLine(const QString& line, QChar delimiter)
{
    QStringList values;
    QString current;
    bool inQuotes = false;

    for (int i = 0; i < line.size(); ++i) {
        const QChar ch = line.at(i);
        if (ch == QLatin1Char('"')) {
            if (inQuotes && i + 1 < line.size() && line.at(i + 1) == QLatin1Char('"')) {
                current.append(QLatin1Char('"'));
                ++i;
            } else {
                inQuotes = !inQuotes;
            }
            continue;
        }

        if (!inQuotes && ch == delimiter) {
            values.append(current.trimmed());
            current.clear();
            continue;
        }

        current.append(ch);
    }

    values.append(current.trimmed());
    return values;
}
}


QList<QVariantList> CsvParser::parseFile(const QString& filePath, QString& errorMessage, int startDataRow, int startDataCol) // <-- 添加参数
{
    QList<QVariantList> dataList;
    QFile file(filePath);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        errorMessage = QString("无法打开文件: %1 - %2").arg(filePath, file.errorString());
        WARNING_LOG << errorMessage;
        return dataList;
    }

    QTextStream in(&file);
    in.setCodec("UTF-8");

    int currentLine = 0; // 跟踪当前读取的行号
    QChar delimiter = QLatin1Char(',');
    bool delimiterDetected = false;
    while (!in.atEnd()) {
        QString line = in.readLine();
        currentLine++;

        if (currentLine == 1 && line.startsWith(QChar(0xFEFF))) {
            line.remove(0, 1);
        }

        if (!delimiterDetected && !line.trimmed().isEmpty()) {
            delimiter = detectDelimiter(line);
            delimiterDetected = true;
        }

        // 跳过起始数据行之前的行
        if (currentLine < startDataRow) {
            continue;
        }

        if (line.trimmed().isEmpty()) continue; // 跳过空行

        QStringList values = splitCsvLine(line, delimiter);
        QVariantList rowData;

        // --- 关键修改：直接将列数据添加到 QVariantList 中，不使用 headers 作为索引 ---
        // 从起始列开始收集数据
        // 确保 startDataCol 是有效的（1-based），并且转换为 0-based 索引
        int actualStartColIndex = startDataCol - 1;
        if (actualStartColIndex < 0) actualStartColIndex = 0; // 防止 startDataCol = 0 或负数

        for (int i = actualStartColIndex; i < values.size(); ++i) {
            rowData.append(values[i].trimmed());
        }
        // --- 错误修改结束 ---

        if (!rowData.isEmpty()) {
            dataList.append(rowData);
        }
    }

    file.close();
    errorMessage.clear();
    DEBUG_LOG << "成功解析CSV文件:" << filePath << "，从行" << startDataRow << "列" << startDataCol << "读取" << dataList.size() << "条记录。";
    return dataList;
}
