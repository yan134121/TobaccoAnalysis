


#include "Logger.h"
#include "NavigatorDAO.h"
#include "DatabaseManager.h"
#include "core/sql/SqlConfigLoader.h"
#include <QSqlQuery>
#include <QSqlError>

NavigatorDAO::NavigatorDAO() {}

QList<QPair<QString, int>> NavigatorDAO::fetchAllModels(QString &error)
{
    QList<QPair<QString, int>> models;
    QSqlQuery query(DatabaseManager::instance().database());
    
    // 使用SqlConfigLoader获取SQL语句，如果配置中不存在则使用默认SQL
    QString sql = SqlConfigLoader::getInstance().getSqlOperation("NavigatorDAO", "select_tobacco_models").sql;
    if (sql.isEmpty()) {
        sql = "SELECT id, model_name, model_code FROM tobacco_model ORDER BY model_code";
    }
    query.prepare(sql);

    if (!query.exec()) {
        error = query.lastError().text();
        return models;
    }
    while (query.next()) {
        // QString name = QString("%1 (%2)").arg(query.value("model_name").toString()).arg(query.value("model_code").toString());
        QString name = QString("%1").arg(query.value("model_code").toString());
        models.append({name, query.value("id").toInt()});
    }
    return models;
}

QList<QPair<QString, int>> NavigatorDAO::fetchBatchesForModel(int modelId, QString &error, const QString &batchType)
{
    QList<QPair<QString, int>> batches;
    QSqlQuery query(DatabaseManager::instance().database());
    
    // 使用SqlConfigLoader获取SQL语句，如果配置中不存在则使用默认SQL
    QString sql = SqlConfigLoader::getInstance().getSqlOperation("NavigatorDAO", "select_batches").sql;
    if (sql.isEmpty()) {
        sql = "SELECT id, batch_code FROM tobacco_batch WHERE model_id = :model_id AND batch_type = :batch_type ORDER BY batch_code";
    }
    query.prepare(sql);
    query.bindValue(":model_id", modelId);
    query.bindValue(":batch_type", batchType);

    if (!query.exec()) {
        error = query.lastError().text();
        return batches;
    }
    while (query.next()) {
        batches.append({query.value("batch_code").toString(), query.value("id").toInt()});
    }
    return batches;
}

QList<QString> NavigatorDAO::fetchBatchCodesForProject(const QString& projectName, QString &error)
{
    QList<QString> batchCodes;
    QSqlQuery query(DatabaseManager::instance().database());
    
    // 使用SqlConfigLoader获取SQL语句，如果配置中不存在则使用默认SQL
    QString sql = SqlConfigLoader::getInstance().getSqlOperation("NavigatorDAO", "select_batch_codes_by_project").sql;
    if (sql.isEmpty()) {
        sql = R"(
        SELECT DISTINCT s.batch_code 
        FROM tg_big_data d
        JOIN single_tobacco_sample s ON d.sample_id = s.id
        WHERE s.project_name = :project_name 
        ORDER BY s.batch_code
        )";
    }
    query.prepare(sql);
    query.bindValue(":project_name", projectName);
    
    if (!query.exec()) {
        error = query.lastError().text();
        return batchCodes;
    }
    while (query.next()) {
        QString batchCode = query.value(0).toString();
        if (!batchCode.isEmpty()) {
            batchCodes.append(batchCode);
        }
    }
    return batchCodes;
}

QList<QVariantMap> NavigatorDAO::fetchSamplesForProjectAndBatch(const QString& projectName, const QString& batchCode, QString &error)
{
    QList<QVariantMap> samples;
    QSqlQuery query(DatabaseManager::instance().database());
    
    // 使用SqlConfigLoader获取SQL语句，如果配置中不存在则使用默认SQL
    QString sql = SqlConfigLoader::getInstance().getSqlOperation("NavigatorDAO", "select_samples_by_project_and_batch").sql;
    if (sql.isEmpty()) {
        sql = R"(
        SELECT DISTINCT
               s.id as sample_id,
               s.project_name,
               b.batch_code,
               s.short_code,
               s.parallel_no,
               s.sample_name,
               s.origin,
               s.grade
        FROM tg_big_data d
        JOIN single_tobacco_sample s ON d.sample_id = s.id
        LEFT JOIN tobacco_batch b ON s.batch_id = b.id
        WHERE s.project_name = :project_name AND b.batch_code = :batch_code
        ORDER BY s.short_code, s.parallel_no
        )";
    }
    query.prepare(sql);
    query.bindValue(":project_name", projectName);
    query.bindValue(":batch_code", batchCode);
    
    if (!query.exec()) {
        error = query.lastError().text();
        return samples;
    }
    
    while (query.next()) {
        QVariantMap sample;
        sample["sample_id"] = query.value("sample_id");
        sample["project_name"] = query.value("project_name");
        sample["batch_code"] = query.value("batch_code");
        sample["short_code"] = query.value("short_code");
        sample["parallel_no"] = query.value("parallel_no");
        sample["sample_name"] = query.value("sample_name");
        sample["origin"] = query.value("origin");
        sample["grade"] = query.value("grade");
        samples.append(sample);
    }
    
    return samples;
}

// --- 【关键】在这里添加缺失的函数实现 ---

QList<QString> NavigatorDAO::fetchShortCodesForBatch(int batchId, QString &error)
{
    QList<QString> shortCodes;
    QSqlQuery query(DatabaseManager::instance().database());
    
    // 使用SqlConfigLoader获取SQL语句，如果配置中不存在则使用默认SQL
    QString sql = SqlConfigLoader::getInstance().getSqlOperation("NavigatorDAO", "select_short_codes_by_batch").sql;
    if (sql.isEmpty()) {
        sql = "SELECT DISTINCT short_code FROM single_tobacco_sample WHERE batch_id = :batch_id ORDER BY short_code";
    }
    query.prepare(sql);
    query.bindValue(":batch_id", batchId);
    
    if (!query.exec()) {
        error = query.lastError().text();
        return shortCodes;
    }
    while (query.next()) {
        shortCodes.append(query.value(0).toString());
    }
    return shortCodes;
}

