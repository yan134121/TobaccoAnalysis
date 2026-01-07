#ifndef CHROMATOGRAPHDATAIMPORTWORKER_H
#define CHROMATOGRAPHDATAIMPORTWORKER_H

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
#include "data_access/ChromatographyDataDAO.h"



class AppInitializer;
class ChromatographyDataDAO;
class SingleTobaccoSampleDAO;



// 色谱数据导入工作线程类
class ChromatographDataImportWorker : public QThread
{
    Q_OBJECT
public:
    ChromatographDataImportWorker(QObject* parent = nullptr);
    ~ChromatographDataImportWorker();

    // 设置导入参数，增加检测日期
    void setParameters(const QString& dirPath, 
                      const QString& projectName, 
                      const QString& batchCode, 
                      const QDate& detectDate,
                      const QString& shortCode,
                      int parallelNo,
                      AppInitializer* appInitializer);
    void stop();

protected:
    void run() override;

private:
    QString m_dirPath;
    QString m_projectName;
    QString m_batchCode;
    QDate m_detectDate;
    QString m_shortCode;
    int m_parallelNo;
    AppInitializer* m_appInitializer;
    bool m_stopped;
    QMutex m_mutex;

    ChromatographyDataDAO* m_chromatographDataDao = nullptr;
    
    // 线程独立的数据库连接
    QSqlDatabase m_threadDb;
    SingleTobaccoSampleDAO* m_singleTobaccoSampleDao = nullptr;
    
    // 初始化线程独立的数据库连接
    bool initThreadDatabase();
    // 关闭线程独立的数据库连接
    void closeThreadDatabase();
    
    // 递归查找tic_back.csv文件
    QString findTicBackCsv(const QString& dirPath);
    // 从文件夹名称解析short_code和parallel_no
    bool parseDataFolderName(const QString& folderName, QString& shortCode, int& parallelNo);
    // 处理CSV文件
    bool processCsvFile(const QString& csvPath, int sampleId, const QString& shortCode, int parallelNo);
    // 创建或获取样本ID
    int createOrGetSample(const QString& shortCode, int parallelNo);

signals:
    void progressChanged(int current, int total);
    void progressMessage(const QString& message);
    void importFinished(int successCount, int totalDataCount);
    void importError(const QString& errorMessage);
};


#endif // CHROMATOGRAPHDATAIMPORTWORKER_H
