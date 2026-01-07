

// 建议文件名: SingleTobaccoSampleDAO.cpp

#include "SingleTobaccoSampleDAO.h" // 修改: 包含对应的头文件
#include "DatabaseConnector.h"
#include "core/sql/SqlConfigLoader.h"
#include "Logger.h"
#include "common.h"
#include <QSqlQuery>
#include <QVariant>
#include <QSqlError>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QDate>

// 修改: 辅助函数名、返回类型更新
SingleTobaccoSampleData SingleTobaccoSampleDAO::createSampleFromQuery(QSqlQuery& query) {
    // 假设 SingleTobaccoSampleData 类已定义了所有必需的 set/get 方法
    SingleTobaccoSampleData sample; // 修改: 实体类型和变量名
    sample.setId(query.value("id").toInt());
    sample.setBatchId(query.value("batch_id").toInt());
    sample.setProjectName(query.value("project_name").toString());
    sample.setShortCode(query.value("short_code").toString());
    sample.setParallelNo(query.value("parallel_no").toInt());
    sample.setSampleName(query.value("sample_name").toString());
    sample.setNote(query.value("note").toString());
    sample.setYear(query.value("year").toInt());
    sample.setOrigin(query.value("origin").toString());
    sample.setPart(query.value("part").toString());
    sample.setGrade(query.value("grade").toString());
    sample.setType(query.value("type").toString());
    sample.setCollectDate(query.value("collect_date").toDate());
    sample.setDetectDate(query.value("detect_date").toDate());

    // 处理 JSON 数据
    QString jsonData = query.value("data_json").toString();
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(jsonData.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError && !jsonData.isEmpty()) {
        WARNING_LOG << "Error parsing JSON data for sample id" << sample.getId() << ":" << parseError.errorString();
    }
    sample.setDataJson(doc);

    sample.setCreatedAt(query.value("created_at").toDateTime());
    return sample;
}

