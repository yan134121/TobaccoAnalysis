// // src/util/FileHandlerFactory.cpp
// #include "FileHandlerFactory.h"
// #include <QFileInfo>
// #include <QDebug>

// FileHandlerFactory::FileHandlerFactory(QObject *parent) : QObject(parent)
// {
//     // 在构造函数中创建所有解析器和写入器的实例
//     // 它们将以 this (FileHandlerFactory) 作为父对象，这样当工厂被销毁时，它们也会被自动销毁
//     m_csvParser = new CsvParser(this);
//     m_xlsxParser = new XlsxParser(this);
//     m_csvWriter = new CsvWriter(this);
//     m_xlsxWriter = new XlsxWriter(this);
//     DEBUG_LOG << "FileHandlerFactory initialized with all file handlers.";
// }

// FileHandlerFactory::~FileHandlerFactory()
// {
//     // 由于所有成员都以 this 作为父对象，Qt 会自动管理它们的内存
//     // 不需要在这里手动 delete m_csvParser 等，QPointer 会自动处理悬空指针问题
//     DEBUG_LOG << "FileHandlerFactory destroyed.";
// }

// src/util/FileHandlerFactory.cpp
#include "FileHandlerFactory.h"
#include "CsvParser.h"
#include "CsvWriter.h"
#include "XlsxParser.h"
#include "XlsxWriter.h"
#include <QFileInfo>
#include <QDebug>
#include "Logger.h"

FileHandlerFactory::FileHandlerFactory(QObject *parent) : QObject(parent)
{
    m_csvParser = new CsvParser(this);
    m_xlsxParser = new XlsxParser(this);
    m_csvWriter = new CsvWriter(this);
    m_xlsxWriter = new XlsxWriter(this);
    DEBUG_LOG << "FileHandlerFactory initialized with all file handlers.";
}

FileHandlerFactory::~FileHandlerFactory()
{
    // 由于所有成员都以 this 作为父对象，Qt 会自动管理它们的内存
    // 不需要在这里手动 delete m_csvParser 等
    DEBUG_LOG << "FileHandlerFactory destroyed.";
}


QString FileHandlerFactory::getFileSuffix(const QString& filePath) const
{
    QFileInfo fileInfo(filePath);
    return fileInfo.suffix().toLower();
}

AbstractFileParser* FileHandlerFactory::getParser(const QString& filePath, QString& errorMessage)
{
    QString suffix = getFileSuffix(filePath);

    if (suffix == "csv") {
        return m_csvParser;
    // } else if (suffix == "xlsx" || suffix == suffix == "xls") { // xlsx 和 xls 都由 XlsxParser 处理
    }else if (suffix == "xlsx" || suffix == "xls") { // <-- 修改为正确的逻辑
        return m_xlsxParser;
    } else {
        errorMessage = "不支持的文件导入格式: ." + suffix;
        WARNING_LOG << errorMessage;
        return nullptr;
    }
}

AbstractFileWriter* FileHandlerFactory::getWriter(const QString& filePath, QString& errorMessage)
{
    QString suffix = getFileSuffix(filePath);

    if (suffix == "csv") {
        return m_csvWriter;
    // } else if (suffix == "xlsx" || suffix == suffix == "xls") {
    }else if (suffix == "xlsx" || suffix == "xls") { // <-- 修改为正确的逻辑
        return m_xlsxWriter;
    } else {
        errorMessage = "不支持的文件导出格式: ." + suffix;
        WARNING_LOG << errorMessage;
        return nullptr;
    }
}

