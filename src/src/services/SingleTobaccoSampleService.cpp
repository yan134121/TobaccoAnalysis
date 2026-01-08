



// src/service/SingleTobaccoSampleService.cpp
#include "SingleTobaccoSampleService.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QFileInfo>
#include "src/data_access/DatabaseConnector.h"
#include "Logger.h"
#include <QSqlError>
#include "src/core/entities/SingleTobaccoSampleData.h"   // 必须是完整定义


// --- 引入所有需要完整定义的 DAO 和 Entity ---
#include "data_access/SingleTobaccoSampleDAO.h"
#include "data_access/TgBigDataDAO.h"
#include "data_access/ProcessTgBigDataDAO.h"
#include "data_access/TgSmallDataDAO.h"
#include "data_access/ChromatographyDataDAO.h"
#include "src/services/algorithm/IAlgorithmService.h"
#include "utils/file_handler/FileHandlerFactory.h"
#include "core/entities/SingleTobaccoSampleData.h"
#include "core/entities/TgBigData.h"
#include "core/entities/ProcessTgBigData.h"
#include "core/entities/TgSmallData.h"
#include "core/entities/ChromatographyData.h"
// --- 结束引入 ---

// 引入 XlsxParser 以便在需要时调用其 parseMultiSheetFile
#include "utils/file_handler/XlsxParser.h"
#include "services/DictionaryOptionService.h" // <-- 添加这行

SingleTobaccoSampleService::SingleTobaccoSampleService(SingleTobaccoSampleDAO* SingleTobaccoSampleDao,
                                                          TgBigDataDAO* tgBigDataDao,
                                                          ProcessTgBigDataDAO* processTgBigDataDao,
                                                          TgSmallDataDAO* tgSmallDataDao,
                                                          ChromatographyDataDAO* chromatographyDataDao,
                                                          IAlgorithmService* algoService,
                                                          FileHandlerFactory* fileHandlerFactory,
                                                          DictionaryOptionService* dictionaryOptionService, // <-- 新增注入
                                                          QObject *parent)
    : QObject(parent),
      m_singleTobaccoSampleDao(SingleTobaccoSampleDao),
      m_tgBigDataDao(tgBigDataDao),
      m_processTgBigDataDao(processTgBigDataDao),
      m_tgSmallDataDao(tgSmallDataDao),
      m_chromatographyDataDao(chromatographyDataDao),
      m_algorithmService(algoService),
      m_fileHandlerFactory(fileHandlerFactory),
      m_dictionaryOptionService(dictionaryOptionService) // <-- 添加这行
{
    if (!m_singleTobaccoSampleDao) FATAL_LOG << "SingleTobaccoSampleService: SingleTobaccoSampleDAO not injected!";
    if (!m_tgBigDataDao) FATAL_LOG << "SingleTobaccoSampleService: TgBigDataDAO not injected!";
    if (!m_tgSmallDataDao) FATAL_LOG << "SingleTobaccoSampleService: TgSmallDataDAO not injected!";
    if (!m_chromatographyDataDao) FATAL_LOG << "SingleTobaccoSampleService: ChromatographyDataDAO not injected!";
    if (!m_algorithmService) FATAL_LOG << "SingleTobaccoSampleService: IAlgorithmService not injected!";
    if (!m_fileHandlerFactory) FATAL_LOG << "SingleTobaccoSampleService: FileHandlerFactory not injected!";
    if (!m_dictionaryOptionService) FATAL_LOG << "SingleTobaccoSampleService: DictionaryOptionService not injected!"; // <-- 新增检查
}

SingleTobaccoSampleService::~SingleTobaccoSampleService()
{
    // 依赖的生命周期通常由 AppInitializer (其父对象) 管理
}

// 辅助函数：数据验证
bool SingleTobaccoSampleService::validateTobacco(const SingleTobaccoSampleData& tobacco, QString& errorMessage)
{
    // if (tobacco.getCode().isEmpty()) {
    //     errorMessage = "编码不能为空。";
    //     return false;
    // }
    // if (tobacco.getYear() <= 0 || tobacco.getYear() > QDate::currentDate().year() + 1) { // 简单校验年份
    //     errorMessage = "年份无效。";
    //     return false;
    // }
    // if (tobacco.getOrigin().isEmpty()) {
    //     errorMessage = "产地不能为空。";
    //     return false;
    // }
    // if (tobacco.getType().isEmpty() || (tobacco.getType() != "单打" && tobacco.getType() != "混打")) {
    //     errorMessage = "类型必须是'单打'或'混打'。";
    //     return false;
    // }
    return true;
}