// 修改: 函数签名 (类名、参数类型、参数名、返回类型) 更新
int SingleTobaccoSampleDAO::insert(SingleTobaccoSampleData& sample) {
    DEBUG_LOG << "Inserting SingleTobaccoSampleData:" << sample.getId();
    QSqlDatabase db = m_db.isValid() ? m_db : DatabaseConnector::getInstance().getDatabase();
    if (!db.isOpen()) {
        WARNING_LOG << "Database not open for insert operation.";
        return false;
    }

    QSqlQuery query(db);
    int modelId = -1;
    int batchId = -1;

    // 1️⃣ 检查 project_name 是否在 tobacco_model 中存在
    QString checkModelSql = SqlConfigLoader::getInstance().getSqlOperation("SingleTobaccoSampleDAO", "check_model_exists").sql;
    if (checkModelSql.isEmpty()) {
        checkModelSql = "SELECT id FROM tobacco_model WHERE model_code = :model_code";
        DEBUG_LOG << "Using default SQL for SingleTobaccoSampleDAO.check_model_exists";
    }
    
    query.prepare(checkModelSql);
    query.bindValue(":model_code", sample.getProjectName());
    if (!query.exec()) {
        WARNING_LOG << "Check tobacco_model failed:" << query.lastError().text();
        return false;
    }

    if (query.next()) {
        // 存在，获取 model_id
        modelId = query.value("id").toInt();
        DEBUG_LOG << "Found existing tobacco_model with ID:" << modelId;
    } else {
        // 不存在，则插入新的 model
        QSqlQuery insertModel(db);
        
        QString insertModelSql = SqlConfigLoader::getInstance().getSqlOperation("SingleTobaccoSampleDAO", "insert_model").sql;
        if (insertModelSql.isEmpty()) {
            insertModelSql = "INSERT INTO tobacco_model (model_code, model_name) VALUES (:code, :name)";
            DEBUG_LOG << "Using default SQL for SingleTobaccoSampleDAO.insert_model";
        }
        
        insertModel.prepare(insertModelSql);
        insertModel.bindValue(":code", sample.getProjectName());
        insertModel.bindValue(":name", sample.getProjectName());
        if (!insertModel.exec()) {
            WARNING_LOG << "Insert into tobacco_model failed:" << insertModel.lastError().text();
            return false;
        }
        modelId = insertModel.lastInsertId().toInt();
        DEBUG_LOG << "Inserted new tobacco_model with ID:" << modelId;
    }

    QString batchCode = sample.getBatchCode();
    if (batchCode.isEmpty()) {
        batchCode = QString("%1_DEFAULT").arg(sample.getProjectName());
        DEBUG_LOG << "Using default batch code:" << batchCode;
    } else {
        DEBUG_LOG << "Using user-specified batch code:" << batchCode;
    }

    QString checkBatchSql = SqlConfigLoader::getInstance().getSqlOperation("TobaccoBatchDAO", "check_batch_exists").sql;
    if (checkBatchSql.isEmpty()) {
        // checkBatchSql = "SELECT id FROM tobacco_batch WHERE model_id = :model_id AND batch_code = :batch_code";
        checkBatchSql =  "SELECT id FROM tobacco_batch WHERE model_id = :model_id AND batch_code = :batch_code AND batch_type = :batch_type",
        DEBUG_LOG << "Using default SQL for TobaccoBatchDAO.check_batch_exists";
    }
    
    query.prepare(checkBatchSql);
    query.bindValue(":model_id", modelId);
    query.bindValue(":batch_code", batchCode);
    QString batchTypeStr = (sample.getBatchType() == BatchType::PROCESS) ? "PROCESS" : "NORMAL";
    query.bindValue(":batch_type", batchTypeStr);
    if (!query.exec()) {
        WARNING_LOG << "Check tobacco_batch failed:" << query.lastError().text();
        return false;
    }

    if (query.next()) {
        batchId = query.value("id").toInt();
        DEBUG_LOG << "Found existing default tobacco_batch with ID:" << batchId;
    } else {
        QSqlQuery insertBatch(db);

        // ⚡ MODIFIED: 添加 batch_type 字段
        QString insertBatchSql = SqlConfigLoader::getInstance().getSqlOperation("TobaccoBatchDAO", "insert").sql;
        if (insertBatchSql.isEmpty()) {
            insertBatchSql = "INSERT INTO tobacco_batch "
                             "(model_id, batch_code, produce_date, description, batch_type) "
                             "VALUES (:model_id, :batch_code, :produce_date, :description, :batch_type)";
            DEBUG_LOG << "Using default SQL for TobaccoBatchDAO.insert_batch with batch_type"; // ⚡ MODIFIED
        }
        
        insertBatch.prepare(insertBatchSql);
        insertBatch.bindValue(":model_id", modelId);
        insertBatch.bindValue(":batch_code", batchCode);
        insertBatch.bindValue(":produce_date", QDate::currentDate());
        insertBatch.bindValue(":description", QString("Batch %1 for project %2").arg(batchCode, sample.getProjectName()));
        // insertBatch.bindValue(":batch_type", sample.isProcessData() ? "PROCESS" : "NORMAL"); // ⚡ MODIFIED
        // ⚡ MODIFIED: 根据 sample.getBbatchType() 设置
        insertBatch.bindValue(":batch_type", sample.getBatchType() == BatchType::PROCESS ? "PROCESS" : "NORMAL");

        if (!insertBatch.exec()) {
            WARNING_LOG << "Insert into tobacco_batch failed:" << insertBatch.lastError().text();
            return false;
        }
        batchId = insertBatch.lastInsertId().toInt();
        DEBUG_LOG << "Created new tobacco_batch with ID:" << batchId << "and batch_code:" << batchCode;
    }


    // 3️⃣ 应用层唯一性检查：确保 project_name-batch_code-short_code-parallel_no 组合唯一
    QString uniqueCheckSql = R"(
        SELECT COUNT(*) as count
        FROM single_tobacco_sample s
        JOIN tobacco_batch b ON s.batch_id = b.id
        WHERE s.project_name = :project_name 
          AND b.batch_code = :batch_code
          AND s.short_code = :short_code
          AND s.parallel_no = :parallel_no
    )";
    
    QSqlQuery uniqueQuery(db);
    uniqueQuery.prepare(uniqueCheckSql);
    uniqueQuery.bindValue(":project_name", sample.getProjectName());
    uniqueQuery.bindValue(":batch_code", batchCode);
    uniqueQuery.bindValue(":short_code", sample.getShortCode());
    uniqueQuery.bindValue(":parallel_no", sample.getParallelNo());
    
    if (!uniqueQuery.exec()) {
        WARNING_LOG << "Uniqueness check failed:" << uniqueQuery.lastError().text();
        return false;
    }
    
    if (uniqueQuery.next()) {
        int count = uniqueQuery.value("count").toInt();
        if (count > 0) {
            WARNING_LOG << "Duplicate sample detected: project_name=" << sample.getProjectName()
                      << ", batch_code=" << batchCode
                      << ", short_code=" << sample.getShortCode()
                      << ", parallel_no=" << sample.getParallelNo();
            return false;
        }
    }
    
    // 4️⃣ 设置正确的 batch_id 到 sample 对象
    sample.setBatchId(batchId);
    
    // 从SqlConfigLoader获取SQL语句，如果不存在则使用默认SQL
    QString sql = SqlConfigLoader::getInstance().getSqlOperation("SingleTobaccoSampleDAO", "insert").sql;
    if (sql.isEmpty()) {
        sql = "INSERT INTO single_tobacco_sample "
              "(batch_id, project_name, short_code, parallel_no, sample_name, "
              "note, year, origin, part, grade, type, collect_date, detect_date, data_json) "
              "VALUES (:batch_id, :project_name, :short_code, :parallel_no, :sample_name, "
              ":note, :year, :origin, :part, :grade, :type, :collect_date, :detect_date, :data_json)";
        DEBUG_LOG << "Using default SQL for SingleTobaccoSampleDAO.insert";
    }
    
    query.prepare(sql);

    // 修改: 使用新的参数名 sample
    query.bindValue(":batch_id", sample.getBatchId());
    query.bindValue(":project_name", sample.getProjectName());
    query.bindValue(":short_code", sample.getShortCode());
    query.bindValue(":parallel_no", sample.getParallelNo());
    query.bindValue(":sample_name", sample.getSampleName());
    query.bindValue(":note", sample.getNote());
    query.bindValue(":year", sample.getYear());
    query.bindValue(":origin", sample.getOrigin());
    query.bindValue(":part", sample.getPart());
    query.bindValue(":grade", sample.getGrade());
    query.bindValue(":type", sample.getType());
    query.bindValue(":collect_date", sample.getCollectDate());
    query.bindValue(":detect_date", sample.getDetectDate());
    // query.bindValue(":data_json", sample.getDataJson().toJson(QJsonDocument::Compact));
