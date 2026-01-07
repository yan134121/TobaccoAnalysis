


// Qt 核心模块
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QSqlDatabase>
#include <QSqlError>
#include <QDateTime>
#include <QRegularExpression>

// 第三方库
#include "third_party/QXlsx/header/xlsxdocument.h"

// 核心实体类
#include "Logger.h"
#include "core/entities/SingleTobaccoSampleData.h"
#include "core/entities/ChromatographyData.h"
#include "data_access/DatabaseConnector.h"
#include "data_access/SingleTobaccoSampleDAO.h"
#include "services/data_import/ChromatographDataImportWorker.h"
#include "gui/views/SingleMaterialDataWidget.h"
#include "src/core/AppInitializer.h"
#include "data_access/ChromatographyDataDAO.h"

// ChromatographDataImportWorker 实现
ChromatographDataImportWorker::ChromatographDataImportWorker(QObject* parent)
    : QThread(parent), m_parallelNo(1), m_appInitializer(nullptr), m_stopped(false),
      m_chromatographDataDao(nullptr),m_singleTobaccoSampleDao(nullptr)
{
}

ChromatographDataImportWorker::~ChromatographDataImportWorker()
{
    stop();
    wait();
    
    // 清理资源
    closeThreadDatabase();
}

// 设置导入参数，包括目录、烟牌号、批次代码和检测日期
void ChromatographDataImportWorker::setParameters(const QString& dirPath, 
                                               const QString& projectName, 
                                               const QString& batchCode, 
                                               const QDate& detectDate,
                                               const QString& shortCode,
                                               int parallelNo,
                                               AppInitializer* appInitializer)
{
    QMutexLocker locker(&m_mutex);
    m_dirPath = dirPath;
    m_projectName = projectName;
    m_batchCode = batchCode;
    m_detectDate = detectDate;
    m_shortCode = shortCode;
    m_parallelNo = parallelNo;
    m_appInitializer = appInitializer;
    m_stopped = false;
}

void ChromatographDataImportWorker::stop()
{
    QMutexLocker locker(&m_mutex);
    m_stopped = true;
}

// 初始化线程独立的数据库连接
bool ChromatographDataImportWorker::initThreadDatabase()
{
    // 创建线程独立的数据库连接
    if (QSqlDatabase::contains("chrom_thread_connection")) {
        m_threadDb = QSqlDatabase::database("chrom_thread_connection");
    } else {
        m_threadDb = QSqlDatabase::addDatabase("QMYSQL", "chrom_thread_connection");
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
            emit importError("无法打开线程数据库连接: " + m_threadDb.lastError().text());
            return false;
        }
        
        // 打印子线程数据库连接信息
        DEBUG_LOG << "色谱数据导入子线程数据库连接信息:";
        DEBUG_LOG << "  连接名称:" << m_threadDb.connectionName();
        DEBUG_LOG << "  连接ID:" << QThread::currentThreadId();
        DEBUG_LOG << "  数据库路径:" << m_threadDb.databaseName();
        DEBUG_LOG << "  连接是否有效:" << m_threadDb.isValid();
        DEBUG_LOG << "  连接是否打开:" << m_threadDb.isOpen();
        
        // 创建线程独立的DAO实例
        m_chromatographDataDao = new ChromatographyDataDAO(m_threadDb);
        m_singleTobaccoSampleDao = new SingleTobaccoSampleDAO(m_threadDb);
        
        return true;
    }
    
    emit importError("无法初始化线程数据库连接: AppInitializer为空");
    return false;
}

// 关闭线程独立的数据库连接
void ChromatographDataImportWorker::closeThreadDatabase()
{
    // 释放DAO资源
    if (m_chromatographDataDao) {
        delete m_chromatographDataDao;
        m_chromatographDataDao = nullptr;
    }
    // 释放DAO资源
    if (m_singleTobaccoSampleDao) {
        delete m_singleTobaccoSampleDao;
        m_singleTobaccoSampleDao = nullptr;
    }
    
    // 关闭数据库连接
    if (m_threadDb.isOpen()) {
        m_threadDb.close();
    }
}

// 递归查找tic_back.csv文件
QString ChromatographDataImportWorker::findTicBackCsv(const QString& dirPath)
{
    QDir dir(dirPath);
    
    // 首先在当前目录查找
    QStringList filters;
    filters << "tic_back.csv";
    QStringList files = dir.entryList(filters, QDir::Files);
    if (!files.isEmpty()) {
        return dir.absoluteFilePath(files.first());
    }
    
    // 递归查找子目录
    QStringList subdirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    foreach (const QString &subdir, subdirs) {
        QString csvPath = findTicBackCsv(dir.absoluteFilePath(subdir));
        if (!csvPath.isEmpty()) {
            return csvPath;
        }
    }
    
    return QString();
}