// --- 修改：获取独特选项的方法实现，调用 DictionaryOptionService ---
QStringList SingleTobaccoSampleService::getAllUniqueOrigins() {
    DEBUG_LOG << "getAllUniqueOrigins";

    DEBUG_LOG << "m_dictionaryOptionService pointer: " << m_dictionaryOptionService;   
    // 打印 m_dao 指针
    if (m_dictionaryOptionService && m_dictionaryOptionService->getDAO()) {
        DEBUG_LOG << "m_dao pointer: " << m_dictionaryOptionService->getDAO();
    } else {
        WARNING_LOG << "m_dao is null!";
    }

    // // 直接通过 m_dictionaryOptionService 访问 m_dao
    // if (m_dictionaryOptionService) {
    //     // 打印 m_dao 指针
    //     DEBUG_LOG << "m_dao pointer: " << m_dictionaryOptionService->m_dao;
    // } else {
    //     WARNING_LOG << "m_dictionaryOptionService is null!";
    // }



    return m_dictionaryOptionService->getOptionsByCategory("Origin");
    // return m_dictionaryOptionService->getOptionsByCategory("Part");
}
QStringList SingleTobaccoSampleService::getAllUniqueParts() {
    return m_dictionaryOptionService->getOptionsByCategory("Part");
}
QStringList SingleTobaccoSampleService::getAllUniqueGrades() {
    return m_dictionaryOptionService->getOptionsByCategory("Grade");
}
QStringList SingleTobaccoSampleService::getAllUniqueBlendTypes() {
    return m_dictionaryOptionService->getOptionsByCategory("BlendType"); // ENUM 对应 BlendType Category
}
// --- 结束修改 ---


// --- 导入大热重数据 (修改为接收 DataColumnMapping 列表) ---
bool SingleTobaccoSampleService::importTgBigDataForSample(int sampleId, const QString& filePath, int replicateNoBase, int startDataRow, int startDataCol, const DataColumnMapping& mapping, QString& errorMessage)
{
    errorMessage.clear();
    if (sampleId == -1) { /* ... */ return false; }
    if (!m_fileHandlerFactory) { /* ... */ return false; }
    if (mapping.replicateMappings.isEmpty()) { errorMessage = "大热重数据导入映射配置为空。"; WARNING_LOG << errorMessage; return false; }


    AbstractFileParser* parser = m_fileHandlerFactory->getParser(filePath, errorMessage);
    if (!parser) { return false; }

    DEBUG_LOG << "Service: 正在导入大热重文件:" << filePath << " for Sample ID:" << sampleId << " from row:" << startDataRow << " (startDataCol is now handled by mapping)";

    // --- 关键修改：解析器读取所有数据，不依赖 startDataCol ---
    QList<QVariantList> fullSheetRawData = parser->parseFile(filePath, errorMessage, startDataRow); // <-- 移除 startDataCol 参数
    if (fullSheetRawData.isEmpty()) { return false; }
    // --- 结束修改 ---

    QSqlDatabase db = DatabaseConnector::getInstance().getDatabase();
    if (!db.isOpen()) { errorMessage = "数据库未连接，无法导入数据。"; FATAL_LOG << errorMessage; return false; }
    db.transaction();

    try {
        QString sourceName = QFileInfo(filePath).fileName();
        m_tgBigDataDao->removeBySampleId(sampleId); // 导入前先删除该 sampleId 之前的大热重数据

        // --- 关键修改：遍历 DataColumnMapping 中的每个平行样映射 ---
        int currentReplicateNo = replicateNoBase; // 平行样号基础值
        for (const SingleReplicateColumnMapping& repMapping : mapping.replicateMappings) {
            QString currentError;
            // 调用辅助解析函数，传入完整的 Sheet 数据和当前平行样的映射
            QList<TgBigData> bigDataList = parseTgBigDataFromRawData(sampleId, currentReplicateNo, sourceName, fullSheetRawData, repMapping, currentError);
            if (bigDataList.isEmpty()) {
                // 如果解析失败，可能是数据真的没了，或者格式错了
                WARNING_LOG << "样本ID " << sampleId << ", 平行样 " << currentReplicateNo << " 的大热重数据解析后为空或失败:" << currentError;
                // 这里可以决定是回滚整个事务，还是只跳过这个平行样
                // 暂时决定：如果某个平行样解析失败或为空，则继续下一个，但如果全部都失败，则返回false
                // errorMessage += QString("平行样 %1 数据解析失败: %2; ").arg(currentReplicateNo).arg(currentError);
                // continue; // 暂时选择继续处理下一个平行样
                // 为了简化，如果一个平行样解析失败，就视为整个导入失败
                errorMessage = currentError;
                db.rollback(); return false;
            }

            if (!m_tgBigDataDao->insertBatch(bigDataList)) {
                errorMessage = QString("样本ID %1, 平行样 %2 的大热重数据批量插入失败: %3").arg(sampleId).arg(currentReplicateNo).arg(db.lastError().text());
                WARNING_LOG << errorMessage; db.rollback(); return false;
            }
            currentReplicateNo++; // 递增平行样编号
        }
        // --- 结束关键修改 ---

        db.commit();
        DEBUG_LOG << "Service: 大热重数据导入成功，共处理" << mapping.replicateMappings.size() << "组平行样。";
        return true;
    } catch (const std::exception& e) { errorMessage = QString("导入过程中发生C++异常: %1").arg(e.what()); FATAL_LOG << errorMessage; db.rollback(); return false; }
    catch (...) { errorMessage = "导入过程中发生未知异常。"; FATAL_LOG << errorMessage; db.rollback(); return false; }
}

