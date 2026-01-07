
// core/AppInitializer.cpp

#include "AppInitializer.h"
#include "utils/ConfigLoader.h"
#include "data_access/DatabaseConnector.h"

// --- 引入所有需要完整定义的 Service/Factory/Algorithm ---
#include "services/SingleTobaccoSampleService.h"
#include "data_access/SingleTobaccoSampleDAO.h"
#include "data_access/TgBigDataDAO.h"           // 
#include "data_access/ProcessTgBigDataDAO.h"    // 
#include "data_access/TgSmallDataDAO.h"         // 
#include "data_access/ChromatographyDataDAO.h"  // 
#include "services/algorithm/MockAlgorithmService.h"
#include "utils/file_handler/FileHandlerFactory.h"
#include "services/DataProcessingService.h"
// 代表性选择服务
#include "services/analysis/ParallelSampleAnalysisService.h"

#include "DictionaryOptionDAO.h"      // 
#include "services/DictionaryOptionService.h" // 
#include <QSqlDatabase> // 
#include <QMessageBox>
#include <QDebug>
#include <QApplication> // 
#include <QDir>
#include <QFile>
#include <QTextStream>



// #include "AppInitializer.h"
// #include "util/ConfigLoader.h"
// #include "DatabaseConnector.h"

// // --- 引入所有需要完整定义的 Service/Factory/Algorithm ---
// #include "service/SingleMaterialTobaccoService.h"
// #include "SingleMaterialTobaccoDAO.h"
// #include "algorithm/MockAlgorithmService.h"
// #include "util/FileHandlerFactory.h"
// // --- 结束引入 ---

// #include <QMessageBox>
// #include <QDebug>
// #include <QCoreApplication>
// #include <QDir>
// #include <QFile>
// #include <QTextStream> // 用于 loadUiStyles

AppInitializer::AppInitializer(QObject *parent)
    : QObject(parent)
{
    // 获取应用程序可执行文件所在的目录
    QString appDirPath = QApplication::applicationDirPath(); // 使用 QApplication::applicationDirPath()

    // 配置文件路径：假定 config.json 位于可执行文件同级目录
    m_configFilePath = appDirPath + "/config/config.json";

    // 初始化所有指针为 nullptr，这是良好的实践，即使它们已经在声明时初始化了
    m_singleTobaccoSampleService = nullptr;
    m_singleTobaccoSampleDAO = nullptr;
    m_algorithmService = nullptr;
    m_fileHandlerFactory = nullptr;
}


AppInitializer::~AppInitializer()
{

    delete m_singleTobaccoSampleDAO;
    delete m_tgBigDataDao;           // <-- 删除
    delete m_tgSmallDataDao;         // <-- 删除
    delete m_chromatographyDataDao;  // <-- 删除
    delete m_dictionaryOptionDAO;      // <-- 删除
    // delete m_tobaccoModelDAO;                // <-- 删除
}