QList<ParallelSampleInfo> NavigatorDAO::fetchParallelSamplesForBatch(int batchId, const QString &shortCode, QString &error)
{
    QList<ParallelSampleInfo> samples;
    QSqlQuery query(DatabaseManager::instance().database());
    
    // 使用SqlConfigLoader获取SQL语句，如果配置中不存在则使用默认SQL
    QString sql = SqlConfigLoader::getInstance().getSqlOperation("NavigatorDAO", "select_parallel_samples_by_batch").sql;
    if (sql.isEmpty()) {
        sql = "SELECT id, parallel_no FROM single_tobacco_sample WHERE batch_id = :batch_id AND short_code = :short_code ORDER BY parallel_no";
    }
    query.prepare(sql);
    query.bindValue(":batch_id", batchId);
    query.bindValue(":short_code", shortCode);
    
    if (!query.exec()) {
        error = query.lastError().text();
        return samples;
    }
    while (query.next()) {
        // 使用 C++11 的列表初始化
        samples.append({query.value("id").toInt(), query.value("parallel_no").toInt()});
    }
    return samples;
}


QVector<QPointF> NavigatorDAO::getSampleCurveData(int sampleId, const QString &dataType, QString &error)
{
    QVector<QPointF> data;
    QSqlQuery query(DatabaseManager::instance().database());
    QString queryString;

    DEBUG_LOG << "NavigatorDAO::getSampleCurveData - Getting data for sampleId:" << sampleId << "dataType:" << dataType;

    // 根据数据类型选择对应的SQL操作
    if (dataType == "大热重") {
        queryString = SqlConfigLoader::getInstance().getSqlOperation("SampleDAO", "select_data_by_sample_id_big").sql;
        if (queryString.isEmpty()) {
            queryString = "SELECT serial_no, weight FROM tg_big_data WHERE sample_id = :sample_id ORDER BY serial_no";
        }
    } else if (dataType == "工序大热重") {
        queryString = SqlConfigLoader::getInstance().getSqlOperation("SampleDAO", "select_data_by_sample_id_process_big").sql;
        if (queryString.isEmpty()) {
            queryString = "SELECT serial_no, weight FROM process_tg_big_data WHERE sample_id = :sample_id ORDER BY serial_no";
        }
    } else if (dataType == "小热重") {
        queryString = SqlConfigLoader::getInstance().getSqlOperation("SampleDAO", "select_data_by_sample_id_small").sql;
        if (queryString.isEmpty()) {
            queryString = "SELECT temperature, tg_value FROM tg_small_data WHERE sample_id = :sample_id ORDER BY temperature";
        }
    } else if (dataType == "色谱") {
        queryString = SqlConfigLoader::getInstance().getSqlOperation("SampleDAO", "select_data_by_sample_id_chrom").sql;
        if (queryString.isEmpty()) {
            queryString = "SELECT retention_time, response_value FROM chromatography_data WHERE sample_id = :sample_id ORDER BY retention_time";
        }
    } else {
        error = QString("未知的数据类型: %1").arg(dataType);
        DEBUG_LOG << "NavigatorDAO::getSampleCurveData - Error:" << error;
        return data;
    }

    DEBUG_LOG << "NavigatorDAO::getSampleCurveData - Query:" << queryString;
    
    query.prepare(queryString);
    query.bindValue(":sample_id", sampleId);

    if (!query.exec()) {
        error = query.lastError().text();
        DEBUG_LOG << "NavigatorDAO::getSampleCurveData - SQL Error:" << error;
        return data;
    }

    while (query.next()) {
        bool okX, okY;
        double x = query.value(0).toDouble(&okX);
        double y = query.value(1).toDouble(&okY);
        if (okX && okY) {
            data.append(QPointF(x, y));
        }
    }

    DEBUG_LOG << "NavigatorDAO::getSampleCurveData - Retrieved" << data.size() << "data points";
    
    // 如果没有数据，尝试输出一些调试信息
    if (data.isEmpty()) {
        DEBUG_LOG << "NavigatorDAO::getSampleCurveData - WARNING: No data found for sampleId:" << sampleId << "dataType:" << dataType;
        
        // 检查样本是否存在
        QSqlQuery checkQuery(DatabaseManager::instance().database());
        // 使用SqlConfigLoader获取SQL语句，如果配置中不存在则使用默认SQL
        QString checkSql = SqlConfigLoader::getInstance().getSqlOperation("NavigatorDAO", "check_sample_exists").sql;
        if (checkSql.isEmpty()) {
            checkSql = "SELECT COUNT(*) FROM single_tobacco_sample WHERE id = :sample_id";
        }
        checkQuery.prepare(checkSql);
        checkQuery.bindValue(":sample_id", sampleId);
        
        if (checkQuery.exec() && checkQuery.next()) {
            int count = checkQuery.value(0).toInt();
            DEBUG_LOG << "NavigatorDAO::getSampleCurveData - Sample exists in database:" << (count > 0 ? "Yes" : "No");
        } else {
            DEBUG_LOG << "NavigatorDAO::getSampleCurveData - Failed to check if sample exists:" << checkQuery.lastError().text();
        }
    }

    return data;
}

