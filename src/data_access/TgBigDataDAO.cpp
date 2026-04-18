#include "TgBigDataDAO.h"
#include "DatabaseConnector.h" // 引入 DatabaseConnector 获取数据库连接
#include "core/sql/SqlConfigLoader.h"
#include <QSqlError>
#include <QDebug>
#include <QVariantList> // 用于批量插入
#include <QJsonDocument>
#include <QSqlRecord>
#include <QSqlQuery>
#include "Logger.h"

// 检测 tg_big_data 表是否包含 import_attributes 列（每次插入前检测，避免迁移后仍沿用旧缓存）
static bool hasImportAttributesColumn(QSqlDatabase& db)
{
    QSqlQuery q(db);
    bool found = false;
    if (q.exec("SELECT 1 FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() "
               "AND TABLE_NAME = 'tg_big_data' AND COLUMN_NAME = 'import_attributes'")) {
        found = q.next();
    }
    if (!found && q.exec("SHOW COLUMNS FROM `tg_big_data` LIKE 'import_attributes'")) {
        found = q.next();
    }
    DEBUG_LOG << "tg_big_data.import_attributes 列存在:" << found;
    return found;
}

TgBigData TgBigDataDAO::createTgBigDataFromQuery(QSqlQuery& query) {
    TgBigData t;
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
            if (!attrStr.isEmpty()) {
                t.setImportAttributes(QJsonDocument::fromJson(attrStr.toUtf8()).object());
            }
        }
    }
    if (query.record().indexOf(QStringLiteral("created_at")) >= 0) {
        t.setCreatedAt(query.value(QStringLiteral("created_at")).toDateTime());
    }
    return t;
}

bool TgBigDataDAO::insert(TgBigData& tgBigData) {
    QSqlDatabase db = m_db.isValid() ? m_db : DatabaseConnector::getInstance().getDatabase();
    if (!db.isOpen()) {
        WARNING_LOG << "Database not open for TgBigData insert operation.";
        return false;
    }

    QSqlQuery query(db);

    const bool withAttrs = hasImportAttributesColumn(db);
    QString sql;
    if (withAttrs) {
        sql = "INSERT INTO tg_big_data (sample_id, serial_no, temperature, weight, tg_value, dtg_value, source_filename, import_attributes) "
              "VALUES (:sample_id, :serial_no, :temperature, :weight, :tg_value, :dtg_value, :source_filename, :import_attributes)";
    } else {
        SqlConfigLoader& loader = SqlConfigLoader::getInstance();
        sql = loader.getSqlOperation("TgBigDataDAO", "insert").sql;
        if (sql.isEmpty()) {
            sql = "INSERT INTO tg_big_data (sample_id, serial_no, temperature, weight, tg_value, dtg_value, source_filename) "
                  "VALUES (:sample_id, :serial_no, :temperature, :weight, :tg_value, :dtg_value, :source_filename)";
        }
    }

    query.prepare(sql);
    query.bindValue(":sample_id", tgBigData.getSampleId());
    query.bindValue(":serial_no", tgBigData.getSerialNo());
    query.bindValue(":temperature", tgBigData.getTemperature());
    query.bindValue(":weight", tgBigData.getWeight());
    query.bindValue(":tg_value", tgBigData.getTgValue());
    query.bindValue(":dtg_value", tgBigData.getDtgValue());
    query.bindValue(":source_filename", tgBigData.getSourceName());
    if (withAttrs) {
        query.bindValue(":import_attributes",
            QString::fromUtf8(QJsonDocument(tgBigData.getImportAttributes()).toJson(QJsonDocument::Compact)));
    }

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
        return true;
    }

    QSqlDatabase db = m_db.isValid() ? m_db : DatabaseConnector::getInstance().getDatabase();
    if (!db.isOpen()) {
        WARNING_LOG << "Database not open for TgBigData batch insert operation.";
        return false;
    }

    const bool withAttrs = hasImportAttributesColumn(db);

    QString sql;
    if (withAttrs) {
        sql = "INSERT INTO tg_big_data (sample_id, serial_no, temperature, weight, tg_value, dtg_value, source_filename, import_attributes) "
              "VALUES (?, ?, ?, ?, ?, ?, ?, ?)";
    } else {
        SqlConfigLoader& loader = SqlConfigLoader::getInstance();
        sql = loader.getSqlOperation("TgBigDataDAO", "insert_batch").sql;
        if (sql.isEmpty()) {
            sql = "INSERT INTO tg_big_data (sample_id, serial_no, temperature, weight, tg_value, dtg_value, source_filename) "
                  "VALUES (?, ?, ?, ?, ?, ?, ?)";
        }
    }

    QSqlQuery query(db);
    query.prepare(sql);

    // QMYSQL 的 execBatch 对 JSON 等类型绑定不可靠，可能导致 import_attributes 未写入；有列时改为逐行插入
    // 不在此函数内开启新事务：导入 Worker 等调用方可能已 transaction()，避免嵌套事务；
    // 无外层事务时每条 INSERT 自动提交。
    if (withAttrs) {
        for (const TgBigData& data : tgBigDataList) {
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
                WARNING_LOG << "Batch insert TgBigData (sequential) failed:" << query.lastError().text();
                return false;
            }
        }
        DEBUG_LOG << "Batch insert TgBigData successful (sequential with import_attributes), rows:" << tgBigDataList.size();
        return true;
    }

    QVariantList sampleIds, serialNos, temperatures, weights, tgValues, dtgValues, sourceNames;
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
    }
    WARNING_LOG << "Batch insert TgBigData failed:" << query.lastError().text();
    return false;
}

QList<TgBigData> TgBigDataDAO::getBySampleId(int sampleId) {
    QSqlDatabase db = m_db.isValid() ? m_db : DatabaseConnector::getInstance().getDatabase();
    if (!db.isOpen()) {
        WARNING_LOG << "Database not open for TgBigData getBySampleId operation.";
        return QList<TgBigData>();
    }

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
