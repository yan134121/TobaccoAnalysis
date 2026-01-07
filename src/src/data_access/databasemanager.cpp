#include "DatabaseManager.h"
#include "Logger.h"

DatabaseManager& DatabaseManager::instance()
{
    static DatabaseManager instance;
    return instance;
}

DatabaseManager::DatabaseManager(QObject *parent) : QObject(parent)
{
    // 确保每次都使用一个新的连接名，避免 Qt 的警告
    m_db = QSqlDatabase::addDatabase("QMYSQL", "tobacco_analysis_connection");
}

DatabaseManager::~DatabaseManager()
{
    if (m_db.isOpen()) {
        m_db.close();
    }
    QSqlDatabase::removeDatabase("tobacco_analysis_connection");
}

bool DatabaseManager::connectToDb(const QString& dbName, const QString& user, const QString& pass, const QString& host, int port)
{
    if (m_db.isOpen()) {
        DEBUG_LOG << "Database is already open.";
        return true;
    }
    
    m_db.setHostName(host);
    m_db.setPort(port);
    m_db.setDatabaseName(dbName);
    m_db.setUserName(user);
    m_db.setPassword(pass);

    if (!m_db.open()) {
        // 数据库连接失败时记录警告日志，但不使用致命级别，避免直接终止程序
        WARNING_LOG << "Failed to connect to database:" << m_db.lastError().text();
        return false;
    }

    // 数据库连接成功
    INFO_LOG << "Database connection successful!";
    return true;
}

void DatabaseManager::disconnectFromDb()
{
    m_db.close();
    INFO_LOG << "Database connection closed.";
}

QSqlDatabase DatabaseManager::database() const
{
    return m_db;
}
