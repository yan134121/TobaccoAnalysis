#ifndef ABSTRACTFILEPARSER_H
#define ABSTRACTFILEPARSER_H

#include <QObject>
#include <QString>
#include <QList>
#include <QVariantMap>

class AbstractFileParser : public QObject
{
    Q_OBJECT
public:
    explicit AbstractFileParser(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~AbstractFileParser() = default;

    // 抽象方法：解析文件并返回一个QList<QVariantMap>
    // virtual QList<QVariantMap> parseFile(const QString& filePath, QString& errorMessage) = 0;
    // virtual QList<QVariantMap> parseFile(const QString& filePath, QString& errorMessage, int startDataRow = 1, int startDataCol = 1) = 0;
    virtual QList<QVariantList> parseFile(const QString& filePath, QString& errorMessage, int startDataRow = 1, int startDataCol = 1) = 0;

    // 可以在这里定义一些通用的辅助方法，例如：
    // virtual QStringList getHeaderNames(const QString& filePath, QString& errorMessage) = 0;
};

#endif // ABSTRACTFILEPARSER_H