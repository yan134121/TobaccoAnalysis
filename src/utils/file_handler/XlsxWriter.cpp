// src/util/XlsxWriter.cpp
#include "XlsxWriter.h"
#include "XlsxWriter.h"
#include <QDebug>
#include <QMapIterator> // 用于遍历QVariantMap的键
#include "Logger.h"

bool XlsxWriter::writeToFile(const QString& filePath,
                            const QList<QVariantMap>& dataList,
                            QString& errorMessage,                       // <-- 同步调整
                            const QStringList& headers) // <-- 同步调整
{
    QXlsx::Document xlsx; // 创建一个新的 XLSX 文档

    // 获取或生成实际表头
    QStringList actualHeaders = headers;
    if (actualHeaders.isEmpty() && !dataList.isEmpty()) {
        actualHeaders = dataList.first().keys();
        DEBUG_LOG << "XlsxWriter: 未指定表头，从数据中提取表头:" << actualHeaders;
    }

    // 写入表头 (第1行)
    if (!actualHeaders.isEmpty()) {
        for (int col = 0; col < actualHeaders.size(); ++col) {
            xlsx.write(1, col + 1, actualHeaders.at(col)); // QXlsx 行和列都是从 1 开始
        }
    } else if (dataList.isEmpty()) {
        DEBUG_LOG << "XlsxWriter: 数据列表为空，无需写入表头。";
    }

    // 写入数据 (从第2行开始)
    int currentRow = 2;
    for (const QVariantMap& rowData : dataList) {
        for (int col = 0; col < actualHeaders.size(); ++col) {
            QString header = actualHeaders.at(col);
            xlsx.write(currentRow, col + 1, rowData.value(header)); // 写入对应列的值
        }
        currentRow++;
    }

    if (!xlsx.saveAs(filePath)) {
        errorMessage = QString("无法保存XLSX文件: %1").arg(filePath);
        WARNING_LOG << errorMessage;
        return false;
    }

    errorMessage.clear();
    DEBUG_LOG << "成功导出数据到XLSX文件:" << filePath << "，共" << dataList.size() << "条记录。";
    return true;
}
