
#include "services/data_import/TgBigDataImportWorker.h"
#include "core/entities/TgBigData.h"
#include "data_access/TgBigDataDAO.h"
#include "data_access/SingleTobaccoSampleDAO.h"
#include "AppInitializer.h"
#include "DatabaseConnector.h"
#include "Logger.h"

#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QSqlDatabase>
#include <QSqlError>
#include <QDateTime>
#include <QRegularExpression>
#include <QDirIterator>


// TgBigDataImportWorker 类实现
TgBigDataImportWorker::TgBigDataImportWorker(QObject* parent)
    : QThread(parent), m_appInitializer(nullptr), m_stopped(false),
      m_tgBigDataDao(nullptr), m_singleTobaccoSampleDao(nullptr)
{
}

TgBigDataImportWorker::~TgBigDataImportWorker()
{
    stop();
    wait();
    
    // 清理资源
    closeThreadDatabase();
}
    
// void TgBigDataImportWorker::setParameters(const QString& dirPath, AppInitializer* appInitializer)
// {
//     QMutexLocker locker(&m_mutex);
//     m_dirPath = dirPath;
//     m_appInitializer = appInitializer;
//     m_stopped = false;
// }
// 设置导入参数，包括目录、烟牌号、批次代码和检测日期
void TgBigDataImportWorker::setParameters(const QString& dirPath, 
                                           const QString& projectName, 
                                           const QString& batchCode, 
                                           const QDate& detectDate,
                                           AppInitializer* appInitializer)
{
    QMutexLocker locker(&m_mutex);
    m_dirPath = dirPath;
    m_projectName = projectName;
    m_batchCode = batchCode;
    m_detectDate = detectDate;
    // m_parallelNo = parallelNo;
    m_appInitializer = appInitializer;
    m_stopped = false;
    m_useCustomColumns = false;
    m_temperatureColumn1Based = -1;
    m_dataColumn1Based = -1;
}

void TgBigDataImportWorker::setParameters(const QString& dirPath,
                                         const QString& projectName,
                                         const QString& batchCode,
                                         const QDate& detectDate,
                                         bool useCustomColumns,
                                         int temperatureColumn1Based,
                                         int dataColumn1Based,
                                         AppInitializer* appInitializer)
{
    QMutexLocker locker(&m_mutex);
    m_dirPath = dirPath;
    m_projectName = projectName;
    m_batchCode = batchCode;
    m_detectDate = detectDate;
    m_appInitializer = appInitializer;
    m_stopped = false;
    m_useCustomColumns = useCustomColumns;
    m_temperatureColumn1Based = temperatureColumn1Based;
    m_dataColumn1Based = dataColumn1Based;
}

void TgBigDataImportWorker::stop()
{
    QMutexLocker locker(&m_mutex);
    m_stopped = true;
}
    
