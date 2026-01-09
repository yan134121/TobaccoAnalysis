#include "third_party/QXlsx/header/xlsxdocument.h"

#include "services/data_import/TgSmallDataImportWorker.h"
#include "core/entities/TgSmallData.h"
#include "data_access/TgSmallDataDAO.h"
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
#include <QApplication>
#include <QFileInfo>




// TgSmallDataImportWorker 实现
TgSmallDataImportWorker::TgSmallDataImportWorker(QObject* parent)
    : QThread(parent), m_parallelNo(1), m_appInitializer(nullptr), m_stopped(false),
      m_tgSmallDataDao(nullptr), m_singleTobaccoSampleDao(nullptr)
{
}

TgSmallDataImportWorker::~TgSmallDataImportWorker()
{
    stop();
    wait();
    
    // 清理资源
    closeThreadDatabase();
}

// 设置导入参数，包括文件路径、烟牌号、批次代码和检测日期
void TgSmallDataImportWorker::setParameters(const QString& filePath, 
                                           const QString& projectName, 
                                           const QString& batchCode, 
                                           const QDate& detectDate,
                                           int parallelNo,
                                           int temperatureColumn,
                                           int dtgColumn,
                                           AppInitializer* appInitializer)
{
    QMutexLocker locker(&m_mutex);
    m_filePath = filePath;
    m_projectName = projectName;
    m_batchCode = batchCode;
    m_detectDate = detectDate;
    m_parallelNo = parallelNo;
    m_temperatureColumn = temperatureColumn;
    m_dtgColumn = dtgColumn;
    m_appInitializer = appInitializer;
    m_stopped = false;
}

void TgSmallDataImportWorker::stop()
{
    QMutexLocker locker(&m_mutex);
    m_stopped = true;
}

// 初始化线程独立的数据库连接
bool TgSmallDataImportWorker::initThreadDatabase()
{
    // 创建线程独立的数据库连接
    if (QSqlDatabase::contains("tgsmall_thread_connection")) {
        m_threadDb = QSqlDatabase::database("tgsmall_thread_connection");
    } else {
        m_threadDb = QSqlDatabase::addDatabase("QMYSQL", "tgsmall_thread_connection");
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
        DEBUG_LOG << "子线程数据库连接信息:";
        DEBUG_LOG << "  连接名称:" << m_threadDb.connectionName();
        DEBUG_LOG << "  连接ID:" << QThread::currentThreadId();
        DEBUG_LOG << "  数据库路径:" << m_threadDb.databaseName();
        DEBUG_LOG << "  连接是否有效:" << m_threadDb.isValid();
        DEBUG_LOG << "  连接是否打开:" << m_threadDb.isOpen();
        
        // 创建线程独立的DAO实例
        m_tgSmallDataDao = new TgSmallDataDAO(m_threadDb);
        m_singleTobaccoSampleDao = new SingleTobaccoSampleDAO(m_threadDb);
        
        return true;
    }
    
    emit importError("无法初始化线程数据库连接: AppInitializer为空");
    return false;
}