// --- 导入小热重数据 和 色谱数据 (请同步修改，移除 startDataCol 参数，并遍历 DataColumnMapping) ---
bool SingleTobaccoSampleService::importTgSmallDataForSample(int sampleId, const QString& filePath, int replicateNoBase, int startDataRow, int startDataCol, const DataColumnMapping& mapping, QString& errorMessage) {
    errorMessage.clear();
    if (sampleId == -1) { errorMessage = "未指定单料烟基础信息ID，无法导入小热重数据。"; WARNING_LOG << errorMessage; return false; }
    if (!m_fileHandlerFactory) { errorMessage = "文件处理器工厂未初始化。"; FATAL_LOG << errorMessage; return false; }
    if (mapping.replicateMappings.isEmpty()) { errorMessage = "小热重数据导入映射配置为空。"; WARNING_LOG << errorMessage; return false; }

    AbstractFileParser* parser = m_fileHandlerFactory->getParser(filePath, errorMessage);
    if (!parser) { return false; }

    DEBUG_LOG << "Service: 正在导入小热重文件:" << filePath << " for Sample ID:" << sampleId << " from row:" << startDataRow << " (startDataCol is now handled by mapping)";

    QList<QVariantList> fullSheetRawData = parser->parseFile(filePath, errorMessage, startDataRow); // <-- 移除 startDataCol
    if (fullSheetRawData.isEmpty()) { return false; }

    QSqlDatabase db = DatabaseConnector::getInstance().getDatabase();
    if (!db.isOpen()) { errorMessage = "数据库未连接，无法导入数据。"; FATAL_LOG << errorMessage; return false; }
    db.transaction();

    try {
        QString sourceName = QFileInfo(filePath).fileName();
        m_tgSmallDataDao->removeBySampleId(sampleId);

        int currentReplicateNo = replicateNoBase;
        for (const SingleReplicateColumnMapping& repMapping : mapping.replicateMappings) {
            QString currentError;
            QList<TgSmallData> smallDataList = parseTgSmallDataFromRawData(sampleId, currentReplicateNo, sourceName, fullSheetRawData, repMapping, currentError); // <-- 传入 fullSheetRawData
            if (smallDataList.isEmpty()) {
                errorMessage = currentError;
                WARNING_LOG << errorMessage; db.rollback(); return false;
            }

            if (!m_tgSmallDataDao->insertBatch(smallDataList)) {
                errorMessage = QString("样本ID %1, 平行样 %2 的小热重数据批量插入失败: %3").arg(sampleId).arg(currentReplicateNo).arg(db.lastError().text());
                WARNING_LOG << errorMessage; db.rollback(); return false;
            }
            currentReplicateNo++;
        }

        db.commit();
        DEBUG_LOG << "Service: 小热重数据导入成功，共处理" << mapping.replicateMappings.size() << "组平行样。";
        return true;
    } catch (const std::exception& e) { errorMessage = QString("导入过程中发生C++异常: %1").arg(e.what()); FATAL_LOG << errorMessage; db.rollback(); return false; }
    catch (...) { errorMessage = "导入过程中发生未知异常。"; FATAL_LOG << errorMessage; db.rollback(); return false; }
}

bool SingleTobaccoSampleService::importChromatographyDataForSample(int sampleId, const QString& filePath, int replicateNoBase, int startDataRow, int startDataCol, const DataColumnMapping& mapping, QString& errorMessage) {
    errorMessage.clear();
    if (sampleId == -1) { errorMessage = "未指定单料烟基础信息ID，无法导入色谱数据。"; WARNING_LOG << errorMessage; return false; }
    if (!m_fileHandlerFactory) { errorMessage = "文件处理器工厂未初始化。"; FATAL_LOG << errorMessage; return false; }
    if (mapping.replicateMappings.isEmpty()) { errorMessage = "色谱数据导入映射配置为空。"; WARNING_LOG << errorMessage; return false; }

    AbstractFileParser* parser = m_fileHandlerFactory->getParser(filePath, errorMessage);
    if (!parser) { return false; }

    DEBUG_LOG << "Service: 正在导入色谱文件:" << filePath << " for Sample ID:" << sampleId << " from row:" << startDataRow << " (startDataCol is now handled by mapping)";

    QList<QVariantList> fullSheetRawData = parser->parseFile(filePath, errorMessage, startDataRow); // <-- 移除 startDataCol
    if (fullSheetRawData.isEmpty()) { return false; }

    QSqlDatabase db = DatabaseConnector::getInstance().getDatabase();
    if (!db.isOpen()) { errorMessage = "数据库未连接，无法导入数据。"; FATAL_LOG << errorMessage; return false; }
    db.transaction();

    try {
        QString sourceName = QFileInfo(filePath).fileName();
        m_chromatographyDataDao->removeBySampleId(sampleId);

        int currentReplicateNo = replicateNoBase;
        for (const SingleReplicateColumnMapping& repMapping : mapping.replicateMappings) {
            QString currentError;
            QList<ChromatographyData> chromatographyDataList = parseChromatographyDataFromRawData(sampleId, currentReplicateNo, sourceName, fullSheetRawData, repMapping, currentError); // <-- 传入 fullSheetRawData
            if (chromatographyDataList.isEmpty()) {
                errorMessage = currentError;
                WARNING_LOG << errorMessage; db.rollback(); return false;
            }

            if (!m_chromatographyDataDao->insertBatch(chromatographyDataList)) {
                errorMessage = QString("样本ID %1, 平行样 %2 的色谱数据批量插入失败: %3").arg(sampleId).arg(currentReplicateNo).arg(db.lastError().text());
                WARNING_LOG << errorMessage; db.rollback(); return false;
            }
            currentReplicateNo++;
        }

        db.commit();
        DEBUG_LOG << "Service: 色谱数据导入成功，共处理" << mapping.replicateMappings.size() << "组平行样。";
        return true;
    } catch (const std::exception& e) { errorMessage = QString("导入过程中发生C++异常: %1").arg(e.what()); FATAL_LOG << errorMessage; db.rollback(); return false; }
    catch (...) { errorMessage = "导入过程中发生未知异常。"; FATAL_LOG << errorMessage; db.rollback(); return false; }
}



