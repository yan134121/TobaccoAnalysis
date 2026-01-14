#include "services/data_import/ProcessTgBigDataImportWorker.h"
#include "core/entities/ProcessTgBigData.h"
#include "data_access/ProcessTgBigDataDAO.h"
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
#include <QMessageBox>  // QMessageBox
#include <QProgressDialog>  // QProgressDialog
#include <QDirIterator>


ProcessTgBigDataImportWorker::ProcessTgBigDataImportWorker(QObject* parent)
    : QThread(parent), m_appInitializer(nullptr), m_stopped(false)
{
}

ProcessTgBigDataImportWorker::~ProcessTgBigDataImportWorker()
{
    stop();
    wait();

    // 清理资源
    closeThreadDatabase();
}

void ProcessTgBigDataImportWorker::setParameters(const QString& dirPath, AppInitializer* appInitializer)
{
    m_dirPath = dirPath;
    m_appInitializer = appInitializer;
    m_stopped = false;
    // 清空可选覆盖的项目与批次（旧接口默认不覆盖）
    m_projectName.clear();
    m_batchCode.clear();
    // 清空列覆盖（旧接口默认沿用当前逻辑）
    m_useCustomColumns = false;
    m_temperatureColumn1Based = -1;
    m_dataColumn1Based = -1;
}

// 新重载：支持传入项目名称与批次代码，用于覆盖从文件名/文件夹解析的值
void ProcessTgBigDataImportWorker::setParameters(const QString& dirPath, const QString& projectName, const QString& batchCode, AppInitializer* appInitializer)
{
    m_dirPath = dirPath;
    m_appInitializer = appInitializer;
    m_stopped = false;
    // 记录用户在导入前确认/修改的项目名称与批次代码
    m_projectName = projectName.trimmed();
    m_batchCode = batchCode.trimmed();
    // 清空列覆盖（该接口未启用列选择）
    m_useCustomColumns = false;
    m_temperatureColumn1Based = -1;
    m_dataColumn1Based = -1;
}

void ProcessTgBigDataImportWorker::setParameters(const QString& dirPath,
                                                const QString& projectName,
                                                const QString& batchCode,
                                                bool useCustomColumns,
                                                int temperatureColumn1Based,
                                                int dataColumn1Based,
                                                AppInitializer* appInitializer)
{
    m_dirPath = dirPath;
    m_appInitializer = appInitializer;
    m_stopped = false;
    m_projectName = projectName.trimmed();
    m_batchCode = batchCode.trimmed();
    m_useCustomColumns = useCustomColumns;
    m_temperatureColumn1Based = temperatureColumn1Based;
    m_dataColumn1Based = dataColumn1Based;
}

