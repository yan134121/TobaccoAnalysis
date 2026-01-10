#ifndef TGSMALLRAWDATADAO_H
#define TGSMALLRAWDATADAO_H

#include "TgSmallData.h"
#include <QList>
#include <QSqlQuery>

class TgSmallRawDataDAO {
public:
    TgSmallRawDataDAO() = default;
    explicit TgSmallRawDataDAO(QSqlDatabase& db) : m_db(db) {}

    bool insert(TgSmallData& tgSmallData);
    bool insertBatch(QList<TgSmallData>& tgSmallDataList);
    QList<TgSmallData> getBySampleId(int sampleId);
    bool removeBySampleId(int sampleId);

private:
    TgSmallData createTgSmallDataFromQuery(QSqlQuery& query);
    QSqlDatabase m_db;
};

#endif // TGSMALLRAWDATADAO_H