query.bindValue(":data_json", QString::fromUtf8(sample.getDataJson().toJson(QJsonDocument::Compact)));

    if (query.exec()) {
        int newId = query.lastInsertId().toInt();
        sample.setId(newId);
        DEBUG_LOG << "Inserted SingleTobaccoSampleData with ID:" << newId;
        return newId;
    } else {
        WARNING_LOG << "Insert SingleTobaccoSampleData failed:" << query.lastError().text();
        return -1;
    }
}

// 修改: 函数签名更新
bool SingleTobaccoSampleDAO::update(const SingleTobaccoSampleData& sample) {
    if (sample.getId() == -1) {
        WARNING_LOG << "Cannot update SingleTobaccoSampleData with invalid ID.";
        return false;
    }

    QSqlDatabase db = m_db.isValid() ? m_db : DatabaseConnector::getInstance().getDatabase();
    if (!db.isOpen()) {
        WARNING_LOG << "Database not open for update operation.";
        return false;
    }

    QSqlQuery query(db);
    // 从SqlConfigLoader获取SQL语句，如果不存在则使用默认SQL
    QString sql = SqlConfigLoader::getInstance().getSqlOperation("SingleTobaccoSampleDAO", "update").sql;
    if (sql.isEmpty()) {
        sql = "UPDATE single_tobacco_sample SET "
              "batch_id = :batch_id, project_name = :project_name, "
              "short_code = :short_code, parallel_no = :parallel_no, sample_name = :sample_name, "
              "note = :note, year = :year, origin = :origin, part = :part, grade = :grade, "
              "type = :type, collect_date = :collect_date, detect_date = :detect_date, data_json = :data_json "
              "WHERE id = :id";
        DEBUG_LOG << "Using default SQL for SingleTobaccoSampleDAO.update";
    }
    
    query.prepare(sql);

    // 修改: 使用新的参数名 sample
    query.bindValue(":batch_id", sample.getBatchId());
    query.bindValue(":project_name", sample.getProjectName());
    query.bindValue(":short_code", sample.getShortCode());
    query.bindValue(":parallel_no", sample.getParallelNo());
    query.bindValue(":sample_name", sample.getSampleName());
    query.bindValue(":note", sample.getNote());
    query.bindValue(":year", sample.getYear());
    query.bindValue(":origin", sample.getOrigin());
    query.bindValue(":part", sample.getPart());
    query.bindValue(":grade", sample.getGrade());
    query.bindValue(":type", sample.getType());
    query.bindValue(":collect_date", sample.getCollectDate());
    query.bindValue(":detect_date", sample.getDetectDate());
    query.bindValue(":data_json", sample.getDataJson().toJson(QJsonDocument::Compact));
    query.bindValue(":id", sample.getId());

    if (query.exec()) {
        DEBUG_LOG << "Updated SingleTobaccoSampleData with ID:" << sample.getId();
        return query.numRowsAffected() > 0;
    } else {
        WARNING_LOG << "Update SingleTobaccoSampleData failed:" << query.lastError().text();
        return false;
    }
}