// --- 辅助解析方法实现 (修改为接收 SingleReplicateColumnMapping 和 fullSheetRawData) ---
QList<TgBigData> SingleTobaccoSampleService::parseTgBigDataFromRawData(int sampleId, int currentReplicateNo, const QString& sourceName, const QList<QVariantList>& fullSheetRawData, const SingleReplicateColumnMapping& currentReplicateMapping, QString& errorMessage)
{
    QList<TgBigData> list;
    errorMessage.clear();
    int currentSeqNo = 0; // 每个平行样数据块内部的序号
    for (const QVariantList& rowData : fullSheetRawData) { // <-- 遍历整个 Sheet 的行数据
        // 关键逻辑：判断当前行数据是否属于当前平行样的数据块
        // 如果当前平行样数据块的第一个核心列 (例如 tgBigTgValueColIndex) 对应的单元格为空，则认为该平行样数据已结束
        int firstCoreColIndex = currentReplicateMapping.tgBigTgValueColIndex - 1; // 转换为 0-based
        if (firstCoreColIndex >= 0 && firstCoreColIndex < rowData.size()) {
            if (rowData.value(firstCoreColIndex).isNull() || rowData.value(firstCoreColIndex).toString().trimmed().isEmpty()) {
                // 如果当前行的核心数据列为空，这可能表示当前平行样的数据已经结束
                // 或者只是文件中有空行，需要更智能的判断
                // 为了简化，这里先假设遇到空的核心列就停止，这意味着一个平行样的数据是连续的
                // WARNING_LOG << "Stopping parsing for current replicate due to empty core column.";
                // break; // 停止处理当前平行样在这一行的数据
                // 暂时选择继续，只跳过当前空行
            }
        } else {
            // 如果核心列索引无效，也跳过此行
            // WARNING_LOG << "Invalid core column index for current replicate, skipping row.";
            // continue;
        }


        // 确保核心列索引有效 (这里是根据 SingleReplicateColumnMapping 校验)
        if (!currentReplicateMapping.isValidForBigTg()) {
            errorMessage = "大热重数据导入映射配置不完整（热重值或热重微分列未指定）。";
            return QList<TgBigData>();
        }

        // 获取列索引 (注意：1-based 转 0-based)
        int tgValueColIdx = currentReplicateMapping.tgBigTgValueColIndex - 1;
        int dtgValueColIdx = currentReplicateMapping.tgBigDtgValueColIndex - 1;
        int tgBigSerialNoColIdx = currentReplicateMapping.tgBigSerialNoColIndex - 1;
        int seqNoColIdx = currentReplicateMapping.seqNoColIndex - 1;
        int sourceNameColIdx = currentReplicateMapping.sourceNameColIndex - 1;

        // 确保行数据有足够的列
        if (tgValueColIdx >= rowData.size() || dtgValueColIdx >= rowData.size() ||
            (tgBigSerialNoColIdx >= 0 && tgBigSerialNoColIdx >= rowData.size()) ||
            (seqNoColIdx >= 0 && seqNoColIdx >= rowData.size()) ||
            (sourceNameColIdx >= 0 && sourceNameColIdx >= rowData.size()))
        {
             errorMessage = QString("大热重数据行缺少必要的列，行数据: %1").arg(QJsonDocument(QJsonArray::fromVariantList(rowData)).toJson(QJsonDocument::Compact).constData());
             return QList<TgBigData>();
        }

        QVariant serialNo = rowData.value(tgBigSerialNoColIdx);
        QVariant tgValue = rowData.value(tgValueColIdx);
        QVariant dtgValue = rowData.value(dtgValueColIdx);

        if (!tgValue.canConvert<double>() || !dtgValue.canConvert<double>()) {
            // 如果数据格式不正确，则跳过该行，或中断处理
            // 这里选择记录警告，并跳过当前行
            WARNING_LOG << QString("大热重数据中 '热重值' 或 '热重微分' 格式错误，行数据: %1").arg(QJsonDocument(QJsonArray::fromVariantList(rowData)).toJson(QJsonDocument::Compact).constData());
            continue; // 跳过当前行，继续下一行
            // 如果需要严格，这里可以 return QList<TgBigData>();
        }

        TgBigData bd;
        bd.setSampleId(sampleId);
        bd.setSerialNo(serialNo.toInt());
        DEBUG_LOG << "serialNo.toInt()" << serialNo.toInt();
        bd.setTemperature((seqNoColIdx >= 0 && seqNoColIdx < rowData.size()) ? rowData.value(seqNoColIdx).toInt() : currentSeqNo++);
        bd.setTgValue(tgValue.toDouble());
        bd.setDtgValue(dtgValue.toDouble());
        bd.setSourceName((sourceNameColIdx >= 0 && sourceNameColIdx < rowData.size()) ? rowData.value(sourceNameColIdx).toString() : sourceName);
        list.append(bd);
    }
    if (list.isEmpty()) { errorMessage = "大热重数据解析后为空或没有有效数据。"; }
    return list;
}