// 从文件夹名称解析short_code和parallel_no
bool ChromatographDataImportWorker::parseDataFolderName(const QString& folderName, QString& shortCode, int& parallelNo)
{
    // 解析文件夹名称，例如：2075-2.D、2075-4.D、20901-1.D、20901-2.D
    QRegularExpression folderPattern("^(\\d+)(?:-(\\d+))?\\.D$");
    QRegularExpressionMatch match = folderPattern.match(folderName);
    
    if (match.hasMatch()) {
        shortCode = match.captured(1);
        
        // 检查是否有平行样编号
        if (match.lastCapturedIndex() >= 2 && !match.captured(2).isEmpty()) {
            parallelNo = match.captured(2).toInt();
        } else {
            // 如果没有明确的平行样编号，默认为1
            parallelNo = 1;
        }

        DEBUG_LOG << "folderName:" << folderName << "shortCode:" << shortCode << "parallelNo:" << parallelNo;
        
        return true;
    }
    
    return false;
}

// 处理CSV文件并导入数据
bool ChromatographDataImportWorker::processCsvFile(const QString& csvPath, int sampleId, const QString& shortCode, int parallelNo)
{
    QFile file(csvPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        WARNING_LOG << "无法打开CSV文件:" << csvPath;
        return false;
    }
    
    QTextStream in(&file);
    
    // 查找"Start of data points"行
    bool foundDataStart = false;
    QString line;
    while (!in.atEnd()) {
        line = in.readLine();
        if (line.contains("Start of data points", Qt::CaseInsensitive)) {
            foundDataStart = true;
            break;
        }
    }
    
    if (!foundDataStart) {
        WARNING_LOG << "CSV文件中未找到'Start of data points'标记:" << csvPath;
        file.close();
        return false;
    }
    
    // 准备SQL语句
    QSqlQuery query(m_threadDb);
    query.prepare("INSERT INTO chromatography_data (sample_id, retention_time, response_value, source_filename) "
                 "VALUES (?, ?, ?, ?)");
    
    // 读取数据点 - 从"Start of data points"的下一行开始
    int count = 0;
    // 先读取下一行，跳过标题行
    if (!in.atEnd()) {
        in.readLine(); // 跳过可能的标题行
    }
    
    while (!in.atEnd()) {
        line = in.readLine().trimmed();
        if (line.isEmpty()) {
            continue;
        }
        
        QStringList parts = line.split(',');
        if (parts.size() >= 2) {
            bool ok1 = false, ok2 = false;
            double retentionTime = parts[0].toDouble(&ok1);
            double responseValue = parts[1].toDouble(&ok2);
            
            if (ok1 && ok2) {
                query.bindValue(0, sampleId);
                query.bindValue(1, retentionTime);
                query.bindValue(2, responseValue);
                query.bindValue(3, QFileInfo(csvPath).fileName()); // 使用文件名作为来源
                
                if (!query.exec()) {
                    WARNING_LOG << "插入数据失败:" << query.lastError().text();
                } else {
                    count++;
                }
            }
        }
    }
    
    file.close();
    INFO_LOG << "从" << csvPath << "导入了" << count << "个数据点";
    return count > 0;
}

// 创建或获取样本ID
int ChromatographDataImportWorker::createOrGetSample(const QString& shortCode, int parallelNo)
{
    // 使用线程独立的DAO实例
    if (!m_singleTobaccoSampleDao) {
        WARNING_LOG << "线程独立的SingleTobaccoSampleDAO实例未初始化";
        return -1;
    }
    
    // 使用short_code查询样本
    QList<SingleTobaccoSampleData> existingSamples = m_singleTobaccoSampleDao->queryByProjectNameAndBatchCodeAndShortCodeAndParallelNo( m_projectName, m_batchCode, shortCode, parallelNo);
    
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
            m_projectName,
            m_batchCode,
            shortCode,
            QString::number(parallelNo)
        );
        newSample.setSampleName(sampleName);
        
        DEBUG_LOG << "创建新色谱样本:" 
                 << "short_code=" << shortCode
                 << "project_name=" << newSample.getProjectName()
                 << "batch_code=" << newSample.getBatchCode()
                 << "parallel_no=" << newSample.getParallelNo();
        
        // 使用线程独立的DAO直接插入新样本
        int newId = m_singleTobaccoSampleDao->insert(newSample);
        if (newId > 0) {
            // 成功插入新样本，直接返回新ID
            DEBUG_LOG << "成功创建色谱样本，ID:" << newId;
            return newId;
        } else {
            WARNING_LOG << "创建色谱样本失败";
        }
    }
    
    WARNING_LOG << "无法创建或获取色谱样本ID";
    return -1;
}