QVector<QPointF> NavigatorDAO::getSmallRawWeightCurveData(int sampleId, QString &error)
{
    QVector<QPointF> data;
    QSqlQuery query(DatabaseManager::instance().database());
    QString queryString = "SELECT serial_no, weight FROM tg_small_data WHERE sample_id = :sample_id ORDER BY serial_no";

    query.prepare(queryString);
    query.bindValue(":sample_id", sampleId);

    if (!query.exec()) {
        error = query.lastError().text();
        DEBUG_LOG << "NavigatorDAO::getSmallRawWeightCurveData - SQL Error:" << error;
        return data;
    }

    while (query.next()) {
        bool okX, okY;
        double x = query.value(0).toDouble(&okX);
        double y = query.value(1).toDouble(&okY);
        if (okX && okY) {
            data.append(QPointF(x, y));
        }
    }

    return data;
}

QVariantMap NavigatorDAO::getSampleDetailInfo(int sampleId, QString& error)
{
    QVariantMap result;
    QSqlQuery query(DatabaseManager::instance().database());
    // 优先从SqlConfigLoader读取SQL；如未配置则使用默认SQL
    QString sql = SqlConfigLoader::getInstance().getSqlOperation("NavigatorDAO", "get_sample_detail_info").sql;
    if (sql.isEmpty()) {
        sql = "SELECT s.id, s.project_name, b.batch_code, s.short_code, s.parallel_no "
              "FROM single_tobacco_sample s "
              "JOIN tobacco_batch b ON s.batch_id = b.id "
              "WHERE s.id = :sample_id";
    }
    query.prepare(sql);
    query.bindValue(":sample_id", sampleId);
    
    if (!query.exec()) {
        error = query.lastError().text();
        return result;
    }
    
    if (query.next()) {
        result["project_name"] = query.value("project_name");
        result["batch_code"] = query.value("batch_code");
        result["short_code"] = query.value("short_code");
        result["parallel_no"] = query.value("parallel_no");
    }
    
    return result;
}

QStringList NavigatorDAO::getAvailableDataTypesForShortCode(int batchId, const QString& shortCode, QString& error)
{
    QStringList dataTypes;
    QSqlQuery query(DatabaseManager::instance().database());
    
    // 检查大热重数据
    QString sql = SqlConfigLoader::getInstance().getSqlOperation("NavigatorDAO", "exists_tg_big_by_batch_and_short_code").sql;
    if (sql.isEmpty()) {
        sql = R"(
            SELECT COUNT(*) as count
            FROM tg_big_data d
            JOIN single_tobacco_sample s ON d.sample_id = s.id
            WHERE s.batch_id = :batch_id AND s.short_code = :short_code
        )";
    }
    query.prepare(sql);
    query.bindValue(":batch_id", batchId);
    query.bindValue(":short_code", shortCode);
    
    if (query.exec() && query.next()) {
        if (query.value("count").toInt() > 0) {
            dataTypes.append("大热重");
        }
    }
    
    // 检查小热重数据
    sql = SqlConfigLoader::getInstance().getSqlOperation("NavigatorDAO", "exists_tg_small_by_batch_and_short_code").sql;
    if (sql.isEmpty()) {
        sql = R"(
            SELECT COUNT(*) as count
            FROM tg_small_data d
            JOIN single_tobacco_sample s ON d.sample_id = s.id
            WHERE s.batch_id = :batch_id AND s.short_code = :short_code
        )";
    }
    query.prepare(sql);
    query.bindValue(":batch_id", batchId);
    query.bindValue(":short_code", shortCode);
    
    if (query.exec() && query.next()) {
        if (query.value("count").toInt() > 0) {
            dataTypes.append("小热重");
        }
    }
    
    // 检查色谱数据
    sql = SqlConfigLoader::getInstance().getSqlOperation("NavigatorDAO", "exists_chrom_by_batch_and_short_code").sql;
    if (sql.isEmpty()) {
        sql = R"(
            SELECT COUNT(*) as count
            FROM chromatography_data d
            JOIN single_tobacco_sample s ON d.sample_id = s.id
            WHERE s.batch_id = :batch_id AND s.short_code = :short_code
        )";
    }
    query.prepare(sql);
    query.bindValue(":batch_id", batchId);
    query.bindValue(":short_code", shortCode);
    
    if (query.exec() && query.next()) {
        if (query.value("count").toInt() > 0) {
            dataTypes.append("色谱");
        }
    }
    
    if (query.lastError().isValid()) {
        error = query.lastError().text();
        DEBUG_LOG << "NavigatorDAO::getAvailableDataTypesForShortCode - SQL Error:" << error;
    }
    
    DEBUG_LOG << "NavigatorDAO::getAvailableDataTypesForShortCode - Available types for batchId:" 
             << batchId << "shortCode:" << shortCode << "types:" << dataTypes;
    
    return dataTypes;
}

int NavigatorDAO::countSamplesForModel(int modelId, QString& error)
{
    int count = 0;
    QSqlQuery query(DatabaseManager::instance().database());
    
    // 优先从SQL配置加载，如未配置则使用默认SQL
    QString sql = SqlConfigLoader::getInstance().getSqlOperation("NavigatorDAO", "count_samples_by_model").sql;
    if (sql.isEmpty()) {
        sql = R"(
            SELECT COUNT(*) AS cnt
            FROM single_tobacco_sample s
            JOIN tobacco_batch b ON s.batch_id = b.id
            WHERE b.model_id = :model_id
        )";
    }
    
    query.prepare(sql);
    query.bindValue(":model_id", modelId);
    
    if (!query.exec()) {
        error = query.lastError().text();
        return 0;
    }
    if (query.next()) {
        count = query.value("cnt").toInt();
    }
    return count;
}