QList<TgSmallData> SingleTobaccoSampleService::parseTgSmallDataFromRawData(int sampleId, int currentReplicateNo, const QString& sourceName, const QList<QVariantList>& fullSheetRawData, const SingleReplicateColumnMapping& currentReplicateMapping, QString& errorMessage)
{
    QList<TgSmallData> list;
    errorMessage.clear();
    int serialCounter = 1;
    for (const QVariantList& rowData : fullSheetRawData) {
        // 关键逻辑：判断当前行数据是否属于当前平行样的数据块
        int firstCoreColIndex = currentReplicateMapping.tgSmallTemperatureColIndex - 1;
        if (firstCoreColIndex >= 0 && firstCoreColIndex < rowData.size()) {
            if (rowData.value(firstCoreColIndex).isNull() || rowData.value(firstCoreColIndex).toString().trimmed().isEmpty()) {
                // WARNING_LOG << "Stopping parsing for current replicate due to empty core column.";
                continue; // 跳过空的核心列行
            }
        } else {
            // continue;
        }

        int tempColIdx = currentReplicateMapping.tgSmallTemperatureColIndex - 1;
        int tgValueColIdx = currentReplicateMapping.tgSmallTgValueColIndex - 1;
        int dtgValueColIdx = currentReplicateMapping.tgSmallDtgValueColIndex - 1;
        if (!currentReplicateMapping.isValidForSmallTg()) {
            tempColIdx = 0;
            tgValueColIdx = 1;
            dtgValueColIdx = 2;
        }
        int replicateNoColIdx = currentReplicateMapping.replicateNoColIndex - 1;
        int sourceNameColIdx = currentReplicateMapping.sourceNameColIndex - 1;

        if (tempColIdx >= rowData.size() || tgValueColIdx >= rowData.size() || dtgValueColIdx >= rowData.size() ||
            (replicateNoColIdx >= 0 && replicateNoColIdx >= rowData.size()) ||
            (sourceNameColIdx >= 0 && sourceNameColIdx >= rowData.size())) {
            errorMessage = QString("小热重数据行缺少必要的列 (温度/热重值/微分)，行数据: %1").arg(QJsonDocument(QJsonArray::fromVariantList(rowData)).toJson(QJsonDocument::Compact).constData());
            WARNING_LOG << errorMessage;
            continue;
        }

        QVariant tempValue = rowData.value(tempColIdx);
        QVariant tgValue = rowData.value(tgValueColIdx);
        QVariant dtgValue = rowData.value(dtgValueColIdx);

        if (!tempValue.canConvert<double>() || !tgValue.canConvert<double>() || !dtgValue.canConvert<double>()) {
            errorMessage = QString("小热重数据中 '温度'、'热重值' 或 '热重微分' 格式错误，行数据: %1").arg(QJsonDocument(QJsonArray::fromVariantList(rowData)).toJson(QJsonDocument::Compact).constData());
            return QList<TgSmallData>();
        }

        TgSmallData sd;
        sd.setSampleId(sampleId);
        sd.setSerialNo(serialCounter++);
        sd.setTemperature(tempValue.toDouble());
        DEBUG_LOG << "tempValue.toDouble()" << tempValue.toDouble();
        sd.setTgValue(tgValue.toDouble());
        sd.setDtgValue(dtgValue.toDouble());
        sd.setSourceName((sourceNameColIdx >= 0 && sourceNameColIdx < rowData.size()) ? rowData.value(sourceNameColIdx).toString() : sourceName);
        list.append(sd);
    }
    if (list.isEmpty()) { errorMessage = "小热重数据解析后为空。"; }
    return list;
}

