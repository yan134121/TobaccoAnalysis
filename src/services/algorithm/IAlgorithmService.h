// src/algorithm/IAlgorithmService.h
#ifndef IALGORITHMSERVICE_H
#define IALGORITHMSERVICE_H

#include <QObject>
#include <QVariantMap>
#include <QJsonDocument>
#include <QString> // 确保 QString 已包含

class IAlgorithmService : public QObject
{
    Q_OBJECT
public:
    explicit IAlgorithmService(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~IAlgorithmService() = default;

    // 抽象方法：模拟一个处理 JSON 数据的方法，实际算法可能更复杂
    // rawData: 输入的原始数据，例如从数据库的 data_json 字段读取
    // params: 传递给算法的参数，例如处理类型（平滑、求导等）、算法参数
    // errorMessage: 如果算法处理失败，返回错误信息
    // 返回：处理后的 QJsonDocument，如果失败则返回 null 的 QJsonDocument
    virtual QJsonDocument processJsonData(const QJsonDocument& rawData,
                                          const QVariantMap& params,
                                          QString& errorMessage) = 0;

    // 实际中可能还有其他算法接口，例如：
    // virtual QVariantMap analyzeDifference(const QVariantMap& input, QString& errorMessage) = 0;
    // virtual QVariantMap predictQuality(const QVariantMap& input, QString& errorMessage) = 0;
};

#endif // IALGORITHMSERVICE_H