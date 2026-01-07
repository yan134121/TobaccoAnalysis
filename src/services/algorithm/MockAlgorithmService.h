// src/algorithm/MockAlgorithmService.h
#ifndef MOCKALGORITHMSERVICE_H
#define MOCKALGORITHMSERVICE_H

#include "IAlgorithmService.h" // 继承自抽象接口
#include <QDebug>
#include <QJsonObject>
#include <QDateTime> // 用于模拟时间戳
#include "Logger.h"

class MockAlgorithmService : public IAlgorithmService
{
    Q_OBJECT
public:
    explicit MockAlgorithmService(QObject *parent = nullptr) : IAlgorithmService(parent) {
        DEBUG_LOG << "MockAlgorithmService initialized.";
    }

    ~MockAlgorithmService() override = default;

    QJsonDocument processJsonData(const QJsonDocument& rawData,
                                  const QVariantMap& params,
                                  QString& errorMessage) override {
        Q_UNUSED(params); // 暂时不使用 params

        DEBUG_LOG << "MockAlgorithmService: 模拟处理数据...";
        // 检查输入是否有效
        if (rawData.isNull() || rawData.isEmpty()) {
            errorMessage = "输入数据为空，无法模拟处理。";
            WARNING_LOG << "MockAlgorithmService Error:" << errorMessage;
            return QJsonDocument(); // 返回 null 文档表示失败
        }

        // 模拟简单的数据处理，例如添加一个 "processed_by_mock" 字段和时间戳
        QJsonObject obj = rawData.object();
        obj["processed_by_mock"] = true;
        obj["processing_timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);

        // 模拟根据参数的不同行为
        if (params.contains("algorithm_type")) {
            QString algoType = params.value("algorithm_type").toString();
            if (algoType == "smoothing") {
                obj["mock_smoothing_applied"] = true;
            } else if (algoType == "derivation") {
                obj["mock_derivation_applied"] = true;
            }
        }

        DEBUG_LOG << "MockAlgorithmService: 模拟处理完成。";
        errorMessage.clear(); // 清空错误信息表示成功
        return QJsonDocument(obj);
    }
};

#endif // MOCKALGORITHMSERVICE_H