bool AppInitializer::initialize()
{
    DEBUG_LOG << "开始应用程序初始化...";

    if (!loadConfiguration()) return false;
    DEBUG_LOG << "配置文件加载成功。";

    if (!initializeLogging()) return false;
    DEBUG_LOG << "日志系统初始化成功。";

    if (!ensureApplicationDirectories()) return false;
    DEBUG_LOG << "应用程序目录已确认。";

    if (!initializeDatabase()) return false;
    DEBUG_LOG << "数据库初始化成功。";

    // --- ：初始化所有Service层实例 (包括注入Factory) ---
    if (!initializeServices()) return false;
    DEBUG_LOG << "应用程序服务层初始化完成。";
    // --- 结束 ---

    if (!loadUiStyles()) {
        WARNING_LOG << "UI样式加载失败，但应用程序仍将继续运行。";
    }
    DEBUG_LOG << "UI样式加载完成。";

    // --- ：初始化默认字典选项 ---
    // 确保 m_dictionaryOptionService 已经初始化
    if (m_dictionaryOptionService && m_dictionaryOptionDAO) {
        DEBUG_LOG << "正在初始化默认字典选项...";
        QString errorMessage;

        // // 默认产地
        // m_dictionaryOptionService->addOption("Origin", "云南", "", errorMessage);
        // m_dictionaryOptionService->addOption("Origin", "贵州", "", errorMessage);
        // // 默认部位
        // m_dictionaryOptionService->addOption("Part", "上部", "", errorMessage);
        // m_dictionaryOptionService->addOption("Part", "中部", "", errorMessage);
        // m_dictionaryOptionService->addOption("Part", "下部", "", errorMessage);
        // // 默认等级
        // m_dictionaryOptionService->addOption("Grade", "A", "", errorMessage);
        // m_dictionaryOptionService->addOption("Grade", "B", "", errorMessage);
        // m_dictionaryOptionService->addOption("Grade", "C", "", errorMessage);
        // // 默认打的方式 (BlendType)
        // m_dictionaryOptionService->addOption("BlendType", "单打", "", errorMessage);
        // m_dictionaryOptionService->addOption("BlendType", "混打", "", errorMessage);
        // --- 关键修改：调整 addOption 调用的参数顺序 ---
        m_dictionaryOptionService->addOption("Origin", "云南", errorMessage, ""); // <-- 调整顺序
        if (!errorMessage.isEmpty()) WARNING_LOG << "初始化默认选项失败:" << errorMessage;
        errorMessage.clear();

        m_dictionaryOptionService->addOption("Origin", "贵州", errorMessage, ""); // <-- 调整顺序
        if (!errorMessage.isEmpty()) WARNING_LOG << "初始化默认选项失败:" << errorMessage;
        errorMessage.clear();

        // ... 对所有 addOption 调用做类似处理 ...
        m_dictionaryOptionService->addOption("Part", "上部", errorMessage, ""); // <-- 调整顺序
        if (!errorMessage.isEmpty()) WARNING_LOG << "初始化默认选项失败:" << errorMessage;
        errorMessage.clear();

        m_dictionaryOptionService->addOption("Part", "中部", errorMessage, ""); // <-- 调整顺序
        if (!errorMessage.isEmpty()) WARNING_LOG << "初始化默认选项失败:" << errorMessage;
        errorMessage.clear();

        m_dictionaryOptionService->addOption("Part", "下部", errorMessage, ""); // <-- 调整顺序
        if (!errorMessage.isEmpty()) WARNING_LOG << "初始化默认选项失败:" << errorMessage;
        errorMessage.clear();

        m_dictionaryOptionService->addOption("Grade", "A", errorMessage, ""); // <-- 调整顺序
        if (!errorMessage.isEmpty()) WARNING_LOG << "初始化默认选项失败:" << errorMessage;
        errorMessage.clear();

        m_dictionaryOptionService->addOption("Grade", "B", errorMessage, ""); // <-- 调整顺序
        if (!errorMessage.isEmpty()) WARNING_LOG << "初始化默认选项失败:" << errorMessage;
        errorMessage.clear();

        m_dictionaryOptionService->addOption("Grade", "C", errorMessage, ""); // <-- 调整顺序
        if (!errorMessage.isEmpty()) WARNING_LOG << "初始化默认选项失败:" << errorMessage;
        errorMessage.clear();

        m_dictionaryOptionService->addOption("BlendType", "单打", errorMessage, ""); // <-- 调整顺序
        if (!errorMessage.isEmpty()) WARNING_LOG << "初始化默认选项失败:" << errorMessage;
        errorMessage.clear();

        m_dictionaryOptionService->addOption("BlendType", "混打", errorMessage, ""); // <-- 调整顺序
        if (!errorMessage.isEmpty()) WARNING_LOG << "初始化默认选项失败:" << errorMessage;
        errorMessage.clear();

        DEBUG_LOG << "默认字典选项初始化完成。";
    } else {
        WARNING_LOG << "DictionaryOptionService 或 DAO 未初始化，无法初始化默认字典选项。";
    }


    

    DEBUG_LOG << "应用程序初始化完成。";
    return true;
}

QVariantMap AppInitializer::getAppConfig() const
{
    return m_fullConfig;
}

SingleTobaccoSampleService* AppInitializer::getSingleTobaccoSampleService() const {
    return m_singleTobaccoSampleService;
}

FileHandlerFactory* AppInitializer::getFileHandlerFactory() const { 
    return m_fileHandlerFactory;
}

