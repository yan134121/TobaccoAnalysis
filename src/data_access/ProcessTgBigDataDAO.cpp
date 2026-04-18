#include "ProcessTgBigDataDAO.h"
#include "DatabaseConnector.h"
#include "core/sql/SqlConfigLoader.h"
#include <QSqlError>
#include <QDebug>
#include <QVariantList>
#include <QJsonDocument>
#include <QSqlQuery>
#include <QSqlRecord>
#include "Logger.h"

static bool hasImportAttrsColumn_proctgbig(QSqlDatabase& db)
{
    QSqlQuery q(db);
    bool found = false;
    if (q.exec("SELECT 1 FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() "
               "AND TABLE_NAME = 'process_tg_big_data' AND COLUMN_NAME = 'import_attributes'"))
        found = q.next();
    if (!found && q.exec("SHOW COLUMNS FROM `process_tg_big_data` LIKE 'import_attributes'"))
        found = q.next();
    DEBUG_LOG << "process_tg_big_data.import_attributes 列存在:" << found;
    return found;
}

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
    if (query.record().indexOf(QStringLiteral("import_attributes")) >= 0) {
        const QVariant attrVar = query.value(QStringLiteral("import_attributes"));
        if (!attrVar.isNull()) {
            const QString attrStr = attrVar.toString();
            if (!attrStr.isEmpty())
                t.setImportAttributes(QJsonDocument::fromJson(attrStr.toUtf8()).object());
        }
    }
    if (query.record().indexOf(QStringLiteral("created_at")) >= 0) {
        t.setCreatedAt(query.value(QStringLiteral("created_at")).toDateTime());
    }
    return t;
}

bool ProcessTgBigDataDAO::insert(ProcessTgBigData& processTgBigData) {
    QSqlDatabase db = m_db.isValid() ? m_db : DatabaseConnector::getInstance().getDatabase();
    if (!db.isOpen()) { WARNING_LOG << "Database not open for ProcessTgBigData insert."; return false; }

    const bool withAttrs = hasImportAttrsColumn_proctgbig(db);
    QString sql;
    if (withAttrs) {
        sql = "INSERT INTO process_tg_big_data (sample_id, serial_no, temperature, weight, tg_value, dtg_value, source_filename, import_attributes) "
              "VALUES (:sample_id, :serial_no, :temperature, :weight, :tg_value, :dtg_value, :source_filename, :import_attributes)";
    } else {
        sql = SqlConfigLoader::getInstance().getSqlOperation("ProcessTgBigDataDAO", "insert").sql;
        if (sql.isEmpty())
            sql = "INSERT INTO process_tg_big_data (sample_id, serial_no, temperature, weight, tg_value, dtg_value, source_filename) "
                  "VALUES (:sample_id, :serial_no, :temperature, :weight, :tg_value, :dtg_value, :source_filename)";
    }

    QSqlQuery query(db);
    query.prepare(sql);
    query.bindValue(":sample_id", processTgBigData.getSampleId());
    query.bindValue(":serial_no", processTgBigData.getSerialNo());
    query.bindValue(":temperature", processTgBigData.getTemperature());
    query.bindValue(":weight", processTgBigData.getWeight());
    query.bindValue(":tg_value", processTgBigData.getTgValue());
    query.bindValue(":dtg_value", processTgBigData.getDtgValue());
    query.bindValue(":source_filename", processTgBigData.getSourceName());
    if (withAttrs)
        query.bindValue(":import_attributes",
            QString::fromUtf8(QJsonDocument(processTgBigData.getImportAttributes()).toJson(QJsonDocument::Compact)));

    if (query.exec()) { processTgBigData.setId(query.lastInsertId().toInt()); return true; }
    WARNING_LOG << "Insert ProcessTgBigData failed:" << query.lastError().text();
    return false;
}

