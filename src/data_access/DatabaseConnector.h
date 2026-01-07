// #ifndef DATABASECONNECTOR_H
// #define DATABASECONNECTOR_H

// #include <QSqlDatabase>
// #include <QString>

// class DatabaseConnector {
// public:
//     static DatabaseConnector& getInstance();

//     bool connect(const QString &host, const QString &dbName,
//                  const QString &user, const QString &password, int port=3306);

//     QSqlDatabase& getDatabase();

// private:
//     DatabaseConnector() = default;
//     DatabaseConnector(const DatabaseConnector&) = delete;
//     DatabaseConnector& operator=(const DatabaseConnector&) = delete;

//     QSqlDatabase database;
// };

// #endif // DATABASECONNECTOR_H


#ifndef DATABASECONNECTOR_H
#define DATABASECONNECTOR_H

#include <QSqlDatabase>
#include <QString>
#include <QVariantMap> // 用于接收配置参数

class DatabaseConnector {
public:
    // 获取单例实例
    static DatabaseConnector& getInstance();

    // 获取数据库连接对象
    QSqlDatabase& getDatabase();

    // 连接或创建数据库。参数从外部配置文件读取，通过 QVariantMap 传入
    bool connectOrCreateDatabase(const QVariantMap& config);

    bool ensureTablesExist(const QString &filePath);

private:
    // 私有构造函数，强制单例模式
    DatabaseConnector();
    // 析构函数，关闭数据库连接
    ~DatabaseConnector();

    // 禁用拷贝构造函数和赋值运算符，确保单例唯一性
    DatabaseConnector(const DatabaseConnector&) = delete;
    DatabaseConnector& operator=(const DatabaseConnector&) = delete;

    QSqlDatabase m_db;           // 主数据库连接对象
    QString m_connectionName;    // 用于主连接的唯一名称
};

#endif // DATABASECONNECTOR_H