int NavigatorDAO::countBatchesForModel(int modelId, QString& error)
{
    int count = 0;
    QSqlQuery query(DatabaseManager::instance().database());
    QString sql = SqlConfigLoader::getInstance().getSqlOperation("NavigatorDAO", "count_batches_by_model").sql;
    if (sql.isEmpty()) {
        sql = R"(
            SELECT COUNT(*) AS cnt
            FROM tobacco_batch
            WHERE model_id = :model_id
        )";
    }
    query.prepare(sql);
    query.bindValue(":model_id", modelId);
    if (!query.exec()) {
        error = query.lastError().text();
        return 0;
    }
    if (query.next()) {
        count = query.value("cnt").toInt();
    }
    return count;
}

int NavigatorDAO::countSamplesForBatch(int batchId, QString& error)
{
    int count = 0;
    QSqlQuery query(DatabaseManager::instance().database());
    QString sql = SqlConfigLoader::getInstance().getSqlOperation("NavigatorDAO", "count_samples_by_batch").sql;
    if (sql.isEmpty()) {
        sql = R"(
            SELECT COUNT(*) AS cnt
            FROM single_tobacco_sample
            WHERE batch_id = :batch_id
        )";
    }
    query.prepare(sql);
    query.bindValue(":batch_id", batchId);
    if (!query.exec()) {
        error = query.lastError().text();
        return 0;
    }
    if (query.next()) {
        count = query.value("cnt").toInt();
    }
    return count;
}

QList<QVariantMap> NavigatorDAO::searchSamplesByName(const QString& keyword, QString& error)
{
    // 
    // 按样本名称进行模糊搜索，同时兼容短码、项目名等字段，
    // 返回用于导航树路径展开所需的必要信息（model_id, batch_id 等）。
    QList<QVariantMap> results;
    QSqlQuery query(DatabaseManager::instance().database());

    // 优先从SQL配置加载，如未配置则使用默认SQL。
    QString sql = SqlConfigLoader::getInstance().getSqlOperation("NavigatorDAO", "search_samples_by_name").sql;
    if (sql.isEmpty()) {
        sql = R"(
            SELECT 
                s.id            AS sample_id,
                s.parallel_no   AS parallel_no,
                s.short_code    AS short_code,
                s.sample_name   AS sample_name,
                s.project_name  AS project_name,
                b.id            AS batch_id,
                b.batch_code    AS batch_code,
                b.batch_type    AS batch_type,
                m.id            AS model_id,
                m.model_name    AS model_name,
                m.model_code    AS model_code
            FROM single_tobacco_sample s
            JOIN tobacco_batch b ON s.batch_id = b.id
            JOIN tobacco_model m ON b.model_id = m.id
            WHERE 
                s.sample_name LIKE :kw
                OR s.short_code LIKE :kw
                OR s.project_name LIKE :kw
            ORDER BY m.model_code, b.batch_code, s.short_code, s.parallel_no
        )";
    }

    // 绑定模糊匹配参数：%keyword%
    query.prepare(sql);
    query.bindValue(":kw", QString("%" + keyword + "%"));

    if (!query.exec()) {
        error = query.lastError().text();
        return results;
    }

    while (query.next()) {
        QVariantMap item;
        item["sample_id"]   = query.value("sample_id");
        item["parallel_no"] = query.value("parallel_no");
        item["short_code"]  = query.value("short_code");
        item["sample_name"] = query.value("sample_name");
        item["project_name"] = query.value("project_name");
        item["batch_id"]    = query.value("batch_id");
        item["batch_code"]  = query.value("batch_code");
        item["batch_type"]  = query.value("batch_type");
        item["model_id"]    = query.value("model_id");
        item["model_name"]  = query.value("model_name");
        item["model_code"]  = query.value("model_code");
        results.append(item);
    }

    return results;
}

// 【新增】获取指定数据类型的所有ShortCode
QList<QString> NavigatorDAO::fetchShortCodesForDataType(const QString& dataType, QString& error)
{
    QList<QString> shortCodes;
    QSqlQuery query(DatabaseManager::instance().database());
    
    // 确定数据表名
    QString tableName;
    if (dataType == "大热重") tableName = "tg_big_data";
    else if (dataType == "小热重") tableName = "tg_small_data";
    else if (dataType == "色谱") tableName = "chromatography_data";
    else return shortCodes;

    // 联合查询，找出存在于指定数据表中的样本ShortCode
    // 使用SqlConfigLoader获取SQL语句
    QString opName = QString("select_short_codes_for_%1").arg(tableName);
    QString sql = SqlConfigLoader::getInstance().getSqlOperation("NavigatorDAO", opName).sql;
    
    if (sql.isEmpty()) {
        // 默认SQL
        sql = QString(R"(
            SELECT DISTINCT s.short_code 
            FROM single_tobacco_sample s
            JOIN %1 d ON s.id = d.sample_id
            ORDER BY s.short_code
        )").arg(tableName);
    }
    
    query.prepare(sql);
    
    if (!query.exec()) {
        error = query.lastError().text();
        DEBUG_LOG << "NavigatorDAO::fetchShortCodesForDataType - SQL Error:" << error;
        return shortCodes;
    }
    
    while (query.next()) {
        shortCodes.append(query.value(0).toString());
    }
    return shortCodes;
}