// 关闭线程独立的数据库连接
void TgSmallDataImportWorker::closeThreadDatabase()
{
    // 释放DAO资源
    if (m_tgSmallDataDao) {
        delete m_tgSmallDataDao;
        m_tgSmallDataDao = nullptr;
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

// 从sheet名称中提取样本编号 - 复用现有方法
QString TgSmallDataImportWorker::extractShortCodeFromSheetName(const QString& sheetName)
{
    // 尝试匹配四位数字
    QRegularExpression re("\\b(\\d{4})\\b");
    QRegularExpressionMatch match = re.match(sheetName);
    if (match.hasMatch()) {
        QString code = match.captured(1);
        DEBUG_LOG << "从工作表名称中提取到四位数字编号:" << code;
        return code;
    }
    
    // 尝试匹配特定格式，如"Sample_XXXX"或"XXXX_Data"
    QRegularExpression reFormat("(?:Sample_|^)(\\d+)(?:_Data|$)");
    QRegularExpressionMatch formatMatch = reFormat.match(sheetName);
    if (formatMatch.hasMatch()) {
        QString code = formatMatch.captured(1);
        DEBUG_LOG << "从特定格式中提取到编号:" << code;
        return code;
    }
    
    // 尝试匹配任意数字
    QRegularExpression reAnyNumber("(\\d+)");
    QRegularExpressionMatch anyMatch = reAnyNumber.match(sheetName);
    if (anyMatch.hasMatch()) {
        QString code = anyMatch.captured(1);
        DEBUG_LOG << "从工作表名称中提取到数字编号:" << code;
        return code;
    }
    
    // 如果没有找到数字，使用sheet名称的前4个字符
    QString code = sheetName.left(4);
    if (code.length() < 4) {
        code = code.leftJustified(4, '0');
    }
    
    DEBUG_LOG << "未找到数字，使用前4个字符作为编号:" << code;
    return code;
}

// 创建或获取样本ID - 使用线程独立的DAO
int TgSmallDataImportWorker::createOrGetSample(const SingleTobaccoSampleData &sampleData)
{
    // 使用线程独立的DAO实例
    if (!m_singleTobaccoSampleDao) {
        WARNING_LOG << "线程独立的SingleTobaccoSampleDAO实例未初始化";
        return -1;
    }
    
    // 获取Service实例 - 这里仍然使用主线程的Service，因为Service主要是业务逻辑，不直接操作数据库
    SingleTobaccoSampleService* service = m_appInitializer->getSingleTobaccoSampleService();
    if (!service) {
        WARNING_LOG << "无法获取SingleTobaccoSampleService实例";
        return -1;
    }
    
    // 对于小热重，我们只使用short_code进行查询，其他字段使用默认值
    QString shortCode = sampleData.getShortCode();
    DEBUG_LOG << "小热重样本查询，使用short_code:" << shortCode;
    
    // 使用线程独立的DAO直接查询数据库
    QList<SingleTobaccoSampleData> existingSamples = m_singleTobaccoSampleDao->queryByProjectNameAndBatchCodeAndShortCodeAndParallelNo(
        sampleData.getProjectName(), sampleData.getBatchCode(), shortCode, sampleData.getParallelNo());

    // 检查查询结果
    DEBUG_LOG << "查询结果数量:" << existingSamples.size();
    
    if (!existingSamples.isEmpty()) {
        // 如果样本已存在，先取出第一条记录
        SingleTobaccoSampleData existingSample = existingSamples.first();
        int existingId = existingSample.getId();
        DEBUG_LOG << "找到现有样本，ID:" << existingId;
        


        // 确认样本ID有效
        if (existingId <= 0) {
            WARNING_LOG << "现有样本ID无效:" << existingId;
            return -1;
        }
        
        // 如果历史数据中 sample_name 为空，则按统一规则补全样本名称
        if (existingSample.getSampleName().trimmed().isEmpty()) {
            QString sampleName = QString("%1-%2-%3-%4").arg(
                existingSample.getProjectName(),
                existingSample.getBatchCode(),
                shortCode,
                QString::number(existingSample.getParallelNo())
            );
            existingSample.setSampleName(sampleName);
            if (!m_singleTobaccoSampleDao->update(existingSample)) {
                WARNING_LOG << "更新小热重样本名称失败, ID:" << existingId;
            } else {
                DEBUG_LOG << "补全小热重样本名称, ID:" << existingId
                          << " sample_name=" << sampleName;
            }
        }
        
        return existingId;
    } else {
        // 如果样本不存在，创建新样本
        SingleTobaccoSampleData newSample;
        
        // 设置short_code
        newSample.setShortCode(shortCode);
        
        // 设置默认值
        newSample.setProjectName(sampleData.getProjectName());  // 默认项目名称
        newSample.setBatchCode(sampleData.getBatchCode());      // 默认批次代码
        newSample.setParallelNo(1);                            // 默认平行样编号
        newSample.setDetectDate(m_detectDate);                  // 设置检测日期
        
        // 设置创建时间
        QDateTime now = QDateTime::currentDateTime();
        newSample.setCreatedAt(now);
        
        // 设置默认年份为当前年份，避免年份验证错误
        newSample.setYear(now.date().year());
        
        // 按统一规范生成样本名称：项目-批次-短码-平行号
        QString sampleName = QString("%1-%2-%3-%4").arg(
            newSample.getProjectName(),
            newSample.getBatchCode(),
            shortCode,
            QString::number(newSample.getParallelNo())
        );
        newSample.setSampleName(sampleName);
        
        DEBUG_LOG << "创建新小热重样本:" 
                 << "short_code=" << shortCode
                 << "project_name=" << newSample.getProjectName()
                 << "batch_code=" << newSample.getBatchCode()
                 << "parallel_no=" << newSample.getParallelNo();
        
        // 使用线程独立的DAO直接插入新样本
        int newId = m_singleTobaccoSampleDao->insert(newSample);
        if (newId > 0) {
            // 成功插入新样本，直接返回新ID
            DEBUG_LOG << "成功创建小热重样本，ID:" << newId;
            return newId;
        } else {
            WARNING_LOG << "创建小热重样本失败";
        }
    }
    
    WARNING_LOG << "无法创建或获取小热重样本ID";
    return -1;
}

void TgSmallDataImportWorker::run()
{
    QMutexLocker locker(&m_mutex);
    QString filePath = m_filePath;
    QString projectName = m_projectName;
    QString batchCode = m_batchCode;
    int parallelNo = m_parallelNo;
    int temperatureColumn = m_temperatureColumn;
    int dtgColumn = m_dtgColumn;
    AppInitializer* appInitializer = m_appInitializer;
    locker.unlock();
    
    // 初始化线程独立的数据库连接
    if (!initThreadDatabase()) {
        return; // 初始化失败，直接返回
    }
    
    if (filePath.isEmpty() || !appInitializer) {
        emit importError("参数无效，无法导入数据");
        return;
    }
    
    if (temperatureColumn <= 0 || dtgColumn <= 0) {
        emit importError("温度列或DTG列设置无效，请设置为大于0的列号");
        return;
    }

    emit progressMessage("正在打开Excel文件...");
    QXlsx::Document xlsx(filePath);
    if (!xlsx.load()) {
        emit importError("无法打开Excel文件，请检查文件格式是否正确");
        return;
    }

    QStringList sheetNames = xlsx.sheetNames();
    if (sheetNames.isEmpty()) {
        emit importError("Excel文件中没有找到任何工作表");
        return;
    }

    emit progressMessage("开始导入数据...");
    emit progressChanged(0, sheetNames.size());

    int successCount = 0;
    int totalDataCount = 0;

    for (int i = 0; i < sheetNames.size(); i++) {
        {
            QMutexLocker locker(&m_mutex);
            if (m_stopped) {
                emit importError("用户取消了导入操作");
                return;
            }
        }

        QString sheetName = sheetNames[i];
        emit progressMessage("正在处理工作表: " + sheetName);
        emit progressChanged(i, sheetNames.size());

        xlsx.selectSheet(sheetName);

        QString shortCode = extractShortCodeFromSheetName(sheetName);

        SingleTobaccoSampleData sampleData;
        sampleData.setProjectName(projectName);
        sampleData.setBatchCode(batchCode);
        sampleData.setShortCode(shortCode);
        sampleData.setParallelNo(parallelNo);

        int sampleId = createOrGetSample(sampleData);
        if (sampleId <= 0) {
            emit progressMessage("警告: 无法创建或获取有效的样本ID，跳过工作表: " + sheetName);
            continue;
        }

        QList<TgSmallData> dataList;
        int row = 2;
        int maxEmptyRows = 5;
        int emptyRowCount = 0;

        while (emptyRowCount < maxEmptyRows && row < 1000) {
            {
                QMutexLocker locker(&m_mutex);
                if (m_stopped) {
                    emit importError("用户取消了导入操作");
                    return;
                }
            }

            std::shared_ptr<QXlsx::Cell> cellTemp = xlsx.cellAt(row, temperatureColumn);
            std::shared_ptr<QXlsx::Cell> cellDtg = xlsx.cellAt(row, dtgColumn);

            if (!cellTemp || !cellDtg || cellTemp->value().isNull() || cellDtg->value().isNull()) {
                emptyRowCount++;
                row++;
                continue;
            }

            emptyRowCount = 0;

            bool tempOk = false;
            bool dtgOk = false;
            double temperature = cellTemp->value().toDouble(&tempOk);
            double dtgValue = cellDtg->value().toDouble(&dtgOk);

            if (tempOk && dtgOk) {
                TgSmallData data;
                data.setSampleId(sampleId);
                data.setSerialNo(dataList.size());
                data.setTemperature(temperature);
                data.setWeight(0.0);
                data.setTgValue(0.0);
                data.setDtgValue(dtgValue);
                data.setSourceName(QFileInfo(filePath).fileName() + ":" + sheetName);

                dataList.append(data);
            }

            row++;
        }

        emit progressMessage("工作表 " + sheetName + " 共读取了 " + QString::number(dataList.size()) + " 条数据");

        if (!dataList.isEmpty()) {
            if (m_tgSmallDataDao) {
                m_tgSmallDataDao->removeBySampleId(sampleId);

                bool saveResult = m_tgSmallDataDao->insertBatch(dataList);
                if (saveResult) {
                    totalDataCount += dataList.size();
                    successCount++;
                } else {
                    emit progressMessage("警告: 保存数据失败，工作表: " + sheetName);
                }
            } else {
                emit progressMessage("警告: 数据访问对象未初始化，无法保存数据");
            }
        }
    }

    emit progressChanged(sheetNames.size(), sheetNames.size());
    emit importFinished(successCount, totalDataCount);
}
