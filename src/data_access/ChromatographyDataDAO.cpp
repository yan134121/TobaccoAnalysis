#include "ChromatographyDataDAO.h"
#include "DatabaseConnector.h"
#include "core/sql/SqlConfigLoader.h"
#include <QSqlError>
#include <QDebug>
#include <QVariantList>
#include "Logger.h"

/*************  ✨ Windsurf Command ⭐  *************/
/**
 * \brief Creates a ChromatographyData object from a QSqlQuery object.
 *
 * \param query A QSqlQuery object that points to a row in the chromatography_data table.
 *
 * \return A ChromatographyData object populated with the columns of the query.
 */
/*******  b057a2f3-b4ba-4733-8d73-96ff2ecf85a6  *******/
ChromatographyData ChromatographyDataDAO::createChromatographyDataFromQuery(QSqlQuery& query) {
    // DEBUG_LOG << "ChromatographyDataDAO::createChromatographyDataFromQuery";
    ChromatographyData t;
    t.setId(query.value("id").toInt());
    t.setSampleId(query.value("sample_id").toInt());
    // t.setReplicateNo(query.value("replicate_no").toInt());
    t.setRetentionTime(query.value("retention_time").toDouble()); // <-- 字段名不同
    t.setResponseValue(query.value("response_value").toDouble()); // <-- 字段名不同
    // t.setSourceName(query.value("source_name").toString());
    t.setCreatedAt(query.value("created_at").toDateTime());
    return t;
}

bool ChromatographyDataDAO::insert(ChromatographyData& chromatographyData) {
    DEBUG_LOG << "ChromatographyDataDAO::insert";
    QSqlDatabase db = m_db.isValid() ? m_db : DatabaseConnector::getInstance().getDatabase();
    if (!db.isOpen()) {
        WARNING_LOG << "Database not open for ChromatographyData insert operation.";
        return false;
    }

    // 从SqlConfigLoader获取SQL语句
    SqlConfigLoader& loader = SqlConfigLoader::getInstance();
    SqlConfigLoader::SqlOperation operation = loader.getSqlOperation("ChromatographyDataDAO", "insert");
    
    QString sql;
    if (!operation.sql.isEmpty()) {
        sql = operation.sql;
    } else {
        WARNING_LOG << "未找到ChromatographyDataDAO.insert的SQL配置，使用默认SQL";
        sql = "INSERT INTO chromatography_data (sample_id, retention_time, response_value, source_filename) "
              "VALUES (:sample_id, :retention_time, :response_value, :source_filename)";
    }
    
    QSqlQuery query(db);
    query.prepare(sql);
    query.bindValue(":sample_id", chromatographyData.getSampleId());
    // query.bindValue(":replicate_no", chromatographyData.getReplicateNo());
    query.bindValue(":retention_time", chromatographyData.getRetentionTime());
    query.bindValue(":response_value", chromatographyData.getResponseValue());
    query.bindValue(":source_filename", chromatographyData.getSourceName());

    if (query.exec()) {
        chromatographyData.setId(query.lastInsertId().toInt());
        return true;
    } else {
        WARNING_LOG << "Insert ChromatographyData failed:" << query.lastError().text();
        return false;
    }
}

