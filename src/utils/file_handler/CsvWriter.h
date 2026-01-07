// src/util/CsvWriter.h
#ifndef CSVWRITER_H
#define CSVWRITER_H

#include "AbstractFileWriter.h"
#include <QFile>
#include <QTextStream>

class CsvWriter : public AbstractFileWriter
{
    Q_OBJECT
public:
    explicit CsvWriter(QObject *parent = nullptr) : AbstractFileWriter(parent) {}
    ~CsvWriter() override = default;

    // bool writeToFile(const QString& filePath,
    //                  const QList<QVariantMap>& dataList,
    //                  const QStringList& headers = QStringList(),
    //                  QString& errorMessage) override;

    // --- 同步调整参数顺序 ---
    bool writeToFile(const QString& filePath,
                     const QList<QVariantMap>& dataList,
                     QString& errorMessage,                       // <-- 同步调整
                     const QStringList& headers) override; // <-- 移除默认值，因为基类已有
    // --- 结束同步调整 ---
};

#endif // CSVWRITER_H