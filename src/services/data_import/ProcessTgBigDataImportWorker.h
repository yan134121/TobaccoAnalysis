#ifndef PROCESSTGBIGDATAIMPORTWORKER_H
#define PROCESSTGBIGDATAIMPORTWORKER_H

#include <QThread>
#include <QMutex>
#include <QString>
#include <QSqlDatabase>

#include "core/entities/ProcessTgBigData.h"
#include "core/entities/SingleTobaccoSampleData.h"
#include "data_access/ProcessTgBigDataDAO.h"

class AppInitializer;
class TgBigDataDAO;
class SingleTobaccoSampleDAO;

// 工序大热重数据导入工作线程类
class ProcessTgBigDataImportWorker : public QThread
{
    Q_OBJECT
public:
    ProcessTgBigDataImportWorker(QObject* parent = nullptr);
    ~ProcessTgBigDataImportWorker();

    // 设置参数：仅目录与初始化器（兼容旧接口）
    void setParameters(const QString& dirPath, AppInitializer* appInitializer);
    // 设置参数：目录、项目名称、批次代码与初始化器（用于覆盖解析出的信息）
    void setParameters(const QString& dirPath, const QString& projectName, const QString& batchCode, AppInitializer* appInitializer);
    // 设置参数：目录、项目名称、批次代码、列选择（可选）与初始化器
    // - useCustomColumns=false：沿用当前自动识别/默认逻辑
    // - useCustomColumns=true ：使用指定的温度列/数据列（1-based）
    void setParameters(const QString& dirPath,
                       const QString& projectName,
                       const QString& batchCode,
                       bool useCustomColumns,
                       int temperatureColumn1Based,
                       int dataColumn1Based,
                       AppInitializer* appInitializer);
    void stop();

    QMap<QString, QStringList> buildGroupedCsvPaths(const QString& directoryPath);
    SingleTobaccoSampleData* parseSampleInfoFromFilename(const QString& filename);
    int createOrGetSample(const QString& filename);
    QList<ProcessTgBigData> readProcessTgBigDataFromCsv(QTextStream& in, const QString& filePath, int sampleId);
    bool initThreadDatabase();
    void closeThreadDatabase();
    QString parseGroupIdFromFilename(const QString& fileName);

protected:
    void run() override;

signals:
    void progressChanged(int current, int total);
    void progressMessage(const QString& message);
    void importFinished(int successCount, int totalDataCount);
    void importError(const QString& errorMessage);

private:
    QString m_dirPath;
    AppInitializer* m_appInitializer;
    bool m_stopped;
    QMutex m_mutex;
    QString m_projectName;
    QString m_batchCode;
    QString m_shortCode;
    int m_parallelNo;

    // 可选的列映射覆盖（用于按用户指定列提取温度/重量）
    bool m_useCustomColumns = false;
    int m_temperatureColumn1Based = -1; // 1-based，<=0 表示无效
    int m_dataColumn1Based = -1;        // 1-based，<=0 表示无效
    
    // 线程独立的数据库连接
    QSqlDatabase m_threadDb;
    ProcessTgBigDataDAO* m_processTgBigDataDao = nullptr;
    SingleTobaccoSampleDAO* m_singleTobaccoSampleDao = nullptr;
    

    QMap<QString, QStringList> m_processTgBigDataFileGroups;
};

#endif // PROCESSTGBIGDATAIMPORTWORKER_H