// 修改: 函数签名 (仅类名) 更新
bool SingleTobaccoSampleDAO::remove(int id) {
    if (id == -1) {
        WARNING_LOG << "Cannot remove SingleTobaccoSampleData with invalid ID.";
        return false;
    }

    QSqlDatabase db = m_db.isValid() ? m_db : DatabaseConnector::getInstance().getDatabase();
    if (!db.isOpen()) {
        WARNING_LOG << "Database not open for remove operation.";
        return false;
    }

    QSqlQuery query(db);
    // 从SqlConfigLoader获取SQL语句，如果不存在则使用默认SQL
    QString sql = SqlConfigLoader::getInstance().getSqlOperation("SingleTobaccoSampleDAO", "remove").sql;
    if (sql.isEmpty()) {
        sql = "DELETE FROM single_tobacco_sample WHERE id = :id";
        DEBUG_LOG << "Using default SQL for SingleTobaccoSampleDAO.remove";
    }
    
    query.prepare(sql);
    query.bindValue(":id", id);

    if (query.exec()) {
        DEBUG_LOG << "Removed SingleTobaccoSampleData with ID:" << id;
        return query.numRowsAffected() > 0;
    } else {
        WARNING_LOG << "Remove SingleTobaccoSampleData failed:" << query.lastError().text();
        return false;
    }
}

