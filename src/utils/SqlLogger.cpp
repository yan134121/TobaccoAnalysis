// SQL日志拦截器实现
#include "SqlLogger.h"

// 单例实例
SqlLogger& SqlLogger::instance() {
    static SqlLogger instance;
    return instance;
}

// 构造函数
SqlLogger::SqlLogger(QObject* parent) : QObject(parent), m_enabled(true) {
}

// 析构函数
SqlLogger::~SqlLogger() {
}

// 记录普通SQL查询
void SqlLogger::logQuery(const QString& sql, bool success, const QSqlError& error) {
    if (!m_enabled) {
        return;
    }
    
    if (success) {
        // 确保SQL日志写入文件
        Logger::instance().log(LOG_SQL, QString("执行SQL: %1").arg(sql), __FILE__, __LINE__, __FUNCTION__);
    } else {
        Logger::instance().log(LOG_ERROR, QString("SQL错误: %1\nSQL: %2").arg(error.text()).arg(sql), __FILE__, __LINE__, __FUNCTION__);
    }
}

// 记录预处理SQL查询
void SqlLogger::logPreparedQuery(const QString& sql, const QMap<QString, QVariant>& bindValues, 
                               bool success, const QSqlError& error) {
    if (!m_enabled) {
        return;
    }
    
    QString logMsg = QString("执行预处理SQL: %1").arg(sql);
    if (!bindValues.isEmpty()) {
        logMsg += "\n绑定参数: ";
        for (auto it = bindValues.constBegin(); it != bindValues.constEnd(); ++it) {
            logMsg += QString("%1=%2, ").arg(it.key()).arg(it.value().toString());
        }
        logMsg.chop(2); // 移除最后的逗号和空格
    }
    
    if (success) {
        Logger::instance().log(LOG_SQL, logMsg, __FILE__, __LINE__, __FUNCTION__);
    } else {
        Logger::instance().log(LOG_ERROR, QString("%1\nSQL错误: %2").arg(logMsg).arg(error.text()), __FILE__, __LINE__, __FUNCTION__);
    }
}

// 记录批处理SQL查询
void SqlLogger::logBatchQuery(const QString& sql, bool success, const QSqlError& error) {
    if (!m_enabled) {
        return;
    }
    
    if (success) {
        Logger::instance().log(LOG_SQL, QString("执行批处理SQL: %1").arg(sql), __FILE__, __LINE__, __FUNCTION__);
    } else {
        Logger::instance().log(LOG_ERROR, QString("批处理SQL错误: %1\nSQL: %2").arg(error.text()).arg(sql), __FILE__, __LINE__, __FUNCTION__);
    }
}

// 记录事务操作
void SqlLogger::logTransaction(const QString& operation, bool success, const QSqlError& error) {
    if (!m_enabled) {
        return;
    }
    
    if (success) {
        Logger::instance().log(LOG_SQL, QString("事务操作: %1").arg(operation), __FILE__, __LINE__, __FUNCTION__);
    } else {
        Logger::instance().log(LOG_ERROR, QString("事务操作错误: %1\n操作: %2").arg(error.text()).arg(operation), __FILE__, __LINE__, __FUNCTION__);
    }
}

// 设置是否启用SQL日志
void SqlLogger::setEnabled(bool enabled) {
    m_enabled = enabled;
}

// 获取是否启用SQL日志
bool SqlLogger::isEnabled() const {
    return m_enabled;
}