#include "TgSmallRawDataDAO.h"
#include "DatabaseConnector.h"
#include "core/sql/SqlConfigLoader.h"
#include "Logger.h"
#include <QSqlError>
#include <QVariantList>

TgSmallData TgSmallRawDataDAO::createTgSmallDataFromQuery(QSqlQuery& query)
{
    TgSmallData t;
    t.setId(query.value("id").toInt());
    t.setSampleId(query.value("sample_id").toInt());
    t.setSerialNo(query.value("serial_no").toInt());
    t.setTemperature(query.value("temperature").toDouble());
    t.setWeight(query.value("weight").toDouble());
    t.setTgValue(query.value("tg_value").toDouble());
    t.setDtgValue(query.value("dtg_value").toDouble());
    t.setSourceName(query.value("source_filename").toString());
    t.setCreatedAt(query.value("created_at").toDateTime());
    return t;
}

bool TgSmallRawDataDAO::insert(TgSmallData& tgSmallData)
{
    QSqlDatabase db = m_db.isValid() ? m_db : DatabaseConnector::getInstance().getDatabase();
    if (!db.isOpen()) {
        WARNING_LOG << "Database not open for TgSmallRawData insert operation.";
        return false;
    }

    SqlConfigLoader& loader = SqlConfigLoader::getInstance();
    SqlConfigLoader::SqlOperation operation = loader.getSqlOperation("TgSmallRawDataDAO", "insert");
    QString sql = operation.sql;
    if (sql.isEmpty()) {
        WARNING_LOG << "未找到TgSmallRawDataDAO.insert的SQL配置，使用默认SQL";
        sql = "INSERT INTO tg_small_raw_data (sample_id, serial_no, temperature, weight, tg_value, dtg_value, source_filename) "
              "VALUES (:sample_id, :serial_no, :temperature, :weight, :tg_value, :dtg_value, :source_filename)";
    }

    QSqlQuery query(db);
    query.prepare(sql);
    query.bindValue(":sample_id", tgSmallData.getSampleId());
    query.bindValue(":serial_no", tgSmallData.getSerialNo());
    query.bindValue(":temperature", tgSmallData.getTemperature());
    query.bindValue(":weight", tgSmallData.getWeight());
    query.bindValue(":tg_value", tgSmallData.getTgValue());
    query.bindValue(":dtg_value", tgSmallData.getDtgValue());
    query.bindValue(":source_filename", tgSmallData.getSourceName());

    if (query.exec()) {
        tgSmallData.setId(query.lastInsertId().toInt());
        return true;
    }

    WARNING_LOG << "Insert TgSmallRawData failed:" << query.lastError().text();
    return false;
}

bool TgSmallRawDataDAO::insertBatch(QList<TgSmallData>& tgSmallDataList)
{
    if (tgSmallDataList.isEmpty()) {
        DEBUG_LOG << "TgSmallRawDataList is empty, no batch insert performed.";
        return true;
    }
    QSqlDatabase db = m_db.isValid() ? m_db : DatabaseConnector::getInstance().getDatabase();
    if (!db.isOpen()) {
        WARNING_LOG << "Database not open for TgSmallRawData batch insert operation.";
        return false;
    }

    SqlConfigLoader& loader = SqlConfigLoader::getInstance();
    SqlConfigLoader::SqlOperation operation = loader.getSqlOperation("TgSmallRawDataDAO", "insert_batch");
    QString sql = operation.sql;
    if (sql.isEmpty()) {
        WARNING_LOG << "未找到TgSmallRawDataDAO.insert_batch的SQL配置，使用默认SQL";
        sql = "INSERT INTO tg_small_raw_data (sample_id, serial_no, temperature, weight, tg_value, dtg_value, source_filename) "
              "VALUES (?, ?, ?, ?, ?, ?, ?)";
    }

    QSqlQuery query(db);
    query.prepare(sql);

    QVariantList sampleIds;
    QVariantList serialNos;
    QVariantList temperatures;
    QVariantList weight;
    QVariantList tgValues;
    QVariantList dtgValues;
    QVariantList sourceNames;

    for (const TgSmallData& data : tgSmallDataList) {
        sampleIds << data.getSampleId();
        serialNos << data.getSerialNo();
        temperatures << data.getTemperature();
        weight << data.getWeight();
        tgValues << data.getTgValue();
        dtgValues << data.getDtgValue();
        sourceNames << data.getSourceName();
    }

    query.addBindValue(sampleIds);
    query.addBindValue(serialNos);
    query.addBindValue(temperatures);
    query.addBindValue(weight);
    query.addBindValue(tgValues);
    query.addBindValue(dtgValues);
    query.addBindValue(sourceNames);

    if (query.execBatch()) {
        DEBUG_LOG << "Batch insert TgSmallRawData successful, inserted" << tgSmallDataList.size() << "rows.";
        return true;
    }

    WARNING_LOG << "Batch insert TgSmallRawData failed:" << query.lastError().text();
    return false;
}

QList<TgSmallData> TgSmallRawDataDAO::getBySampleId(int sampleId)
{
    QSqlDatabase db = m_db.isValid() ? m_db : DatabaseConnector::getInstance().getDatabase();
    if (!db.isOpen()) {
        WARNING_LOG << "Database not open for TgSmallRawData query operation.";
        return QList<TgSmallData>();
    }

    SqlConfigLoader& loader = SqlConfigLoader::getInstance();
    SqlConfigLoader::SqlOperation operation = loader.getSqlOperation("TgSmallRawDataDAO", "select_by_sample_id");
    QString sql = operation.sql;
    if (sql.isEmpty()) {
        WARNING_LOG << "未找到TgSmallRawDataDAO.select_by_sample_id的SQL配置，使用默认SQL";
        sql = "SELECT * FROM tg_small_raw_data WHERE sample_id = :sample_id ORDER BY id, temperature";
    }

    QSqlQuery query(db);
    query.prepare(sql);
    query.bindValue(":sample_id", sampleId);

    QList<TgSmallData> list;
    if (query.exec()) {
        while (query.next()) {
            list.append(createTgSmallDataFromQuery(query));
        }
    } else {
        WARNING_LOG << "Query TgSmallRawData failed:" << query.lastError().text();
    }

    return list;
}

bool TgSmallRawDataDAO::removeBySampleId(int sampleId)
{
    QSqlDatabase db = m_db.isValid() ? m_db : DatabaseConnector::getInstance().getDatabase();
    if (!db.isOpen()) {
        WARNING_LOG << "Database not open for TgSmallRawData delete operation.";
        return false;
    }

    SqlConfigLoader& loader = SqlConfigLoader::getInstance();
    SqlConfigLoader::SqlOperation operation = loader.getSqlOperation("TgSmallRawDataDAO", "delete_by_sample_id");
    QString sql = operation.sql;
    if (sql.isEmpty()) {
        WARNING_LOG << "未找到TgSmallRawDataDAO.delete_by_sample_id的SQL配置，使用默认SQL";
        sql = "DELETE FROM tg_small_raw_data WHERE sample_id = :sample_id";
    }

    QSqlQuery query(db);
    query.prepare(sql);
    query.bindValue(":sample_id", sampleId);

    if (query.exec()) {
        DEBUG_LOG << "Removed TgSmallRawData for sample_id" << sampleId << ", affected" << query.numRowsAffected() << "rows.";
        return query.numRowsAffected() > 0;
    }

    WARNING_LOG << "Delete TgSmallRawData failed:" << query.lastError().text();
    return false;
}