QList<ChromatographyData> SingleTobaccoSampleService::parseChromatographyDataFromRawData(int sampleId, int currentReplicateNo, const QString& sourceName, const QList<QVariantList>& fullSheetRawData, const SingleReplicateColumnMapping& currentReplicateMapping, QString& errorMessage)
{
    DEBUG_LOG << __FILE__ << __LINE__;
    DEBUG_LOG;
    QList<ChromatographyData> list;
    errorMessage.clear();
    for (const QVariantList& rowData : fullSheetRawData) {
        // 关键逻辑：判断当前行数据是否属于当前平行样的数据块
        int firstCoreColIndex = currentReplicateMapping.chromaRetentionTimeColIndex - 1;
        if (firstCoreColIndex >= 0 && firstCoreColIndex < rowData.size()) {
            if (rowData.value(firstCoreColIndex).isNull() || rowData.value(firstCoreColIndex).toString().trimmed().isEmpty()) {
                // 如果当前行的核心数据列为空，这可能表示当前平行样的数据已经结束
                // 或者只是文件中有空行，需要更智能的判断
                // WARNING_LOG << "Stopping parsing for current replicate due to empty core column.";
                continue; // 跳过空的核心列行
            }
        } else {
            // 如果核心列索引无效，也跳过此行
            // WARNING_LOG << "Invalid core column index for current replicate, skipping row.";
            // continue;
        }


        // 校验列索引
        if (!currentReplicateMapping.isValidForChromatography()) {
            errorMessage = "色谱数据导入映射配置不完整（保留时间或响应值列未指定）。";
            return QList<ChromatographyData>();
        }

        int rtColIdx = currentReplicateMapping.chromaRetentionTimeColIndex - 1;
        int respColIdx = currentReplicateMapping.chromaResponseValueColIndex - 1;
        int replicateNoColIdx = currentReplicateMapping.replicateNoColIndex - 1;
        int sourceNameColIdx = currentReplicateMapping.sourceNameColIndex - 1;

        // 确保行数据有足够的列
        if (rtColIdx >= rowData.size() || respColIdx >= rowData.size() ||
            (replicateNoColIdx >= 0 && replicateNoColIdx >= rowData.size()) ||
            (sourceNameColIdx >= 0 && sourceNameColIdx >= rowData.size())) {
            errorMessage = QString("色谱数据行缺少必要的列 (保留时间/响应值)，行数据: %1").arg(QJsonDocument(QJsonArray::fromVariantList(rowData)).toJson(QJsonDocument::Compact).constData());
            return QList<ChromatographyData>();
        }

        QVariant retentionTime = rowData.value(rtColIdx);
        QVariant responseValue = rowData.value(respColIdx);

        if (!retentionTime.canConvert<double>() || !responseValue.canConvert<double>()) {
            // 如果数据格式不正确，则跳过该行，或中断处理
            WARNING_LOG << QString("色谱数据中 '保留时间' 或 '响应值' 格式错误，行数据: %1").arg(QJsonDocument(QJsonArray::fromVariantList(rowData)).toJson(QJsonDocument::Compact).constData());
            continue; // 跳过当前行，继续下一行
            // 如果需要严格，这里可以 return QList<ChromatographyData>();
        }

        ChromatographyData cd;
        cd.setSampleId(sampleId);
        cd.setReplicateNo((replicateNoColIdx >= 0 && replicateNoColIdx < rowData.size()) ? rowData.value(replicateNoColIdx).toInt() : currentReplicateNo);
        cd.setRetentionTime(retentionTime.toDouble());
        cd.setResponseValue(responseValue.toDouble());
        cd.setSourceName((sourceNameColIdx >= 0 && sourceNameColIdx < rowData.size()) ? rowData.value(sourceNameColIdx).toString() : sourceName);
        list.append(cd);
    }
    if (list.isEmpty()) { errorMessage = "色谱数据解析后为空。"; }
    return list;
}


// --- 新增：获取指定样本的传感器数据的方法实现 ---
QList<TgBigData> SingleTobaccoSampleService::getTgBigDataForSample(int sampleId)
{
    if (!m_tgBigDataDao) {
        FATAL_LOG << "SingleTobaccoSampleService: TgBigDataDAO not injected!";
        return QList<TgBigData>();
    }
    return m_tgBigDataDao->getBySampleId(sampleId);
}

QList<TgSmallData> SingleTobaccoSampleService::getTgSmallDataForSample(int sampleId)
{
    if (!m_tgSmallDataDao) {
        FATAL_LOG << "SingleTobaccoSampleService: TgSmallDataDAO not injected!";
        return QList<TgSmallData>();
    }
    return m_tgSmallDataDao->getBySampleId(sampleId);
}

QList<ChromatographyData> SingleTobaccoSampleService::getChromatographyDataForSample(int sampleId)
{
    if (!m_chromatographyDataDao) {
        FATAL_LOG << "SingleTobaccoSampleService: ChromatographyDataDAO not injected!";
        return QList<ChromatographyData>();
    }
    return m_chromatographyDataDao->getBySampleId(sampleId);
}
// --- 结束新增 ---


