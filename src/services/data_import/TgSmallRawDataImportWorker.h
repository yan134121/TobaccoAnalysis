#ifndef TGSMALLRAWDATAIMPORTWORKER_H
#define TGSMALLRAWDATAIMPORTWORKER_H

#include <QThread>
#include <QMutex>
#include <QSqlDatabase>
#include <QDate>

class AppInitializer;
class TgSmallRawDataDAO;
class SingleTobaccoSampleDAO;
class SingleTobaccoSampleData;

class TgSmallRawDataImportWorker : public QThread
{
    Q_OBJECT
public:
    explicit TgSmallRawDataImportWorker(QObject* parent = nullptr);
    ~TgSmallRawDataImportWorker() override;

    void setParameters(const QString& filePath,
                       const QString& projectName,
                       const QString& batchCode,
                       const QDate& detectDate,
                       int parallelNo,
                       int temperatureColumn,
                       int dtgColumn,
                       AppInitializer* appInitializer);
    // 设置导入参数：支持“不选择(自动识别)”与“按指定列导入”
    // - useCustomColumns=false：自动查找 serial_no 作为X（找不到就递增生成），查找 weight/重量 作为Y（找不到则报格式不符合）
    // - useCustomColumns=true ：使用指定X列/Y列（列号从1开始；xColumn=0 表示递增生成X）
    void setParameters(const QString& filePath,
                       const QString& projectName,
                       const QString& batchCode,
                       const QDate& detectDate,
                       int parallelNo,
                       bool useCustomColumns,
                       int xColumn1BasedOr0,
                       int yColumn1Based,
                       AppInitializer* appInitializer);

    void stop();

signals:
    void progressChanged(int current, int total);
    void progressMessage(const QString& message);
    void importFinished(int successCount, int totalDataCount);
    void importError(const QString& errorMessage);

protected:
    void run() override;

private:
    bool initThreadDatabase();
    void closeThreadDatabase();
    QString extractShortCodeFromSheetName(const QString& sheetName);
    int createOrGetSample(const SingleTobaccoSampleData& sampleData);

    QString m_filePath;
    QString m_projectName;
    QString m_batchCode;
    QDate m_detectDate;
    int m_parallelNo = 1;
    int m_temperatureColumn = 1;
    int m_dtgColumn = 3;
    bool m_useCustomColumns = true;
    int m_xColumn1BasedOr0 = 1;
    int m_yColumn1Based = 3; // weight 列（1-based）
    AppInitializer* m_appInitializer = nullptr;
    bool m_stopped = false;

    QMutex m_mutex;
    QSqlDatabase m_threadDb;
    TgSmallRawDataDAO* m_tgSmallRawDataDao = nullptr;
    SingleTobaccoSampleDAO* m_singleTobaccoSampleDao = nullptr;
};

#endif // TGSMALLRAWDATAIMPORTWORKER_H
