

// src/service/SingleMaterialTobaccoService.h
#ifndef SINGLEMATERIALTOBACCOSERVICE_H
#define SINGLEMATERIALTOBACCOSERVICE_H

#include <QObject>
#include <QList>
#include <QMap>
#include <QVariant>
#include <QString>

// --- 前向声明所有依赖的类 ---
// 前向声明
class SingleTobaccoSampleDAO;
class TgBigDataDAO;
class ProcessTgBigDataDAO;
class TgSmallDataDAO;
class ChromatographyDataDAO;
class IAlgorithmService;
class FileHandlerFactory;
class SingleTobaccoSampleData;
class TgBigData;
class ProcessTgBigData;
class TgSmallData;
class ChromatographyData;
// 文件处理接口
class AbstractFileParser;
class AbstractFileWriter;
// --- 结束前向声明 ---
class DictionaryOptionService; // <-- 新增前向声明

// --- 修改：SingleReplicateColumnMapping 结构体，添加更多通用列 ---
// 列索引都是 1-based (用户输入)，在代码中访问 QVariantList 时会转换为 0-based
struct SingleReplicateColumnMapping {
    // 通用列，可能用于判断行是否为空，或作为数据的一部分
    int firstDataColIndex = -1; // 当前平行样数据块的起始列 (1-based)
    int lastDataColIndex = -1;  // 当前平行样数据块的结束列 (1-based)
    int replicateNoColIndex = -1; // 如果平行样号也在文件中的某一列
    int seqNoColIndex = -1;       // 序号列
    int sourceNameColIndex = -1;  // 源文件名列
    
    // 兼容旧代码的字段
    int temperatureCol = -1;
    int weightCol = -1;
    int tgValueCol = -1;
    int dtgValueCol = -1;
    int startDataRow = 1;
    int startDataCol = 1;

    // 大热重特有
    int tgBigSerialNoColIndex = -1; 
    int tgBigTgValueColIndex = -1;
    int tgBigDtgValueColIndex = -1;

    // 小热重特有
    int tgSmallTemperatureColIndex = -1;
    int tgSmallTgValueColIndex = -1;
    int tgSmallDtgValueColIndex = -1;

    // 色谱特有
    int chromaRetentionTimeColIndex = -1;
    int chromaResponseValueColIndex = -1;

    // 辅助验证：确保核心列被映射
    bool isValidForBigTg() const { return tgBigSerialNoColIndex > 0 && tgBigTgValueColIndex > 0 && tgBigDtgValueColIndex > 0; }
    bool isValidForSmallTg() const { return tgSmallTemperatureColIndex > 0 && tgSmallTgValueColIndex > 0 && tgSmallDtgValueColIndex > 0; }
    bool isValidForChromatography() const { return chromaRetentionTimeColIndex > 0 && chromaResponseValueColIndex > 0; }
};

// DataColumnMapping 仍然包含一个 SingleReplicateColumnMapping 的列表
struct DataColumnMapping {
    QList<SingleReplicateColumnMapping> replicateMappings;
    
    // 兼容旧代码的字段
    int temperatureCol = -1;
    int weightCol = -1;
    int tgValueCol = -1;
    int dtgValueCol = -1;
    int startDataRow = 1;
    int startDataCol = 1;
    
    bool isEmpty() const { return replicateMappings.isEmpty(); }
};
// --- 结束修改 ---

// // --- 新增/修改：定义基于列索引的映射结构体 ---
// // 列索引都是 1-based (用户输入)，在代码中访问 QVariantList 时会转换为 0-based
// // -1 表示未指定或不需要从文件读取
// struct DataColumnMapping {
//     // 大热重数据字段的映射
//     int tgBigReplicateNoColIndex = -1;
//     int tgBigSeqNoColIndex = -1;
//     int tgBigTgValueColIndex = -1;
//     int tgBigDtgValueColIndex = -1;
//     int tgBigSourceNameColIndex = -1; // 如果文件名在文件中的某一列

//     // 小热重数据字段的映射
//     int tgSmallReplicateNoColIndex = -1;
//     int tgSmallTemperatureColIndex = -1;
//     int tgSmallTgValueColIndex = -1;
//     int tgSmallDtgValueColIndex = -1;
//     int tgSmallSourceNameColIndex = -1;

//     // 色谱数据字段的映射
//     int chromaReplicateNoColIndex = -1;
//     int chromaRetentionTimeColIndex = -1;
//     int chromaResponseValueColIndex = -1;
//     int chromaSourceNameColIndex = -1;

//     // 辅助验证
//     bool isValidForBigTg() const { return tgBigTgValueColIndex > 0 && tgBigDtgValueColIndex > 0; }
//     bool isValidForSmallTg() const { return tgSmallTemperatureColIndex > 0 && tgSmallTgValueColIndex > 0 && tgSmallDtgValueColIndex > 0; }
//     bool isValidForChromatography() const { return chromaRetentionTimeColIndex > 0 && chromaResponseValueColIndex > 0; }
// };
// // --- 结束新增/修改 ---

class SingleTobaccoSampleService : public QObject
{
    Q_OBJECT
public:
    // ... (构造函数和析构函数保持不变) ...
    explicit SingleTobaccoSampleService(SingleTobaccoSampleDAO* singleTobaccoSampleDao,
                                          TgBigDataDAO* tgBigDataDao,
                                          ProcessTgBigDataDAO* processTgBigDataDao,
                                          TgSmallDataDAO* tgSmallDataDao,
                                          ChromatographyDataDAO* chromatographyDataDao,
                                          IAlgorithmService* algoService,
                                          FileHandlerFactory* fileHandlerFactory,
                                          DictionaryOptionService* dictionaryOptionService, // <-- 新增注入
                                          QObject *parent = nullptr);
    ~SingleTobaccoSampleService() override;

