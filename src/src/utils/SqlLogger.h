// SQL日志拦截器头文件
#ifndef SQLLOGGER_H
#define SQLLOGGER_H

#include <QObject>
#include <QSqlQuery>
#include <QSqlError>
#include <QMap>
#include <QVariant>
#include "Logger.h"

/**
 * @brief SQL日志拦截器类
 * 
 * 这个类用于拦截和记录SQL查询操作，提供统一的日志记录接口
 */
class SqlLogger : public QObject {
    Q_OBJECT

public:
    // 获取单例实例
    static SqlLogger& instance();
    
    // 记录普通SQL查询
    void logQuery(const QString& sql, bool success = true, const QSqlError& error = QSqlError());
    
    // 记录预处理SQL查询
    void logPreparedQuery(const QString& sql, const QMap<QString, QVariant>& bindValues, 
                         bool success = true, const QSqlError& error = QSqlError());
    
    // 记录批处理SQL查询
    void logBatchQuery(const QString& sql, bool success = true, const QSqlError& error = QSqlError());
    
    // 记录事务操作
    void logTransaction(const QString& operation, bool success = true, const QSqlError& error = QSqlError());
    
    // 设置是否启用SQL日志
    void setEnabled(bool enabled);
    
    // 获取是否启用SQL日志
    bool isEnabled() const;

private:
    SqlLogger(QObject* parent = nullptr);
    ~SqlLogger();
    
    // 禁止拷贝和赋值
    SqlLogger(const SqlLogger&) = delete;
    SqlLogger& operator=(const SqlLogger&) = delete;
    
    bool m_enabled;
};

// 便捷宏定义
#define SQL_LOG_QUERY(sql) SqlLogger::instance().logQuery(sql)
#define SQL_LOG_QUERY_ERROR(sql, error) SqlLogger::instance().logQuery(sql, false, error)
#define SQL_LOG_PREPARED(sql, bindValues) SqlLogger::instance().logPreparedQuery(sql, bindValues)
#define SQL_LOG_PREPARED_ERROR(sql, bindValues, error) SqlLogger::instance().logPreparedQuery(sql, bindValues, false, error)
#define SQL_LOG_BATCH(sql) SqlLogger::instance().logBatchQuery(sql)
#define SQL_LOG_BATCH_ERROR(sql, error) SqlLogger::instance().logBatchQuery(sql, false, error)
#define SQL_LOG_TRANSACTION(operation) SqlLogger::instance().logTransaction(operation)
#define SQL_LOG_TRANSACTION_ERROR(operation, error) SqlLogger::instance().logTransaction(operation, false, error)

#endif // SQLLOGGER_H