void ProcessTgBigDataImportWorker::stop()
{
    QMutexLocker locker(&m_mutex);
    m_stopped = true;
}

 
void ProcessTgBigDataImportWorker::run()
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
    QList<ProcessTgBigData> dataToSave;
    
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
                QList<ProcessTgBigData> dataList = readProcessTgBigDataFromCsv(in, filePath, sampleId);
                
                if (dataList.isEmpty()) {
                    WARNING_LOG << "从文件中未读取到有效数据:" << filePath;
                    failCount++;
                } else {
                    // 删除该样本的旧数据
                    if (m_processTgBigDataDao) {
                        m_processTgBigDataDao->removeBySampleId(sampleId);
                        
                        // 保存当前文件的数据
                        bool saveResult = m_processTgBigDataDao->insertBatch(dataList);
                        
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
bool ProcessTgBigDataImportWorker::initThreadDatabase()
{
        // 创建线程独立的数据库连接
        if (QSqlDatabase::contains("processTgBig_thread_connection")) {
            m_threadDb = QSqlDatabase::database("processTgBig_thread_connection");
        } else {
            m_threadDb = QSqlDatabase::addDatabase("QMYSQL", "processTgBig_thread_connection");
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
            DEBUG_LOG << "工序大热重数据导入子线程数据库连接信息:";
            DEBUG_LOG << "  连接名称:" << m_threadDb.connectionName();
            DEBUG_LOG << "  连接ID:" << QThread::currentThreadId();
            DEBUG_LOG << "  数据库路径:" << m_threadDb.databaseName();
            DEBUG_LOG << "  连接是否有效:" << m_threadDb.isValid();
            DEBUG_LOG << "  连接是否打开:" << m_threadDb.isOpen();
            
            // 创建线程独立的DAO实例
            m_processTgBigDataDao = new ProcessTgBigDataDAO(m_threadDb);
            m_singleTobaccoSampleDao = new SingleTobaccoSampleDAO(m_threadDb);
            
            return true;
        }
        
        emit importError("无法初始化线程数据库连接: AppInitializer为空");
        return false;
    }
    

       
// 关闭线程独立的数据库连接
void ProcessTgBigDataImportWorker::closeThreadDatabase()
{
        // 释放DAO资源
        if (m_processTgBigDataDao) {
            delete m_processTgBigDataDao;
            m_processTgBigDataDao = nullptr;
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
    


// 递归扫描目录并按组构建CSV路径映射
QMap<QString, QStringList> ProcessTgBigDataImportWorker::buildGroupedCsvPaths(const QString& directoryPath)
{
    QMap<QString, QStringList> groups;

    // 使用QDirIterator递归扫描
    QDirIterator it(directoryPath, QStringList() << "*.csv" << "*.CSV", QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString filePath = it.next();
        QFileInfo info(filePath);
        QString fname = info.fileName();

        // 使用正则识别CSV：这里迭代器已按扩展名过滤，但额外安全判断
        QRegularExpression csvRe("\\.csv$", QRegularExpression::CaseInsensitiveOption);
        if (!csvRe.match(fname).hasMatch()) continue;

        // 构建分组标识
        QString groupId = parseGroupIdFromFilename(fname);
        groups[groupId].append(filePath);
    }

    return groups;
}



SingleTobaccoSampleData* ProcessTgBigDataImportWorker::parseSampleInfoFromFilename(const QString& filename)
{
    // 创建样本对象
    SingleTobaccoSampleData* sample = new SingleTobaccoSampleData();
    
    // 移除文件扩展名和可能的后缀（如"盘5测量数据"）
    QString baseName = filename;
    baseName = baseName.split(".").first(); // 移除扩展名
    
    // 移除可能的后缀
    QStringList suffixes = {"测量数据", "盘", "YS-盘"};
    for (const QString& suffix : suffixes) {
        int idx = baseName.indexOf(suffix);
        if (idx > 0) {
            baseName = baseName.left(idx);
        }
    }
    
    // 清理末尾的连字符
    while (baseName.endsWith("-")) {
        baseName.chop(1);
    }
    
    DEBUG_LOG << "清理后的文件名基础部分:" << baseName;
    
    // 第一种格式: 1906-202412011-CGH(1) 或 1906-202412011-JZH(1)-YS 或 1906-202412011-JZH-YS(1) 或 1906-202412040-JZQ-YZ(1) 或 1906-202412040-JZZ-YA(1)
    // 首先检查是否有 JZZ-YA(1) 格式
    QRegularExpression yaParallelRegex("([A-Za-z]+)-YA\\((\\d+)\\)");
    QRegularExpressionMatch yaMatch = yaParallelRegex.match(baseName);
    
    QString shortCode;
    int parallelNo = 1; // 默认平行样编号
    
    if (yaMatch.hasMatch()) {
        // 处理 JZZ-YA(1) 格式
        QString prefix = yaMatch.captured(1);  // 如 JZZ
        parallelNo = yaMatch.captured(2).toInt();  // 1
        
        if (prefix.length() >= 2) {
            QString mainCode = prefix.left(prefix.length() - 1);
            QString suffix = prefix.right(1);
            shortCode = mainCode + "-YA-" + suffix;  // 如 JZ-YA-Z
        } else {
            shortCode = prefix + "-YA";  // 如果代码太短，直接使用
        }
        
        // 构建标准格式的样本名称
        QString sampleName = QString("%1-%2-%3-%4").arg(
            baseName.split("-")[0],              // 烟牌号 (1906)
            baseName.split("-")[1],              // 批次代码 (202412040)
            shortCode,                           // 短代码 (JZ-YA-Z)
            QString::number(parallelNo)          // 平行样编号 (1)
        );
        
        // 设置样本信息
        sample->setProjectName(baseName.split("-")[0]);
        sample->setBatchCode(baseName.split("-")[1]);
        sample->setShortCode(shortCode);
        sample->setParallelNo(parallelNo);
        sample->setSampleName(sampleName);
        
        DEBUG_LOG << "解析样本信息(YA括号格式):" 
                 << "烟牌号=" << sample->getProjectName()
                 << "批次代码=" << sample->getBatchCode()
                 << "短代码=" << sample->getShortCode()
                 << "平行样编号=" << sample->getParallelNo()
                 << "样本名称=" << sample->getSampleName();
        
        return sample;
    }
    
    // 检查是否有 JZQ-YZ(1) 格式
    QRegularExpression yzParallelRegex("([A-Za-z]+)-YZ\\((\\d+)\\)");
    QRegularExpressionMatch yzMatch = yzParallelRegex.match(baseName);
    
    if (yzMatch.hasMatch()) {
        // 处理 JZQ-YZ(1) 格式
        QString prefix = yzMatch.captured(1);  // 如 JZQ
        parallelNo = yzMatch.captured(2).toInt();  // 1
        
        if (prefix.length() >= 2) {
            QString mainCode = prefix.left(prefix.length() - 1);
            QString suffix = prefix.right(1);
            shortCode = mainCode + "-YZ-" + suffix;  // 如 JZ-YZ-Q
        } else {
            shortCode = prefix + "-YZ";  // 如果代码太短，直接使用
        }
        
        // 构建标准格式的样本名称
        QString sampleName = QString("%1-%2-%3-%4").arg(
            baseName.split("-")[0],              // 烟牌号 (1906)
            baseName.split("-")[1],              // 批次代码 (202412040)
            shortCode,                           // 短代码 (JZ-YZ-Q)
            QString::number(parallelNo)          // 平行样编号 (1)
        );
        
        // 设置样本信息
        sample->setProjectName(baseName.split("-")[0]);
        sample->setBatchCode(baseName.split("-")[1]);
        sample->setShortCode(shortCode);
        sample->setParallelNo(parallelNo);
        sample->setSampleName(sampleName);
        
        DEBUG_LOG << "解析样本信息(YZ括号格式):" 
                 << "烟牌号=" << sample->getProjectName()
                 << "批次代码=" << sample->getBatchCode()
                 << "短代码=" << sample->getShortCode()
                 << "平行样编号=" << sample->getParallelNo()
                 << "样本名称=" << sample->getSampleName();
        
        return sample;
    }
    
    // 检查是否有 JZH-YS(1) 或 JZZ-YS(1) 格式
    QRegularExpression ysParallelRegex("([A-Za-z]+)-YS\\((\\d+)\\)");
    QRegularExpressionMatch ysMatch = ysParallelRegex.match(baseName);
    
    if (ysMatch.hasMatch()) {
        // 处理 JZH-YS(1) 或 JZZ-YS(1) 格式
        QString prefix = ysMatch.captured(1);  // 如 JZH 或 JZZ
        parallelNo = ysMatch.captured(2).toInt();  // 1
        
        if (prefix.length() >= 2) {
            QString mainCode = prefix.left(prefix.length() - 1);
            QString suffix = prefix.right(1);
            shortCode = mainCode + "-YS-" + suffix;  // 如 JZ-YS-H 或 JZ-YS-Z
        } else {
            shortCode = prefix + "-YS";  // 如果代码太短，直接使用
        }
        
        // 构建标准格式的样本名称
        QString sampleName = QString("%1-%2-%3-%4").arg(
            baseName.split("-")[0],              // 烟牌号 (1906)
            baseName.split("-")[1],              // 批次代码 (202412011)
            shortCode,                           // 短代码 (JZ-YS-H 或 JZ-YS-Z)
            QString::number(parallelNo)          // 平行样编号 (1)
        );
        
        // 设置样本信息
        sample->setProjectName(baseName.split("-")[0]);
        sample->setBatchCode(baseName.split("-")[1]);
        sample->setShortCode(shortCode);
        sample->setParallelNo(parallelNo);
        sample->setSampleName(sampleName);
        
        DEBUG_LOG << "解析样本信息(YS括号格式):" 
                 << "烟牌号=" << sample->getProjectName()
                 << "批次代码=" << sample->getBatchCode()
                 << "短代码=" << sample->getShortCode()
                 << "平行样编号=" << sample->getParallelNo()
                 << "样本名称=" << sample->getSampleName();
        
        return sample;
    }
    
    // 检查是否有 JZZ(3)-YZ 格式
    QRegularExpression yzSuffixRegex("([A-Za-z]+)\\((\\d+)\\)-YZ");
    QRegularExpressionMatch yzSuffixMatch = yzSuffixRegex.match(baseName);
    
    if (yzSuffixMatch.hasMatch()) {
        // 处理 JZZ(3)-YZ 格式
        QString prefix = yzSuffixMatch.captured(1);  // 如 JZZ
        parallelNo = yzSuffixMatch.captured(2).toInt();  // 3
        
        if (prefix.length() >= 2) {
            QString mainCode = prefix.left(prefix.length() - 1);
            QString suffix = prefix.right(1);
            shortCode = mainCode + "-YZ-" + suffix;  // 如 JZ-YZ-Z
        } else {
            shortCode = prefix + "-YZ";  // 如果代码太短，直接使用
        }
        
        // 构建标准格式的样本名称
        QString sampleName = QString("%1-%2-%3-%4").arg(
            baseName.split("-")[0],              // 烟牌号 (1906)
            baseName.split("-")[1],              // 批次代码 (202412011)
            shortCode,                           // 短代码 (JZ-YZ-Z)
            QString::number(parallelNo)          // 平行样编号 (3)
        );
        
        // 设置样本信息
        sample->setProjectName(baseName.split("-")[0]);
        sample->setBatchCode(baseName.split("-")[1]);
        sample->setShortCode(shortCode);
        sample->setParallelNo(parallelNo);
        sample->setSampleName(sampleName);
        
        DEBUG_LOG << "解析样本信息(YZ后缀格式):" 
                 << "烟牌号=" << sample->getProjectName()
                 << "批次代码=" << sample->getBatchCode()
                 << "短代码=" << sample->getShortCode()
                 << "平行样编号=" << sample->getParallelNo()
                 << "样本名称=" << sample->getSampleName();
        
        return sample;
    }
    
    // 处理带括号的平行样编号，如CGH(1)或JZH(1)
    QRegularExpression parallelRegex("([A-Za-z]+)([A-Za-z]*)\\((\\d+)\\)");
    QRegularExpressionMatch match = parallelRegex.match(baseName);
    
    if (match.hasMatch()) {
        // 第一种格式的处理
        QString fullCode = match.captured(1) + match.captured(2);  // 如 CGH 或 JZH
        parallelNo = match.captured(3).toInt();  // 1
        
        // 检查是否有YS后缀
        bool hasYS = baseName.contains("-YS", Qt::CaseInsensitive);
        
        // 构建标准格式的短代码
        if (fullCode == "JZH" && hasYS) {
            shortCode = "JZ-YS-H";  // 特殊处理 JZH-YS 为 JZ-YS-H
        } else if (fullCode == "CGH") {
            shortCode = "CG-H";     // 特殊处理 CGH 为 CG-H
        } else {
            // 尝试将三字母代码分为前缀和后缀
            // 假设最后一个字母是后缀，前面的是前缀
            if (fullCode.length() >= 2) {
                QString prefix = fullCode.left(fullCode.length() - 1);
                QString suffix = fullCode.right(1);
                
                if (hasYS) {
                    shortCode = prefix + "-YS-" + suffix;  // 如 JZ-YS-H
                } else {
                    shortCode = prefix + "-" + suffix;     // 如 CG-H
                }
            } else {
                shortCode = fullCode;  // 如果代码太短，直接使用
            }
        }
        
        // 构建标准格式的样本名称
        QString sampleName = QString("%1-%2-%3-%4").arg(
            baseName.split("-")[0],              // 烟牌号 (1906)
            baseName.split("-")[1],              // 批次代码 (202412011)
            shortCode,                           // 短代码 (CG-H 或 JZ-YS-H)
            QString::number(parallelNo)          // 平行样编号 (1)
        );
        
        // 设置样本信息
        sample->setProjectName(baseName.split("-")[0]);
        sample->setBatchCode(baseName.split("-")[1]);
        sample->setShortCode(shortCode);
        sample->setParallelNo(parallelNo);
        sample->setSampleName(sampleName);
        
        DEBUG_LOG << "解析样本信息(第一种格式):" 
                 << "烟牌号=" << sample->getProjectName()
                 << "批次代码=" << sample->getBatchCode()
                 << "短代码=" << sample->getShortCode()
                 << "平行样编号=" << sample->getParallelNo()
                 << "样本名称=" << sample->getSampleName();
        
        return sample;
    }
    
    // 第二种格式: 已经是标准格式 1906-202412011-CG-H-1 或 1906-202412011-JZ-YS-H-1
    QStringList parts = baseName.split("-");
    if (parts.size() < 4) {
        WARNING_LOG << "文件名格式不正确，无法解析:" << filename;
        delete sample;
        return nullptr;
    }
    
    // 设置烟牌号（第一部分）
    sample->setProjectName(parts[0]);
    
    // 设置批次代码（第二部分）
    sample->setBatchCode(parts[1]);
    
    // 处理短代码和平行样编号
    if (parts.size() == 4) {
        // 格式: 1906-202412011-CG-H-1
        shortCode = parts[2] + "-" + parts[3].section("-", 0, -2);
        QString lastPart = parts[3];
        
        // 检查最后一部分是否包含平行样编号
        QRegularExpression endDigitRegex("(.*?)-(\\d+)$");
        match = endDigitRegex.match(lastPart);
        
        if (match.hasMatch()) {
            shortCode = parts[2] + "-" + match.captured(1);
            parallelNo = match.captured(2).toInt();
        } else if (lastPart.toInt() > 0) {
            // 如果最后一部分是纯数字，则视为平行样编号
            parallelNo = lastPart.toInt();
        } else {
            // 否则将整个部分作为短代码的一部分
            shortCode = parts[2] + "-" + lastPart;
        }
    } else {
        // 格式: 1906-202412011-JZ-YS-H-1
        // 最后一部分应该是平行样编号
        parallelNo = parts.last().toInt();
        if (parallelNo == 0 && !parts.last().startsWith("0")) {
            // 如果最后一部分不是数字，则使用默认平行样编号
            parallelNo = 1;
            shortCode = parts.mid(2).join("-");
        } else {
            // 否则，短代码是中间所有部分
            shortCode = parts.mid(2, parts.size() - 3).join("-");
        }
    }
    
    // 设置短代码和平行样编号
    sample->setShortCode(shortCode);
    sample->setParallelNo(parallelNo);
    
    // 构建样本名称
    QString sampleName = QString("%1-%2-%3-%4").arg(
        sample->getProjectName(),
        sample->getBatchCode(),
        sample->getShortCode(),
        QString::number(sample->getParallelNo())
    );
    
    sample->setSampleName(sampleName);
    
    DEBUG_LOG << "解析样本信息(第二种格式):" 
             << "烟牌号=" << sample->getProjectName()
             << "批次代码=" << sample->getBatchCode()
             << "短代码=" << sample->getShortCode()
             << "平行样编号=" << sample->getParallelNo()
             << "样本名称=" << sample->getSampleName();
    
    return sample;
}

int ProcessTgBigDataImportWorker::createOrGetSample(const QString& filename)
{
    // 使用线程独立的DAO实例
    if (!m_singleTobaccoSampleDao) {
        WARNING_LOG << "线程独立的SingleTobaccoSampleDAO实例未初始化";
        return -1;
    }

    // 从文件名解析样本信息
    SingleTobaccoSampleData* parsedSample = parseSampleInfoFromFilename(filename);
    if (!parsedSample) {
        return -1;
    }
    
    // 如果用户通过对话框提供了覆盖的项目名称或批次代码，则应用覆盖
    // 优先采用用户输入的值，避免仅从文件名解析造成错误归属
    if (!m_projectName.isEmpty()) {
        parsedSample->setProjectName(m_projectName);
    }
    if (!m_batchCode.isEmpty()) {
        parsedSample->setBatchCode(m_batchCode);
    }
    // 按统一规范重新生成样本名称，确保唯一键一致
    QString rebuiltName = QString("%1-%2-%3-%4").arg(
        parsedSample->getProjectName(),
        parsedSample->getBatchCode(),
        parsedSample->getShortCode(),
        QString::number(parsedSample->getParallelNo())
    );
    parsedSample->setSampleName(rebuiltName);
    
    
     // 使用查询样本
    QList<SingleTobaccoSampleData> existingSamples = m_singleTobaccoSampleDao->queryByProjectNameAndBatchCodeAndShortCodeAndParallelNo(
        parsedSample->getProjectName(), parsedSample->getBatchCode(), parsedSample->getShortCode(), parsedSample->getParallelNo());
    
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

        // 如果不存在，使用SingleTobaccoSampleDAO创建新样本
        SingleTobaccoSampleDAO sampleDAO;
        // ⚡ 设置批次类型
        parsedSample->setBatchType(BatchType::PROCESS);   // 或 BatchType::PROCESS，根据业务逻辑
        if (sampleDAO.insert(*parsedSample)) {
            // 获取新创建的样本ID
            int newId = parsedSample->getId();
            delete parsedSample;
            return newId;
        } else {
            // WARNING_LOG << "创建样本失败:" << query.lastError().text();
            WARNING_LOG << "创建样本失败:";
        }
    }
    
    delete parsedSample;
    return -1;
}

// 从CSV文本流中读取ProcessTgBigData数据
QList<ProcessTgBigData> ProcessTgBigDataImportWorker::readProcessTgBigDataFromCsv(QTextStream& in, const QString& filePath, int sampleId)
{
    QList<ProcessTgBigData> dataList;
    QFileInfo fileInfo(filePath);
    QString fileName = fileInfo.fileName();
    
    // 添加详细日志
    DEBUG_LOG << "开始从文件读取数据:" << filePath << "样本ID:" << sampleId;
    
    // 读取CSV内容 
    QList<QStringList> csvData; 
    while (!in.atEnd()) { 
        QString line = in.readLine(); 
        QStringList fields = line.split(","); 
        csvData.append(fields); 
    }
    
    if (csvData.isEmpty()) {
        WARNING_LOG << "CSV文件为空:" << filePath;
        return dataList;
    }
    
    DEBUG_LOG << "读取到" << csvData.size() << "行数据";
    
    // 查找"序号"和"天平示数"列 
    int serialNoColIndex = -1; 
    int weightColIndex = -1; 
    int temperatureColIndex = -1; // 添加温度列索引 
    int headerRowIndex = -1;      // 记录表头行索引

    // 如果启用自定义列：优先使用用户指定的温度列/数据列（1-based -> 0-based）
    // 注意：这里只覆盖“温度/重量”的列选择；序号列仍尽量自动识别，找不到时用行号递增
    if (m_useCustomColumns) {
        const int tempIdx = m_temperatureColumn1Based - 1;
        const int dataIdx = m_dataColumn1Based - 1;
        if (tempIdx >= 0) temperatureColIndex = tempIdx;
        if (dataIdx >= 0) weightColIndex = dataIdx;
        // headerRowIndex 仍需要定位到表头之后开始读数据：先尝试自动识别表头；识别不到就默认从第一行开始读（即 headerRowIndex=0）
    }
    
    // 查找列索引 
    for (int i = 0; i < csvData.size(); i++) { 
        const QStringList& row = csvData.at(i); 
        bool foundSerialNo = false;
        bool foundWeight = (weightColIndex >= 0); // 若已指定重量列，则视为已找到
        
        for (int j = 0; j < row.size(); j++) { 
            QString cellValue = row.at(j).trimmed(); 
            if (cellValue.contains("序号", Qt::CaseInsensitive)) { 
                serialNoColIndex = j;
                foundSerialNo = true;
            } 
            // 若未指定重量列，才通过表头关键字自动寻找重量列
            if (weightColIndex < 0 &&
                (cellValue.contains("天平示数", Qt::CaseInsensitive) || 
                 cellValue.contains("天平克数", Qt::CaseInsensitive) ||
                 cellValue.contains("重量", Qt::CaseInsensitive))) { 
                weightColIndex = j;
                foundWeight = true;
            }
            // 若未指定温度列，才自动寻找温度列
            if (temperatureColIndex < 0 && cellValue.contains("温度", Qt::CaseInsensitive)) { 
                temperatureColIndex = j; 
            }
        } 
        
        // 如果找到了必要的列，记录表头行索引并跳出循环
        if ((foundSerialNo || serialNoColIndex >= 0) && foundWeight) { 
            headerRowIndex = i;
            break; 
        } 
    } 
    
    // 如果找不到必要的列，尝试使用固定列索引
    if (weightColIndex < 0) { 
        WARNING_LOG << "未找到必要的列(序号或重量)，尝试使用固定列索引:" << filePath;
        
        // 根据文件名判断数据类型，设置默认列索引
        if (fileName.contains("JZH") || fileName.contains("JZQ") || fileName.contains("JZZ")) {
            // 假设第一列是序号，第二列是重量
            if (serialNoColIndex < 0) serialNoColIndex = 0;
            weightColIndex = 1;
            if (csvData.size() > 0 && csvData[0].size() > 2) {
                if (temperatureColIndex < 0) temperatureColIndex = 2;  // 假设第三列是温度
            }
            // 假设第一行是表头
            headerRowIndex = 0;
            DEBUG_LOG << "使用固定列索引 - 序号:" << serialNoColIndex << "重量:" << weightColIndex << "温度:" << temperatureColIndex;
        } else {
            WARNING_LOG << "无法确定文件格式，无法继续解析:" << filePath;
            return dataList;
        }
    }

    // 若启用了自定义列，但未找到表头行，则默认从第一行开始读取（headerRowIndex=0）
    if (m_useCustomColumns && headerRowIndex < 0) {
        headerRowIndex = 0;
    }
    
    // 提取数据并保存
    int successCount = 0;
    int failCount = 0;
    
    // 只处理表头行之后的数据
    if (headerRowIndex >= 0) {
        // 从表头行的下一行开始提取数据
        for (int i = headerRowIndex + 1; i < csvData.size(); i++) { 
            const QStringList& row = csvData.at(i); 
            
            // 确保行有足够的列 
            const int requiredMax = qMax(qMax(serialNoColIndex, weightColIndex), temperatureColIndex);
            if (requiredMax < 0 || row.size() <= requiredMax) {
                continue;
            }

            QString serialNoStr;
            if (serialNoColIndex >= 0) {
                serialNoStr = row.at(serialNoColIndex).trimmed();
            }
            const QString weight = row.at(weightColIndex).trimmed(); 
                
                // 获取温度值（如果有） 
                double temperature = 0.0; 
                if (temperatureColIndex >= 0 && row.size() > temperatureColIndex) { 
                    QString tempStr = row.at(temperatureColIndex).trimmed(); 
                    bool ok = false; 
                    double tempVal = tempStr.toDouble(&ok); 
                    if (ok) temperature = tempVal; 
                } 
                
                // 跳过空行和包含表头文字的行
                if (!weight.isEmpty() &&
                    !serialNoStr.contains("序号", Qt::CaseInsensitive) &&
                    !weight.contains("天平示数", Qt::CaseInsensitive) && 
                    !weight.contains("天平克数", Qt::CaseInsensitive) &&
                    !weight.contains("重量", Qt::CaseInsensitive)) { 
                    
                    // 转换为数值 
                    bool serialNoOk = false; 
                    int serialNoVal = 0;
                    if (serialNoColIndex >= 0 && !serialNoStr.isEmpty()) {
                        serialNoVal = serialNoStr.toInt(&serialNoOk);
                    } else {
                        // 若未能识别到序号列，则使用行号递增（从0开始）
                        serialNoVal = dataList.size();
                        serialNoOk = true;
                    }
                    
                    bool weightOk = false; 
                    double weightVal = weight.toDouble(&weightOk); 
                    
                    if (serialNoOk && weightOk) { 
                        // 创建数据对象并添加到批量保存列表 
                        ProcessTgBigData data; 
                        data.setSampleId(sampleId); 
                        data.setSerialNo(serialNoVal); 
                        data.setWeight(weightVal); 
                        data.setTemperature(temperature); 
                        data.setSourceName(fileName); 
                        data.setCreatedAt(QDateTime::currentDateTime()); 
                        
                        dataList.append(data); 
                        successCount++; 
                    } else { 
                        WARNING_LOG << "数据转换失败:" << serialNoStr << weight; 
                        failCount++; 
                    } 
                } 
        }
    }
    
    // 每100条数据输出一次日志
    if (dataList.size() % 100 == 0) {
        DEBUG_LOG << "已处理" << dataList.size() << "条数据";
    }
    
    DEBUG_LOG << "文件处理完成，共提取" << dataList.size() << "条有效数据";
    
    return dataList;
}

// 从文件名解析分组ID
QString ProcessTgBigDataImportWorker::parseGroupIdFromFilename(const QString& fileName)
{
        // 默认使用文件名作为分组ID
        return fileName;
}
