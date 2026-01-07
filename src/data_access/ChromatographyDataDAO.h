#ifndef CHROMATOGRAPHYDATADAO_H
#define CHROMATOGRAPHYDATADAO_H

#include "ChromatographyData.h" // 引入数据实体
#include <QList>
#include <QSqlQuery>

class ChromatographyDataDAO {
public:

    ChromatographyDataDAO() = default;
    explicit ChromatographyDataDAO(QSqlDatabase& db) : m_db(db) {}

    // 插入一条色谱记录
    bool insert(ChromatographyData& chromatographyData);
    // 批量插入色谱记录
    bool insertBatch(QList<ChromatographyData>& chromatographyDataList);
    // 根据 sampleId 查询所有色谱记录
    QList<ChromatographyData> getBySampleId(int sampleId);
    // 根据 sampleId 删除所有色谱记录
    bool removeBySampleId(int sampleId);

private:
    ChromatographyData createChromatographyDataFromQuery(QSqlQuery& query);
    QSqlDatabase m_db; // 数据库连接
};

#endif // CHROMATOGRAPHYDATADAO_H