// 【新增】获取指定ShortCode和数据类型的平行样信息（ID, ParallelNo, Timestamp）
QList<NavigatorDAO::SampleLeafInfo> NavigatorDAO::fetchParallelSamplesForShortCodeAndType(const QString& shortCode, const QString& dataType, QString& error)
{
    QList<SampleLeafInfo> samples;
    QSqlQuery query(DatabaseManager::instance().database());
    
    QString tableName;
    QString timeField = "create_time"; // 默认时间字段，根据实际表结构调整
    
    if (dataType == "大热重") {
        tableName = "tg_big_data";
        // 大热重表通常没有独立的时间戳，使用样本表的 create_time 或 detect_date
        // 但如果数据是一次性导入的，可能需要查关联表的字段
    } else if (dataType == "小热重") {
        tableName = "tg_small_data";
    } else if (dataType == "色谱") {
        tableName = "chromatography_data";
        // 色谱表可能有独立时间
    } else {
        return samples;
    }

    // 注意：这里我们需要从 single_tobacco_sample 表获取ID和平行号，同时确认数据存在
    // 时间戳我们优先取 single_tobacco_sample 的 detect_date 或 create_time
    // 如果数据表有特定时间戳，也可以取
    
    QString sql = QString(R"(
        SELECT DISTINCT s.id, s.parallel_no, s.detect_date, s.created_at, s.project_name, b.batch_code
        FROM single_tobacco_sample s
        JOIN tobacco_batch b ON s.batch_id = b.id
        JOIN %1 d ON s.id = d.sample_id
        WHERE s.short_code = :short_code
        ORDER BY s.parallel_no
    )").arg(tableName);
    
    query.prepare(sql);
    query.bindValue(":short_code", shortCode);
    
    if (!query.exec()) {
        error = query.lastError().text();
        DEBUG_LOG << "NavigatorDAO::fetchParallelSamplesForShortCodeAndType - SQL Error:" << error;
        return samples;
    }
    
    while (query.next()) {
        SampleLeafInfo info;
        info.id = query.value("id").toInt();
        info.parallelNo = query.value("parallel_no").toInt();
        info.projectName = query.value("project_name").toString();
        info.batchCode = query.value("batch_code").toString();
        QDateTime dt = query.value("detect_date").toDateTime();
        if (!dt.isValid()) {
             // 尝试使用 created_at 作为回退
             dt = query.value("created_at").toDateTime();
        }
        
        // 如果仍然无效，则显示 N/A，而不是当前时间，避免用户困惑
        if (dt.isValid()) {
            info.timestamp = dt.toString("yyyyMMdd_HHmmss");
        } else {
            info.timestamp = "N/A";
        }
        
        samples.append(info);
    }
    return samples;
}

// 【新增】获取工序大热重数据的所有项目名
QList<QString> NavigatorDAO::fetchProjectsForProcessData(QString& error)
{
    QList<QString> projects;
    QSqlQuery query(DatabaseManager::instance().database());
    
    // 查询存在工序大热重数据的项目名
    QString sql = R"(
        SELECT DISTINCT s.project_name
        FROM single_tobacco_sample s
        JOIN process_tg_big_data d ON s.id = d.sample_id
        ORDER BY s.project_name
    )";
    
    query.prepare(sql);
    if (!query.exec()) {
        error = query.lastError().text();
        return projects;
    }
    
    while (query.next()) {
        projects.append(query.value(0).toString());
    }
    return projects;
}

// 【新增】获取指定工序大热重项目的所有批次
QList<QPair<QString, int>> NavigatorDAO::fetchBatchesForProcessProject(const QString& projectName, QString& error)
{
    QList<QPair<QString, int>> batches;
    QSqlQuery query(DatabaseManager::instance().database());
    
    QString sql = R"(
        SELECT DISTINCT b.batch_code, b.id
        FROM tobacco_batch b
        JOIN single_tobacco_sample s ON s.batch_id = b.id
        JOIN process_tg_big_data d ON s.id = d.sample_id
        WHERE s.project_name = :project_name
        ORDER BY b.batch_code
    )";
    
    query.prepare(sql);
    query.bindValue(":project_name", projectName);
    
    if (!query.exec()) {
        error = query.lastError().text();
        return batches;
    }
    
    while (query.next()) {
        batches.append({query.value(0).toString(), query.value(1).toInt()});
    }
    return batches;
}

// 【新增】获取指定工序大热重批次的所有样本（ShortCode, ParallelNo）
QList<NavigatorDAO::SampleLeafInfo> NavigatorDAO::fetchSamplesForProcessBatch(const QString& batchCode, QString& error)
{
    QList<SampleLeafInfo> samples;
    QSqlQuery query(DatabaseManager::instance().database());
    
    QString sql = R"(
        SELECT DISTINCT s.id, s.short_code, s.parallel_no, s.project_name, b.batch_code
        FROM single_tobacco_sample s
        JOIN tobacco_batch b ON s.batch_id = b.id
        JOIN process_tg_big_data d ON s.id = d.sample_id
        WHERE b.batch_code = :batch_code
        ORDER BY s.short_code, s.parallel_no
    )";
    
    query.prepare(sql);
    query.bindValue(":batch_code", batchCode);
    
    if (!query.exec()) {
        error = query.lastError().text();
        return samples;
    }
    
    while (query.next()) {
        SampleLeafInfo info;
        info.id = query.value("id").toInt();
        info.parallelNo = query.value("parallel_no").toInt();
        info.shortCode = query.value("short_code").toString();
        info.projectName = query.value("project_name").toString();
        info.batchCode = query.value("batch_code").toString();
        // 工序数据不需要时间戳显示，置空
        info.timestamp = ""; 
        samples.append(info);
    }
    return samples;
}

// =============================
// 级联删除实现（中文注释）
// =============================

