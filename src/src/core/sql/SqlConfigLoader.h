#ifndef SQLCONFIGLOADER_H
#define SQLCONFIGLOADER_H

#include <QString>
#include <QMap>
#include <QJsonObject>
#include <QJsonDocument>
#include <QtXml/QDomDocument>
#include <QFile>
#include <QDebug>

/**
 * @brief SQL配置加载器类
 * 
 * 用于从JSON或XML配置文件中加载SQL语句，实现SQL语句与代码的解耦
 * 支持两种配置文件格式：JSON和XML
 */
class SqlConfigLoader
{
public:
    /**
     * @brief 配置文件类型枚举
     */
    enum ConfigType {
        JSON_CONFIG,    // JSON配置文件
        XML_CONFIG      // XML配置文件
    };

    /**
     * @brief SQL操作信息结构体
     */
    struct SqlOperation {
        QString sql;                    // SQL语句
        QStringList parameters;         // 参数列表
        QString description;            // 操作描述
        
        SqlOperation() = default;
        SqlOperation(const QString& sqlStr, const QStringList& params, const QString& desc)
            : sql(sqlStr), parameters(params), description(desc) {}
    };

private:
    static SqlConfigLoader* instance;
    QMap<QString, QMap<QString, SqlOperation>> daoOperations;  // DAO类名 -> 操作名 -> SQL操作
    QMap<QString, QString> createTableSqls;                    // 表名 -> 建表SQL
    QString configFilePath;
    ConfigType configType;
    bool isLoaded;

    SqlConfigLoader();

public:
    /**
     * @brief 获取单例实例
     */
    static SqlConfigLoader& getInstance();

    /**
     * @brief 析构函数
     */
    ~SqlConfigLoader();

    /**
     * @brief 加载配置文件
     * @param filePath 配置文件路径
     * @param type 配置文件类型
     * @return 是否加载成功
     */
    bool loadConfig(const QString& filePath, ConfigType type = JSON_CONFIG);

    /**
     * @brief 获取DAO操作的SQL语句
     * @param daoName DAO类名
     * @param operationName 操作名
     * @return SQL操作信息，如果不存在返回空的SqlOperation
     */
    SqlOperation getSqlOperation(const QString& daoName, const QString& operationName) const;

    /**
     * @brief 获取建表SQL语句
     * @param tableName 表名
     * @return 建表SQL语句，如果不存在返回空字符串
     */
    QString getCreateTableSql(const QString& tableName) const;

    /**
     * @brief 检查是否已加载配置
     * @return 是否已加载
     */
    bool isConfigLoaded() const { return isLoaded; }

    /**
     * @brief 获取所有DAO类名
     * @return DAO类名列表
     */
    QStringList getDaoNames() const;

    /**
     * @brief 获取指定DAO的所有操作名
     * @param daoName DAO类名
     * @return 操作名列表
     */
    QStringList getOperationNames(const QString& daoName) const;

    /**
     * @brief 获取所有表名
     * @return 表名列表
     */
    QStringList getTableNames() const;

    /**
     * @brief 重新加载配置文件
     * @return 是否重新加载成功
     */
    bool reloadConfig();

    /**
     * @brief 清空已加载的配置
     */
    void clearConfig();

private:
    /**
     * @brief 从JSON文件加载配置
     * @param filePath 文件路径
     * @return 是否加载成功
     */
    bool loadFromJson(const QString& filePath);

    /**
     * @brief 从XML文件加载配置
     * @param filePath 文件路径
     * @return 是否加载成功
     */
    bool loadFromXml(const QString& filePath);

    /**
     * @brief 解析JSON格式的DAO操作
     * @param daoOperationsJson DAO操作的JSON对象
     */
    void parseJsonDaoOperations(const QJsonObject& daoOperationsJson);

    /**
     * @brief 解析JSON格式的建表语句
     * @param createTablesJson 建表语句的JSON对象
     */
    void parseJsonCreateTables(const QJsonObject& createTablesJson);

    /**
     * @brief 解析XML格式的DAO操作
     * @param daoOperationsElement DAO操作的XML元素
     */
    void parseXmlDaoOperations(const QDomElement& daoOperationsElement);

    /**
     * @brief 解析XML格式的建表语句
     * @param createTablesElement 建表语句的XML元素
     */
    void parseXmlCreateTables(const QDomElement& createTablesElement);
};

/**
 * @brief SQL配置加载器的便捷宏定义
 */
#define SQL_CONFIG SqlConfigLoader::getInstance()

/**
 * @brief 获取SQL语句的便捷宏定义
 * @param dao DAO类名
 * @param operation 操作名
 */
#define GET_SQL(dao, operation) SQL_CONFIG.getSqlOperation(dao, operation).sql

/**
 * @brief 获取SQL参数的便捷宏定义
 * @param dao DAO类名
 * @param operation 操作名
 */
#define GET_SQL_PARAMS(dao, operation) SQL_CONFIG.getSqlOperation(dao, operation).parameters

/**
 * @brief 获取建表SQL的便捷宏定义
 * @param table 表名
 */
#define GET_CREATE_TABLE_SQL(table) SQL_CONFIG.getCreateTableSql(table)

#endif // SQLCONFIGLOADER_H