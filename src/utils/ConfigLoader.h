#ifndef CONFIGLOADER_H
#define CONFIGLOADER_H

#include <QString>
#include <QVariantMap> // 用于存储从JSON解析出的配置数据
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QDebug>
#include <QJsonParseError>

class ConfigLoader
{
public:
    // 静态方法，用于从指定路径加载JSON配置文件
    static QVariantMap loadJsonConfig(const QString& filePath);
};

#endif // CONFIGLOADER_H