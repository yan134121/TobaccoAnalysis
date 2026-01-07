#include "CsvParser.h"
#include <QDebug>
#include <QStringList>
#include <QFile>
#include <QTextStream>
#include "Logger.h"


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
    while (!in.atEnd()) {
        QString line = in.readLine();
        currentLine++;

        // 跳过起始数据行之前的行
        if (currentLine < startDataRow) {
            continue;
        }

        if (line.trimmed().isEmpty()) continue; // 跳过空行

        // QStringList values = line.split(',', Qt::SkipEmptyParts);
        QStringList values = line.split(',', Qt::KeepEmptyParts);
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
