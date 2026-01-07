// src/util/XlsxParser.cpp
#include "XlsxParser.h"
#include "XlsxParser.h"
#include <QVariant>
#include "Logger.h"

// parseFile (覆盖父类，现在调用 readWorksheet 并传入 startDataCol)
QList<QVariantList> XlsxParser::parseFile(const QString& filePath, QString& errorMessage, int startDataRow, int startDataCol)
{
    DEBUG_LOG << "开始解析XLSX文件:" << filePath;
    QXlsx::Document xlsx(filePath);
    if (!xlsx.load()) {
        errorMessage = QString("无法打开或加载XLSX文件: %1").arg(filePath);
        WARNING_LOG << errorMessage;
        return QList<QVariantList>();
    }
    // --- 关键修改：调用 readWorksheet 并传入 startDataCol ---
    return this->readWorksheet(xlsx.currentWorksheet(), startDataRow, startDataCol, errorMessage); // <-- 传入 startDataCol
    // --- 结束关键修改 ---
}



// QMap<QString, QList<QVariantMap>> XlsxParser::parseMultiSheetFile(const QString& filePath, QString& errorMessage)
// {
//     QMap<QString, QList<QVariantMap>> allSheetsData;
//     QXlsx::Document xlsx(filePath);

//     if (!xlsx.load()) {
//         errorMessage = QString("无法打开或加载XLSX文件: %1").arg(filePath);
//         WARNING_LOG << errorMessage;
//         return allSheetsData;
//     }

//     QStringList sheetNames = xlsx.sheetNames();
//     if (sheetNames.isEmpty()) {
//         errorMessage = "XLSX文件没有工作表。";
//         WARNING_LOG << errorMessage;
//         return allSheetsData;
//     }

//     errorMessage.clear();

//     for (const QString& sheetName : sheetNames) {
//         if (!xlsx.selectSheet(sheetName)) {
//             WARNING_LOG << "无法选择工作表:" << sheetName;
//             continue;
//         }
//         QXlsx::Worksheet *worksheet = xlsx.currentWorksheet();
//         if (worksheet) {
//             QString sheetErrorMessage;
//             // 对于 multi-sheet，这里假设每个 Sheet 的数据都从第 1 行开始 (或根据 sheetName 提供不同起始行)
//             // 如果需要每个 Sheet 不同起始行，需要修改此函数签名。
//             QList<QVariantMap> sheetData = this->readWorksheet(worksheet, 1, sheetErrorMessage); // <-- 假设 startDataRow = 1
//             if (!sheetData.isEmpty()) {
//                 allSheetsData[sheetName] = sheetData;
//                 DEBUG_LOG << "成功解析工作表:" << sheetName << "，读取" << sheetData.size() << "条记录。";
//             } else {
//                 WARNING_LOG << "工作表 '" << sheetName << "' 解析失败或为空:" << sheetErrorMessage;
//             }
//         }
//     }

//     if (allSheetsData.isEmpty()) {
//         errorMessage = "XLSX文件解析后没有有效数据。";
//     }
//     return allSheetsData;
// }


QMap<QString, QList<QVariantMap>> XlsxParser::parseMultiSheetFile(const QString& filePath, QString& errorMessage, int startDataRow) // <-- 接收 startDataRow
{
    QMap<QString, QList<QVariantMap>> allSheetsData;
    QXlsx::Document xlsx(filePath);

    if (!xlsx.load()) {
        errorMessage = QString("无法打开或加载XLSX文件: %1").arg(filePath);
        WARNING_LOG << errorMessage;
        return allSheetsData;
    }

    QStringList sheetNames = xlsx.sheetNames();
    if (sheetNames.isEmpty()) {
        errorMessage = "XLSX文件没有工作表。";
        WARNING_LOG << errorMessage;
        return allSheetsData;
    }

    errorMessage.clear();

    for (const QString& sheetName : sheetNames) {
        if (!xlsx.selectSheet(sheetName)) {
            WARNING_LOG << "无法选择工作表:" << sheetName;
            continue;
        }
        QXlsx::Worksheet *worksheet = xlsx.currentWorksheet();
        if (worksheet) {
            QString sheetErrorMessage;
            // 调用 readWorksheet，传入 startDataRow
            QList<QVariantMap> sheetData = this->readWorksheetWithHeaders(worksheet, startDataRow, sheetErrorMessage); // <-- 传入 startDataRow
            if (!sheetData.isEmpty()) {
                allSheetsData[sheetName] = sheetData;
                DEBUG_LOG << "成功解析工作表:" << sheetName << "，从行" << startDataRow << "读取" << sheetData.size() << "条记录。";
            } else {
                WARNING_LOG << "工作表 '" << sheetName << "' 解析失败或为空:" << sheetErrorMessage;
            }
        }
    }

    if (allSheetsData.isEmpty()) {
        errorMessage = "XLSX文件解析后没有有效数据。";
    }
    return allSheetsData;
}


