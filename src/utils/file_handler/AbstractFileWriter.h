#ifndef ABSTRACTFILEWRITER_H
#define ABSTRACTFILEWRITER_H

#include <QObject>
#include <QList>
#include <QVariantMap> // 假设Service层提供QVariantMap列表进行导出

class AbstractFileWriter : public QObject
{
    Q_OBJECT
public:
    explicit AbstractFileWriter(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~AbstractFileWriter() = default;

    // 抽象方法：将数据写入指定文件
    // dataList: 要导出的数据列表
    // headers: (可选) 如果需要指定列的顺序或自定义表头
    // virtual bool writeToFile(const QString& filePath,
    //                          const QList<QVariantMap>& dataList,
    //                          const QStringList& headers = QStringList(),
    //                          QString& errorMessage) = 0;
                            
    // --- 关键修改：调整参数顺序 ---
    virtual bool writeToFile(const QString& filePath,
                             const QList<QVariantMap>& dataList,
                             QString& errorMessage,                       // <-- errorMessage 移到前面
                             const QStringList& headers = QStringList()) = 0; // <-- headers 带着默认值放到后面
    // --- 结束关键修改 ---
};

#endif // ABSTRACTFILEWRITER_H