bool ProcessTgBigDataDAO::insertBatch(QList<ProcessTgBigData>& processTgBigDataList) {
    if (processTgBigDataList.isEmpty()) { DEBUG_LOG << "ProcessTgBigDataList is empty."; return true; }

    QSqlDatabase db = m_db.isValid() ? m_db : DatabaseConnector::getInstance().getDatabase();
    if (!db.isOpen()) { WARNING_LOG << "Database not open for ProcessTgBigData batch insert."; return false; }

    const bool withAttrs = hasImportAttrsColumn_proctgbig(db);
    QString sql;
    if (withAttrs) {
        sql = "INSERT INTO process_tg_big_data (sample_id, serial_no, temperature, weight, tg_value, dtg_value, source_filename, import_attributes) "
              "VALUES (?, ?, ?, ?, ?, ?, ?, ?)";
    } else {
        sql = SqlConfigLoader::getInstance().getSqlOperation("ProcessTgBigDataDAO", "insert_batch").sql;
        if (sql.isEmpty())
            sql = "INSERT INTO process_tg_big_data (sample_id, serial_no, temperature, weight, tg_value, dtg_value, source_filename) "
                  "VALUES (?, ?, ?, ?, ?, ?, ?)";
    }

    QSqlQuery query(db);
    query.prepare(sql);

    // 不在此开启事务：导入 Worker 等可能已 transaction()，避免嵌套；无外层时逐条自动提交。
    if (withAttrs) {
        for (const ProcessTgBigData& data : processTgBigDataList) {
            query.bindValue(0, data.getSampleId());
            query.bindValue(1, data.getSerialNo());
            query.bindValue(2, data.getTemperature());
            query.bindValue(3, data.getWeight());
            query.bindValue(4, data.getTgValue());
            query.bindValue(5, data.getDtgValue());
            query.bindValue(6, data.getSourceName());
            query.bindValue(7, QString::fromUtf8(
                QJsonDocument(data.getImportAttributes()).toJson(QJsonDocument::Compact)));
            if (!query.exec()) {
                WARNING_LOG << "Batch insert ProcessTgBigData (sequential) failed:" << query.lastError().text();
                return false;
            }
        }
        return true;
    }

    QVariantList sampleIds, serialNos, temperatures, weights, tgValues, dtgValues, sourceNames;
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
    if (!query.execBatch()) {
        WARNING_LOG << "Batch insert ProcessTgBigData failed:" << query.lastError().text();
        return false;
    }
    return true;
}

QList<ProcessTgBigData> ProcessTgBigDataDAO::getBySampleId(int sampleId) {
    QList<ProcessTgBigData> result;
    QSqlDatabase db = m_db.isValid() ? m_db : DatabaseConnector::getInstance().getDatabase();
    if (!db.isOpen()) { WARNING_LOG << "Database not open for ProcessTgBigData query."; return result; }

    QString sql = SqlConfigLoader::getInstance().getSqlOperation("ProcessTgBigDataDAO", "get_by_sample_id").sql;
    if (sql.isEmpty()) sql = "SELECT * FROM process_tg_big_data WHERE sample_id = :sample_id ORDER BY serial_no";

    QSqlQuery query(db);
    query.prepare(sql);
    query.bindValue(":sample_id", sampleId);

    if (query.exec()) {
        while (query.next()) result.append(createProcessTgBigDataFromQuery(query));
    } else {
        WARNING_LOG << "Query ProcessTgBigData by sample_id failed:" << query.lastError().text();
    }
    return result;
}

bool ProcessTgBigDataDAO::removeBySampleId(int sampleId) {
    QSqlDatabase db = m_db.isValid() ? m_db : DatabaseConnector::getInstance().getDatabase();
    if (!db.isOpen()) { WARNING_LOG << "Database not open for ProcessTgBigData remove."; return false; }

    QString sql = SqlConfigLoader::getInstance().getSqlOperation("ProcessTgBigDataDAO", "remove_by_sample_id").sql;
    if (sql.isEmpty()) sql = "DELETE FROM process_tg_big_data WHERE sample_id = :sample_id";

    QSqlQuery query(db);
    query.prepare(sql);
    query.bindValue(":sample_id", sampleId);

    if (query.exec()) return true;
    WARNING_LOG << "Remove ProcessTgBigData by sample_id failed:" << query.lastError().text();
    return false;
}
