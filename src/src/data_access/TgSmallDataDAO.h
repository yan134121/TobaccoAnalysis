#ifndef TGSMALLDATADAO_H
#define TGSMALLDATADAO_H

#include "TgSmallData.h" // 引入数据实体
#include <QList>
#include <QSqlQuery>

class TgSmallDataDAO {
public:
    // 构造函数
    TgSmallDataDAO() = default;
    explicit TgSmallDataDAO(QSqlDatabase& db) : m_db(db) {}
    
    // 插入一条小热重记录
    bool insert(TgSmallData& tgSmallData);
    // 批量插入小热重记录
    bool insertBatch(QList<TgSmallData>& tgSmallDataList);
    // 根据 sampleId 查询所有小热重记录
    QList<TgSmallData> getBySampleId(int sampleId);
    // 根据 sampleId 删除所有小热重记录
    bool removeBySampleId(int sampleId);

private:
    TgSmallData createTgSmallDataFromQuery(QSqlQuery& query);
    QSqlDatabase m_db; // 数据库连接
};

#endif // TGSMALLDATADAO_H