QList<SingleTobaccoSampleData> SingleTobaccoSampleService::getAllTobaccos()
{
    return m_singleTobaccoSampleDao->queryAll();
}

QList<SingleTobaccoSampleData> SingleTobaccoSampleService::queryTobaccos(const QMap<QString, QVariant>& conditions)
{
    return m_singleTobaccoSampleDao->query(conditions);
}

SingleTobaccoSampleData SingleTobaccoSampleService::getTobaccoById(int id)
{
    return m_singleTobaccoSampleDao->getById(id);
}

bool SingleTobaccoSampleService::addTobacco(SingleTobaccoSampleData& tobacco, QString& errorMessage)
{
    if (!validateTobacco(tobacco, errorMessage)) {
        return false;
    }

    // --- 关键修改：在添加 SingleTobaccoSampleData 之前，先处理字典选项 ---
    QString dictErrorMessage; // 使用局部变量接收字典服务的错误信息

    // // 产地
    // if (!m_dictionaryOptionService->addOption("Origin", tobacco.getOrigin(), dictErrorMessage)) {
    //     errorMessage = QString("处理产地选项 '%1' 失败: %2").arg(tobacco.getOrigin(), dictErrorMessage);
    //     WARNING_LOG << errorMessage; return false;
    // }
    // // 部位
    // if (!m_dictionaryOptionService->addOption("Part", tobacco.getPart(), dictErrorMessage)) {
    //     errorMessage = QString("处理部位选项 '%1' 失败: %2").arg(tobacco.getPart(), dictErrorMessage);
    //     WARNING_LOG << errorMessage; return false;
    // }
    // // 等级
    // if (!m_dictionaryOptionService->addOption("Grade", tobacco.getGrade(), dictErrorMessage)) {
    //     errorMessage = QString("处理等级选项 '%1' 失败: %2").arg(tobacco.getGrade(), dictErrorMessage);
    //     WARNING_LOG << errorMessage; return false;
    // }
    // // 类型 (单打/混打)
    // if (!m_dictionaryOptionService->addOption("BlendType", tobacco.getType(), dictErrorMessage)) { // '类型' 也从字典管理
    //     errorMessage = QString("处理类型选项 '%1' 失败: %2").arg(tobacco.getType(), dictErrorMessage);
    //     WARNING_LOG << errorMessage; return false;
    // }
    // // --- 结束关键修改 ---


    if (m_singleTobaccoSampleDao->insert(tobacco)) {
        DEBUG_LOG << "Service: 添加单料烟成功，ID:" << tobacco.getId();
        return true;
    } else {
        errorMessage = "添加单料烟到数据库失败: " + DatabaseConnector::getInstance().getDatabase().lastError().text();
        WARNING_LOG << errorMessage;
        return false;
    }
}

bool SingleTobaccoSampleService::updateTobacco(const SingleTobaccoSampleData& tobacco, QString& errorMessage)
{
    if (tobacco.getId() == -1) {
        errorMessage = "无效的数据ID，无法更新。";
        WARNING_LOG << errorMessage;
        return false;
    }
    if (!validateTobacco(tobacco, errorMessage)) {
        return false;
    }

    // --- 关键修改：在更新 SingleTobaccoSampleData 之前，先处理字典选项 ---
    QString dictErrorMessage; // 使用局部变量接收字典服务的错误信息

    // 产地
    if (!m_dictionaryOptionService->addOption("Origin", tobacco.getOrigin(), dictErrorMessage)) {
        errorMessage = QString("处理产地选项 '%1' 失败: %2").arg(tobacco.getOrigin(), dictErrorMessage);
        WARNING_LOG << errorMessage; return false;
    }
    // 部位
    if (!m_dictionaryOptionService->addOption("Part", tobacco.getPart(), dictErrorMessage)) {
        errorMessage = QString("处理部位选项 '%1' 失败: %2").arg(tobacco.getPart(), dictErrorMessage);
        WARNING_LOG << errorMessage; return false;
    }
    // 等级
    if (!m_dictionaryOptionService->addOption("Grade", tobacco.getGrade(), dictErrorMessage)) {
        errorMessage = QString("处理等级选项 '%1' 失败: %2").arg(tobacco.getGrade(), dictErrorMessage);
        WARNING_LOG << errorMessage; return false;
    }
    // 类型 (单打/混打)
    if (!m_dictionaryOptionService->addOption("BlendType", tobacco.getType(), dictErrorMessage)) {
        errorMessage = QString("处理类型选项 '%1' 失败: %2").arg(tobacco.getType(), dictErrorMessage);
        WARNING_LOG << errorMessage; return false;
    }
    // --- 结束关键修改 ---


    
    if (m_singleTobaccoSampleDao->update(tobacco)) {
        DEBUG_LOG << "Service: 更新单料烟成功，ID:" << tobacco.getId();
        return true;
    } else {
        errorMessage = "更新单料烟到数据库失败: " + DatabaseConnector::getInstance().getDatabase().lastError().text();
        WARNING_LOG << errorMessage;
        return false;
    }
}

