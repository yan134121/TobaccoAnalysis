#ifndef TGBIGDATAIMPORTWORKER_H
#define TGBIGDATAIMPORTWORKER_H

#include <QWidget>
#include <QMessageBox> // 用于显示消息框
#include <QList>       // 用于存储选中的ID
#include <QMap>        // 用于构建查询条件
#include <QVariant>    // 用于查询条件的值
#include <QStringList> // 用于保存文件路径列表
#include <QThread>     // 用于多线程处理
#include <QMutex>      // 用于线程同步
#include <QWaitCondition> // 用于线程等待
#include <QSqlDatabase>
#include <QDate>


#include "SingleTobaccoSampleData.h"
#include "TgBigData.h"
#include "TgSmallData.h"
#include "ChromatographyData.h"
#include "data_access/TgBigDataDAO.h"


class AppInitializer;
class TgBigDataDAO;
class SingleTobaccoSampleDAO;



// 大热重数据导入工作线程类
class TgBigDataImportWorker : public QThread
{
    Q_OBJECT
public:
    TgBigDataImportWorker(QObject* parent = nullptr);
    ~TgBigDataImportWorker();

    // 设置导入参数，增加检测日期
    void setParameters(const QString& dirPath, 
                       const QString& projectName, 
                       const QString& batchCode, 
                       const QDate& detectDate,
                       AppInitializer* appInitializer);
    void stop();

protected:
    void run() override;

signals:
    void progressChanged(int current, int total);
    void progressMessage(const QString& message);
    void importFinished(int successCount, int totalDataCount);
    void importError(const QString& errorMessage);
    
private:
    QString m_dirPath;
    AppInitializer* m_appInitializer = nullptr;
    bool m_stopped = false;
    QMutex m_mutex;
    QString m_projectName;
    QString m_batchCode;
    QDate m_detectDate;
    int m_parallelNo;
    
    // 线程独立的数据库连接
    QSqlDatabase m_threadDb;
    TgBigDataDAO* m_tgBigDataDao = nullptr;
    SingleTobaccoSampleDAO* m_singleTobaccoSampleDao = nullptr;
    
    // 初始化线程独立的数据库连接
    bool initThreadDatabase();
    // 关闭线程独立的数据库连接
    void closeThreadDatabase();
    
    // 创建或获取样本ID
    int createOrGetSample(const QString& fileName);
    
    // 从文件名解析样本信息
    bool parseSampleInfoFromFilename(const QString& fileName, QString& shortCode, int& parallelNo);
    
    // 构建分组的CSV路径
    QMap<QString, QStringList> buildGroupedCsvPaths(const QString& dirPath);
    
    // 从文件名解析组ID
    QString parseGroupIdFromFilename(const QString& fileName);
    
    // 从CSV读取大热重数据
    QList<TgBigData> readTgBigDataFromCsv(QTextStream& in, const QString& filePath, int sampleId);

        // QString m_dirPath;
    // AppInitializer* m_appInitializer;
    // bool m_stopped;
    // QMutex m_mutex;
    // QSqlDatabase m_threadDb;
    // TgBigDataDAO* m_tgBigDataDao;
    // SingleTobaccoSampleDAO* m_singleTobaccoSampleDao;
    
};


#endif // TGBIGDATAIMPORTWORKER_H
