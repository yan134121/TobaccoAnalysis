#include "TgSmallDataDAO.h"
#include "DatabaseConnector.h"
#include "core/sql/SqlConfigLoader.h"
#include "Logger.h"
#include <QSqlError>
#include <QDebug>
#include <QVariantList>
#include <QJsonDocument>
#include <QSqlQuery>
#include <QSqlRecord>

static bool hasImportAttrsColumn_tgsmall(QSqlDatabase& db)
{
    QSqlQuery q(db);
    bool found = false;
    if (q.exec("SELECT 1 FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() "
               "AND TABLE_NAME = 'tg_small_data' AND COLUMN_NAME = 'import_attributes'"))
        found = q.next();
    if (!found && q.exec("SHOW COLUMNS FROM `tg_small_data` LIKE 'import_attributes'"))
        found = q.next();
    DEBUG_LOG << "tg_small_data.import_attributes 列存在:" << found;
    return found;
}

TgSmallData TgSmallDataDAO::createTgSmallDataFromQuery(QSqlQuery& query) {
    TgSmallData t;
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

bool TgSmallDataDAO::insert(TgSmallData& tgSmallData) {
    QSqlDatabase db = m_db.isValid() ? m_db : DatabaseConnector::getInstance().getDatabase();
    if (!db.isOpen()) { WARNING_LOG << "Database not open for TgSmallData insert."; return false; }

    const bool withAttrs = hasImportAttrsColumn_tgsmall(db);
    QString sql;
    if (withAttrs) {
        sql = "INSERT INTO tg_small_data (sample_id, serial_no, temperature, weight, tg_value, dtg_value, source_filename, import_attributes) "
              "VALUES (:sample_id, :serial_no, :temperature, :weight, :tg_value, :dtg_value, :source_filename, :import_attributes)";
    } else {
        sql = SqlConfigLoader::getInstance().getSqlOperation("TgSmallDataDAO", "insert").sql;
        if (sql.isEmpty())
            sql = "INSERT INTO tg_small_data (sample_id, serial_no, temperature, weight, tg_value, dtg_value, source_filename) "
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
    if (withAttrs)
        query.bindValue(":import_attributes",
            QString::fromUtf8(QJsonDocument(tgSmallData.getImportAttributes()).toJson(QJsonDocument::Compact)));

    if (query.exec()) { tgSmallData.setId(query.lastInsertId().toInt()); return true; }
    WARNING_LOG << "Insert TgSmallData failed:" << query.lastError().text();
    return false;
}

bool TgSmallDataDAO::insertBatch(QList<TgSmallData>& tgSmallDataList) {
    if (tgSmallDataList.isEmpty()) { DEBUG_LOG << "TgSmallDataList is empty."; return true; }
    QSqlDatabase db = m_db.isValid() ? m_db : DatabaseConnector::getInstance().getDatabase();
    if (!db.isOpen()) { WARNING_LOG << "Database not open for TgSmallData batch insert."; return false; }

    const bool withAttrs = hasImportAttrsColumn_tgsmall(db);
    QString sql;
    if (withAttrs) {
        sql = "INSERT INTO tg_small_data (sample_id, serial_no, temperature, weight, tg_value, dtg_value, source_filename, import_attributes) "
              "VALUES (?, ?, ?, ?, ?, ?, ?, ?)";
    } else {
        sql = SqlConfigLoader::getInstance().getSqlOperation("TgSmallDataDAO", "insert_batch").sql;
        if (sql.isEmpty())
            sql = "INSERT INTO tg_small_data (sample_id, serial_no, temperature, weight, tg_value, dtg_value, source_filename) "
                  "VALUES (?, ?, ?, ?, ?, ?, ?)";
    }

    QSqlQuery query(db);
    query.prepare(sql);

    if (withAttrs) {
        for (const TgSmallData& data : tgSmallDataList) {
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
                WARNING_LOG << "Batch insert TgSmallData (sequential) failed:" << query.lastError().text();
                return false;
            }
        }
        DEBUG_LOG << "Batch insert TgSmallData successful (sequential with import_attributes), rows:" << tgSmallDataList.size();
        return true;
    }

    QVariantList sampleIds, serialNos, temperatures, weight, tgValues, dtgValues, sourceNames;
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
        DEBUG_LOG << "Batch insert TgSmallData successful, inserted" << tgSmallDataList.size() << "rows.";
        return true;
    }
    WARNING_LOG << "Batch insert TgSmallData failed:" << query.lastError().text();
    return false;
}

QList<TgSmallData> TgSmallDataDAO::getBySampleId(int sampleId) {
    QSqlDatabase db = m_db.isValid() ? m_db : DatabaseConnector::getInstance().getDatabase();
    if (!db.isOpen()) { WARNING_LOG << "Database not open for TgSmallData query."; return {}; }

    QString sql = SqlConfigLoader::getInstance().getSqlOperation("TgSmallDataDAO", "select_by_sample_id").sql;
    if (sql.isEmpty()) sql = "SELECT * FROM tg_small_data WHERE sample_id = :sample_id ORDER BY id, temperature";

    QSqlQuery query(db);
    query.prepare(sql);
    query.bindValue(":sample_id", sampleId);

    QList<TgSmallData> list;
    if (query.exec()) {
        while (query.next()) list.append(createTgSmallDataFromQuery(query));
    } else {
        WARNING_LOG << "Query TgSmallData failed:" << query.lastError().text();
    }
    return list;
}

bool TgSmallDataDAO::removeBySampleId(int sampleId) {
    QSqlDatabase db = m_db.isValid() ? m_db : DatabaseConnector::getInstance().getDatabase();
    if (!db.isOpen()) { WARNING_LOG << "Database not open for TgSmallData delete."; return false; }

    QString sql = SqlConfigLoader::getInstance().getSqlOperation("TgSmallDataDAO", "delete_by_sample_id").sql;
    if (sql.isEmpty()) sql = "DELETE FROM tg_small_data WHERE sample_id = :sample_id";

    QSqlQuery query(db);
    query.prepare(sql);
    query.bindValue(":sample_id", sampleId);

    if (query.exec()) {
        DEBUG_LOG << "Removed TgSmallData for sample_id" << sampleId << ", affected" << query.numRowsAffected() << "rows.";
        return query.numRowsAffected() > 0;
    }
    WARNING_LOG << "Delete TgSmallData failed:" << query.lastError().text();
    return false;
}
