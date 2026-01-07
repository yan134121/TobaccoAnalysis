#ifndef PROCESSTGBIGDATADAO_H
#define PROCESSTGBIGDATADAO_H

#include "ProcessTgBigData.h" // 引入数据实体
#include <QList>
#include <QSqlQuery> // 引入 QSqlQuery，因为辅助函数中会用到引用

class ProcessTgBigDataDAO {
public:
    // 构造函数
    ProcessTgBigDataDAO() = default;
    explicit ProcessTgBigDataDAO(QSqlDatabase& db) : m_db(db) {}

    // 插入一条大热重处理记录，并更新 processTgBigData 对象的 id
    bool insert(ProcessTgBigData& processTgBigData);
    // 批量插入大热重处理记录
    bool insertBatch(QList<ProcessTgBigData>& processTgBigDataList);
    // 根据 sampleId 查询所有大热重处理记录
    QList<ProcessTgBigData> getBySampleId(int sampleId);
    // 根据 sampleId 删除所有大热重处理记录
    bool removeBySampleId(int sampleId);

private:
    // 辅助函数，将 QSqlQuery 的当前行数据填充到 ProcessTgBigData 对象中
    ProcessTgBigData createProcessTgBigDataFromQuery(QSqlQuery& query);
    QSqlDatabase m_db; // 数据库连接
};

#endif // PROCESSTGBIGDATADAO_H