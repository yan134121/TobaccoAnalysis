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
                       int tgColumn,
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
    int m_tgColumn = 3;
    AppInitializer* m_appInitializer = nullptr;
    bool m_stopped = false;

    QMutex m_mutex;
    QSqlDatabase m_threadDb;
    TgSmallRawDataDAO* m_tgSmallRawDataDao = nullptr;
    SingleTobaccoSampleDAO* m_singleTobaccoSampleDao = nullptr;
};

#endif // TGSMALLRAWDATAIMPORTWORKER_H