void TgBigDataImportWorker::run()
{
    QMutexLocker locker(&m_mutex);
    QString dirPath = m_dirPath;
    locker.unlock();
    
    // 初始化线程独立的数据库连接
    if (!initThreadDatabase()) {
        return; // 初始化失败，直接返回
    }
    
    emit progressMessage(tr("正在扫描文件夹..."));
    
    // 扫描并分组CSV文件路径
    QMap<QString, QStringList> fileGroups = buildGroupedCsvPaths(dirPath);
    
    // 显示统计信息
    int fileCount = 0;
    for (const auto& list : fileGroups.values()) fileCount += list.size();
    
    if (fileCount == 0) {
        emit importError(tr("未找到有效的CSV文件"));
        return;
    }
    
    QString summary = tr("扫描完成：%1 组，%2 个CSV文件。").arg(fileGroups.size()).arg(fileCount);
    emit progressMessage(summary);
    
    // 逐个读取CSV文件并提取数据
    int successCount = 0;
    int failCount = 0;
    int currentProgress = 0;
    
    // 准备批量保存的数据列表
    QList<TgBigData> dataToSave;
    
    // 开始事务
    m_threadDb.transaction();
    
    try {
        // 遍历所有分组的文件
        for (const auto& groupId : fileGroups.keys()) {
            const QStringList& filePaths = fileGroups.value(groupId);
            
            for (const QString& filePath : filePaths) {
                // 检查是否被要求停止
                {
                    QMutexLocker locker(&m_mutex);
                    if (m_stopped) {
                        m_threadDb.rollback();
                        emit importError(tr("用户取消了导入操作"));
                        return;
                    }
                }
                
                // 更新进度
                emit progressChanged(currentProgress++, fileCount);
                emit progressMessage(tr("正在处理文件: %1").arg(QFileInfo(filePath).fileName()));
                
                // 为每个文件创建一个独立的样本ID
                int sampleId = -1;
                QFileInfo fileInfo(filePath);
                sampleId = createOrGetSample(fileInfo.fileName());
                if (sampleId == -1) {
                    WARNING_LOG << "无法从文件名创建样本:" << filePath;
                    failCount++;
                    continue;
                }
                
                // 读取CSV文件
                QFile file(filePath);
                if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    WARNING_LOG << "无法打开文件:" << filePath;
                    failCount++;
                    continue;
                }
                
                QTextStream in(&file);
                
                // 设置编码为UTF-8
                in.setCodec("UTF-8");
                
                // 读取CSV数据
                QList<TgBigData> dataList = readTgBigDataFromCsv(in, filePath, sampleId);
                
                if (dataList.isEmpty()) {
                    WARNING_LOG << "从文件中未读取到有效数据:" << filePath;
                    failCount++;
                } else {
                    // 删除该样本的旧数据
                    if (m_tgBigDataDao) {
                        m_tgBigDataDao->removeBySampleId(sampleId);
                        
                        // 保存当前文件的数据
                        bool saveResult = m_tgBigDataDao->insertBatch(dataList);
                        
                        if (saveResult) {
                            dataToSave.append(dataList);
                            successCount++;
                        } else {
                            WARNING_LOG << "保存数据失败:" << filePath;
                            failCount++;
                        }
                    }
                }
                
                file.close();
            }
        }
        
        m_threadDb.commit();
        emit importFinished(successCount, dataToSave.size());
        
    } catch (const std::exception &e) {
        m_threadDb.rollback();
        emit importError(tr("导入过程中发生错误: %1").arg(e.what()));
    }
}
    
// 初始化线程独立的数据库连接
bool TgBigDataImportWorker::initThreadDatabase()
{
        // 创建线程独立的数据库连接
        if (QSqlDatabase::contains("tgbig_thread_connection")) {
            m_threadDb = QSqlDatabase::database("tgbig_thread_connection");
        } else {
            m_threadDb = QSqlDatabase::addDatabase("QMYSQL", "tgbig_thread_connection");
        }
        
        // 从主连接获取数据库配置
        if (m_appInitializer) {
            // 获取与主线程相同的数据库配置
            QSqlDatabase mainDb = DatabaseConnector::getInstance().getDatabase();
            
            // 复制主数据库的连接参数
            m_threadDb.setHostName(mainDb.hostName());
            m_threadDb.setDatabaseName(mainDb.databaseName());
            m_threadDb.setUserName(mainDb.userName());
            m_threadDb.setPassword(mainDb.password());
            m_threadDb.setPort(mainDb.port());
            
            if (!m_threadDb.open()) {
                FATAL_LOG << "[数据库连接失败]"
                << "\n主机:" << m_threadDb.hostName()
                << "\n端口:" << m_threadDb.port()
                << "\n数据库:" << m_threadDb.databaseName()
                << "\n用户名:" << m_threadDb.userName()
                << "\n错误信息:" << m_threadDb.lastError().text()
                << "\n线程ID:" << QThread::currentThreadId();

                emit importError("无法打开线程数据库连接: " + m_threadDb.lastError().text());
                return false;
            }
            
            // 打印子线程数据库连接信息
            DEBUG_LOG << "大热重数据导入子线程数据库连接信息:";
            DEBUG_LOG << "  连接名称:" << m_threadDb.connectionName();
            DEBUG_LOG << "  连接ID:" << QThread::currentThreadId();
            DEBUG_LOG << "  数据库路径:" << m_threadDb.databaseName();
            DEBUG_LOG << "  连接是否有效:" << m_threadDb.isValid();
            DEBUG_LOG << "  连接是否打开:" << m_threadDb.isOpen();
            
            // 创建线程独立的DAO实例
            m_tgBigDataDao = new TgBigDataDAO(m_threadDb);
            m_singleTobaccoSampleDao = new SingleTobaccoSampleDAO(m_threadDb);
            
            return true;
        }
        
        emit importError("无法初始化线程数据库连接: AppInitializer为空");
        return false;
    }
    
