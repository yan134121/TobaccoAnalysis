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
                shortCode,
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

    SingleTobaccoSampleData newSample;
    newSample.setShortCode(shortCode);
    newSample.setProjectName(sampleData.getProjectName());
    newSample.setBatchCode(sampleData.getBatchCode());
    newSample.setParallelNo(1);
    newSample.setDetectDate(m_detectDate);
    QDateTime now = QDateTime::currentDateTime();
    newSample.setCreatedAt(now);
    newSample.setYear(now.date().year());
    newSample.setSampleName(QString("%1-%2-%3-%4").arg(
        newSample.getProjectName(),
        newSample.getBatchCode(),
        shortCode,
        QString::number(newSample.getParallelNo())));

    DEBUG_LOG << "创建新小热重（原始数据）样本:"
              << "short_code=" << shortCode
              << "project_name=" << newSample.getProjectName()
              << "batch_code=" << newSample.getBatchCode()
              << "parallel_no=" << newSample.getParallelNo();

    int newId = m_singleTobaccoSampleDao->insert(newSample);
    if (newId > 0) {
        DEBUG_LOG << "成功创建小热重（原始数据）样本，ID:" << newId;
        return newId;
    }

    WARNING_LOG << "创建小热重（原始数据）样本失败";
    return -1;
}

void TgSmallRawDataImportWorker::run()
{
    QMutexLocker locker(&m_mutex);
    QString filePath = m_filePath;
    QString projectName = m_projectName;
    QString batchCode = m_batchCode;
    int parallelNo = m_parallelNo;
    int temperatureColumn = m_temperatureColumn;
    int dtgColumn = m_dtgColumn;
    QDate detectDate = m_detectDate;
    AppInitializer* appInitializer = m_appInitializer;
    locker.unlock();

    if (!initThreadDatabase()) {
        return;
    }

    if (filePath.isEmpty() || !appInitializer) {
        emit importError("参数无效，无法导入数据");
        closeThreadDatabase();
        return;
    }

    if (temperatureColumn <= 0 || dtgColumn <= 0) {
        emit importError("温度列或DTG列设置无效，请设置为大于0的列号");
        closeThreadDatabase();
        return;
    }

    if (m_filePath.isEmpty()) {
        emit importError("未指定文件路径");
        closeThreadDatabase();
        return;
    }

    emit progressMessage(tr("正在读取Excel文件..."));

    QXlsx::Document xlsx(filePath);
    if (!xlsx.load()) {
        emit importError("无法打开Excel文件，请检查文件格式是否正确");
        closeThreadDatabase();
        return;
    }
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
        sampleData.setProjectName(projectName);
        sampleData.setBatchCode(batchCode);
        sampleData.setShortCode(shortCode);
        sampleData.setParallelNo(parallelNo);
        sampleData.setDetectDate(detectDate);

        int sampleId = createOrGetSample(sampleData);
        if (sampleId <= 0) {
            WARNING_LOG << "无法创建或获取样本ID:" << shortCode;
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
                    emit progressMessage(tr("导入已取消"));
                    closeThreadDatabase();
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

            bool ok1 = false;
            bool ok2 = false;
            double temperature = cellTemp->value().toDouble(&ok1);
            double dtgValue = cellDtg->value().toDouble(&ok2);
            if (!ok1 || !ok2) {
                row++;
                continue;
            }

            TgSmallData data;
            data.setSampleId(sampleId);
            data.setSerialNo(dataList.size());
            data.setTemperature(temperature);
            data.setWeight(0.0);
            data.setTgValue(0.0);
            data.setDtgValue(dtgValue);
            data.setSourceName(QFileInfo(filePath).fileName() + ":" + sheetName);
            dataList.append(data);
            row++;
        }

        if (dataList.isEmpty()) {
            WARNING_LOG << "工作表未读取到有效数据:" << sheetName;
            continue;
        }
        m_tgSmallRawDataDao->removeBySampleId(sampleId);
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
