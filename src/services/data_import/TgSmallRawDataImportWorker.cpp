#include "third_party/QXlsx/header/xlsxdocument.h"

#include "services/data_import/TgSmallRawDataImportWorker.h"
#include "core/entities/TgSmallData.h"
#include "data_access/TgSmallRawDataDAO.h"
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

TgSmallRawDataImportWorker::TgSmallRawDataImportWorker(QObject* parent)
    : QThread(parent)
{
}

TgSmallRawDataImportWorker::~TgSmallRawDataImportWorker()
{
    stop();
    wait();
    closeThreadDatabase();
}

void TgSmallRawDataImportWorker::setParameters(const QString& filePath,
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

void TgSmallRawDataImportWorker::stop()
{
    QMutexLocker locker(&m_mutex);
    m_stopped = true;
}

bool TgSmallRawDataImportWorker::initThreadDatabase()
{
    if (QSqlDatabase::contains("tgsmall_raw_thread_connection")) {
        m_threadDb = QSqlDatabase::database("tgsmall_raw_thread_connection");
    } else {
        m_threadDb = QSqlDatabase::addDatabase("QMYSQL", "tgsmall_raw_thread_connection");
    }

    if (m_appInitializer) {
        QSqlDatabase mainDb = DatabaseConnector::getInstance().getDatabase();
        m_threadDb.setHostName(mainDb.hostName());
        m_threadDb.setDatabaseName(mainDb.databaseName());
        m_threadDb.setUserName(mainDb.userName());
        m_threadDb.setPassword(mainDb.password());
        m_threadDb.setPort(mainDb.port());

        if (!m_threadDb.open()) {
            emit importError("无法打开线程数据库连接: " + m_threadDb.lastError().text());
            return false;
        }

        DEBUG_LOG << "子线程数据库连接信息:";
        DEBUG_LOG << "  连接名称:" << m_threadDb.connectionName();
        DEBUG_LOG << "  连接ID:" << QThread::currentThreadId();
        DEBUG_LOG << "  数据库路径:" << m_threadDb.databaseName();
        DEBUG_LOG << "  连接是否有效:" << m_threadDb.isValid();
        DEBUG_LOG << "  连接是否打开:" << m_threadDb.isOpen();

        m_tgSmallRawDataDao = new TgSmallRawDataDAO(m_threadDb);
        m_singleTobaccoSampleDao = new SingleTobaccoSampleDAO(m_threadDb);

        return true;
    }

    emit importError("无法初始化线程数据库连接: AppInitializer为空");
    return false;
}

void TgSmallRawDataImportWorker::closeThreadDatabase()
{
    delete m_tgSmallRawDataDao;
    m_tgSmallRawDataDao = nullptr;

    delete m_singleTobaccoSampleDao;
    m_singleTobaccoSampleDao = nullptr;

    if (m_threadDb.isOpen()) {
        m_threadDb.close();
    }
}

QString TgSmallRawDataImportWorker::extractShortCodeFromSheetName(const QString& sheetName)
{
    QRegularExpression re("\\b(\\d{4})\\b");
    QRegularExpressionMatch match = re.match(sheetName);
    if (match.hasMatch()) {
        QString code = match.captured(1);
        DEBUG_LOG << "从工作表名称中提取到四位数字编号:" << code;
        return code;
    }

    QRegularExpression reFormat("(?:Sample_|^)(\\d+)(?:_Data|$)");
    QRegularExpressionMatch formatMatch = reFormat.match(sheetName);
    if (formatMatch.hasMatch()) {
        QString code = formatMatch.captured(1);
        DEBUG_LOG << "从特定格式中提取到编号:" << code;
        return code;
    }

    QRegularExpression reAnyNumber("(\\d+)");
    QRegularExpressionMatch anyMatch = reAnyNumber.match(sheetName);
    if (anyMatch.hasMatch()) {
        QString code = anyMatch.captured(1);
        DEBUG_LOG << "从工作表名称中提取到数字编号:" << code;
        return code;
    }

    QString code = sheetName.left(4);
    if (code.length() < 4) {
        code = code.leftJustified(4, '0');
    }

    DEBUG_LOG << "未找到数字，使用前4个字符作为编号:" << code;
    return code;
}

int TgSmallRawDataImportWorker::createOrGetSample(const SingleTobaccoSampleData& sampleData)
{
    if (!m_singleTobaccoSampleDao) {
        WARNING_LOG << "线程独立的SingleTobaccoSampleDAO实例未初始化";
        return -1;
    }

    SingleTobaccoSampleService* service = m_appInitializer->getSingleTobaccoSampleService();
    if (!service) {
        WARNING_LOG << "无法获取SingleTobaccoSampleService实例";
        return -1;
    }

    QString shortCode = sampleData.getShortCode();
    DEBUG_LOG << "小热重（原始数据）样本查询，使用short_code:" << shortCode;

    QList<SingleTobaccoSampleData> existingSamples = m_singleTobaccoSampleDao->queryByProjectNameAndBatchCodeAndShortCodeAndParallelNo(
        sampleData.getProjectName(), sampleData.getBatchCode(), shortCode, sampleData.getParallelNo());

    DEBUG_LOG << "查询结果数量:" << existingSamples.size();

    if (!existingSamples.isEmpty()) {
        SingleTobaccoSampleData existingSample = existingSamples.first();
        int existingId = existingSample.getId();
        DEBUG_LOG << "找到现有样本，ID:" << existingId;

        if (existingId <= 0) {
            WARNING_LOG << "现有样本ID无效:" << existingId;
            return -1;
        }

        if (existingSample.getSampleName().trimmed().isEmpty()) {
            QString sampleName = QString("%1-%2-%3-%4").arg(
                existingSample.getProjectName(),
                existingSample.getBatchCode(),
                existingSample.getShortCode(),
                QString::number(existingSample.getParallelNo()));

            existingSample.setSampleName(sampleName);
            if (!m_singleTobaccoSampleDao->update(existingSample)) {
                WARNING_LOG << "更新小热重（原始数据）样本名称失败, ID:" << existingId;
            } else {
                DEBUG_LOG << "补全小热重（原始数据）样本名称, ID:" << existingId
                          << "newName:" << sampleName;
            }
        }

        return existingId;
    }

    SingleTobaccoSampleData newSample = sampleData;
    newSample.setSampleName(QString("%1-%2-%3-%4").arg(
        sampleData.getProjectName(),
        sampleData.getBatchCode(),
        sampleData.getShortCode(),
        QString::number(sampleData.getParallelNo())));

    int newId = m_singleTobaccoSampleDao->create(newSample);
    if (newId > 0) {
        DEBUG_LOG << "成功创建小热重（原始数据）样本，ID:" << newId;
        return newId;
    }

    WARNING_LOG << "创建小热重（原始数据）样本失败";
    return -1;
}

void TgSmallRawDataImportWorker::run()
{
    if (!initThreadDatabase()) {
        return;
    }

    if (m_filePath.isEmpty()) {
        emit importError("未指定文件路径");
        closeThreadDatabase();
        return;
    }

    emit progressMessage(tr("正在读取Excel文件..."));

    QXlsx::Document xlsx(m_filePath);
    QStringList sheetNames = xlsx.sheetNames();
    if (sheetNames.isEmpty()) {
        emit importError("Excel文件不包含任何工作表");
        closeThreadDatabase();
        return;
    }

    int successCount = 0;
    int totalDataCount = 0;
    int processedSheets = 0;

    for (const QString& sheetName : sheetNames) {
        {
            QMutexLocker locker(&m_mutex);
            if (m_stopped) {
                emit progressMessage(tr("导入已取消"));
                closeThreadDatabase();
                return;
            }
        }

        processedSheets++;
        emit progressChanged(processedSheets, sheetNames.size());
        emit progressMessage(tr("处理工作表: %1").arg(sheetName));

        xlsx.selectSheet(sheetName);
        QString shortCode = extractShortCodeFromSheetName(sheetName);
        if (shortCode.isEmpty()) {
            WARNING_LOG << "无法从工作表名称中提取短码:" << sheetName;
            continue;
        }

        SingleTobaccoSampleData sampleData;
        sampleData.setProjectName(m_projectName);
        sampleData.setBatchCode(m_batchCode);
        sampleData.setShortCode(shortCode);
        sampleData.setParallelNo(m_parallelNo);
        sampleData.setDetectDate(m_detectDate);

        int sampleId = createOrGetSample(sampleData);
        if (sampleId <= 0) {
            WARNING_LOG << "无法创建或获取样本ID:" << shortCode;
            continue;
        }

        QList<TgSmallData> dataList;
        int row = 1;
        while (true) {
            QVariant tempVar = xlsx.read(row, m_temperatureColumn);
            QVariant dtgVar = xlsx.read(row, m_dtgColumn);
            if (tempVar.isNull() && dtgVar.isNull()) {
                break;
            }

            bool ok1, ok2;
            double temperature = tempVar.toDouble(&ok1);
            double dtgValue = dtgVar.toDouble(&ok2);
            if (!ok1 || !ok2) {
                row++;
                continue;
            }

            TgSmallData data;
            data.setSampleId(sampleId);
            data.setSerialNo(row);
            data.setTemperature(temperature);
            data.setDtgValue(dtgValue);
            data.setSourceName(QFileInfo(m_filePath).fileName());
            dataList.append(data);
            row++;
        }

        if (dataList.isEmpty()) {
            WARNING_LOG << "工作表未读取到有效数据:" << sheetName;
            continue;
        }

        if (!m_tgSmallRawDataDao->insertBatch(dataList)) {
            WARNING_LOG << "批量插入小热重（原始数据）失败:" << sheetName;
            continue;
        }

        successCount++;
        totalDataCount += dataList.size();
    }

    emit importFinished(successCount, totalDataCount);
    closeThreadDatabase();
}