// 关闭线程独立的数据库连接
void TgBigDataImportWorker::closeThreadDatabase()
{
        // 释放DAO资源
        if (m_tgBigDataDao) {
            delete m_tgBigDataDao;
            m_tgBigDataDao = nullptr;
        }
        
        if (m_singleTobaccoSampleDao) {
            delete m_singleTobaccoSampleDao;
            m_singleTobaccoSampleDao = nullptr;
        }
        
        // 关闭数据库连接
        if (m_threadDb.isOpen()) {
            m_threadDb.close();
        }
    }
    
// 创建或获取样本ID
int TgBigDataImportWorker::createOrGetSample(const QString& fileName)
    {
        // 使用线程独立的DAO实例
        if (!m_singleTobaccoSampleDao) {
            WARNING_LOG << "线程独立的SingleTobaccoSampleDAO实例未初始化";
            return -1;
        }
        
        // 从文件名解析样本信息
        QString shortCode;
        int parallelNo = 1;
        
        if (parseSampleInfoFromFilename(fileName, shortCode, parallelNo)) {
            // 使用short_code查询样本
            QList<SingleTobaccoSampleData> existingSamples = m_singleTobaccoSampleDao->queryByProjectNameAndBatchCodeAndShortCodeAndParallelNo(m_projectName, m_batchCode, shortCode, parallelNo);
            
            if (!existingSamples.isEmpty()) {
                // 如果样本已存在，返回其ID
                int existingId = existingSamples.first().getId();
                DEBUG_LOG << "找到现有样本，ID:" << existingId;
                
                // 确认样本ID有效
                if (existingId <= 0) {
                    WARNING_LOG << "现有样本ID无效:" << existingId;
                    return -1;
                }
                
                return existingId;
            } else {
                // 如果样本不存在，创建新样本
                SingleTobaccoSampleData newSample;
                
                // 设置样本信息
                newSample.setShortCode(shortCode);
                newSample.setProjectName(m_projectName);
                newSample.setBatchCode(m_batchCode);
                newSample.setParallelNo(parallelNo);
                newSample.setDetectDate(m_detectDate); // 设置检测日期
                
                // 设置创建时间
                QDateTime now = QDateTime::currentDateTime();
                newSample.setCreatedAt(now);
                
                // 设置默认年份为当前年份，避免年份验证错误
                newSample.setYear(now.date().year());
                
                // 设置样本名称
                QString sampleName = QString("%1-%2-%3-%4").arg(
                    newSample.getProjectName(),
                    newSample.getBatchCode(),
                    shortCode,
                    QString::number(parallelNo)
                );
                newSample.setSampleName(sampleName);
                
                DEBUG_LOG << "创建新大热重样本:" 
                         << "short_code=" << shortCode
                         << "project_name=" << newSample.getProjectName()
                         << "batch_code=" << newSample.getBatchCode()
                         << "parallel_no=" << newSample.getParallelNo();
                
                // 使用线程独立的DAO直接插入新样本
                int newId = m_singleTobaccoSampleDao->insert(newSample);
                if (newId > 0) {
                    // 成功插入新样本，直接返回新ID
                    DEBUG_LOG << "成功创建大热重样本，ID:" << newId;
                    return newId;
                } else {
                    WARNING_LOG << "创建大热重样本失败";
                }
            }
        } else {
            WARNING_LOG << "无法从文件名解析样本信息:" << fileName;
        }
        
        WARNING_LOG << "无法创建或获取大热重样本ID";
        return -1;
    }
    