bool NavigatorDAO::deleteProjectCascade(const QString& projectName, bool processBranch, QString& error)
{
    QSqlDatabase db = DatabaseManager::instance().database();
    if (!db.isOpen()) { error = "数据库未连接"; return false; }

    bool ok = db.transaction();
    if (!ok) { error = db.lastError().text(); return false; }

    // 根据分支类型筛选批次（NORMAL 或 PROCESS）
    const QString batchType = processBranch ? "PROCESS" : "NORMAL";

    // 优先按样本的 project_name 反查批次与型号ID；未命中则回退按型号代码匹配
    QList<int> batchIds;
    QList<int> modelIds;
    {
        QSqlQuery q(db);
        // 从配置加载查询；默认SQL如未配置
        QString sql = SqlConfigLoader::getInstance().getSqlOperation("NavigatorDAO", "select_batch_and_model_ids_by_project_name_and_type").sql;
        if (sql.isEmpty()) {
            sql = R"(
                SELECT DISTINCT b.id, b.model_id
                FROM single_tobacco_sample s
                JOIN tobacco_batch b ON s.batch_id = b.id
                WHERE s.project_name = :project_name
                  AND b.batch_type = :batch_type
            )";
        }
        q.prepare(sql);
        q.bindValue(":project_name", projectName);
        q.bindValue(":batch_type", batchType);
        if (!q.exec()) {
            db.rollback();
            error = q.lastError().text();
            return false;
        }
        while (q.next()) {
            int bid = q.value(0).toInt();
            int mid = q.value(1).toInt();
            if (!batchIds.contains(bid)) batchIds.append(bid);
            if (mid > 0 && !modelIds.contains(mid)) modelIds.append(mid);
        }
    }
    if (batchIds.isEmpty()) {
        int modelId = -1;
        {
            QSqlQuery q(db);
            QString sql = SqlConfigLoader::getInstance().getSqlOperation("NavigatorDAO", "select_model_id_by_model_code").sql;
            if (sql.isEmpty()) {
                sql = "SELECT id FROM tobacco_model WHERE model_code = :project_name";
            }
            q.prepare(sql);
            q.bindValue(":project_name", projectName);
            if (!q.exec() || !q.next()) {
                db.rollback();
                error = q.lastError().text();
                return false;
            }
            modelId = q.value(0).toInt();
        }
        if (modelId > 0 && !modelIds.contains(modelId)) modelIds.append(modelId);
        QSqlQuery q2(db);
        {
            QString listSql = SqlConfigLoader::getInstance().getSqlOperation("NavigatorDAO", "select_batch_ids_by_model_id_and_type").sql;
            if (listSql.isEmpty()) {
                listSql = "SELECT id FROM tobacco_batch WHERE model_id = :model_id AND batch_type = :batch_type";
            }
            q2.prepare(listSql);
        }
        q2.bindValue(":model_id", modelId);
        q2.bindValue(":batch_type", batchType);
        if (!q2.exec()) {
            db.rollback();
            error = q2.lastError().text();
            return false;
        }
        while (q2.next()) batchIds.append(q2.value(0).toInt());
    }

    // 逐批次删除样本与数据，并统计删除数量
    int deletedSamples = 0;
    int deletedBatches = 0;
    qint64 deletedDataRows = 0;
    for (int batchId : batchIds) {
        // 查询该批次的所有样本ID
        QList<int> sampleIds;
        {
            QSqlQuery q(db);
            QString sql = SqlConfigLoader::getInstance().getSqlOperation("NavigatorDAO", "select_sample_ids_by_batch_id").sql;
            if (sql.isEmpty()) {
                sql = "SELECT id FROM single_tobacco_sample WHERE batch_id = :batch_id";
            }
            q.prepare(sql);
            q.bindValue(":batch_id", batchId);
            if (!q.exec()) { db.rollback(); error = q.lastError().text(); return false; }
            while (q.next()) sampleIds.append(q.value(0).toInt());
        }

        // 删除每个样本的数据
        for (int sid : sampleIds) {
            QSqlQuery q1(db), q2(db), q3(db), q4(db), q5(db);
            if (processBranch) {
                // 工序分支：仅删除工序大热重数据 + 样本
                {
                    QString delSql = SqlConfigLoader::getInstance().getSqlOperation("NavigatorDAO", "delete_process_tg_big_by_sample_id").sql;
                    if (delSql.isEmpty()) {
                        delSql = "DELETE FROM process_tg_big_data WHERE sample_id = :sid";
                    }
                    q1.prepare(delSql);
                }
                q1.bindValue(":sid", sid); if (!q1.exec()) { db.rollback(); error = q1.lastError().text(); return false; }
                deletedDataRows += q1.numRowsAffected();

                {
                    QString delSql = SqlConfigLoader::getInstance().getSqlOperation("NavigatorDAO", "delete_sample_by_id").sql;
                    if (delSql.isEmpty()) {
                        delSql = "DELETE FROM single_tobacco_sample WHERE id = :sid";
                    }
                    q5.prepare(delSql);
                }
                q5.bindValue(":sid", sid); if (!q5.exec()) { db.rollback(); error = q5.lastError().text(); return false; }
                deletedSamples += q5.numRowsAffected();
            } else {
                // 样本分支：删除三类数据 + 样本
                {
                    QString delSql = SqlConfigLoader::getInstance().getSqlOperation("NavigatorDAO", "delete_tg_big_by_sample_id").sql;
                    if (delSql.isEmpty()) {
                        delSql = "DELETE FROM tg_big_data WHERE sample_id = :sid";
                    }
                    q1.prepare(delSql);
                }
                q1.bindValue(":sid", sid); if (!q1.exec()) { db.rollback(); error = q1.lastError().text(); return false; }
                deletedDataRows += q1.numRowsAffected();

                {
                    QString delSql = SqlConfigLoader::getInstance().getSqlOperation("NavigatorDAO", "delete_tg_small_by_sample_id").sql;
                    if (delSql.isEmpty()) {
                        delSql = "DELETE FROM tg_small_data WHERE sample_id = :sid";
                    }
                    q2.prepare(delSql);
                }
                q2.bindValue(":sid", sid); if (!q2.exec()) { db.rollback(); error = q2.lastError().text(); return false; }
                deletedDataRows += q2.numRowsAffected();

                {
                    QString delSql = SqlConfigLoader::getInstance().getSqlOperation("NavigatorDAO", "delete_chrom_by_sample_id").sql;
                    if (delSql.isEmpty()) {
                        delSql = "DELETE FROM chromatography_data WHERE sample_id = :sid";
                    }
                    q3.prepare(delSql);
                }
                q3.bindValue(":sid", sid); if (!q3.exec()) { db.rollback(); error = q3.lastError().text(); return false; }
                deletedDataRows += q3.numRowsAffected();

                {
                    QString delSql = SqlConfigLoader::getInstance().getSqlOperation("NavigatorDAO", "delete_sample_by_id").sql;
                    if (delSql.isEmpty()) {
                        delSql = "DELETE FROM single_tobacco_sample WHERE id = :sid";
                    }
                    q5.prepare(delSql);
                }
                q5.bindValue(":sid", sid); if (!q5.exec()) { db.rollback(); error = q5.lastError().text(); return false; }
                deletedSamples += q5.numRowsAffected();
            }
        }

        // 删除批次
        QSqlQuery qb(db);
        {
            QString delSql = SqlConfigLoader::getInstance().getSqlOperation("NavigatorDAO", "delete_batch_by_id").sql;
            if (delSql.isEmpty()) {
                delSql = "DELETE FROM tobacco_batch WHERE id = :bid";
            }
            qb.prepare(delSql);
        }
        qb.bindValue(":bid", batchId);
        if (!qb.exec()) { db.rollback(); error = qb.lastError().text(); return false; }
        deletedBatches += qb.numRowsAffected();
    }

    // 删除对应型号（tobacco_model）
    int deletedModels = 0;
    for (int mid : modelIds) {
        QSqlQuery qm(db);
        {
            QString delSql = SqlConfigLoader::getInstance().getSqlOperation("NavigatorDAO", "delete_model_by_id").sql;
            if (delSql.isEmpty()) {
                delSql = "DELETE FROM tobacco_model WHERE id = :mid";
            }
            qm.prepare(delSql);
        }
        qm.bindValue(":mid", mid);
        if (!qm.exec()) { db.rollback(); error = qm.lastError().text(); return false; }
        deletedModels += qm.numRowsAffected();
    }

    // 如无任何删除动作则失败，避免误报成功
    if (deletedSamples == 0 && deletedBatches == 0 && deletedDataRows == 0 && deletedModels == 0) {
        db.rollback();
        error = "未找到匹配的批次、样本或项目（tobacco_model）。";
        return false;
    }

    // 提交事务
    if (!db.commit()) { db.rollback(); error = db.lastError().text(); return false; }
    return true;
}

