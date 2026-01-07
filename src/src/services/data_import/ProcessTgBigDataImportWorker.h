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
    
    // 线程独立的数据库连接
    QSqlDatabase m_threadDb;
    ProcessTgBigDataDAO* m_processTgBigDataDao = nullptr;
    SingleTobaccoSampleDAO* m_singleTobaccoSampleDao = nullptr;
    

    QMap<QString, QStringList> m_processTgBigDataFileGroups;
};

#endif // PROCESSTGBIGDATAIMPORTWORKER_H