bool TgBigDataImportWorker::parseSampleInfoFromFilename(
        const QString& fileName, QString& shortCode, int& parallelNo)
{
    // 去掉扩展名 .txt/.csv 等
    QString baseName = fileName;
    int dotIndex = baseName.lastIndexOf('.');
    if (dotIndex > 0)
        baseName = baseName.left(dotIndex);

    // 找括号位置 (英文或中文)
    int lp = baseName.indexOf('(');
    int lp_cn = baseName.indexOf('（');
    int pos = -1;
    if (lp >= 0 && lp_cn >= 0)
        pos = qMin(lp, lp_cn);
    else if (lp >= 0)
        pos = lp;
    else if (lp_cn >= 0)
        pos = lp_cn;

    // 未找到括号 → shortCode=整串，parallelNo=1
    if (pos < 0) {
        shortCode = baseName.trimmed();
        parallelNo = 1;
        return true;
    }

    // 提取 shortCode
    shortCode = baseName.left(pos).trimmed();

    // 提取括号部分
    int rp = baseName.lastIndexOf(')');
    int rp_cn = baseName.lastIndexOf('）');
    int endPos = qMax(rp, rp_cn);
    QString inside = "";
    if (endPos > pos)
        inside = baseName.mid(pos, endPos - pos + 1);

    // 提取括号内连续数字
    QRegExp numRx("(\\d+)");
    if (numRx.indexIn(inside) != -1)
        parallelNo = numRx.cap(1).toInt();
    else
        parallelNo = 1; // 没数字则默认

    return true;
}

    
// 扫描并分组CSV文件路径
QMap<QString, QStringList> TgBigDataImportWorker::buildGroupedCsvPaths(const QString& dirPath)
{
    QMap<QString, QStringList> result;

    // 使用 QDirIterator 支持递归扫描
    QDirIterator it(dirPath, QStringList() << "*.csv", QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString filePath = it.next();
        QString fileName = QFileInfo(filePath).fileName();

        // 从文件名解析分组ID
        QString groupId = parseGroupIdFromFilename(fileName);

        // 将文件路径添加到对应的分组
        if (!result.contains(groupId)) {
            result.insert(groupId, QStringList());
        }
        result[groupId].append(filePath);
    }

    DEBUG_LOG << "扫描完成，总分组数:" << result.size();
    for (auto key : result.keys()) {
        DEBUG_LOG << "分组:" << key << "文件数:" << result[key].size();
    }

    return result;
}

    
// 从文件名解析分组ID
QString TgBigDataImportWorker::parseGroupIdFromFilename(const QString& fileName)
{
        // 默认使用文件名作为分组ID
        return fileName;
    }
    
