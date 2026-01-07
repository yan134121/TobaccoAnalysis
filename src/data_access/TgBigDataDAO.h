#ifndef TGBIGDATADAO_H
#define TGBIGDATADAO_H

#include "TgBigData.h" // 引入数据实体
#include <QList>
#include <QSqlQuery> 

class TgBigDataDAO {
public:
    // 构造函数
    TgBigDataDAO() = default;
    explicit TgBigDataDAO(QSqlDatabase& db) : m_db(db) {}


    bool insert(TgBigData& tgBigData);

    bool insertBatch(QList<TgBigData>& tgBigDataList);

    QList<TgBigData> getBySampleId(int sampleId);

    bool removeBySampleId(int sampleId);

private:

    TgBigData createTgBigDataFromQuery(QSqlQuery& query);
    QSqlDatabase m_db; // 数据库连接
};

#endif // TGBIGDATADAO_H