// 修改: 函数签名 (类名, 返回类型) 更新
SingleTobaccoSampleData SingleTobaccoSampleDAO::getById(int id) {
    QSqlDatabase db = m_db.isValid() ? m_db : DatabaseConnector::getInstance().getDatabase();
    if (!db.isOpen()) {
        WARNING_LOG << "Database not open for getById operation.";
        return SingleTobaccoSampleData(); // 修改: 返回新的实体类型
    }

    QSqlQuery query(db);
    // 从SqlConfigLoader获取SQL语句，如果不存在则使用默认SQL
    QString sql = SqlConfigLoader::getInstance().getSqlOperation("SingleTobaccoSampleDAO", "getById").sql;
    if (sql.isEmpty()) {
        sql = "SELECT * FROM single_tobacco_sample WHERE id = :id";
        DEBUG_LOG << "Using default SQL for SingleTobaccoSampleDAO.getById";
    }
    
    query.prepare(sql);
    query.bindValue(":id", id);

    if (query.exec() && query.next()) {
        return createSampleFromQuery(query); // 修改: 调用新的辅助函数
    } else {
        WARNING_LOG << "Get SingleTobaccoSampleData by ID" << id << "failed or not found:" << query.lastError().text();
        return SingleTobaccoSampleData(); // 修改: 返回新的实体类型
    }
}

// 修改: 函数签名 (类名, 返回类型) 更新
QList<SingleTobaccoSampleData> SingleTobaccoSampleDAO::queryAll() {
    QSqlDatabase db = m_db.isValid() ? m_db : DatabaseConnector::getInstance().getDatabase();
    if (!db.isOpen()) {
        WARNING_LOG << "Database not open for queryAll operation.";
        return QList<SingleTobaccoSampleData>(); // 修改: 返回新的实体列表类型
    }

    // 从SqlConfigLoader获取SQL语句，如果不存在则使用默认SQL
    QString sql = SqlConfigLoader::getInstance().getSqlOperation("SingleTobaccoSampleDAO", "select_all").sql;
    if (sql.isEmpty()) {
        sql = "SELECT * FROM single_tobacco_sample";
        DEBUG_LOG << "Using default SQL for SingleTobaccoSampleDAO.queryAll";
    }
    
    // QSqlQuery query(db);
    // query.prepare(sql);
    QSqlQuery query(sql, db);
    QList<SingleTobaccoSampleData> list; // 修改: 列表类型更新

    if (query.exec()) {
        while (query.next()) {
            list.append(createSampleFromQuery(query)); // 修改: 调用新的辅助函数
        }
    } else {
        WARNING_LOG << "Query all SingleTobaccoSampleData failed:" << query.lastError().text();
    }
    return list;
}

// 修改: 函数签名 (类名, 返回类型) 更新
QList<SingleTobaccoSampleData> SingleTobaccoSampleDAO::query(const QMap<QString, QVariant>& conditions) {
    QSqlDatabase db = m_db.isValid() ? m_db : DatabaseConnector::getInstance().getDatabase();
    if (!db.isOpen()) {
        WARNING_LOG << "Database not open for query operation.";
        return QList<SingleTobaccoSampleData>(); // 修改: 返回新的实体列表类型
    }

    // 从SqlConfigLoader获取SQL语句，如果不存在则使用默认SQL
    QString queryString = SqlConfigLoader::getInstance().getSqlOperation("SingleTobaccoSampleDAO", "queryBase").sql;
    if (queryString.isEmpty()) {
        queryString = "SELECT * FROM single_tobacco_sample";
        DEBUG_LOG << "Using default SQL for SingleTobaccoSampleDAO.queryBase";
    }
    QStringList whereClauses;
    
    QMapIterator<QString, QVariant> i(conditions);
    while (i.hasNext()) {
        i.next();
        QString column = i.key();
        QVariant value = i.value();
        if (value.type() == QVariant::String && value.toString().contains('%')) {
            whereClauses << QString("%1 LIKE :%2").arg(column, column);
        } else {
            whereClauses << QString("%1 = :%2").arg(column, column);
        }
    }

    if (!whereClauses.isEmpty()) {
        queryString += " WHERE " + whereClauses.join(" AND ");
    }

    QSqlQuery query(db);
    query.prepare(queryString);

    QMapIterator<QString, QVariant> bindIt(conditions);
    while (bindIt.hasNext()) {
        bindIt.next();
        query.bindValue(":" + bindIt.key(), bindIt.value());
    }

    QList<SingleTobaccoSampleData> list; // 修改: 列表类型更新
    if (query.exec()) {
        while (query.next()) {
            list.append(createSampleFromQuery(query)); // 修改: 调用新的辅助函数
        }
    } else {
        WARNING_LOG << "Query SingleTobaccoSampleData with conditions failed:" << query.lastError().text();
    }
    return list;
}

