#include "TgBigDataDAO.h"
#include "DatabaseConnector.h" // 引入 DatabaseConnector 获取数据库连接
#include "core/sql/SqlConfigLoader.h"
#include <QSqlError>
#include <QDebug>
#include <QVariantList> // 用于批量插入
#include "Logger.h"

// TgBigDataDAO::TgBigDataDAO(QObject *parent) : QObject(parent) {} // 如果继承QObject，需要实现构造

TgBigData TgBigDataDAO::createTgBigDataFromQuery(QSqlQuery& query) {
    TgBigData t;
    t.setId(query.value("id").toInt());
    // t.setSampleId(query.value("sample_id").toInt());
    // t.setReplicateNo(query.value("replicate_no").toInt());
    // t.setSeqNo(query.value("seq_no").toInt());
    // t.setTgValue(query.value("tg_value").toDouble());
    // t.setDtgValue(query.value("dtg_value").toDouble());
    // t.setSourceName(query.value("source_name").toString());
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

bool TgBigDataDAO::insert(TgBigData& tgBigData) {
    QSqlDatabase db = m_db.isValid() ? m_db : DatabaseConnector::getInstance().getDatabase();
    if (!db.isOpen()) {
        WARNING_LOG << "Database not open for TgBigData insert operation.";
        return false;
    }

    QSqlQuery query(db);
    
    // 从SqlConfigLoader获取SQL语句
    SqlConfigLoader& loader = SqlConfigLoader::getInstance();
    SqlConfigLoader::SqlOperation operation = loader.getSqlOperation("TgBigDataDAO", "insert");
    
    QString sql;
    if (!operation.sql.isEmpty()) {
        sql = operation.sql;
    } else {
        WARNING_LOG << "未找到TgBigDataDAO.insert的SQL配置，使用默认SQL";
        sql = "INSERT INTO tg_big_data (sample_id, serial_no, temperature, weight, tg_value, dtg_value, source_filename) "
              "VALUES (:sample_id, :serial_no, :temperature, :weight, :tg_value, :dtg_value, :source_filename)";
    }
    
    query.prepare(sql);
    query.bindValue(":sample_id", tgBigData.getSampleId());
    query.bindValue(":serial_no", tgBigData.getSerialNo());
    query.bindValue(":temperature", tgBigData.getTemperature());
    query.bindValue(":weight", tgBigData.getWeight());
    query.bindValue(":tg_value", tgBigData.getTgValue());
    query.bindValue(":dtg_value", tgBigData.getDtgValue());
    query.bindValue(":source_filename", tgBigData.getSourceName());

    if (query.exec()) {
        tgBigData.setId(query.lastInsertId().toInt());
        return true;
    } else {
        WARNING_LOG << "Insert TgBigData failed:" << query.lastError().text();
        return false;
    }
}

bool TgBigDataDAO::insertBatch(QList<TgBigData>& tgBigDataList) {
    if (tgBigDataList.isEmpty()) {
        DEBUG_LOG << "TgBigDataList is empty, no batch insert performed.";
        return true; // 视为空列表插入为成功
    }

    QSqlDatabase db = m_db.isValid() ? m_db : DatabaseConnector::getInstance().getDatabase();
    if (!db.isOpen()) {
        WARNING_LOG << "Database not open for TgBigData batch insert operation.";
        return false;
    }

    // 从SqlConfigLoader获取SQL语句
    SqlConfigLoader& loader = SqlConfigLoader::getInstance();
    SqlConfigLoader::SqlOperation operation = loader.getSqlOperation("TgBigDataDAO", "insert_batch");
    
    QString sql;
    if (!operation.sql.isEmpty()) {
        sql = operation.sql;
    } else {
        WARNING_LOG << "未找到TgBigDataDAO.insert_batch的SQL配置，使用默认SQL";
        sql = "INSERT INTO tg_big_data (sample_id, serial_no, temperature, weight, tg_value, dtg_value, source_filename) "
              "VALUES (?, ?, ?, ?, ?, ?, ?)"; // 使用 ? 占位符进行批处理
    }
    
    QSqlQuery query(db);
    query.prepare(sql);

    // 绑定所有值，执行批处理
    QVariantList sampleIds;
    QVariantList serialNos;
    QVariantList temperatures;
    QVariantList weights;
    QVariantList tgValues;
    QVariantList dtgValues;
    QVariantList sourceNames;

    for (const TgBigData& data : tgBigDataList) {
        sampleIds << data.getSampleId();
        serialNos << data.getSerialNo();
        temperatures << data.getTemperature();
        weights << data.getWeight();
        tgValues << data.getTgValue();
        dtgValues << data.getDtgValue();
        sourceNames << data.getSourceName();
    }

    query.addBindValue(sampleIds);
    query.addBindValue(serialNos);
    query.addBindValue(temperatures);
    query.addBindValue(weights);
    query.addBindValue(tgValues);
    query.addBindValue(dtgValues);
    query.addBindValue(sourceNames);

    if (query.execBatch()) {
        DEBUG_LOG << "Batch insert TgBigData successful, inserted" << tgBigDataList.size() << "rows.";
        return true;
    } else {
        WARNING_LOG << "Batch insert TgBigData failed:" << query.lastError().text();
        return false;
    }
}

QList<TgBigData> TgBigDataDAO::getBySampleId(int sampleId) {
    QSqlDatabase db = m_db.isValid() ? m_db : DatabaseConnector::getInstance().getDatabase();
    if (!db.isOpen()) {
        WARNING_LOG << "Database not open for TgBigData getBySampleId operation.";
        return QList<TgBigData>();
    }

    // 从SqlConfigLoader获取SQL语句
    SqlConfigLoader& loader = SqlConfigLoader::getInstance();
    SqlConfigLoader::SqlOperation operation = loader.getSqlOperation("TgBigDataDAO", "select_by_sample_id");
    
    QString sql;
    if (!operation.sql.isEmpty()) {
        sql = operation.sql;
    } else {
        WARNING_LOG << "未找到TgBigDataDAO.select_by_sample_id的SQL配置，使用默认SQL";
        sql = "SELECT * FROM tg_big_data WHERE sample_id = :sample_id ORDER BY id, serial_no";
    }
    
    QSqlQuery query(db);
    query.prepare(sql);
    query.bindValue(":sample_id", sampleId);

    QList<TgBigData> list;
    if (query.exec()) {
        while (query.next()) {
            list.append(createTgBigDataFromQuery(query));
        }
    } else {
        WARNING_LOG << "Get TgBigData by sampleId failed:" << query.lastError().text();
    }
    return list;
}

bool TgBigDataDAO::removeBySampleId(int sampleId) {
    QSqlDatabase db = m_db.isValid() ? m_db : DatabaseConnector::getInstance().getDatabase();
    if (!db.isOpen()) {
        WARNING_LOG << "Database not open for TgBigData removeBySampleId operation.";
        return false;
    }

    // 从SqlConfigLoader获取SQL语句
    SqlConfigLoader& loader = SqlConfigLoader::getInstance();
    SqlConfigLoader::SqlOperation operation = loader.getSqlOperation("TgBigDataDAO", "delete_by_sample_id");
    
    QString sql;
    if (!operation.sql.isEmpty()) {
        sql = operation.sql;
    } else {
        WARNING_LOG << "未找到TgBigDataDAO.delete_by_sample_id的SQL配置，使用默认SQL";
        sql = "DELETE FROM tg_big_data WHERE sample_id = :sample_id";
    }
    
    QSqlQuery query(db);
    query.prepare(sql);
    query.bindValue(":sample_id", sampleId);

    if (query.exec()) {
        DEBUG_LOG << "Removed TgBigData for sample_id" << sampleId << ", affected" << query.numRowsAffected() << "rows.";
        return query.numRowsAffected() > 0;
    } else {
        WARNING_LOG << "Remove TgBigData by sampleId failed:" << query.lastError().text();
        return false;
    }
}
