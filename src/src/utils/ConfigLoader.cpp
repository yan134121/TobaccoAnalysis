#include "ConfigLoader.h"
#include "Logger.h"

QVariantMap ConfigLoader::loadJsonConfig(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        WARNING_LOG << "Failed to open config file:" << filePath << "-" << file.errorString();
        return QVariantMap();
    }

    QByteArray fileContent = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(fileContent, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        WARNING_LOG << "Failed to parse config file" << filePath << ":" << parseError.errorString();
        return QVariantMap();
    }

    if (!doc.isObject()) {
        WARNING_LOG << "Config file" << filePath << "is not a valid JSON object.";
        return QVariantMap();
    }

    return doc.object().toVariantMap();
}