TobaccoModelService* AppInitializer::getTobaccoModelService() const {
    return m_tobaccoModelService;
}


QString AppInitializer::getDatabasePath() const
{
    // 从配置中获取数据库路径，或使用默认路径
    QVariantMap dbConfig = m_fullConfig.value("database").toMap();
    return dbConfig.value("path", "tobacco.db").toString();
}


void AppInitializer::showCriticalError(const QString& title, const QString& message) const
{
    QMessageBox::critical(nullptr, title, message);
    WARNING_LOG << "Critical Error:" << title << "-" << message;
}

bool AppInitializer::loadConfiguration()
{
    m_fullConfig = ConfigLoader::loadJsonConfig(m_configFilePath);
    if (m_fullConfig.isEmpty()) {
        showCriticalError("配置加载失败", "无法加载 config.json 配置文件。请检查文件是否存在且格式正确。\n路径: " + m_configFilePath);
        return false;
    }
    DEBUG_LOG << "=== Full Configuration Loaded ===";
    for (auto it = m_fullConfig.constBegin(); it != m_fullConfig.constEnd(); ++it) {
        if (it.value().type() == QVariant::Map) { // 使用 QVariant::Map
            QVariantMap nestedMap = it.value().toMap();
            DEBUG_LOG << "  " << it.key() << ": {";
            for (auto nestedIt = nestedMap.constBegin(); nestedIt != nestedMap.constEnd(); ++nestedIt) {
                 DEBUG_LOG << "    " << nestedIt.key() << ":" << nestedIt.value().toString();
            }
            DEBUG_LOG << "  }";
        } else {
            DEBUG_LOG << "  " << it.key() << ":" << it.value().toString();
        }
    }
    DEBUG_LOG << "===============================";
    return true;
}

bool AppInitializer::initializeDatabase()
{
    QVariantMap dbConfig = m_fullConfig.value("database").toMap();
    if (dbConfig.isEmpty()) {
        showCriticalError("数据库配置错误", "配置文件中缺少 'database' 配置段。");
        return false;
    }

    // 数据库无法连接/创建时，给出警告但不阻止应用继续启动
    if (!DatabaseConnector::getInstance().connectOrCreateDatabase(dbConfig)) {
        QMessageBox::warning(nullptr,
                             "数据库未连接",
                             "无法连接到数据库或创建数据库，请检查您的 MySQL 服务、网络连接及 config.json 配置。\n"
                             "当前将以未连接数据库模式运行，部分依赖数据库的功能可能不可用。");
        WARNING_LOG << "数据库连接/创建失败，应用将以未连接数据库模式继续运行。";
        // 提前返回 true，表示初始化流程继续，但数据库相关功能不可用
        return true;
    }

    QVariantMap sqlConfig = m_fullConfig.value("sql").toMap();
    if (sqlConfig.isEmpty()) {
        WARNING_LOG << "配置文件中缺少 'sql' 配置段，跳过数据库表结构初始化。";
        return true;
    }

    QString createTablesScriptRelativePath = sqlConfig.value("create_tables_script").toString();
    if (createTablesScriptRelativePath.isEmpty()) {
        WARNING_LOG << "配置文件中 'sql' 段缺少 'create_tables_script' 路径，跳过建表。";
        return true;
    }

    QString sqlSchemaFilePath = QCoreApplication::applicationDirPath() + "/" + createTablesScriptRelativePath;
    if (createTablesScriptRelativePath.startsWith(":/")) {
         sqlSchemaFilePath = createTablesScriptRelativePath;
    }

    QFile sqlFile(sqlSchemaFilePath);
    if (!sqlFile.exists()) {
        showCriticalError("SQL脚本文件不存在", "无法找到用于初始化数据库表结构的SQL脚本文件。\n请确保文件位于: " + sqlSchemaFilePath);
        return false;
    }

    DEBUG_LOG << "数据库连接已建立，准备执行建表脚本...";
    if (!DatabaseConnector::getInstance().ensureTablesExist(sqlSchemaFilePath)) {
        showCriticalError("数据库表初始化失败", "无法创建或检查数据库表结构。\n请检查SQL脚本文件内容及数据库用户权限。");
        return false;
    }
    return true;
}

