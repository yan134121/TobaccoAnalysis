#include "ConfigManager.h"
#include <QDir>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QFile>

QString ConfigManager::getSqlConfigPath()
{
    QString jsonPath = findConfigFile("sql_config.json");
    if (!jsonPath.isEmpty()) {
        return jsonPath;
    }
    
    QString xmlPath = findConfigFile("sql_config.xml");
    if (!xmlPath.isEmpty()) {
        return xmlPath;
    }
    
    // 返回默认路径
    return getConfigDirectory() + "/sql_config.json";
}

QString ConfigManager::getDatabaseConfigPath()
{
    QString path = findConfigFile("database_config.json");
    if (!path.isEmpty()) {
        return path;
    }
    
    // 返回默认路径
    return getConfigDirectory() + "/database_config.json";
}

QString ConfigManager::getLogConfigPath()
{
    QString path = findConfigFile("log_config.json");
    if (!path.isEmpty()) {
        return path;
    }
    
    // 返回默认路径
    return getConfigDirectory() + "/log_config.json";
}

QString ConfigManager::getConfigDirectory()
{
    // 优先级：当前目录 > 应用程序目录 > 用户配置目录
    QStringList searchPaths = {
        QDir::currentPath() + "/config",
        QCoreApplication::applicationDirPath() + "/config",
        QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
    };
    
    for (const QString& path : searchPaths) {
        if (QDir(path).exists()) {
            return path;
        }
    }
    
    // 如果都不存在，返回当前目录下的config
    return QDir::currentPath() + "/config";
}

QString ConfigManager::findConfigFile(const QString& filename)
{
    QStringList searchPaths = {
        QDir::currentPath() + "/config/" + filename,
        QCoreApplication::applicationDirPath() + "/config/" + filename,
        QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/" + filename
    };
    
    for (const QString& path : searchPaths) {
        if (QFile::exists(path)) {
            return path;
        }
    }
    
    return QString();
}

bool ConfigManager::ensureConfigDirectory()
{
    QString configDir = getConfigDirectory();
    QDir dir;
    if (!dir.exists(configDir)) {
        return dir.mkpath(configDir);
    }
    return true;
}