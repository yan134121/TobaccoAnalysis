// 日志模块实现

#include "Logger.h"
#include <QTextStream>
#include <QDateTime>
#include <QFileInfo>
#include <QDir>
#include <QThread>
#include <QMutexLocker>
#include <QCoreApplication>

// 单例实例
Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

// 构造函数
Logger::Logger() : m_logLevel(LOG_DEBUG), m_consoleOutput(true) {
}

// 析构函数
Logger::~Logger() {
    close();
}

// 初始化日志系统
bool Logger::init(const QString& logFilePath) {
    QMutexLocker locker(&m_mutex);
    
    // 如果已经打开，先关闭
    if (m_logFile.isOpen()) {
        m_logFile.close();
    }
    
    // 确保目录存在
    QFileInfo fileInfo(logFilePath);
    QDir dir = fileInfo.dir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    // 检查日志文件是否过期（超过7天）
    if (fileInfo.exists()) {
        QDateTime lastModified = fileInfo.lastModified();
        QDateTime sevenDaysAgo = QDateTime::currentDateTime().addDays(-7);
        
        if (lastModified < sevenDaysAgo) {
            QFile::remove(logFilePath);  // 删除旧日志文件
            log(LOG_INFO, "Log file older than 7 days has been deleted.");
        }
    }
    
    // 打开日志文件
    m_logFile.setFileName(logFilePath);
    if (!m_logFile.open(QIODevice::Append | QIODevice::Text)) {
        qCritical() << "Failed to open log file:" << logFilePath;
        return false;
    }
    
    // 写入启动信息
    QTextStream out(&m_logFile);
    QString time = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    out << QString("\n[%1] [INFO] ====================== 应用程序启动 ======================\n").arg(time);
    out.flush();
    
    return true;
}

// 写入日志
void Logger::log(LogLevel level, const QString& message, const char* file, int line, const char* function) {
    // 检查日志级别
    if (level < m_logLevel) {
        return;
    }
    
    QMutexLocker locker(&m_mutex);
    
    // 获取当前时间
    QString time = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    
    // 获取线程ID
    QString threadId = QString("0x%1").arg(reinterpret_cast<quintptr>(QThread::currentThreadId()), 0, 16);
    
    // 构建日志消息
    QString logMessage;
    if (file && line > 0) {
        // 提取文件名（不包含路径）
        QString fileName = QString(file).split('/').last().split('\\').last();
        
        if (function) {
            logMessage = QString("[%1] [%2] [%3] %4 (%5:%6, %7)\n")
                .arg(time)
                .arg(levelToString(level))
                .arg(threadId)
                .arg(message)
                .arg(fileName)
                .arg(line)
                .arg(function);
        } else {
            logMessage = QString("[%1] [%2] [%3] %4 (%5:%6)\n")
                .arg(time)
                .arg(levelToString(level))
                .arg(threadId)
                .arg(message)
                .arg(fileName)
                .arg(line);
        }
    } else {
        logMessage = QString("[%1] [%2] [%3] %4\n")
            .arg(time)
            .arg(levelToString(level))
            .arg(threadId)
            .arg(message);
    }
    
    // 写入日志文件
    if (m_logFile.isOpen()) {
        QTextStream out(&m_logFile);
        out << logMessage;
        out.flush();
    }
    
    // 输出到控制台
    if (m_consoleOutput) {
        switch (level) {
        case LOG_DEBUG:
            qDebug().noquote() << logMessage;
            break;
        case LOG_INFO:
            qInfo().noquote() << logMessage;
            break;
        case LOG_WARNING:
            qWarning().noquote() << logMessage;
            break;
        case LOG_ERROR:
        case LOG_FATAL:
            qCritical().noquote() << logMessage;
            break;
        case LOG_SQL:
            qDebug().noquote() << "SQL:" << logMessage;
            break;
        }
    }
    
    // 如果是致命错误，终止程序
    if (level == LOG_FATAL) {
        abort();
    }
}

// 设置日志级别
void Logger::setLogLevel(LogLevel level) {
    m_logLevel = level;
}

// 获取当前日志级别
LogLevel Logger::getLogLevel() const {
    return m_logLevel;
}

// 关闭日志
void Logger::close() {
    QMutexLocker locker(&m_mutex);
    
    if (m_logFile.isOpen()) {
        // 写入关闭信息
        QTextStream out(&m_logFile);
        QString time = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
        out << QString("[%1] [INFO] ====================== 应用程序关闭 ======================\n").arg(time);
        out.flush();
        
        m_logFile.close();
    }
}

// 是否将日志同时输出到控制台
void Logger::setConsoleOutput(bool enable) {
    m_consoleOutput = enable;
}

// 日志级别转字符串
QString Logger::levelToString(LogLevel level) {
    switch (level) {
    case LOG_DEBUG:   return "DEBUG";
    case LOG_INFO:    return "INFO";
    case LOG_WARNING: return "WARNING";
    case LOG_ERROR:   return "ERROR";
    case LOG_FATAL:   return "FATAL";
    case LOG_SQL:     return "SQL";
    default:          return "UNKNOWN";
    }
}

// 打印线程信息
void printThreadInfo(const QString& funcName) {
    static Qt::HANDLE mainThreadId = QThread::currentThreadId(); // 程序启动时主线程ID，放静态变量
    Qt::HANDLE currentThreadId = QThread::currentThreadId();
    
    QString threadInfo = QString("%1 当前线程ID: 0x%2 主线程ID: 0x%3 %4")
        .arg(funcName)
        .arg(reinterpret_cast<quintptr>(currentThreadId), 0, 16)
        .arg(reinterpret_cast<quintptr>(mainThreadId), 0, 16)
        .arg(currentThreadId == mainThreadId ? "[主线程]" : "[非主线程]");
    
    LOG_DEBUG(threadInfo);
}
