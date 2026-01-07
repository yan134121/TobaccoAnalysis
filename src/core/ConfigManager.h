#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QString>

class ConfigManager
{
public:
    static QString getSqlConfigPath();
    static QString getDatabaseConfigPath();
    static QString getLogConfigPath();
    static bool ensureConfigDirectory();
    
private:
    static QString getConfigDirectory();
    static QString findConfigFile(const QString& filename);
};

#endif // CONFIGMANAGER_H