bool AppInitializer::initializeLogging()
{
    QVariantMap loggingConfig = m_fullConfig.value("logging").toMap();
    if (loggingConfig.isEmpty()) {
        WARNING_LOG << "配置文件中缺少 'logging' 配置段，将使用默认日志设置。";
        return true;
    }

    QString logLevel = loggingConfig.value("level", "info").toString();
    QString outputFile = loggingConfig.value("output_file", "").toString();
    DEBUG_LOG << "日志系统配置:" << "级别=" << logLevel << "输出文件=" << outputFile;
    // TODO: 调用您的 Logger 类的初始化方法
    return true;
}

// 示例：这个方法现在可以被 initializeServices() 调用
bool AppInitializer::initializeAlgorithmService()
{
    if (m_algorithmService) return true; // 如果已经创建，避免重复

    QVariantMap algoConfig = m_fullConfig.value("algorithm_service").toMap();
    if (algoConfig.isEmpty()) {
        WARNING_LOG << "配置文件中缺少 'algorithm_service' 配置段，算法服务将不会被初始化。";
        // 允许继续，因为算法服务可能不是关键的启动项
        m_algorithmService = new MockAlgorithmService(this); // 即使没配置，也创建一个Mock
        return true;
    }

    // TODO: 根据 serviceType 创建并初始化具体的算法服务客户端 (例如 AlgorithmClient::init(serviceType, algoConfig);)
    // 目前使用MockAlgorithmService
    m_algorithmService = new MockAlgorithmService(this);
    DEBUG_LOG << "算法服务配置:" << "类型=" << algoConfig.value("type").toString();
    return true;
}

bool AppInitializer::ensureApplicationDirectories()
{
    QVariantMap appSettings = m_fullConfig.value("app_settings").toMap();
    QString tempFilesPath = appSettings.value("temp_files_path", "./temp_data").toString();

    QString fullTempPath = QCoreApplication::applicationDirPath() + "/" + tempFilesPath;
    if (tempFilesPath.startsWith(":/")) {
        // 资源路径不用于存储可写文件，因此这里不做创建
    } else {
        QDir dir(fullTempPath);
        if (!dir.exists()) {
            if (!dir.mkpath(".")) {
                showCriticalError("目录创建失败", "无法创建临时文件目录: " + fullTempPath);
                return false;
            }
            DEBUG_LOG << "创建了应用程序临时目录:" << fullTempPath;
        } else {
            DEBUG_LOG << "应用程序临时目录已存在:" << fullTempPath;
        }
    }
    return true;
}

bool AppInitializer::loadUiStyles()
{
    QVariantMap appSettings = m_fullConfig.value("app_settings").toMap();
    QString uiThemeFile = appSettings.value("ui_theme").toString();

    if (uiThemeFile.isEmpty()) {
        DEBUG_LOG << "未配置UI主题文件，使用默认样式。";
        return true;
    }

    QString styleFilePath = QCoreApplication::applicationDirPath() + "/" + uiThemeFile;
    if (uiThemeFile.startsWith(":/")) {
        styleFilePath = uiThemeFile;
    }

    QFile file(styleFilePath);
    if (!file.exists()) {
        WARNING_LOG << "UI主题文件不存在:" << styleFilePath;
        return false;
    }

    if (file.open(QFile::ReadOnly | QFile::Text)) {
        QString styleSheet = QTextStream(&file).readAll();
        qApp->setStyleSheet(styleSheet);
        file.close();
        DEBUG_LOG << "UI主题文件加载成功:" << styleFilePath;
        return true;
    } else {
        WARNING_LOG << "无法加载UI主题文件:" << styleFilePath << "-" << file.errorString();
        return false;
    }
}

