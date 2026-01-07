#include "ProcessTgBigDataDAO.h"
#include "DatabaseConnector.h" // 引入 DatabaseConnector 获取数据库连接
#include "core/sql/SqlConfigLoader.h"
#include <QSqlError>
#include <QDebug>
#include <QVariantList> // 用于批量插入
#include "Logger.h"

ProcessTgBigData ProcessTgBigDataDAO::createProcessTgBigDataFromQuery(QSqlQuery& query) {
    ProcessTgBigData t;
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

bool ProcessTgBigDataDAO::insert(ProcessTgBigData& processTgBigData) {
    QSqlDatabase db = m_db.isValid() ? m_db : DatabaseConnector::getInstance().getDatabase();
    if (!db.isOpen()) {
        WARNING_LOG << "Database not open for ProcessTgBigData insert operation.";
        return false;
    }

    QSqlQuery query(db);
    
    // 从SqlConfigLoader获取SQL语句
    SqlConfigLoader& loader = SqlConfigLoader::getInstance();
    SqlConfigLoader::SqlOperation operation = loader.getSqlOperation("ProcessTgBigDataDAO", "insert");
    
    QString sql;
    if (!operation.sql.isEmpty()) {
        sql = operation.sql;
    } else {
        WARNING_LOG << "未找到ProcessTgBigDataDAO.insert的SQL配置，使用默认SQL";
        sql = "INSERT INTO process_tg_big_data (sample_id, serial_no, temperature, weight, tg_value, dtg_value, source_filename) "
              "VALUES (:sample_id, :serial_no, :temperature, :weight, :tg_value, :dtg_value, :source_filename)";
    }
    
    query.prepare(sql);
    query.bindValue(":sample_id", processTgBigData.getSampleId());
    query.bindValue(":serial_no", processTgBigData.getSerialNo());
    query.bindValue(":temperature", processTgBigData.getTemperature());
    query.bindValue(":weight", processTgBigData.getWeight());
    query.bindValue(":tg_value", processTgBigData.getTgValue());
    query.bindValue(":dtg_value", processTgBigData.getDtgValue());
    query.bindValue(":source_filename", processTgBigData.getSourceName());

    if (query.exec()) {
        processTgBigData.setId(query.lastInsertId().toInt());
        return true;
    } else {
        WARNING_LOG << "Insert ProcessTgBigData failed:" << query.lastError().text();
        return false;
    }
}

bool ProcessTgBigDataDAO::insertBatch(QList<ProcessTgBigData>& processTgBigDataList) {
    if (processTgBigDataList.isEmpty()) {
        DEBUG_LOG << "ProcessTgBigDataList is empty, no batch insert performed.";
        return true; // 视为空列表插入为成功
    }

    QSqlDatabase db = m_db.isValid() ? m_db : DatabaseConnector::getInstance().getDatabase();
    if (!db.isOpen()) {
        WARNING_LOG << "Database not open for ProcessTgBigData batch insert operation.";
        return false;
    }

    // 从SqlConfigLoader获取SQL语句
    SqlConfigLoader& loader = SqlConfigLoader::getInstance();
    SqlConfigLoader::SqlOperation operation = loader.getSqlOperation("ProcessTgBigDataDAO", "insert_batch");
    
    QString sql;
    if (!operation.sql.isEmpty()) {
        sql = operation.sql;
    } else {
        WARNING_LOG << "未找到ProcessTgBigDataDAO.insert_batch的SQL配置，使用默认SQL";
        sql = "INSERT INTO process_tg_big_data (sample_id, serial_no, temperature, weight, tg_value, dtg_value, source_name) "
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

    for (const ProcessTgBigData& data : processTgBigDataList) {
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

    bool success = db.transaction(); // 开始事务
    if (!success) {
        WARNING_LOG << "Failed to start transaction for batch insert:" << db.lastError().text();
        return false;
    }

    success = query.execBatch(); // 执行批处理
    if (!success) {
        WARNING_LOG << "Batch insert failed:" << query.lastError().text();
        db.rollback();
        return false;
    }

    success = db.commit(); // 提交事务
    if (!success) {
        WARNING_LOG << "Failed to commit transaction for batch insert:" << db.lastError().text();
        db.rollback();
        return false;
    }

    return true;
}

QList<ProcessTgBigData> ProcessTgBigDataDAO::getBySampleId(int sampleId) {
    QList<ProcessTgBigData> result;
    
    QSqlDatabase db = m_db.isValid() ? m_db : DatabaseConnector::getInstance().getDatabase();
    if (!db.isOpen()) {
        WARNING_LOG << "Database not open for ProcessTgBigData query operation.";
        return result;
    }

    QSqlQuery query(db);
    
    // 从SqlConfigLoader获取SQL语句
    SqlConfigLoader& loader = SqlConfigLoader::getInstance();
    SqlConfigLoader::SqlOperation operation = loader.getSqlOperation("ProcessTgBigDataDAO", "get_by_sample_id");
    
    QString sql;
    if (!operation.sql.isEmpty()) {
        sql = operation.sql;
    } else {
        WARNING_LOG << "未找到ProcessTgBigDataDAO.get_by_sample_id的SQL配置，使用默认SQL";
        sql = "SELECT * FROM process_tg_big_data WHERE sample_id = :sample_id ORDER BY serial_no";
    }
    
    query.prepare(sql);
    query.bindValue(":sample_id", sampleId);

    if (query.exec()) {
        while (query.next()) {
            result.append(createProcessTgBigDataFromQuery(query));
        }
    } else {
        WARNING_LOG << "Query ProcessTgBigData by sample_id failed:" << query.lastError().text();
    }

    return result;
}

bool ProcessTgBigDataDAO::removeBySampleId(int sampleId) {
    QSqlDatabase db = m_db.isValid() ? m_db : DatabaseConnector::getInstance().getDatabase();
    if (!db.isOpen()) {
        WARNING_LOG << "Database not open for ProcessTgBigData remove operation.";
        return false;
    }

    QSqlQuery query(db);
    
    // 从SqlConfigLoader获取SQL语句
    SqlConfigLoader& loader = SqlConfigLoader::getInstance();
    SqlConfigLoader::SqlOperation operation = loader.getSqlOperation("ProcessTgBigDataDAO", "remove_by_sample_id");
    
    QString sql;
    if (!operation.sql.isEmpty()) {
        sql = operation.sql;
    } else {
        WARNING_LOG << "未找到ProcessTgBigDataDAO.remove_by_sample_id的SQL配置，使用默认SQL";
        sql = "DELETE FROM process_tg_big_data WHERE sample_id = :sample_id";
    }
    
    query.prepare(sql);
    query.bindValue(":sample_id", sampleId);

    if (query.exec()) {
        return true;
    } else {
        WARNING_LOG << "Remove ProcessTgBigData by sample_id failed:" << query.lastError().text();
        return false;
    }
}
