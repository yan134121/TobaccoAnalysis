// src/util/CsvWriter.cpp
#include "CsvWriter.h"
#include <QDebug>
#include <QMapIterator>
#include <QTextCodec> // 用于设置文件编码
#include "Logger.h"

bool CsvWriter::writeToFile(const QString& filePath,
                            const QList<QVariantMap>& dataList,
                            QString& errorMessage,                       // <-- 同步调整
                            const QStringList& headers) // <-- 同步调整
{
    QFile file(filePath);
    // Truncate 模式会在文件存在时清空文件内容
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        errorMessage = QString("无法创建或打开文件进行写入: %1 - %2").arg(filePath, file.errorString());
        WARNING_LOG << errorMessage;
        return false;
    }

    QTextStream out(&file);
    out.setCodec(QTextCodec::codecForName("UTF-8")); // 显式设置为 UTF-8 编码

    QStringList actualHeaders = headers;
    if (actualHeaders.isEmpty() && !dataList.isEmpty()) {
        // 如果没有提供表头，从第一条数据中提取键作为表头
        actualHeaders = dataList.first().keys();
        DEBUG_LOG << "CsvWriter: 未指定表头，从数据中提取表头:" << actualHeaders;
    }

    // 写入表头
    if (!actualHeaders.isEmpty()) {
        out << actualHeaders.join(",") << "\n";
    } else if (dataList.isEmpty()) {
        DEBUG_LOG << "CsvWriter: 数据列表为空，无需写入表头。";
    }


    // 写入数据
    for (const QVariantMap& rowData : dataList) {
        QStringList rowValues;
        for (const QString& header : actualHeaders) {
            // 获取值并进行 CSV 转义
            QString value = rowData.value(header).toString();
            // 简单的 CSV 转义：如果值包含逗号、双引号或换行符，则用双引号包裹，并把内部的双引号替换为两个双引号
            if (value.contains(',') || value.contains('"') || value.contains('\n')) {
                value.replace('"', "\"\""); // 双引号替换为两个双引号
                value = "\"" + value + "\""; // 用双引号包裹整个值
            }
            rowValues << value;
        }
        out << rowValues.join(",") << "\n";
    }

    file.close();
    errorMessage.clear();
    DEBUG_LOG << "成功导出数据到CSV文件:" << filePath << "，共" << dataList.size() << "条记录。";
    return true;
}