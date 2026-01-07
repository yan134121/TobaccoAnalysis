// 日志模块头文件
// #include "logger.h" 
#ifndef LOGGER_H
#define LOGGER_H

#include <QString>
#include <QDebug>
#include <QFile>
#include <QMutex>

// 日志级别枚举
enum LogLevel {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR,
    LOG_FATAL,
    LOG_SQL
};

// 日志类
class Logger {
public:
    // 获取单例实例
    static Logger& instance();
    
    // 初始化日志系统
    bool init(const QString& logFilePath = "app_log.txt");
    
    // 写入日志
    void log(LogLevel level, const QString& message, const char* file = nullptr, int line = -1, const char* function = nullptr);
    
    // 设置日志级别
    void setLogLevel(LogLevel level);
    
    // 获取当前日志级别
    LogLevel getLogLevel() const;
    
    // 关闭日志
    void close();
    
    // 是否将日志同时输出到控制台
    void setConsoleOutput(bool enable);
    
private:
    Logger();
    ~Logger();
    
    // 禁止拷贝和赋值
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    QString levelToString(LogLevel level);
    
    QFile m_logFile;
    QMutex m_mutex;
    LogLevel m_logLevel;
    bool m_consoleOutput;
};

// 流式日志宏定义
class LogStream {
public:
    LogStream(LogLevel level, const char* file, int line, const char* function)
        : m_level(level), m_file(file), m_line(line), m_function(function) {}
    
    ~LogStream() {
        Logger::instance().log(m_level, m_message, m_file, m_line, m_function);
    }
    
    // 数值类型
    LogStream& operator<<(int value) {
        m_message += QString::number(value);
        return *this;
    }
    
    LogStream& operator<<(unsigned int value) {
        m_message += QString::number(value);
        return *this;
    }
    
    LogStream& operator<<(long value) {
        m_message += QString::number(value);
        return *this;
    }
    
    LogStream& operator<<(unsigned long value) {
        m_message += QString::number(value);
        return *this;
    }
    
    LogStream& operator<<(long long value) {
        m_message += QString::number(value);
        return *this;
    }
    
    LogStream& operator<<(unsigned long long value) {
        m_message += QString::number(value);
        return *this;
    }
    
    LogStream& operator<<(float value) {
        m_message += QString::number(value);
        return *this;
    }
    
    LogStream& operator<<(double value) {
        m_message += QString::number(value);
        return *this;
    }
    
    // 特化QString类型，避免多余的转换
    LogStream& operator<<(const QString& value) {
        m_message += value;
        return *this;
    }
    
    // 特化字符串常量
    LogStream& operator<<(const char* value) {
        m_message += QString(value);
        return *this;
    }
    
    // 特化QStringList类型
    LogStream& operator<<(const QStringList& value) {
        m_message += value.join(", ");
        return *this;
    }
    
    // 特化指针类型
    template<typename T>
    LogStream& operator<<(T* const& ptr) {
        m_message += QString("0x%1").arg(reinterpret_cast<quintptr>(ptr), 0, 16);
        return *this;
    }
    
    // 枚举类型特化
    template<typename T, typename std::enable_if<std::is_enum<T>::value, int>::type = 0>
    LogStream& operator<<(const T& value) {
        m_message += QString::number(static_cast<int>(value));
        return *this;
    }
    
    // 通用类型，尝试使用QVariant转换
    template<typename T, typename std::enable_if<!std::is_enum<T>::value, int>::type = 0>
    LogStream& operator<<(const T& value) {
        QVariant var;
        var.setValue(value);
        if (var.canConvert<QString>()) {
            m_message += var.toString();
        } else {
            m_message += "[Object]";
        }
        return *this;
    }
    
private:
    LogLevel m_level;
    QString m_message;
    const char* m_file;
    int m_line;
    const char* m_function;
};

// 便捷宏定义 - 流式风格
#define DEBUG_LOG LogStream(LOG_DEBUG, __FILE__, __LINE__, __FUNCTION__)
#define INFO_LOG LogStream(LOG_INFO, __FILE__, __LINE__, __FUNCTION__)
#define WARNING_LOG LogStream(LOG_WARNING, __FILE__, __LINE__, __FUNCTION__)
#define ERROR_LOG LogStream(LOG_ERROR, __FILE__, __LINE__, __FUNCTION__)
#define FATAL_LOG LogStream(LOG_FATAL, __FILE__, __LINE__, __FUNCTION__)
#define SQL_LOG LogStream(LOG_SQL, __FILE__, __LINE__, __FUNCTION__)

// 保留旧的宏定义以兼容现有代码
#define LOG_DEBUG(msg) Logger::instance().log(LOG_DEBUG, msg, __FILE__, __LINE__, __FUNCTION__)
#define LOG_INFO(msg) Logger::instance().log(LOG_INFO, msg, __FILE__, __LINE__, __FUNCTION__)
#define LOG_WARNING(msg) Logger::instance().log(LOG_WARNING, msg, __FILE__, __LINE__, __FUNCTION__)
#define LOG_ERROR(msg) Logger::instance().log(LOG_ERROR, msg, __FILE__, __LINE__, __FUNCTION__)
#define LOG_FATAL(msg) Logger::instance().log(LOG_FATAL, msg, __FILE__, __LINE__, __FUNCTION__)
#define LOG_SQL(msg) Logger::instance().log(LOG_SQL, msg, __FILE__, __LINE__, __FUNCTION__)

// SQL日志记录宏
#define LOG_SQL_QUERY(query) SQL_LOG << "执行SQL: " << query
#define LOG_SQL_PREPARED(query, bindValues) { \
    QString logMsg = QString("执行预处理SQL: %1").arg(query); \
    if (!bindValues.isEmpty()) { \
        logMsg += "\n绑定参数: "; \
        for (auto it = bindValues.constBegin(); it != bindValues.constEnd(); ++it) { \
            logMsg += QString("%1=%2, ").arg(it.key()).arg(it.value().toString()); \
        } \
        logMsg.chop(2); \
    } \
    Logger::instance().log(LOG_SQL, logMsg, __FILE__, __LINE__, __FUNCTION__); \
}
#define LOG_SQL_ERROR(error, query) ERROR_LOG << "SQL错误: " << error << "\nSQL: " << query
#define DEBUG_PTR(ptr) DEBUG_LOG << #ptr << " = " << QString("0x%1").arg(reinterpret_cast<quintptr>(ptr), 0, 16)

// 初始化日志系统（在 main 函数里调用一次）
inline bool initLogger(const QString& logFilePath = "app_log.txt") {
    return Logger::instance().init(logFilePath);
}

// 打印线程信息
void printThreadInfo(const QString& funcName);

#endif // LOGGER_H
