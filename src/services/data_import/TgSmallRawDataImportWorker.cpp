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
    // 兼容旧接口：视为“按指定列导入”，X=temperatureColumn（0表示递增生成），Y=dtgColumn（本worker现在作为weight列）
    m_useCustomColumns = true;
    m_xColumn1BasedOr0 = temperatureColumn;
    m_yColumn1Based = dtgColumn;
    m_appInitializer = appInitializer;
    m_stopped = false;
}

void TgSmallRawDataImportWorker::setParameters(const QString& filePath,
                                              const QString& projectName,
                                              const QString& batchCode,
                                              const QDate& detectDate,
                                              int parallelNo,
                                              bool useCustomColumns,
                                              int xColumn1BasedOr0,
                                              int yColumn1Based,
                                              AppInitializer* appInitializer)
{
    QMutexLocker locker(&m_mutex);
    m_filePath = filePath;
    m_projectName = projectName;
    m_batchCode = batchCode;
    m_detectDate = detectDate;
    m_parallelNo = parallelNo;
    m_useCustomColumns = useCustomColumns;
    m_xColumn1BasedOr0 = xColumn1BasedOr0;
    m_yColumn1Based = yColumn1Based;
    // 保留旧字段
    m_temperatureColumn = xColumn1BasedOr0;
    m_dtgColumn = yColumn1Based;
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
    bool useCustomColumns = m_useCustomColumns;
    int xColumn1BasedOr0 = m_xColumn1BasedOr0;
    int yColumn1Based = m_yColumn1Based;
    QDate detectDate = m_detectDate;
    AppInitializer* appInitializer = m_appInitializer;
    locker.unlock();

    DEBUG_LOG << "小热重（原始数据）导入开始:"
              << "filePath=" << filePath
              << "projectName=" << projectName
              << "batchCode=" << batchCode
              << "parallelNo=" << parallelNo
              << "detectDate=" << detectDate.toString(Qt::ISODate)
              << "xColumn=" << xColumn1BasedOr0
              << "yColumn(weight)=" << yColumn1Based
              << "useCustomColumns=" << useCustomColumns;

    if (!initThreadDatabase()) {
        return;
    }

    if (filePath.isEmpty() || !appInitializer) {
        emit importError("参数无效，无法导入数据");
        closeThreadDatabase();
        return;
    }

    if (useCustomColumns) {
        if (xColumn1BasedOr0 < 0 || yColumn1Based <= 0) {
            emit importError("X轴列或Y轴列设置无效：X列可为0或大于0，Y列必须大于0");
            closeThreadDatabase();
            return;
        }
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
    DEBUG_LOG << "Excel工作表数量:" << sheetNames.size()
              << "sheetNames=" << sheetNames;
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
        QXlsx::Worksheet* worksheet = xlsx.currentWorksheet();
        if (!worksheet) {
            WARNING_LOG << "无法获取工作表对象:" << sheetName;
            continue;
        }

        DEBUG_LOG << "开始处理工作表:" << sheetName
                  << "xColumn=" << xColumn1BasedOr0
                  << "yColumn(weight)=" << yColumn1Based
                  << "useCustomColumns=" << useCustomColumns;
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

        // 预先分配容量以提高性能
        QList<TgSmallData> dataList;
        dataList.reserve(12000);
        
        // 缓存sourceName字符串，避免重复构造
        QString sourceName = QFileInfo(filePath).fileName() + ":" + sheetName;
        
        int row = 2;
        int maxEmptyRows = 5;
        int emptyRowCount = 0;
        int nullValueRows = 0;
        int nonNumericRows = 0;
        int loggedNullRows = 0;
        int loggedNonNumericRows = 0;
        const int checkInterval = 100; // 每100行检查一次停止标志

        // 不选择：自动识别模式，扫描表头行，找 温度 / 重量（两者都必须找到）
        int headerRow = 1;
        int temperatureCol1Based = 0;
        int weightCol1Based = 0;
        if (!useCustomColumns) {
            const int maxScanRows = 30;
            const int maxScanCols = 80;
            for (int r = 1; r <= maxScanRows; ++r) {
                bool foundAny = false;
                for (int c = 1; c <= maxScanCols; ++c) {
                    const QString v = worksheet->read(r, c).toString().trimmed();
                    if (v.isEmpty()) continue;
                    const QString lower = v.toLower();
                    if (lower.contains("temp") || v.contains("温度")) {
                        temperatureCol1Based = c;
                        foundAny = true;
                    }
                    if (lower.contains("weight") || v.contains("重量") || v.contains("天平示数") || v.contains("天平克数")) {
                        weightCol1Based = c;
                        foundAny = true;
                    }
                }
                if (temperatureCol1Based > 0 && weightCol1Based > 0 && foundAny) {
                    headerRow = r;
                    break;
                }
            }
            if (temperatureCol1Based <= 0 || weightCol1Based <= 0) {
                emit importError("输入文件格式不符合：未找到温度列或重量列");
                closeThreadDatabase();
                return;
            }
            row = headerRow + 1;
        } else {
            weightCol1Based = yColumn1Based;
            row = 2;
        }

        while (emptyRowCount < maxEmptyRows && row < 12000) {
            // 减少停止检查频率，每100行检查一次
            if (row % checkInterval == 0) {
                QMutexLocker locker(&m_mutex);
                if (m_stopped) {
                    emit progressMessage(tr("导入已取消"));
                    closeThreadDatabase();
                    return;
                }
            }

            // Y: weight（必须）
            QVariant weightValue = worksheet->read(row, weightCol1Based);
            if (weightValue.isNull() || weightValue.toString().trimmed().isEmpty()) {
                if (loggedNullRows < 5) {
                    DEBUG_LOG << "空单元格或缺失单元格，跳过行:" << row;
                    loggedNullRows++;
                }
                nullValueRows++;
                emptyRowCount++;
                row++;
                continue;
            }

            emptyRowCount = 0;

            // 转换为数值
            bool ok2 = false;
            double weightDouble = weightValue.toDouble(&ok2);
            
            if (!ok2) {
                if (loggedNonNumericRows < 5) {
                    DEBUG_LOG << "非数字数据，跳过行:" << row
                              << "weightValue=" << weightValue.toString();
                    loggedNonNumericRows++;
                }
                nonNumericRows++;
                row++;
                continue;
            }

            TgSmallData data;
            data.setSampleId(sampleId);

            // X轴处理：
            // - 不选择：X=温度列（必须）
            // - 指定列：xColumn=0 => 自动生成 serial_no 作为X；否则读取指定列作为X
            double xValue = 0.0;
            if (!useCustomColumns) {
                QVariant tempValue = worksheet->read(row, temperatureCol1Based);
                if (tempValue.isNull() || tempValue.toString().trimmed().isEmpty()) {
                    emit importError("输入文件格式不符合：温度列数据为空");
                    closeThreadDatabase();
                    return;
                }
                bool okX = false;
                xValue = tempValue.toDouble(&okX);
                if (!okX) {
                    emit importError("输入文件格式不符合：温度列数据非数字");
                    closeThreadDatabase();
                    return;
                }
                data.setSerialNo(dataList.size());
                data.setTemperature(xValue); // 现有绘图使用 temperature 作为X
            } else {
                if (xColumn1BasedOr0 == 0) {
                    const int serialNoVal = dataList.size();
                    data.setSerialNo(serialNoVal);
                    xValue = static_cast<double>(serialNoVal);
                    data.setTemperature(xValue);
                } else {
                    QVariant xCell = worksheet->read(row, xColumn1BasedOr0);
                    if (xCell.isNull() || xCell.toString().trimmed().isEmpty()) {
                        emptyRowCount++;
                        row++;
                        continue;
                    }
                    bool okX = false;
                    xValue = xCell.toDouble(&okX);
                    if (!okX) {
                        row++;
                        continue;
                    }
                    data.setSerialNo(dataList.size());
                    data.setTemperature(xValue);
                }
            }

            data.setWeight(weightDouble);
            data.setTgValue(0.0);
            data.setDtgValue(0.0);
            data.setSourceName(sourceName);
            dataList.append(data);
            row++;
        }

        if (dataList.isEmpty()) {
            WARNING_LOG << "工作表未读取到有效数据:" << sheetName
                        << "nullValueRows=" << nullValueRows
                        << "nonNumericRows=" << nonNumericRows;
            continue;
        }
        DEBUG_LOG << "工作表解析完成:" << sheetName
                  << "sampleId=" << sampleId
                  << "有效数据行数=" << dataList.size()
                  << "nullValueRows=" << nullValueRows
                  << "nonNumericRows=" << nonNumericRows;
        m_tgSmallRawDataDao->removeBySampleId(sampleId);
        if (!m_tgSmallRawDataDao->insertBatch(dataList)) {
            WARNING_LOG << "批量插入小热重（原始数据）失败:" << sheetName;
            continue;
        }

        successCount++;
        totalDataCount += dataList.size();
    }

    DEBUG_LOG << "小热重（原始数据）导入完成:"
              << "successSheets=" << successCount
              << "totalDataCount=" << totalDataCount;
    emit importFinished(successCount, totalDataCount);
    closeThreadDatabase();
}