QList<QVariantMap> XlsxParser::readWorksheetWithHeaders(QXlsx::Worksheet* worksheet, int startDataRow, QString& errorMessage)
{
    QList<QVariantMap> dataList;
    if (!worksheet) {
        errorMessage = "工作表无效。";
        return dataList;
    }

    int rowCount = worksheet->dimension().rowCount();
    int colCount = worksheet->dimension().columnCount();

    if (rowCount < startDataRow || colCount < 1) { // 确保有足够的行读取表头和数据
        errorMessage = "工作表为空或数据起始行超出范围。";
        return dataList;
    }

    // 读取表头 (假设 startDataRow 就是数据表头的行号)
    QStringList headers;
    for (int col = 1; col <= colCount; ++col) {
        headers << worksheet->read(startDataRow, col).toString().trimmed();
    }
    if (headers.isEmpty() || headers.first().isEmpty()) {
        errorMessage = "工作表在指定起始行没有有效表头。";
        return dataList;
    }

    // 读取数据行 (从 startDataRow + 1 行开始)
    for (int row = startDataRow + 1; row <= rowCount; ++row) {
        QVariantMap rowData;
        bool hasValidDataInRow = false;
        for (int col = 1; col <= colCount; ++col) {
            QString header = headers.value(col - 1);
            if (header.isEmpty()) continue;

            QVariant value = worksheet->read(row, col);
            if (!value.isNull() && !value.toString().trimmed().isEmpty()) {
                hasValidDataInRow = true;
            }
            rowData[header] = value;
        }
        if (hasValidDataInRow) {
            dataList.append(rowData);
        } else {
             DEBUG_LOG << "跳过 XLSX 中的空数据行:" << row;
        }
    }
    errorMessage.clear(); // 清空错误信息
    return dataList;
}



// --- 关键修改：readWorksheet 的参数和返回类型 ---
QList<QVariantList> XlsxParser::readWorksheet(QXlsx::Worksheet* worksheet, int startDataRow, int startDataCol, QString& errorMessage)
{
    QList<QVariantList> dataList;
    if (!worksheet) {
        errorMessage = "工作表无效。";
        return dataList;
    }

    int rowCount = worksheet->dimension().rowCount();
    int colCount = worksheet->dimension().columnCount();

    if (rowCount < startDataRow || colCount < startDataCol) {
        errorMessage = "工作表为空或数据起始行/列超出范围。";
        return dataList;
    }

    // 从 startDataRow 和 startDataCol 开始读取数据
    for (int row = startDataRow; row <= rowCount; ++row) {
        QVariantList rowData;
        bool hasValidDataInRow = false;
        for (int col = startDataCol; col <= colCount; ++col) {
            QVariant value = worksheet->read(row, col);
            if (!value.isNull() && !value.toString().trimmed().isEmpty()) {
                hasValidDataInRow = true;
            }
            rowData.append(value);
        }
        if (hasValidDataInRow && !rowData.isEmpty()) {
            dataList.append(rowData);
        } else {
            DEBUG_LOG << "跳过 XLSX 中的空数据行:" << row;
        }
    }
    errorMessage.clear();
    return dataList;
}