bool SingleTobaccoSampleService::removeTobaccos(const QList<int>& ids, QString& errorMessage)
{
    if (ids.isEmpty()) {
        errorMessage = "没有指定要删除的记录。";
        WARNING_LOG << errorMessage;
        return false;
    }

    QSqlDatabase db = DatabaseConnector::getInstance().getDatabase();
    if (!db.isOpen()) {
        errorMessage = "数据库未连接，无法删除数据。";
        FATAL_LOG << errorMessage;
        return false;
    }
    db.transaction(); // 开启事务

    for (int id : ids) {
        if (!m_singleTobaccoSampleDao->remove(id)) { // DAO 负责删除基础信息
            db.rollback(); // 任意一条删除失败则回滚整个事务
            errorMessage = QString("删除ID为 %1 的基础信息失败: %2。").arg(id).arg(db.lastError().text());
            WARNING_LOG << errorMessage;
            return false;
        }
        // 由于数据库中设置了 ON DELETE CASCADE，删除 single_material_tobacco 会自动删除关联的传感器数据
        // 如果没有设置 CASCADE，这里需要手动调用 m_tgBigDataDao->removeBySampleId(id); 等
    }
    db.commit(); // 提交事务
    DEBUG_LOG << "Service: 批量删除基础信息成功，共" << ids.size() << "条记录。";
    return true;
}

bool SingleTobaccoSampleService::processTobaccoData(int tobaccoId, const QVariantMap& params, QString& errorMessage)
{
    if (!m_algorithmService) { /* ... */ return false; }
    SingleTobaccoSampleData tobacco = m_singleTobaccoSampleDao->getById(tobaccoId);
    if (tobacco.getId() == -1) { /* ... */ return false; }
    QJsonDocument rawData = tobacco.getDataJson();
    if (rawData.isEmpty() || rawData.isNull()) { /* ... */ return false; }

    DEBUG_LOG << "Service: 正在调用算法处理数据 ID:" << tobaccoId << "，算法参数:" << params;
    QJsonDocument processedData = m_algorithmService->processJsonData(rawData, params, errorMessage);
    if (processedData.isNull() || processedData.isEmpty()) { /* ... */ return false; }

    tobacco.setDataJson(processedData);
    if (!m_singleTobaccoSampleDao->update(tobacco)) {
        errorMessage = "更新处理后数据到数据库失败: " + DatabaseConnector::getInstance().getDatabase().lastError().text();
        WARNING_LOG << errorMessage;
        return false;
    }

    DEBUG_LOG << "Service: 数据 ID:" << tobaccoId << "处理成功。";
    return true;
}

bool SingleTobaccoSampleService::exportDataToFile(const QString& filePath,
                                                      const QList<SingleTobaccoSampleData>& dataToExport,
                                                      QString& errorMessage)
{
    if (!m_fileHandlerFactory) { /* ... */ return false; }
    AbstractFileWriter* writer = m_fileHandlerFactory->getWriter(filePath, errorMessage);
    if (!writer) { /* ... */ return false; }

    QList<QVariantMap> dataMaps;
    for (const SingleTobaccoSampleData& tobacco : dataToExport) {
        QVariantMap map;
        map["ID"] = tobacco.getId();
        // map["编码"] = tobacco.getCode();
        map["年份"] = tobacco.getYear();
        map["产地"] = tobacco.getOrigin();
        map["部位"] = tobacco.getPart();
        map["等级"] = tobacco.getGrade();
        map["类型"] = tobacco.getType();
        map["数采日期"] = tobacco.getCollectDate().toString(Qt::ISODate);
        map["检测日期"] = tobacco.getDetectDate().toString(Qt::ISODate);
        map["原始数据JSON"] = tobacco.getDataJson().toJson(QJsonDocument::Compact);
        map["创建时间"] = tobacco.getCreatedAt().toString(Qt::ISODate);
        dataMaps.append(map);
    }

    QStringList headers;
    headers << "ID" << "编码" << "年份" << "产地" << "部位" << "等级" << "类型"
            << "数采日期" << "检测日期" << "原始数据JSON" << "创建时间";

    if (writer->writeToFile(filePath, dataMaps, errorMessage, headers)) {
        DEBUG_LOG << "Service: 数据导出成功。";
        return true;
    } else {
        WARNING_LOG << "Service: 数据导出失败:" << errorMessage;
        return false;
    }
}


QString SingleTobaccoSampleService::buildSampleDisplayName(int sampleId) {
        SingleTobaccoSampleDAO dao;
        SampleIdentifier sid = dao.getSampleIdentifierById(sampleId);
        return QString("%1-%2-%3-%4")
                .arg(sid.projectName)
                .arg(sid.batchCode)
                .arg(sid.shortCode)
                .arg(sid.parallelNo);
            }
