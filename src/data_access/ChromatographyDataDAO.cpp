#include "ChromatographyDataDAO.h"
#include "DatabaseConnector.h"
#include "core/sql/SqlConfigLoader.h"
#include <QSqlError>
#include <QDebug>
#include <QVariantList>
#include <QJsonDocument>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QElapsedTimer>
#include "Logger.h"

static bool hasImportAttrsColumn_chrom(QSqlDatabase& db)
{
    QSqlQuery q(db);
    bool found = false;
    if (q.exec("SELECT 1 FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() "
               "AND TABLE_NAME = 'chromatography_data' AND COLUMN_NAME = 'import_attributes'"))
        found = q.next();
    if (!found && q.exec("SHOW COLUMNS FROM `chromatography_data` LIKE 'import_attributes'"))
        found = q.next();
    DEBUG_LOG << "chromatography_data.import_attributes 列存在:" << found;
    return found;
}

ChromatographyData ChromatographyDataDAO::createChromatographyDataFromQuery(QSqlQuery& query) {
    ChromatographyData t;
    t.setId(query.value("id").toInt());
    t.setSampleId(query.value("sample_id").toInt());
    t.setRetentionTime(query.value("retention_time").toDouble());
    t.setResponseValue(query.value("response_value").toDouble());
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

bool ChromatographyDataDAO::insert(ChromatographyData& chromatographyData) {
    QSqlDatabase db = m_db.isValid() ? m_db : DatabaseConnector::getInstance().getDatabase();
    if (!db.isOpen()) { WARNING_LOG << "Database not open for ChromatographyData insert."; return false; }

    const bool withAttrs = hasImportAttrsColumn_chrom(db);
    QString sql;
    if (withAttrs) {
        sql = "INSERT INTO chromatography_data (sample_id, retention_time, response_value, source_filename, import_attributes) "
              "VALUES (:sample_id, :retention_time, :response_value, :source_filename, :import_attributes)";
    } else {
        sql = SqlConfigLoader::getInstance().getSqlOperation("ChromatographyDataDAO", "insert").sql;
        if (sql.isEmpty())
            sql = "INSERT INTO chromatography_data (sample_id, retention_time, response_value, source_filename) "
                  "VALUES (:sample_id, :retention_time, :response_value, :source_filename)";
    }

    QSqlQuery query(db);
    query.prepare(sql);
    query.bindValue(":sample_id", chromatographyData.getSampleId());
    query.bindValue(":retention_time", chromatographyData.getRetentionTime());
    query.bindValue(":response_value", chromatographyData.getResponseValue());
    query.bindValue(":source_filename", chromatographyData.getSourceName());
    if (withAttrs)
        query.bindValue(":import_attributes",
            QString::fromUtf8(QJsonDocument(chromatographyData.getImportAttributes()).toJson(QJsonDocument::Compact)));

    if (query.exec()) { chromatographyData.setId(query.lastInsertId().toInt()); return true; }
    WARNING_LOG << "Insert ChromatographyData failed:" << query.lastError().text();
    return false;
}

bool ChromatographyDataDAO::insertBatch(QList<ChromatographyData>& chromatographyDataList) {
    if (chromatographyDataList.isEmpty()) { DEBUG_LOG << "ChromatographyDataList is empty."; return true; }
    QSqlDatabase db = m_db.isValid() ? m_db : DatabaseConnector::getInstance().getDatabase();
    if (!db.isOpen()) { return false; }

    const bool withAttrs = hasImportAttrsColumn_chrom(db);
    QString sql;
    if (withAttrs) {
        sql = "INSERT INTO chromatography_data (sample_id, retention_time, response_value, source_filename, import_attributes) "
              "VALUES (?, ?, ?, ?, ?)";
    } else {
        sql = SqlConfigLoader::getInstance().getSqlOperation("ChromatographyDataDAO", "insert_batch").sql;
        if (sql.isEmpty())
            sql = "INSERT INTO chromatography_data (sample_id, retention_time, response_value, source_filename) "
                  "VALUES (?, ?, ?, ?)";
    }

    QSqlQuery query(db);
    query.prepare(sql);

    if (withAttrs) {
        for (const ChromatographyData& data : chromatographyDataList) {
            query.bindValue(0, data.getSampleId());
            query.bindValue(1, data.getRetentionTime());
            query.bindValue(2, data.getResponseValue());
            query.bindValue(3, data.getSourceName());
            query.bindValue(4, QString::fromUtf8(
                QJsonDocument(data.getImportAttributes()).toJson(QJsonDocument::Compact)));
            if (!query.exec()) {
                WARNING_LOG << "Batch insert ChromatographyData (sequential) failed:" << query.lastError().text();
                return false;
            }
        }
        DEBUG_LOG << "Batch insert ChromatographyData successful (sequential with import_attributes), rows:" << chromatographyDataList.size();
        return true;
    }

    QVariantList sampleIds, retentionTimes, responseValues, sourceNames;
    for (const ChromatographyData& data : chromatographyDataList) {
        sampleIds << data.getSampleId();
        retentionTimes << data.getRetentionTime();
        responseValues << data.getResponseValue();
        sourceNames << data.getSourceName();
    }

    query.addBindValue(sampleIds);
    query.addBindValue(retentionTimes);
    query.addBindValue(responseValues);
    query.addBindValue(sourceNames);

    if (query.execBatch()) {
        DEBUG_LOG << "Batch insert ChromatographyData successful, inserted" << chromatographyDataList.size() << "rows.";
        return true;
    }
    WARNING_LOG << "Batch insert ChromatographyData failed:" << query.lastError().text();
    return false;
}

QList<ChromatographyData> ChromatographyDataDAO::getBySampleId(int sampleId) {
    QElapsedTimer timer;
    timer.restart();

    QSqlDatabase db = m_db.isValid() ? m_db : DatabaseConnector::getInstance().getDatabase();
    if (!db.isOpen()) { return {}; }

    QString sql = SqlConfigLoader::getInstance().getSqlOperation("ChromatographyDataDAO", "select_by_sample_id").sql;
    if (sql.isEmpty()) sql = "SELECT * FROM chromatography_data WHERE sample_id = :sample_id ORDER BY id, retention_time";

    QSqlQuery query(db);
    query.prepare(sql);
    query.bindValue(":sample_id", sampleId);

    QList<ChromatographyData> list;
    if (query.exec()) {
        while (query.next()) list.append(createChromatographyDataFromQuery(query));
    }

    DEBUG_LOG << "Query took:" << timer.elapsed() << "ms";
    return list;
}

bool ChromatographyDataDAO::removeBySampleId(int sampleId) {
    QSqlDatabase db = m_db.isValid() ? m_db : DatabaseConnector::getInstance().getDatabase();
    if (!db.isOpen()) { return false; }

    QString sql = SqlConfigLoader::getInstance().getSqlOperation("ChromatographyDataDAO", "delete_by_sample_id").sql;
    if (sql.isEmpty()) sql = "DELETE FROM chromatography_data WHERE sample_id = :sample_id";

    QSqlQuery query(db);
    query.prepare(sql);
    query.bindValue(":sample_id", sampleId);

    if (query.exec()) {
        DEBUG_LOG << "Removed ChromatographyData for sample_id" << sampleId << ", affected" << query.numRowsAffected() << "rows.";
        return query.numRowsAffected() > 0;
    }
    return false;
}