// 根据短码查询样品
QList<SingleTobaccoSampleData> SingleTobaccoSampleDAO::queryByShortCode(const QString& shortCode) {
    QMap<QString, QVariant> conditions;
    conditions["short_code"] = shortCode;
    return query(conditions);
}

// 根据短码和并行号查询样品
QList<SingleTobaccoSampleData> SingleTobaccoSampleDAO::queryByShortCodeAndParallelNo(
    const QString& shortCode, int parallelNo)
{
    // 构造查询条件
    QMap<QString, QVariant> conditions;
    conditions["short_code"] = shortCode;
    conditions["parallel_no"] = parallelNo;

    // 调用通用 query() 方法
    return query(conditions);
}


// QList<SingleTobaccoSampleData> SingleTobaccoSampleDAO::queryByProjectNameAndBatchCodeAndShortCodeAndParallelNo(
//     const QString& projectName, const QString& batchCode, const QString& shortCode, int parallelNo)
//     {
//         // 构造查询条件
//         QMap<QString, QVariant> conditions;
//         conditions["project_name"] = projectName;
//         conditions["batch_code"] = batchCode;
//         conditions["short_code"] = shortCode;
//         conditions["parallel_no"] = parallelNo;

//         // 调用通用 query() 方法
//         return query(conditions);
//     }
QList<SingleTobaccoSampleData> SingleTobaccoSampleDAO::queryByProjectNameAndBatchCodeAndShortCodeAndParallelNo(
    const QString& projectName, const QString& batchCode, const QString& shortCode, int parallelNo)
{
    QSqlDatabase db = m_db.isValid() ? m_db : DatabaseConnector::getInstance().getDatabase();
    if (!db.isOpen()) {
        WARNING_LOG << "Database not open for queryByProjectNameAndBatchCodeAndShortCodeAndParallelNo.";
        return QList<SingleTobaccoSampleData>();
    }

    // 从SqlConfigLoader获取SQL语句
    QString sql = SqlConfigLoader::getInstance()
                    .getSqlOperation("SingleTobaccoSampleDAO", "query_by_project_batch_short_parallel").sql;

    if (sql.isEmpty()) {
        // 默认SQL（防止配置缺失）
        sql = R"(
            SELECT s.*
            FROM single_tobacco_sample s
            JOIN tobacco_batch b ON s.batch_id = b.id
            WHERE s.project_name = :project_name
              AND b.batch_code = :batch_code
              AND s.short_code = :short_code
              AND s.parallel_no = :parallel_no
        )";
        DEBUG_LOG << "Using default SQL for SingleTobaccoSampleDAO.queryByProjectNameAndBatchCodeAndShortCodeAndParallelNo";
    }

    QSqlQuery query(db);
    query.prepare(sql);

    // 绑定参数
    query.bindValue(":project_name", projectName);
    query.bindValue(":batch_code", batchCode);
    query.bindValue(":short_code", shortCode);
    query.bindValue(":parallel_no", parallelNo);

    QList<SingleTobaccoSampleData> list;

    if (query.exec()) {
        while (query.next()) {
            list.append(createSampleFromQuery(query)); // 调用辅助函数生成实体
        }
    } else {
        WARNING_LOG << "Query SingleTobaccoSampleData failed:" << query.lastError().text();
    }

    return list;
}


