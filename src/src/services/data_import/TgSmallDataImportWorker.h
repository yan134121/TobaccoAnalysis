#ifndef TGSMALLDATAIMPORTWORKER_H
#define TGSMALLDATAIMPORTWORKER_H

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
class TgSmallDataDAO;
class SingleTobaccoSampleDAO;


// 小热重数据导入工作线程类
class TgSmallDataImportWorker : public QThread
{
    Q_OBJECT
public:
    TgSmallDataImportWorker(QObject* parent = nullptr);
    ~TgSmallDataImportWorker();

    // 设置导入参数，增加检测日期
    void setParameters(const QString& filePath, 
                      const QString& projectName, 
                      const QString& batchCode, 
                      const QDate& detectDate,
                      int parallelNo,
                      int temperatureColumn,
                      int dtgColumn,
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
    QString m_filePath;
    QString m_projectName;
    QString m_batchCode;
    QDate m_detectDate;
    int m_parallelNo;
    int m_temperatureColumn = 1;
    int m_dtgColumn = 3;
    AppInitializer* m_appInitializer;
    bool m_stopped;
    QMutex m_mutex;
    
    // 线程独立的数据库连接和DAO
    QSqlDatabase m_threadDb;
    TgSmallDataDAO* m_tgSmallDataDao = nullptr;
    SingleTobaccoSampleDAO* m_singleTobaccoSampleDao = nullptr;
    
    // 初始化线程独立的数据库连接
    bool initThreadDatabase();
    // 关闭线程独立的数据库连接
    void closeThreadDatabase();

    // 从sheet名称中提取样本编号
    QString extractShortCodeFromSheetName(const QString& sheetName);
    // 创建或获取样本ID
    int createOrGetSample(const SingleTobaccoSampleData& sampleData);
};


#endif // TGSMALLDATAIMPORTWORKER_H
