


// src/util/FileHandlerFactory.h
#ifndef FILEHANDLERFACTORY_H
#define FILEHANDLERFACTORY_H

#include <QObject>
#include <QString>
#include "AbstractFileParser.h"
#include "AbstractFileWriter.h"
#include "CsvParser.h"
#include "XlsxParser.h"
#include "CsvWriter.h"
#include "XlsxWriter.h"
// 不需要包含 <QPointer> 了

class FileHandlerFactory : public QObject
{
    Q_OBJECT
public:
    explicit FileHandlerFactory(QObject *parent = nullptr);
    ~FileHandlerFactory() override;

    AbstractFileParser* getParser(const QString& filePath, QString& errorMessage);
    AbstractFileWriter* getWriter(const QString& filePath, QString& errorMessage);

private:
    // 缓存解析器/写入器实例，避免重复创建和内存泄露
    // 由于这些对象将以 this (FileHandlerFactory) 作为父对象创建
    // 当工厂被销毁时，Qt 会自动管理它们的内存
    CsvParser* m_csvParser = nullptr;    // <-- 修改为原始指针
    XlsxParser* m_xlsxParser = nullptr;  // <-- 修改为原始指针
    CsvWriter* m_csvWriter = nullptr;    // <-- 修改为原始指针
    XlsxWriter* m_xlsxWriter = nullptr;  // <-- 修改为原始指针

    QString getFileSuffix(const QString& filePath) const;
};

#endif // FILEHANDLERFACTORY_H