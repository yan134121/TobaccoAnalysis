// #ifndef NAVIGATORDAO_H
// #define NAVIGATORDAO_H

// #include <QList>
// #include <QPair>
// #include <QString>


// #endif // NAVIGATORDAO_H

#ifndef NAVIGATORDAO_H
#define NAVIGATORDAO_H

#include <QList>
#include <QPair>
#include <QString>
#include <QVariantMap>
#include <QVector>
#include <QPointF>

// 【关键】新增这个结构体的定义
struct ParallelSampleInfo {
    int id; // single_tobacco_sample 表的 id
    int parallelNo; // 平行样编号
};

class NavigatorDAO
{
public:
    NavigatorDAO();

    // 返回: <型号名称, 型号ID> 列表
    QList<QPair<QString, int>> fetchAllModels(QString& error);

    // 返回: <批次号, 批次ID> 列表 - 适配新的tobacco_batch表，根据批次类型筛选
    QList<QPair<QString, int>> fetchBatchesForModel(int modelId, QString& error, const QString &batchType = "NORMAL");
    

    // 获取指定项目的所有批次代码
    QList<QString> fetchBatchCodesForProject(const QString& projectName, QString& error);

    // 获取指定项目和批次的样本信息
    QList<QVariantMap> fetchSamplesForProjectAndBatch(const QString& projectName, const QString& batchCode, QString& error);

    // 获取指定批次的所有样本编码 - 适配新的表结构
    QList<QString> fetchShortCodesForBatch(int batchId, QString& error);
    
    // 获取指定批次和样本编码的平行样 - 适配新的表结构
    QList<ParallelSampleInfo> fetchParallelSamplesForBatch(int batchId, const QString& shortCode, QString& error);
    
    // 保持向后兼容的方法名

    // 获取样本曲线数据
    QVector<QPointF> getSampleCurveData(int sampleId, const QString& dataType, QString& error);
    
    // 获取样本详细信息（project_name, batch_code, short_code, parallel_no）
    QVariantMap getSampleDetailInfo(int sampleId, QString& error);
    
    // 检查指定批次和样本编码下是否有特定数据类型的数据
    QStringList getAvailableDataTypesForShortCode(int batchId, const QString& shortCode, QString& error);
    
    // 【新增】按样本名称（或相关字段）进行全局模糊搜索，返回路径信息
    // 返回的每项包含：
    // - sample_id, parallel_no, short_code, sample_name
    // - project_name, model_id
    // - batch_id, batch_code, batch_type
    QList<QVariantMap> searchSamplesByName(const QString& keyword, QString& error);

    // 【新增】获取指定数据类型的所有ShortCode
    QList<QString> fetchShortCodesForDataType(const QString& dataType, QString& error);

    // 【新增】获取指定ShortCode和数据类型的平行样信息（ID, ParallelNo, Timestamp）
    struct SampleLeafInfo {
        int id;
        int parallelNo;
        QString timestamp;
        QString shortCode;
        QString projectName; //
        QString batchCode;   //
    };
    QList<SampleLeafInfo> fetchParallelSamplesForShortCodeAndType(const QString& shortCode, const QString& dataType, QString& error);


    QList<QString> fetchProjectsForProcessData(QString& error);


    QList<QPair<QString, int>> fetchBatchesForProcessProject(const QString& projectName, QString& error);

    QList<SampleLeafInfo> fetchSamplesForProcessBatch(const QString& batchCode, QString& error);
    

    bool deleteProjectCascade(const QString& projectName, bool processBranch, QString& error);
    bool deleteBatchCascade(const QString& projectName, const QString& batchCode, bool processBranch, QString& error);
    bool deleteSampleCascade(int sampleId, bool processBranch, QString& error);
    bool deleteSampleDataByType(int sampleId, const QString& dataType, QString& error);
    

    int countSamplesForModel(int modelId, QString& error);

    int countBatchesForModel(int modelId, QString& error);

    int countSamplesForBatch(int batchId, QString& error);
    
};

#endif // NAVIGATORDAO_H
