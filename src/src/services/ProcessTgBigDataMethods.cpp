#include "SingleTobaccoSampleService.h"
#include <QDebug>
#include <QFileInfo>
#include "utils/file_handler/XlsxParser.h"
#include "utils/file_handler/FileHandlerFactory.h"
#include "data_access/ProcessTgBigDataDAO.h"
#include "core/entities/ProcessTgBigData.h"
#include "Logger.h"

// 导入ProcessTgBigData数据
bool SingleTobaccoSampleService::importProcessTgBigDataForSample(int sampleId, const QString& filePath, int replicateNo, int startDataRow, int startDataCol, const DataColumnMapping& mapping, QString& errorMessage)
{
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        errorMessage = QString("文件不存在: %1").arg(filePath);
        return false;
    }

    // 获取文件处理器
    QString parseError;
    auto fileHandler = m_fileHandlerFactory->getParser(filePath, parseError);
    if (!fileHandler) {
        errorMessage = parseError.isEmpty() ? 
            QString("不支持的文件类型: %1").arg(fileInfo.suffix()) : parseError;
        return false;
    }

    // 解析文件内容
    QList<QVariantList> rawData;
    bool parseSuccess = false;

    if (fileInfo.suffix().toLower() == "xlsx" || fileInfo.suffix().toLower() == "xls") {
        // 使用XlsxParser处理Excel文件
        XlsxParser xlsxParser;
        QString parseError;
        
        auto sheetsData = xlsxParser.parseMultiSheetFile(filePath, parseError, startDataRow);
        parseSuccess = !sheetsData.isEmpty();
        if (parseSuccess) {
            // 使用第一个sheet的数据
            if (!sheetsData.isEmpty()) {
                auto firstSheetData = sheetsData.first();
                // 转换数据格式
                for (const auto& row : firstSheetData) {
                    QVariantList rowData;
                    for (const auto& item : row) {
                        rowData.append(item);
                    }
                    rawData.append(rowData);
                }
            }
        } else {
            errorMessage = parseError;
        }
    } else {
        // 使用通用文件处理器
        QString parseError;
        rawData = fileHandler->parseFile(filePath, parseError, mapping.startDataRow, mapping.startDataCol);
        parseSuccess = parseError.isEmpty() && !rawData.isEmpty();
        if (!parseSuccess) {
            errorMessage = parseError.isEmpty() ? "解析文件失败或文件内容为空" : parseError;
        }
    }

    if (!parseSuccess || rawData.isEmpty()) {
        errorMessage = "解析文件失败或文件内容为空";
        return false;
    }

    // 为当前平行样创建列映射
    SingleReplicateColumnMapping currentReplicateMapping;
    currentReplicateMapping.temperatureCol = mapping.temperatureCol;
    currentReplicateMapping.weightCol = mapping.weightCol;
    currentReplicateMapping.tgValueCol = mapping.tgValueCol;
    currentReplicateMapping.dtgValueCol = mapping.dtgValueCol;
    currentReplicateMapping.startDataRow = startDataRow;
    currentReplicateMapping.startDataCol = startDataCol;

    // 解析数据并创建ProcessTgBigData对象列表
    QList<ProcessTgBigData> processTgBigDataList = parseProcessTgBigDataFromRawData(
        sampleId, replicateNo, fileInfo.fileName(), rawData, currentReplicateMapping, errorMessage);

    if (processTgBigDataList.isEmpty()) {
        if (errorMessage.isEmpty()) {
            errorMessage = "未能从文件中提取有效的处理后大热重数据";
        }
        return false;
    }

    // 先删除该样本的现有数据
    m_processTgBigDataDao->removeBySampleId(sampleId);

    // 批量插入新数据
    bool insertSuccess = m_processTgBigDataDao->insertBatch(processTgBigDataList);
    if (!insertSuccess) {
        errorMessage = "保存处理后大热重数据到数据库失败";
        return false;
    }

    return true;
}

// 解析原始数据为ProcessTgBigData对象列表
QList<ProcessTgBigData> SingleTobaccoSampleService::parseProcessTgBigDataFromRawData(
    int sampleId, int currentReplicateNo, const QString& sourceName, 
    const QList<QVariantList>& fullSheetRawData, 
    const SingleReplicateColumnMapping& currentReplicateMapping, 
    QString& errorMessage)
{
    QList<ProcessTgBigData> result;
    
    // 检查数据行数是否足够
    if (fullSheetRawData.size() <= currentReplicateMapping.startDataRow) {
        errorMessage = QString("数据行数不足，需要至少 %1 行").arg(currentReplicateMapping.startDataRow + 1);
        return result;
    }
    
    // 从起始行开始处理数据
    for (int rowIdx = currentReplicateMapping.startDataRow; rowIdx < fullSheetRawData.size(); rowIdx++) {
        const QVariantList& row = fullSheetRawData.at(rowIdx);
        
        // 检查列数是否足够
        int maxColNeeded = qMax(qMax(currentReplicateMapping.temperatureCol, currentReplicateMapping.weightCol),
                               qMax(currentReplicateMapping.tgValueCol, currentReplicateMapping.dtgValueCol));
        
        if (row.size() <= maxColNeeded || maxColNeeded < 0) {
            // 跳过无效行
            continue;
        }
        
        // 提取数据
        bool temperatureOk = false, weightOk = false, tgValueOk = false, dtgValueOk = false;
        double temperature = 0.0, weight = 0.0, tgValue = 0.0, dtgValue = 0.0;
        
        if (currentReplicateMapping.temperatureCol >= 0 && currentReplicateMapping.temperatureCol < row.size()) {
            temperature = row.at(currentReplicateMapping.temperatureCol).toDouble(&temperatureOk);
        }
        
        if (currentReplicateMapping.weightCol >= 0 && currentReplicateMapping.weightCol < row.size()) {
            weight = row.at(currentReplicateMapping.weightCol).toDouble(&weightOk);
        }
        
        if (currentReplicateMapping.tgValueCol >= 0 && currentReplicateMapping.tgValueCol < row.size()) {
            tgValue = row.at(currentReplicateMapping.tgValueCol).toDouble(&tgValueOk);
        }
        
        if (currentReplicateMapping.dtgValueCol >= 0 && currentReplicateMapping.dtgValueCol < row.size()) {
            dtgValue = row.at(currentReplicateMapping.dtgValueCol).toDouble(&dtgValueOk);
        }
        
        // 至少需要温度和一个有效值
        if (temperatureOk && (weightOk || tgValueOk || dtgValueOk)) {
            ProcessTgBigData data;
            data.setSampleId(sampleId);
            data.setSerialNo(currentReplicateNo);
            data.setTemperature(temperature);
            data.setWeight(weight);
            data.setTgValue(tgValue);
            data.setDtgValue(dtgValue);
            data.setSourceName(sourceName);
            data.setCreatedAt(QDateTime::currentDateTime());
            
            result.append(data);
        }
    }
    
    return result;
}

// 获取指定样本的处理后大热重数据
QList<ProcessTgBigData> SingleTobaccoSampleService::getProcessTgBigDataForSample(int sampleId)
{
    return m_processTgBigDataDao->getBySampleId(sampleId);
}