// src/util/XlsxWriter.h
#ifndef XLSXWRITER_H
#define XLSXWRITER_H

#include "AbstractFileWriter.h"
// #include <QXlsx/xlsxdocument.h> // 引入 QXlsx 库
#include "xlsxdocument.h"

class XlsxWriter : public AbstractFileWriter
{
    Q_OBJECT
public:
    explicit XlsxWriter(QObject *parent = nullptr) : AbstractFileWriter(parent) {}
    ~XlsxWriter() override = default;

    // bool writeToFile(const QString& filePath,
    //                  const QList<QVariantMap>& dataList,
    //                  const QStringList& headers = QStringList(),
    //                  QString& errorMessage) override;

    // --- 同步调整参数顺序 ---
    bool writeToFile(const QString& filePath,
                     const QList<QVariantMap>& dataList,
                     QString& errorMessage,                       
                     const QStringList& headers) override; //
    // --- 结束同步调整 ---
};

#endif // XLSXWRITER_H