// --- ：初始化所有Service层实例 ---
bool AppInitializer::initializeServices() {
    DEBUG_LOG << "正在初始化服务层实例...";

    // 1. 初始化算法服务 (在创建 Service 之前)
    if (!initializeAlgorithmService()) return false; // 确保 m_algorithmService 被创建

    // 2. 创建 DAO 实例
    // m_singleTobaccoSampleDAO = new SingleMaterialTobaccoDAO(this);
    m_singleTobaccoSampleDAO = new SingleTobaccoSampleDAO(); // <-- 修改为不传入父对象
    m_tgBigDataDao = new TgBigDataDAO();           // <-- 创建
    m_processTgBigDataDao = new ProcessTgBigDataDAO(); // <-- 创建
    m_tgSmallDataDao = new TgSmallDataDAO();         // <-- 创建
    m_chromatographyDataDao = new ChromatographyDataDAO();  // <-- 创建
    m_dictionaryOptionDAO = new DictionaryOptionDAO();      // <-- 创建
    // --- 创建 DataProcessingService ---
    m_dataProcessingService = new DataProcessingService(this);
    // --- 创建 SampleComparisonService ---
    m_sampleComparisonService = new SampleComparisonService(this);
    // --- 创建 ParallelSampleAnalysisService ---
    m_parallelSampleAnalysisService = new ParallelSampleAnalysisService(this, this);

    // m_tobaccoModelDAO = new TobaccoModelDAO();                  // <-- 创建

    // 3. 创建文件处理器工厂实例
    m_fileHandlerFactory = new FileHandlerFactory(this);



    m_dictionaryOptionService = new DictionaryOptionService(m_dictionaryOptionDAO, this); // <-- 添加这行

    // // 4. 创建 SingleMaterialTobaccoService 实例，并注入所有依赖
    // m_singleTobaccoSampleService = new SingleMaterialTobaccoService(
    //     m_singleTobaccoSampleDAO,
    //     m_tgBigDataDao,           // <-- 注入新的 DAO
    //     m_tgSmallDataDao,         // <-- 注入新的 DAO
    //     m_chromatographyDataDao,  // <-- 注入新的 DAO
    //     m_algorithmService,
    //     m_fileHandlerFactory, // 注入工厂实例
    //     this
    // );
    // --- 关键修改：确保这里的参数列表与 SingleMaterialTobaccoService 的构造函数签名完全匹配 ---
    m_singleTobaccoSampleService = new SingleTobaccoSampleService(
        m_singleTobaccoSampleDAO,     // 参数 1
        m_tgBigDataDao,                 // 参数 2
        m_processTgBigDataDao,          // 参数 3 ()
        m_tgSmallDataDao,               // 参数 4
        m_chromatographyDataDao,        // 参数 5
        m_algorithmService,             // 参数 6
        m_fileHandlerFactory,           // 参数 7
        m_dictionaryOptionService,      // 参数 8 (注入 DictionaryOptionService)
        this                            // 参数 9 (parent)
    );
    // --- 结束关键修改 ---

    // // 检查所有核心组件是否成功创建
    // if (!m_singleTobaccoSampleDAO || !m_algorithmService || !m_fileHandlerFactory || !m_singleTobaccoSampleService) {
    //     showCriticalError("服务初始化失败", "未能成功创建核心服务实例。");
    //     return false;
    // }

    // 检查所有核心组件是否成功创建
    if (!m_singleTobaccoSampleDAO || !m_tgBigDataDao || !m_tgSmallDataDao || !m_chromatographyDataDao ||
        !m_dictionaryOptionDAO || 
        // !m_tobaccoModelDAO || 
        !m_algorithmService || !m_fileHandlerFactory ||
        !m_dictionaryOptionService || 
        // !m_tobaccoModelService || 
        !m_singleTobaccoSampleService || !m_dataProcessingService || !m_sampleComparisonService || !m_parallelSampleAnalysisService) {
        showCriticalError("服务初始化失败", "未能成功创建核心服务实例。");
        return false;
    }
    return true;
}

DictionaryOptionService* AppInitializer::getDictionaryOptionService() const {
    return m_dictionaryOptionService;
}

// --- 实现 Getter 方法 ---
DataProcessingService* AppInitializer::getDataProcessingService() const
{
    return m_dataProcessingService;
}

// --- 实现 SampleComparisonService 的 Getter ---
SampleComparisonService* AppInitializer::getSampleComparisonService() const
{
    return m_sampleComparisonService;
}

// --- 实现 ParallelSampleAnalysisService 的 Getter ---
ParallelSampleAnalysisService* AppInitializer::getParallelSampleAnalysisService() const
{
    return m_parallelSampleAnalysisService;
}
