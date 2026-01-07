


// core/AppInitializer.h
#ifndef APPINITIALIZER_H
#define APPINITIALIZER_H

#include <QObject>
#include <QVariantMap>

#include "services/DataProcessingService.h"
#include "services/analysis/SampleComparisonService.h"


// --- 前向声明所有依赖的类 ---
class SampleComparisonService; // <-- 前向声明 SampleComparisonService
class SingleTobaccoSampleService;
class SingleTobaccoSampleDAO;
class IAlgorithmService;
class FileHandlerFactory; // <-- 引入 FileHandlerFactory

// DAO 类的前向声明 <-- 错误修复点在这里
class TgBigDataDAO;           // 前向声明
class ProcessTgBigDataDAO;    // 前向声明
class TgSmallDataDAO;         // 前向声明
class ChromatographyDataDAO;  // 前向声明

// 文件处理接口
class AbstractFileParser;
class AbstractFileWriter;
// --- 结束前向声明 ---
class DictionaryOptionDAO;    // 前向声明
class DictionaryOptionService; // 前向声明

class FileHandlerFactory; // <-- 确保前向声明

class TobaccoModelDAO;                // 前向声明
class TobaccoModelService;            // 前向声明


class AppInitializer : public QObject
{
    Q_OBJECT
public:
    explicit AppInitializer(QObject *parent = nullptr);
    ~AppInitializer() override; // 虚析构函数

    bool initialize();
    QVariantMap getAppConfig() const;

    // --- 新增 Getter 方法 ---
    SingleTobaccoSampleService* getSingleTobaccoSampleService() const;
    FileHandlerFactory* getFileHandlerFactory() const; // 
    DictionaryOptionService* getDictionaryOptionService() const; // 
    SampleComparisonService* getSampleComparisonService() const; //
    // AbstractFileParser* getFileParser() const;
    // AbstractFileWriter* getFileWriter() const;


    TobaccoModelService* getTobaccoModelService() const;             //  Getter
    
    // 添加DAO的Getter方法
    TgBigDataDAO* getTgBigDataDao() const { return m_tgBigDataDao; }
    ProcessTgBigDataDAO* getProcessTgBigDataDao() const { return m_processTgBigDataDao; }
    TgSmallDataDAO* getTgSmallDataDao() const { return m_tgSmallDataDao; }
    ChromatographyDataDAO* getChromatographyDataDao() const { return m_chromatographyDataDao; }
    SingleTobaccoSampleDAO* getSingleTobaccoSampleDAO() const { return m_singleTobaccoSampleDAO; }
    
    // 获取数据库文件路径
    QString getDatabasePath() const;

    // --- 为 DataProcessingService 添加 Getter ---
    DataProcessingService* getDataProcessingService() const;
    // --- 为 ParallelSampleAnalysisService 添加 Getter ---
    class ParallelSampleAnalysisService* getParallelSampleAnalysisService() const;


private:
    QVariantMap m_fullConfig;
    QString m_configFilePath;

    SingleTobaccoSampleService* m_singleTobaccoSampleService = nullptr;
    SingleTobaccoSampleDAO* m_singleTobaccoSampleDAO = nullptr;

    TgBigDataDAO* m_tgBigDataDao = nullptr;           // <-- 实例
    ProcessTgBigDataDAO* m_processTgBigDataDao = nullptr;  // 实例
    TgSmallDataDAO* m_tgSmallDataDao = nullptr;         // <-- 实例
    ChromatographyDataDAO* m_chromatographyDataDao = nullptr;  // <-- 实例

    IAlgorithmService* m_algorithmService = nullptr;
    FileHandlerFactory* m_fileHandlerFactory = nullptr; // <-- 实例，存储工厂实例

    bool loadConfiguration();
    bool initializeDatabase();
    bool initializeLogging();
    bool initializeServices(); // 负责创建并注入所有 Service, Factory
    bool initializeAlgorithmService(); // 这行，声明方法
    bool ensureApplicationDirectories();
    bool loadUiStyles();

    void showCriticalError(const QString& title, const QString& message) const;

    DictionaryOptionDAO* m_dictionaryOptionDAO = nullptr;      // 
    DictionaryOptionService* m_dictionaryOptionService = nullptr; // 


    TobaccoModelDAO* m_tobaccoModelDAO = nullptr;                // 
    TobaccoModelService* m_tobaccoModelService = nullptr;            // 

    // --- 添加一个变量来持有服务实例 ---
    DataProcessingService* m_dataProcessingService = nullptr;
    SampleComparisonService* m_sampleComparisonService = nullptr; // <-- 添加一个变量来持有服务实例
    class ParallelSampleAnalysisService* m_parallelSampleAnalysisService = nullptr; // 组内代表性选择服务
};

#endif // APPINITIALIZER_H