void ChromatographDataImportWorker::run()
{
    QMutexLocker locker(&m_mutex);
    QString dirPath = m_dirPath;
    QString projectName = m_projectName;
    QString batchCode = m_batchCode;
    QString shortCode = m_shortCode;
    int parallelNo = m_parallelNo;
    locker.unlock();
    
    // 初始化线程独立的数据库连接
    if (!initThreadDatabase()) {
        return; // 初始化失败，直接返回
    }
    
    emit progressMessage(tr("正在扫描文件夹..."));
    
    // 查找所有符合条件的文件夹和tic_back.csv文件
    QList<QPair<QString, QString>> csvFiles; // 文件路径和文件夹名称的对
    QDir dir(dirPath);
    QStringList folders = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    
    int folderCount = folders.size();
    int currentFolder = 0;
    
    foreach (const QString &folder, folders) {
        // 检查是否被要求停止
        {
            QMutexLocker locker(&m_mutex);
            if (m_stopped) {
                emit importError(tr("用户取消了导入操作"));
                return;
            }
        }
        
        emit progressChanged(currentFolder, folderCount);
        emit progressMessage(tr("正在扫描文件夹: %1").arg(folder));
        
        // 检查文件夹名称格式，例如：2075-2.D、2075-4.D、20901-1.D、20901-2.D
        QRegularExpression folderPattern("^(\\d+)(?:-(\\d+))?\\.D$");
        QRegularExpressionMatch match = folderPattern.match(folder);
        
        if (match.hasMatch()) {
            // 在子文件夹中查找tic_back.csv
            QString folderPath = dir.absoluteFilePath(folder);
            QString csvPath = findTicBackCsv(folderPath);
            
            if (!csvPath.isEmpty()) {
                csvFiles.append(qMakePair(csvPath, folder));
            }
        }
        
        currentFolder++;
    }
    
    if (csvFiles.isEmpty()) {
        emit importError(tr("未找到有效的tic_back.csv文件"));
        return;
    }
    
    // 开始处理找到的CSV文件
    emit progressMessage(tr("正在处理CSV文件..."));
    emit progressChanged(0, csvFiles.size());
    
    int totalFiles = csvFiles.size();
    int processedFiles = 0;
    int successCount = 0;
    int totalDataCount = 0;
    
    // 开始事务
    m_threadDb.transaction();
    
    try {
        foreach (const auto &csvFile, csvFiles) {
            // 检查是否被要求停止
            {
                QMutexLocker locker(&m_mutex);
                if (m_stopped) {
                    m_threadDb.rollback();
                    emit importError(tr("用户取消了导入操作"));
                    return;
                }
            }
            
            emit progressChanged(processedFiles, totalFiles);
            QString csvPath = csvFile.first;
            QString folderName = csvFile.second;
            
            emit progressMessage(tr("正在处理文件: %1").arg(QFileInfo(csvPath).fileName()));
            
            // 从文件夹名称解析short_code和parallel_no
            QString fileShortCode;
            int fileParallelNo = 0;
            
            if (parseDataFolderName(folderName, fileShortCode, fileParallelNo)) {
                // 为每个文件创建一个独立的样本
                int sampleId = createOrGetSample(fileShortCode, fileParallelNo);
                
                if (sampleId > 0) {
                    // 删除该样本的旧数据
                    if (m_chromatographDataDao) {
                        m_chromatographDataDao->removeBySampleId(sampleId);

                        // 处理CSV文件并导入数据
                        if (processCsvFile(csvPath, sampleId, fileShortCode, fileParallelNo)) {
                            successCount++;
                            
                            // 获取导入的数据点数量
                            QSqlQuery countQuery(m_threadDb);
                            countQuery.prepare("SELECT COUNT(*) FROM chromatography_data WHERE sample_id = ?");
                            countQuery.bindValue(0, sampleId);
                            
                            if (countQuery.exec() && countQuery.next()) {
                                totalDataCount += countQuery.value(0).toInt();
                            }
                        }
                    }
                }
            }
            
            processedFiles++;
        }
        
        m_threadDb.commit();
        emit importFinished(successCount, totalDataCount);
        
    } catch (const std::exception &e) {
        m_threadDb.rollback();
        emit importError(tr("导入过程中发生错误: %1").arg(e.what()));
    }
}