bool NavigatorDAO::deleteBatchCascade(const QString& projectName, const QString& batchCode, bool processBranch, QString& error)
{
    QSqlDatabase db = DatabaseManager::instance().database();
    if (!db.isOpen()) { error = "数据库未连接"; return false; }
    if (!db.transaction()) { error = db.lastError().text(); return false; }

    const QString batchType = processBranch ? "PROCESS" : "NORMAL";

    // 查 model_id
    int modelId = -1;
    {
        QSqlQuery q(db);
        QString sql = SqlConfigLoader::getInstance().getSqlOperation("NavigatorDAO", "select_model_id_by_model_code").sql;
        if (sql.isEmpty()) {
            sql = "SELECT id FROM tobacco_model WHERE model_code = :project_name";
        }
        q.prepare(sql);
        q.bindValue(":project_name", projectName);
        if (!q.exec() || !q.next()) { db.rollback(); error = q.lastError().text(); return false; }
        modelId = q.value(0).toInt();
    }

    // 查 batch_id
    int batchId = -1;
    {
        QSqlQuery q(db);
        QString sql = SqlConfigLoader::getInstance().getSqlOperation("NavigatorDAO", "select_batch_id_by_model_id_and_code_and_type").sql;
        if (sql.isEmpty()) {
            sql = "SELECT id FROM tobacco_batch WHERE model_id = :model_id AND batch_code = :batch_code AND batch_type = :batch_type";
        }
        q.prepare(sql);
        q.bindValue(":model_id", modelId);
        q.bindValue(":batch_code", batchCode);
        q.bindValue(":batch_type", batchType);
        if (!q.exec() || !q.next()) { db.rollback(); error = q.lastError().text(); return false; }
        batchId = q.value(0).toInt();
    }

    // 查样本ID
    QList<int> sampleIds;
    {
        QSqlQuery q(db);
        QString sql = SqlConfigLoader::getInstance().getSqlOperation("NavigatorDAO", "select_sample_ids_by_batch_id").sql;
        if (sql.isEmpty()) {
            sql = "SELECT id FROM single_tobacco_sample WHERE batch_id = :batch_id";
        }
        q.prepare(sql);
        q.bindValue(":batch_id", batchId);
        if (!q.exec()) { db.rollback(); error = q.lastError().text(); return false; }
        while (q.next()) sampleIds.append(q.value(0).toInt());
    }

    // 删除数据与样本
    for (int sid : sampleIds) {
        QSqlQuery q1(db), q2(db), q3(db), q4(db), q5(db);
        if (processBranch) {
            {
                QString delSql = SqlConfigLoader::getInstance().getSqlOperation("NavigatorDAO", "delete_process_tg_big_by_sample_id").sql;
                if (delSql.isEmpty()) {
                    delSql = "DELETE FROM process_tg_big_data WHERE sample_id = :sid";
                }
                q1.prepare(delSql);
            }
            q1.bindValue(":sid", sid); if (!q1.exec()) { db.rollback(); error = q1.lastError().text(); return false; }
            {
                QString delSql = SqlConfigLoader::getInstance().getSqlOperation("NavigatorDAO", "delete_sample_by_id").sql;
                if (delSql.isEmpty()) {
                    delSql = "DELETE FROM single_tobacco_sample WHERE id = :sid";
                }
                q5.prepare(delSql);
            }
            q5.bindValue(":sid", sid); if (!q5.exec()) { db.rollback(); error = q5.lastError().text(); return false; }
        } else {
            {
                QString delSql = SqlConfigLoader::getInstance().getSqlOperation("NavigatorDAO", "delete_tg_big_by_sample_id").sql;
                if (delSql.isEmpty()) {
                    delSql = "DELETE FROM tg_big_data WHERE sample_id = :sid";
                }
                q1.prepare(delSql);
            }
            q1.bindValue(":sid", sid); if (!q1.exec()) { db.rollback(); error = q1.lastError().text(); return false; }
            {
                QString delSql = SqlConfigLoader::getInstance().getSqlOperation("NavigatorDAO", "delete_tg_small_by_sample_id").sql;
                if (delSql.isEmpty()) {
                    delSql = "DELETE FROM tg_small_data WHERE sample_id = :sid";
                }
                q2.prepare(delSql);
            }
            q2.bindValue(":sid", sid); if (!q2.exec()) { db.rollback(); error = q2.lastError().text(); return false; }
            {
                QString delSql = SqlConfigLoader::getInstance().getSqlOperation("NavigatorDAO", "delete_chrom_by_sample_id").sql;
                if (delSql.isEmpty()) {
                    delSql = "DELETE FROM chromatography_data WHERE sample_id = :sid";
                }
                q3.prepare(delSql);
            }
            q3.bindValue(":sid", sid); if (!q3.exec()) { db.rollback(); error = q3.lastError().text(); return false; }
            {
                QString delSql = SqlConfigLoader::getInstance().getSqlOperation("NavigatorDAO", "delete_sample_by_id").sql;
                if (delSql.isEmpty()) {
                    delSql = "DELETE FROM single_tobacco_sample WHERE id = :sid";
                }
                q5.prepare(delSql);
            }
            q5.bindValue(":sid", sid); if (!q5.exec()) { db.rollback(); error = q5.lastError().text(); return false; }
        }
    }

    // 删除批次
    {
        QSqlQuery qb(db);
        QString delSql = SqlConfigLoader::getInstance().getSqlOperation("NavigatorDAO", "delete_batch_by_id").sql;
        if (delSql.isEmpty()) {
            delSql = "DELETE FROM tobacco_batch WHERE id = :bid";
        }
        qb.prepare(delSql);
        qb.bindValue(":bid", batchId);
        if (!qb.exec()) { db.rollback(); error = qb.lastError().text(); return false; }
    }

    if (!db.commit()) { db.rollback(); error = db.lastError().text(); return false; }
    return true;
}