    // ... (基础信息 CRUD 方法不变) ...
    QList<SingleTobaccoSampleData> getAllTobaccos();
    QList<SingleTobaccoSampleData> queryTobaccos(const QMap<QString, QVariant>& conditions);
    SingleTobaccoSampleData getTobaccoById(int id);
    bool addTobacco(SingleTobaccoSampleData& tobacco, QString& errorMessage);
    bool updateTobacco(const SingleTobaccoSampleData& tobacco, QString& errorMessage);
    bool removeTobaccos(const QList<int>& ids, QString& errorMessage);

    // --- 修改：获取独特选项的方法，调用 DictionaryOptionService ---
    QStringList getAllUniqueOrigins();
    QStringList getAllUniqueParts();
    QStringList getAllUniqueGrades();
    QStringList getAllUniqueBlendTypes(); // <-- 单打/混打也从字典获取
    // --- 结束修改 ---

    // --- 修改：导入方法接收 startDataCol 和 DataColumnMapping 参数 ---
    bool importTgBigDataForSample(int sampleId, const QString& filePath, int replicateNo, int startDataRow, int startDataCol, const DataColumnMapping& mapping, QString& errorMessage);
    bool importProcessTgBigDataForSample(int sampleId, const QString& filePath, int replicateNo, int startDataRow, int startDataCol, const DataColumnMapping& mapping, QString& errorMessage);
    bool importTgSmallDataForSample(int sampleId, const QString& filePath, int replicateNo, int startDataRow, int startDataCol, const DataColumnMapping& mapping, QString& errorMessage);
    bool importChromatographyDataForSample(int sampleId, const QString& filePath, int replicateNo, int startDataRow, int startDataCol, const DataColumnMapping& mapping, QString& errorMessage);
    // --- 结束修改 ---

    // --- 新增：获取指定样本的传感器数据的方法 ---
    QList<TgBigData> getTgBigDataForSample(int sampleId);
    QList<ProcessTgBigData> getProcessTgBigDataForSample(int sampleId);
    QList<TgSmallData> getTgSmallDataForSample(int sampleId);
    QList<ChromatographyData> getChromatographyDataForSample(int sampleId);
    // --- 结束新增 ---

    // ... (数据处理方法不变) ...
    bool processTobaccoData(int tobaccoId, const QVariantMap& params, QString& errorMessage);
    // 导出数据 (不变，仍是导出 SingleTobaccoSampleData 基础信息)
    bool exportDataToFile(const QString& filePath,
                          const QList<SingleTobaccoSampleData>& dataToExport,
                          QString& errorMessage);

    static QString buildSampleDisplayName(int sampleId);

private:
    // ... (成员变量不变) ...
    SingleTobaccoSampleDAO* m_singleTobaccoSampleDao;
    TgBigDataDAO* m_tgBigDataDao;
    ProcessTgBigDataDAO* m_processTgBigDataDao;
    TgSmallDataDAO* m_tgSmallDataDao;
    ChromatographyDataDAO* m_chromatographyDataDao;
    IAlgorithmService* m_algorithmService;
    FileHandlerFactory* m_fileHandlerFactory;


    bool validateTobacco(const SingleTobaccoSampleData& tobacco, QString& errorMessage);

    // // --- 修改：辅助解析方法接收 QList<QVariantList> 和 DataColumnMapping 参数 ---
    // QList<TgBigData> parseTgBigDataFromRawData(int sampleId, int replicateNo, const QString& sourceName, const QList<QVariantList>& rawDataLists, const DataColumnMapping& mapping, QString& errorMessage);
    // QList<TgSmallData> parseTgSmallDataFromRawData(int sampleId, int replicateNo, const QString& sourceName, const QList<QVariantList>& rawDataLists, const DataColumnMapping& mapping, QString& errorMessage);
    // QList<ChromatographyData> parseChromatographyDataFromRawData(int sampleId, int replicateNo, const QString& sourceName, const QList<QVariantList>& rawDataLists, const DataColumnMapping& mapping, QString& errorMessage);
    // // --- 结束修改 ---
    // --- 辅助解析方法接收 SingleReplicateColumnMapping ---
    QList<TgBigData> parseTgBigDataFromRawData(int sampleId, int currentReplicateNo, const QString& sourceName, const QList<QVariantList>& fullSheetRawData, const SingleReplicateColumnMapping& currentReplicateMapping, QString& errorMessage); // <-- 传入 fullSheetRawData
    QList<ProcessTgBigData> parseProcessTgBigDataFromRawData(int sampleId, int currentReplicateNo, const QString& sourceName, const QList<QVariantList>& fullSheetRawData, const SingleReplicateColumnMapping& currentReplicateMapping, QString& errorMessage);
    QList<TgSmallData> parseTgSmallDataFromRawData(int sampleId, int currentReplicateNo, const QString& sourceName, const QList<QVariantList>& fullSheetRawData, const SingleReplicateColumnMapping& currentReplicateMapping, QString& errorMessage);
    QList<ChromatographyData> parseChromatographyDataFromRawData(int sampleId, int currentReplicateNo, const QString& sourceName, const QList<QVariantList>& fullSheetRawData, const SingleReplicateColumnMapping& currentReplicateMapping, QString& errorMessage);
    // --- 结束修改 ---

    DictionaryOptionService* m_dictionaryOptionService; // <-- 新增成员
};

#endif // SINGLEMATERIALTOBACCOSERVICE_H