// 根据样品ID获取样品标识project_name,batch_code,short_code,parallel_no
SampleIdentifier SingleTobaccoSampleDAO::getSampleIdentifierById(int sampleId)
{
    SampleIdentifier identifier;

    QSqlDatabase db = m_db.isValid() ? m_db : DatabaseConnector::getInstance().getDatabase();
    if (!db.isOpen()) {
        qWarning() << "Database not open for getSampleIdentifierById operation.";
        return identifier;
    }

    QSqlQuery query(db);

    // SQL: 连接批次表获取 batch_code
    QString sql = SqlConfigLoader::getInstance()
                      .getSqlOperation("SingleTobaccoSampleDAO", "getSampleIdentifierById")
                      .sql;
    if (sql.isEmpty()) {
        sql = R"(
            SELECT s.project_name, b.batch_code, s.short_code, s.parallel_no
            FROM single_tobacco_sample AS s
            LEFT JOIN tobacco_batch AS b ON s.batch_id = b.id
            WHERE s.id = :id
        )";
        qDebug() << "Using default SQL with batch join for getSampleIdentifierById";
    }

    query.prepare(sql);
    query.bindValue(":id", sampleId);

    if (!query.exec()) {
        qWarning() << "Query failed:" << query.lastError().text();
        return identifier;
    }

    if (query.next()) {
        identifier.sampleId          = sampleId;  // 新增，直接用传入的 sampleId
        identifier.projectName = query.value("project_name").toString();
        identifier.batchCode   = query.value("batch_code").toString();
        identifier.shortCode   = query.value("short_code").toString();
        identifier.parallelNo  = query.value("parallel_no").toInt();
    } else {
        qWarning() << "No record found for sampleId =" << sampleId;
    }

    return identifier;
}



QList<SampleIdentifier> SingleTobaccoSampleDAO::getSampleIdentifiersByProjectAndBatch(
    const QString& projectName,
    const QString& batchCode, 
    const BatchType batchType)
{
    QList<SampleIdentifier> result;

    QSqlDatabase db = m_db.isValid() ? m_db : DatabaseConnector::getInstance().getDatabase();
    if (!db.isOpen()) return result;

    QSqlQuery query(db);

    QString sql = SqlConfigLoader::getInstance()
                      .getSqlOperation("SingleTobaccoSampleDAO", "get_sampleIdentifiers_by_project_batch")
                      .sql;

    if (sql.isEmpty()) {
        sql = R"(
            SELECT s.id, s.project_name, b.batch_code, s.short_code, s.parallel_no
            FROM single_tobacco_sample AS s
            LEFT JOIN tobacco_batch AS b ON s.batch_id = b.id
            WHERE s.project_name = :projectName
              AND b.batch_code = :batchCode
              AND b.batch_type = :batchType
        )";
        qDebug() << "Using default SQL with batch join and batch_type filter for getSampleIdentifiersByProjectAndBatch";
    }

    query.prepare(sql);
    query.bindValue(":projectName", projectName);
    query.bindValue(":batchCode", batchCode);
    query.bindValue(":batchType", batchType == BatchType::NORMAL ? "NORMAL" : "PROCESS");

    if (!query.exec()) {
        qWarning() << "Query failed:" << query.lastError().text();
        return result;
    }

    while (query.next()) {
        SampleIdentifier sample;
        sample.sampleId   = query.value("id").toInt();
        sample.projectName = query.value("project_name").toString();
        sample.batchCode   = query.value("batch_code").toString();
        sample.shortCode   = query.value("short_code").toString();
        sample.parallelNo  = query.value("parallel_no").toInt();
        result.append(sample);
    }

    return result;
}

