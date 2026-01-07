// #include "DatabaseConnector.h"
// #include <QSqlDatabase>
// #include <QDebug>

// DatabaseConnector& DatabaseConnector::getInstance() {
//     static DatabaseConnector instance;
//     return instance;
// }

// bool DatabaseConnector::connect(const QString &host, const QString &dbName,
//                                 const QString &user, const QString &password, int port) {
//     if (QSqlDatabase::contains("qt_sql_default_connection")) {
//         database = QSqlDatabase::database("qt_sql_default_connection");
//     } else {
//         database = QSqlDatabase::addDatabase("QMYSQL");
//     }

//     database.setHostName(host);
//     database.setDatabaseName(dbName);
//     database.setUserName(user);
//     database.setPassword(password);
//     database.setPort(port);

//     if (!database.open()) {
//         WARNING_LOG << "Database connection failed:" << database.lastError().text();
//         return false;
//     }
//     return true;
// }

// QSqlDatabase& DatabaseConnector::getDatabase() {
//     return database;
// }

#include "common.h"
#include "DatabaseConnector.h"
#include "Logger.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QThread> // 用于生成临时连接名
#include <QFile>

// 构造函数：初始化主数据库连接对象及其名称
DatabaseConnector::DatabaseConnector()
{
    // 为主应用程序数据库连接指定一个唯一的名称
    // 这样可以避免与可能存在的临时连接混淆，也避免使用默认连接名
    m_connectionName = QString("tobacco_app_db_connection_%1").arg(reinterpret_cast<quintptr>(QThread::currentThreadId()));
    m_db = QSqlDatabase::addDatabase("QMYSQL", m_connectionName);
}

// 析构函数：关闭主数据库连接并从Qt的连接列表中移除
DatabaseConnector::~DatabaseConnector() {
    if (m_db.isOpen()) {
        m_db.close();
    }
    // 移除数据库连接
    QSqlDatabase::removeDatabase(m_connectionName);
}

// 获取单例实例
DatabaseConnector& DatabaseConnector::getInstance() {
    static DatabaseConnector instance;
    return instance;
}

// 获取数据库连接对象
QSqlDatabase& DatabaseConnector::getDatabase() {
    // DEBUG_LOG << "getDatabase() called. Connection name:" << m_db.connectionName()
    //          << "Driver:" << m_db.driverName()
    //          << "isValid:" << m_db.isValid()
    //          << "isOpen:" << m_db.isOpen();
    return m_db;
}

bool DatabaseConnector::connectOrCreateDatabase(const QVariantMap& config)
{
    // 从传入的配置中提取数据库连接参数
    QString host = config.value("host").toString();
    QString dbName = config.value("dbName").toString();
    QString user = config.value("user").toString();
    QString password = config.value("password").toString();
    int port = config.value("port", 3306).toInt(); // 提供默认端口

    // 检查配置是否完整
    if (host.isEmpty() || dbName.isEmpty() || user.isEmpty() || password.isEmpty()) {
        WARNING_LOG << "数据库连接配置不完整，请检查配置文件。";
        return false;
    }

    // 如果主数据库连接已经打开且连接到目标数据库，则直接返回true
    if (m_db.isOpen() && m_db.databaseName() == dbName && m_db.hostName() == host) {
        DEBUG_LOG << "数据库连接已存在并有效:" << dbName;
        return true;
    }

    // --- 步骤 1: 先连接 MySQL 服务 (不指定具体数据库)，以便执行 CREATE DATABASE 命令 ---
    QString tmpConnectionName = QString("tmp_db_creation_conn_%1").arg(reinterpret_cast<quintptr>(QThread::currentThreadId()));
    QSqlDatabase tmpDb = QSqlDatabase::addDatabase("QMYSQL", tmpConnectionName);
    tmpDb.setHostName(host);
    tmpDb.setUserName(user);
    tmpDb.setPassword(password);
    tmpDb.setPort(port);

    if (!tmpDb.open()) {
        WARNING_LOG << "无法连接到MySQL服务（用于创建数据库）:" << tmpDb.lastError().text();
        QSqlDatabase::removeDatabase(tmpConnectionName); // 清理临时连接
        return false;
    }

    // --- 步骤 2: 判断数据库是否存在，如果不存在就创建 ---
    QSqlQuery query(tmpDb);
    // 使用反引号 (`) 包裹数据库名，以防数据库名包含特殊字符
    QString createDbSql = QString("CREATE DATABASE IF NOT EXISTS `%1` CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci").arg(dbName);
    if (!query.exec(createDbSql)) {
        WARNING_LOG << "创建数据库失败:" << query.lastError().text() << "SQL:" << createDbSql;
        tmpDb.close();
        QSqlDatabase::removeDatabase(tmpConnectionName);
        return false;
    }

    tmpDb.close(); // 关闭临时连接
    QSqlDatabase::removeDatabase(tmpConnectionName); // 从Qt连接列表中移除临时连接

    // --- 步骤 3: 再连接到指定数据库 (使用 m_db 成员) ---
    // 先确保 m_db 处于关闭状态，如果之前连接失败过
    if (m_db.isOpen()) {
        m_db.close();
    }

    m_db.setHostName(host);
    m_db.setDatabaseName(dbName);
    m_db.setUserName(user);
    m_db.setPassword(password);
    m_db.setPort(port);

    // // --- 关键修改：在这里设置连接选项，声明客户端编码 ---
    // // 告诉 MySQL 服务器客户端将使用 utf8mb4 编码进行通信
    // m_db.setConnectOptions("SET NAMES 'utf8mb4'");
    // // 另一种更通用的写法，但 'SET NAMES' 几乎总是有效且推荐的
    // // m_db.setConnectOptions("MYSQL_OPT_RECONNECT=1;MYSQL_SET_CHARSET_NAME=utf8mb4");
    // // --- 结束关键修改 ---

    if (!m_db.open()) {
        WARNING_LOG << "连接到指定数据库失败:" << m_db.lastError().text();
        return false;
    }

    DEBUG_LOG << "数据库连接成功，当前使用数据库:" << dbName;

    // // --- 关键修改：在这里设置连接选项，声明客户端编码 ---
    // // 告诉 MySQL 服务器客户端将使用 utf8mb4 编码进行通信
    // m_db.setConnectOptions("SET NAMES 'utf8mb4'");
    // // 另一种更通用的写法，但 'SET NAMES' 几乎总是有效且推荐的
    // // m_db.setConnectOptions("MYSQL_OPT_RECONNECT=1;MYSQL_SET_CHARSET_NAME=utf8mb4");
    // // --- 结束关键修改 ---

    return true;
}



bool DatabaseConnector::ensureTablesExist(const QString &filePath) {

    DEBUG_LOG << "执行SQL文件:" << filePath;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        WARNING_LOG << "无法打开SQL文件:" << filePath;
        return false;
    }

    QTextStream in(&file);
    // --- 关键修改：显式设置 QTextStream 的编码 ---
    in.setCodec("UTF-8"); // <-- 添加这行
    // --- 错误修改结束 ---
    QString sql = in.readAll();
    file.close();

    QSqlQuery query(m_db); // 使用 DatabaseConnector 内部数据库
    if (!query.exec(sql)) {
        WARNING_LOG << "执行SQL文件失败:" << query.lastError().text();
        return false;
    }

    return true;
}