// 读取CSV数据
QList<TgBigData> TgBigDataImportWorker::readTgBigDataFromCsv(QTextStream& in, const QString& filePath, int sampleId)
{
    QList<TgBigData> result;
    QMap<QString, int> columnIndex; // key: 列名, value: 列索引(0-based)

    // 若启用自定义列：直接使用用户指定列（1-based -> 0-based）
    if (m_useCustomColumns) {
        const int tempIdx = m_temperatureColumn1Based - 1;
        const int dataIdx = m_dataColumn1Based - 1;
        if (tempIdx >= 0) columnIndex["temperature"] = tempIdx;
        if (dataIdx >= 0) columnIndex["weight"] = dataIdx;
        // serialNo 尽量从表头识别；识别不到则用递增生成
    }

    // 1) 尝试找到标题行（用于识别序号列，或在未自定义时识别重量列）
    QString firstDataLine;
    bool hasBufferedFirstDataLine = false;
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;

        QStringList fields = line.split(',');

        int serialCol = -1;
        int weightCol = -1;
        int temperatureCol = -1;

        for (int i = 0; i < fields.size(); ++i) {
            QString field = fields[i].trimmed();
            if (field.contains("序号")) serialCol = i;
            else if (field.contains("天平示数") || field.contains("天平克数") || field.contains("重量")) weightCol = i;
            else if (field.contains("温度")) temperatureCol = i;
        }

        // 记录序号列（无论是否自定义）
        if (serialCol != -1) columnIndex["serialNo"] = serialCol;

        // 未自定义重量列时，用表头识别
        if (!m_useCustomColumns && weightCol != -1) columnIndex["weight"] = weightCol;
        // 未自定义温度列时，用表头识别（大热重数据结构里有 temperature 字段）
        if (!m_useCustomColumns && temperatureCol != -1) columnIndex["temperature"] = temperatureCol;

        // 如果是未自定义：至少需要 weight；serialNo 若没有可以后续递增生成
        // 如果是自定义：至少需要 weight（来自自定义）；标题行可能无法识别，当前行可能已经是数据行
        if (!m_useCustomColumns) {
            if (columnIndex.contains("weight")) {
                break; // 标题行已找到
            }
        } else {
            // 自定义列：如果本行像是数据行（对应列能解析出数字），则把它作为第一条数据缓存起来
            const int weightIdx = columnIndex.value("weight", -1);
            if (weightIdx >= 0 && fields.size() > weightIdx) {
                bool ok = false;
                fields[weightIdx].toDouble(&ok);
                if (ok) {
                    firstDataLine = line;
                    hasBufferedFirstDataLine = true;
                }
            }
            break;
        }
    }

    if (!columnIndex.contains("weight")) {
        WARNING_LOG << "CSV缺少必要列：天平示数/重量（或未指定数据列）" << filePath;
        return result;
    }

    auto handleDataLine = [&](const QString& line) {
        QStringList fields = line.split(',');
        const int serialIdx = columnIndex.value("serialNo", -1);
        const int weightIdx = columnIndex.value("weight", -1);
        const int tempIdx = columnIndex.value("temperature", -1);
        const int requiredMax = qMax(qMax(serialIdx, weightIdx), tempIdx);
        if (requiredMax < 0 || fields.size() <= requiredMax) return;

        // weight
        bool okWeight = false;
        double weight = fields[weightIdx].trimmed().toDouble(&okWeight);
        if (!okWeight) return;

        // serialNo：优先读取序号列，否则递增生成
        int serialNo = 0;
        bool okSerial = false;
        if (serialIdx >= 0) {
            serialNo = fields[serialIdx].trimmed().toInt(&okSerial);
        } else {
            serialNo = result.size();
            okSerial = true;
        }
        if (!okSerial) return;

        // temperature（可选）
        double temperature = 0.0;
        if (tempIdx >= 0) {
            bool okTemp = false;
            temperature = fields[tempIdx].trimmed().toDouble(&okTemp);
            if (!okTemp) temperature = 0.0;
        }

        TgBigData data;
        data.setSampleId(sampleId);
        data.setSerialNo(serialNo);
        data.setWeight(weight);
        data.setTemperature(temperature);
        data.setSourceName(QFileInfo(filePath).fileName());
        result.append(data);
    };

    // 2) 读取数据行
    if (hasBufferedFirstDataLine) {
        handleDataLine(firstDataLine);
    }
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;
        handleDataLine(line);
    }

    return result;
}

    