bool ChromatographyDataDAO::insertBatch(QList<ChromatographyData>& chromatographyDataList) {
    if (chromatographyDataList.isEmpty()) {
        DEBUG_LOG << "ChromatographyDataList is empty, no batch insert performed.";
        return true;
    }
    QSqlDatabase db = m_db.isValid() ? m_db : DatabaseConnector::getInstance().getDatabase();
    if (!db.isOpen()) { /* ... */ return false; }
    
    // 从SqlConfigLoader获取SQL语句
    SqlConfigLoader& loader = SqlConfigLoader::getInstance();
    SqlConfigLoader::SqlOperation operation = loader.getSqlOperation("ChromatographyDataDAO", "insert_batch");
    
    QString sql;
    if (!operation.sql.isEmpty()) {
        sql = operation.sql;
    } else {
        WARNING_LOG << "未找到ChromatographyDataDAO.insert_batch的SQL配置，使用默认SQL";
        sql = "INSERT INTO chromatography_data (sample_id, retention_time, response_value, source_filename) "
              "VALUES (?, ?, ?, ?)";
    }
    
    QSqlQuery query(db);
    query.prepare(sql);

    QVariantList sampleIds;
    QVariantList replicateNos;
    QVariantList retentionTimes;
    QVariantList responseValues;
    QVariantList sourceNames;

    for (const ChromatographyData& data : chromatographyDataList) {
        sampleIds << data.getSampleId();
        // replicateNos << data.getReplicateNo();
        retentionTimes << data.getRetentionTime();
        responseValues << data.getResponseValue();
        sourceNames << data.getSourceName();
    }

    query.addBindValue(sampleIds);
    // query.addBindValue(replicateNos);
    query.addBindValue(retentionTimes);
    query.addBindValue(responseValues);
    query.addBindValue(sourceNames);

    if (query.execBatch()) {
        DEBUG_LOG << "Batch insert ChromatographyData successful, inserted" << chromatographyDataList.size() << "rows.";
        return true;
    } else {
        WARNING_LOG << "Batch insert ChromatographyData failed:" << query.lastError().text();
        return false;
    }
}

#include <QElapsedTimer>

QList<ChromatographyData> ChromatographyDataDAO::getBySampleId(int sampleId) {

    QElapsedTimer timer;  //  先声明
    timer.restart();

    QSqlDatabase db = m_db.isValid() ? m_db : DatabaseConnector::getInstance().getDatabase();
    if (!db.isOpen()) { /* ... */ return QList<ChromatographyData>(); }
    
    // 从SqlConfigLoader获取SQL语句
    SqlConfigLoader& loader = SqlConfigLoader::getInstance();
    SqlConfigLoader::SqlOperation operation = loader.getSqlOperation("ChromatographyDataDAO", "select_by_sample_id");
    
    QString sql;
    if (!operation.sql.isEmpty()) {
        sql = operation.sql;
    } else {
        WARNING_LOG << "未找到ChromatographyDataDAO.select_by_sample_id的SQL配置，使用默认SQL";
        sql = "SELECT * FROM chromatography_data WHERE sample_id = :sample_id ORDER BY id, retention_time";
    }
    
    QSqlQuery query(db);
    query.prepare(sql);
    query.bindValue(":sample_id", sampleId);
    QList<ChromatographyData> list;
    if (query.exec()) {
        while (query.next()) {
            list.append(createChromatographyDataFromQuery(query));
        }
    } else { /* ... */ }

    DEBUG_LOG << "Query took:" << timer.elapsed() << "ms";

    return list;
}

bool ChromatographyDataDAO::removeBySampleId(int sampleId) {
    QSqlDatabase db = m_db.isValid() ? m_db : DatabaseConnector::getInstance().getDatabase();
    if (!db.isOpen()) { /* ... */ return false; }
    
    // 从SqlConfigLoader获取SQL语句
    SqlConfigLoader& loader = SqlConfigLoader::getInstance();
    SqlConfigLoader::SqlOperation operation = loader.getSqlOperation("ChromatographyDataDAO", "delete_by_sample_id");
    
    QString sql;
    if (!operation.sql.isEmpty()) {
        sql = operation.sql;
    } else {
        WARNING_LOG << "未找到ChromatographyDataDAO.delete_by_sample_id的SQL配置，使用默认SQL";
        sql = "DELETE FROM chromatography_data WHERE sample_id = :sample_id";
    }
    
    QSqlQuery query(db);
    query.prepare(sql);
    query.bindValue(":sample_id", sampleId);
    if (query.exec()) {
        DEBUG_LOG << "Removed ChromatographyData for sample_id" << sampleId << ", affected" << query.numRowsAffected() << "rows.";
        return query.numRowsAffected() > 0;
    } else { /* ... */ return false; }
}
