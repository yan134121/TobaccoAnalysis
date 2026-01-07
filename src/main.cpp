#include "gui/MainWindow.h"
#include "data_access/DatabaseManager.h" // 
#include "core/common.h" // 
#include <QApplication>
#include <QMessageBox>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include "core/singletons/StringManager.h"

//调试
#include "MyWidget.h"
#include "Folder.h"


#include "core/AppInitializer.h"
#include "SqlConfigLoader.h"
#include "gui/dialogs/TgBigParameterSettingsDialog.h"
#include "Logger.h"

// #define ENABLE_LOG// 不屏蔽：启用日志功能 屏蔽：禁用日志功能
int main(int argc, char *argv[])
{
    // //启用 DPI 自适应
    // QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    // QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);


    // 调试
    // Folder f("MyFolder");         // 这里会输出 qDebug
    // MyWidget w;
    // w.setFolder(&f);              // 这里只是指针，不会再输出
    // return 0;

    // 在 main.cpp 或初始化函数里注册
    qRegisterMetaType<SegmentData>("SegmentData");
    qRegisterMetaType<StageData>("StageData");
    qRegisterMetaType<SampleDataFlexible>("SampleDataFlexible");
    // qRegisterMetaType<ParallelSampleData>("ParallelSampleData");
    qRegisterMetaType<SampleGroup>("SampleGroup");
    qRegisterMetaType<BatchGroupData>("BatchGroupData");

    
    qRegisterMetaType<NavigatorNodeInfo>(); // 注册自定义类型
    qRegisterMetaType<MultiStageData>(); // 
    qRegisterMetaType<ProcessingParameters>("ProcessingParameters");
    // 【新】注册 DifferenceResultRow
    qRegisterMetaType<DifferenceResultRow>();
    
    qRegisterMetaType<QList<DifferenceResultRow>>("QList<DifferenceResultRow>");
    QApplication a(argc, argv);





    #ifdef _DEBUG
        DEBUG_LOG << "Debug mode";
    #else
        DEBUG_LOG << "Release mode";
    #endif

    #ifdef ENABLE_LOG
    // 禁用日志初始化（不写入文件、不输出到控制台）
    QString logPath = QDir::currentPath() + "/logs/app_log.txt";
    if (!initLogger(logPath)) {
        QMessageBox::warning(nullptr, "日志警告", "日志系统初始化失败，将使用默认设置");
    }
    LOG_INFO("应用程序启动");

    #else
    // 完全关闭 Logger 输出（可选）
    // 1) 关闭控制台输出
    Logger::instance().setConsoleOutput(false);
    // 2) 将日志级别提升到致命，仅保留 FATAL
    Logger::instance().setLogLevel(LOG_FATAL);
    // 3) 确保不写文件（如果之前已打开日志文件）
    Logger::instance().close();
    #endif

    qDebug() << "Hello from qDebug()";
    DEBUG_LOG << "Hello from Logger";
    
    AppInitializer initializer;
    if (!initializer.initialize()) {
        LOG_ERROR("应用程序初始化失败");
        return -1; // 应用程序初始化失败，AppInitializer 内部已显示错误消息
    }

    // 从配置文件读取数据库连接参数
    QString configPath = QDir::currentPath() + "/config/config.json";
    QFile configFile(configPath);
    if (!configFile.exists()) {
        QMessageBox::critical(nullptr, "配置错误", "配置文件不存在: " + configPath);
        return -1;
    }
    
    if (!configFile.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(nullptr, "配置错误", "无法打开配置文件: " + configPath);
        return -1;
    }
    
    QByteArray configData = configFile.readAll();
    configFile.close();
    
    QJsonDocument jsonDoc = QJsonDocument::fromJson(configData);
    if (jsonDoc.isNull() || !jsonDoc.isObject()) {
        QMessageBox::critical(nullptr, "配置错误", "配置文件格式错误");
        return -1;
    }
    
    QJsonObject config = jsonDoc.object();
    QJsonObject dbConfig = config["database"].toObject();
    
    QString host = dbConfig["host"].toString("127.0.0.1");
    int port = dbConfig["port"].toInt(3306);
    QString dbName = dbConfig["dbName"].toString();
    QString user = dbConfig["user"].toString();
    QString password = dbConfig["password"].toString();
    
    // // 1. 获取可用驱动列表并转换为字符串
    // QStringList driverList = QSqlDatabase::drivers();
    // QString allDrivers = driverList.join(", "); // 将 ["QSQLITE", "QMYSQL"] 变成 "QSQLITE, QMYSQL"

    // // 3. 组合详细的调试信息
    // QString debugInfo = QString(
    //     "【环境诊断】\n"
    //     "系统可用驱动: [%1]\n\n"
    //     "【读取到的配置】\n"
    //     "主机: %2\n"
    //     "端口: %3\n"
    //     "库名: %4\n"
    //     "用户: %5\n"
    //     "密码: %6\n\n"
    //     "提示: 如果可用驱动里没有 QMYSQL，说明插件缺失。\n"
    //     "如果库里有 QMYSQL 但仍报错，请检查 libmysql.dll 是否在 exe 目录下。"
    // ).arg(allDrivers)
    //     .arg(host)
    //     .arg(port)
    //     .arg(dbName)
    //     .arg(user)
    //     .arg(password);

    // // 3. 弹出警告框
    // QMessageBox::warning(nullptr, "连接配置核对", debugInfo);

    // // 诊断输出
    // if (!DatabaseManager::instance().connectToDb(dbName, user, password, host, port)) {
    //     // 修改这部分代码来查看具体错误
    //     QString errorText = QSqlDatabase::database().lastError().text();
    //     QMessageBox::warning(nullptr, "数据库未连接",
    //         "详细错误信息：" + errorText);
    // }
    
    // 使用配置文件中的参数连接数据库
    if (!DatabaseManager::instance().connectToDb(dbName, user, password, host, port)) {
        // 数据库连接失败时不再退出程序，而是给出警告提示，允许以“未连接数据库”模式继续运行
        QMessageBox::warning(nullptr,
                             "数据库未连接",
                             "无法连接到数据库，请检查配置或联系管理员。\n当前将以未连接数据库模式运行，部分功能可能不可用。");
        LOG_WARNING("数据库连接失败，应用将以未连接数据库模式继续运行");
    }

    StringManager::instance(); // 确保单例被创建和加载

    // 初始化SQL配置加载器
    SqlConfigLoader& sqlLoader = SqlConfigLoader::getInstance();
    
    // 构建配置文件路径
    QString sql_configPath = QDir::currentPath() + "/config/sql_config.json";
    
    // 检查配置文件是否存在
    if (!QFile::exists(sql_configPath)) {
        FATAL_LOG << "SQL配置文件不存在:" << sql_configPath;
        QMessageBox::critical(nullptr, "配置错误", 
                             QString("SQL配置文件不存在:\n%1").arg(sql_configPath));
        return -1;
    }
    
    // 加载SQL配置
    bool success = sqlLoader.loadConfig(sql_configPath, SqlConfigLoader::JSON_CONFIG);
    if (!success) {
        FATAL_LOG << "SQL配置加载失败";
        QMessageBox::critical(nullptr, "配置错误", "SQL配置文件加载失败，程序无法启动");
        return -1;
    }
    
    INFO_LOG << "SQL配置加载成功";



    // AppInitializer initializer;
    // if (!initializer.initialize()) {
    //     return -1; // 应用程序初始化失败，AppInitializer 内部已显示错误消息
    // }

    // MainWindow w;
    MainWindow w(&initializer); // <-- 将 initializer 传递给 MainWindow 构造函数
    w.show();

    int result = a.exec();

    // 关闭数据库连接
    DatabaseManager::instance().disconnectFromDb();
    return result;
}