bool NavigatorDAO::deleteSampleCascade(int sampleId, bool processBranch, QString& error)
{
    QSqlDatabase db = DatabaseManager::instance().database();
    if (!db.isOpen()) { error = "数据库未连接"; return false; }
    if (!db.transaction()) { error = db.lastError().text(); return false; }

    if (processBranch) {
        QSqlQuery q1(db), q5(db);
        {
            QString delSql = SqlConfigLoader::getInstance().getSqlOperation("NavigatorDAO", "delete_process_tg_big_by_sample_id").sql;
            if (delSql.isEmpty()) {
                delSql = "DELETE FROM process_tg_big_data WHERE sample_id = :sid";
            }
            q1.prepare(delSql);
        }
        q1.bindValue(":sid", sampleId);
        if (!q1.exec()) { db.rollback(); error = q1.lastError().text(); return false; }

        {
            QString delSql = SqlConfigLoader::getInstance().getSqlOperation("NavigatorDAO", "delete_sample_by_id").sql;
            if (delSql.isEmpty()) {
                delSql = "DELETE FROM single_tobacco_sample WHERE id = :sid";
            }
            q5.prepare(delSql);
        }
        q5.bindValue(":sid", sampleId);
        if (!q5.exec()) { db.rollback(); error = q5.lastError().text(); return false; }
    } else {
        QSqlQuery q1(db), q2(db), q3(db), q5(db);
        {
            QString delSql = SqlConfigLoader::getInstance().getSqlOperation("NavigatorDAO", "delete_tg_big_by_sample_id").sql;
            if (delSql.isEmpty()) {
                delSql = "DELETE FROM tg_big_data WHERE sample_id = :sid";
            }
            q1.prepare(delSql);
        }
        q1.bindValue(":sid", sampleId);
        if (!q1.exec()) { db.rollback(); error = q1.lastError().text(); return false; }

        {
            QString delSql = SqlConfigLoader::getInstance().getSqlOperation("NavigatorDAO", "delete_tg_small_by_sample_id").sql;
            if (delSql.isEmpty()) {
                delSql = "DELETE FROM tg_small_data WHERE sample_id = :sid";
            }
            q2.prepare(delSql);
        }
        q2.bindValue(":sid", sampleId);
        if (!q2.exec()) { db.rollback(); error = q2.lastError().text(); return false; }

        {
            QString delSql = SqlConfigLoader::getInstance().getSqlOperation("NavigatorDAO", "delete_chrom_by_sample_id").sql;
            if (delSql.isEmpty()) {
                delSql = "DELETE FROM chromatography_data WHERE sample_id = :sid";
            }
            q3.prepare(delSql);
        }
        q3.bindValue(":sid", sampleId);
        if (!q3.exec()) { db.rollback(); error = q3.lastError().text(); return false; }

        {
            QString delSql = SqlConfigLoader::getInstance().getSqlOperation("NavigatorDAO", "delete_sample_by_id").sql;
            if (delSql.isEmpty()) {
                delSql = "DELETE FROM single_tobacco_sample WHERE id = :sid";
            }
            q5.prepare(delSql);
        }
        q5.bindValue(":sid", sampleId);
        if (!q5.exec()) { db.rollback(); error = q5.lastError().text(); return false; }
    }

    if (!db.commit()) { db.rollback(); error = db.lastError().text(); return false; }
    return true;
}


