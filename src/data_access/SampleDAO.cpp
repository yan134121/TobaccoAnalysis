// // #include "SampleDAO.h"
// // #include "core/entities/SingleTobaccoSample.h"
// // #include "DatabaseManager.h"

// // SampleDAO::SampleDAO() {}

#include "SampleDAO.h"
#include "Logger.h" 
#include "core/entities/SingleTobaccoSample.h"
#include "data_access/DatabaseManager.h"
#include "core/sql/SqlConfigLoader.h"
#include "common.h"

SampleDAO::SampleDAO() {}

QList<SingleTobaccoSample*> SampleDAO::fetchParallelSamplesInfo(const QString &projectName, const QString &batchCode, 
     const QString &shortCode,
     const int &parallelNo, 
     QString &error)
{
    DEBUG_LOG << "projectName: " << projectName << "batchCode: " << batchCode << "shortCode: " << shortCode;
    QList<SingleTobaccoSample*> samples;
    QSqlQuery query(DatabaseManager::instance().database());


    // 使用SqlConfigLoader获取SQL语句，如果配置中不存在则使用默认SQL
    QString sql = SqlConfigLoader::getInstance().getSqlOperation("SampleDAO", "fetch_parallel_samples_info").sql;
    if (sql.isEmpty()) {
        sql = "SELECT s.id, s.batch_id, s.project_name, b.batch_code, s.short_code, s.parallel_no, s.sample_name, s.origin, s.grade, s.year, s.part, s.type, s.collect_date, s.detect_date "
              "FROM single_tobacco_sample s "
              "JOIN tobacco_batch b ON s.batch_id = b.id "
              "WHERE s.project_name = :project_name "
              "AND b.batch_code = :batch_code "
              "AND s.short_code = :short_code "
              "AND s.parallel_no = :parallel_no";
    }
    query.prepare(sql);

    query.bindValue(":project_name", projectName);
    query.bindValue(":batch_code", batchCode);
    query.bindValue(":short_code", shortCode);
    query.bindValue(":parallel_no", parallelNo);

    if (!query.exec()) {
        error = query.lastError().text();
        return samples;
    }

    while (query.next()) {
        SingleTobaccoSample* sample = new SingleTobaccoSample();
        // ... (填充 sample 对象的代码，和之前一样)
        sample->setId(query.value("id").toInt());
        sample->setProjectName(query.value("project_name").toString());
        sample->setBatchCode(query.value("batch_code").toString());
        sample->setShortCode(query.value("short_code").toString());
        sample->setParallelNo(query.value("parallel_no").toInt());
        sample->setSampleName(query.value("sample_name").toString());
        sample->setOrigin(query.value("origin").toString());
        sample->setGrade(query.value("grade").toString());

        DEBUG_LOG << "Fetched sample:"
             << "ID=" << sample->id()
             << "Project=" << sample->projectName()
             << "Batch=" << sample->batchCode()
             << "ShortCode=" << sample->shortCode()
             << "ParallelNo=" << sample->parallelNo();

        samples.append(sample);
    }
    return samples;
}


QVector<QPointF> SampleDAO::fetchChartDataForSample(int sampleId, const DataType dataType, QString &error)
{
    QVector<QPointF> data;
    QSqlQuery query(DatabaseManager::instance().database());
    QString queryString;
    QString xColumn, yColumn;

    // 根据数据类型，选择要查询的表和列
    if (dataType == DataType::TG_BIG) {
        queryString = SqlConfigLoader::getInstance().getSqlOperation("SampleDAO", "select_data_by_sample_id_big").sql;
        if (queryString.isEmpty()) {
            queryString = "SELECT serial_no, weight FROM tg_big_data WHERE sample_id = :sample_id ORDER BY serial_no";
        }
        xColumn = "serial_no";
        yColumn = "weight";
    } else if (dataType == DataType::TG_SMALL) {
        queryString = SqlConfigLoader::getInstance().getSqlOperation("SampleDAO", "select_data_by_sample_id_small").sql;
        if (queryString.isEmpty()) {
            queryString = "SELECT serial_no, tg_value FROM tg_small_data WHERE sample_id = :sample_id ORDER BY serial_no";
        }
        xColumn = "serial_no";
        yColumn = "tg_value";
    // } else if (dataType == DataType::CHROMATOGRAPHY || dataType == DataType::GC) {
       } else if (dataType == DataType::CHROMATOGRAM) {
        queryString = SqlConfigLoader::getInstance().getSqlOperation("SampleDAO", "select_data_by_sample_id_chrom").sql;
        if (queryString.isEmpty()) {
            queryString = "SELECT retention_time, response_value FROM chromatography_data WHERE sample_id = :sample_id ORDER BY retention_time";
        }
        xColumn = "retention_time";
        yColumn = "response_value";
    } else if (dataType == DataType::PROCESS_TG_BIG) {
        queryString = SqlConfigLoader::getInstance().getSqlOperation("SampleDAO", "select_data_by_sample_id_process_big").sql;
        if (queryString.isEmpty()) {
            queryString = "SELECT serial_no, weight FROM process_tg_big_data WHERE sample_id = :sample_id ORDER BY serial_no";
        }
        xColumn = "serial_no";
        yColumn = "weight";
    } else {
        error = QString("未知的数据类型: %1").arg(dataType);
        DEBUG_LOG << "Unknown data type:" << dataType;
        return data;
    }

    query.prepare(queryString);
    query.bindValue(":sample_id", sampleId);

    if (!query.exec()) {
        error = query.lastError().text();
        WARNING_LOG << "Failed to fetch chart data for sample ID" << sampleId << "Type:" << dataType << ":" << error;
        return data;
    }

    // 循环读取查询结果，填充数据点
    while (query.next()) {
        // 直接使用索引而不是列名，避免可能的列名错误
        bool okX, okY;
        double x = query.value(0).toDouble(&okX);
        double y = query.value(1).toDouble(&okY);
        if (okX && okY) {
            data.append(QPointF(x, y));
        } else {
            WARNING_LOG << "Data conversion error for sample ID" << sampleId << "at row" << query.at();
        }
    }

    return data;
}

QVector<QPointF> SampleDAO::fetchSmallRawWeightData(int sampleId, QString &error)
{
    QVector<QPointF> data;
    QSqlQuery query(DatabaseManager::instance().database());
    QString queryString = "SELECT serial_no, weight FROM tg_small_data WHERE sample_id = :sample_id ORDER BY serial_no";

    query.prepare(queryString);
    query.bindValue(":sample_id", sampleId);

    if (!query.exec()) {
        error = query.lastError().text();
        WARNING_LOG << "Failed to fetch small TG raw weight data for sample ID" << sampleId << ":" << error;
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

QVector<QPointF> SampleDAO::fetchSmallRawDtgData(int sampleId, QString &error)
{
    QVector<QPointF> data;
    QSqlQuery query(DatabaseManager::instance().database());
    QString queryString = "SELECT temperature, dtg_value FROM tg_small_data WHERE sample_id = :sample_id ORDER BY temperature";

    query.prepare(queryString);
    query.bindValue(":sample_id", sampleId);

    if (!query.exec()) {
        error = query.lastError().text();
        WARNING_LOG << "Failed to fetch small TG raw DTG data for sample ID" << sampleId << ":" << error;
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



QList<QVariantMap> SampleDAO::getSamplesByDataType(const QString &dataType)
{
    QString tableName;
    if (dataType == "大热重") {
        tableName = "tg_big_data";
    } else if (dataType == "小热重") {
        tableName = "tg_small_data";
    } else if (dataType == "色谱") {
        tableName = "chromatography_data";
    } else {
        return {}; // 未知数据类型
    }

    QSqlQuery query(DatabaseManager::instance().database());
    
    // 使用SqlConfigLoader获取SQL语句，如果配置中不存在则使用默认SQL
    QString sqlKey;
    if (dataType == "大热重") {
        sqlKey = "select_samples_by_data_type_big";
    } else if (dataType == "小热重") {
        sqlKey = "select_samples_by_data_type_small";
    } else if (dataType == "色谱") {
        sqlKey = "select_samples_by_data_type_chrom";
    }
    
    QString sql = SqlConfigLoader::getInstance().getSqlOperation("SampleDAO", sqlKey).sql;
    if (sql.isEmpty()) {
        sql = QString(R"(
            SELECT DISTINCT s.id AS sample_id,
                   s.project_name,
                   s.batch_code,
                   s.short_code,
                   s.parallel_no,
                   s.sample_name,
                   s.data_type
            FROM %1 d
            JOIN single_tobacco_sample s ON d.sample_id = s.id
        )").arg(tableName);
    }
    
    query.prepare(sql);

    QList<QVariantMap> results;
    if (query.exec()) {
        while (query.next()) {
            QVariantMap row;
            row["sample_id"]    = query.value("sample_id");
            row["project_name"] = query.value("project_name");
            row["batch_code"]   = query.value("batch_code");
            row["short_code"]   = query.value("short_code");
            row["parallel_no"]  = query.value("parallel_no");
            row["sample_name"]  = query.value("sample_name");
            row["data_type"]    = dataType; // 确保设置正确的数据类型
            results << row;
        }
    }

    DEBUG_LOG << "SampleDAO::getSamplesByDataType" << results.size();
    return results;
}

QVariantMap SampleDAO::getSampleById(int sampleId)
{
    QVariantMap result;
    QSqlQuery query(DatabaseManager::instance().database());
    
    // 使用SqlConfigLoader获取SQL语句，如果配置中不存在则使用默认SQL
    QString sql = SqlConfigLoader::getInstance().getSqlOperation("SampleDAO", "select_sample_by_id").sql;
    if (sql.isEmpty()) {
        sql = R"(
            SELECT id, batch_id, project_name, short_code, parallel_no, 
                   sample_name, origin, grade, year, part, type, collect_date, detect_date, created_at
            FROM single_tobacco_sample 
            WHERE id = :sample_id
        )";
    }
    query.prepare(sql);
    
    query.bindValue(":sample_id", sampleId);
    
    if (query.exec() && query.next()) {
        // 基本字段填充（兼容自定义 SQL 配置，不强依赖列顺序）
        result["id"] = query.value("id");
        result["batch_id"] = query.value("batch_id");
        result["project_name"] = query.value("project_name");
        result["short_code"] = query.value("short_code");
        result["parallel_no"] = query.value("parallel_no");
        result["sample_name"] = query.value("sample_name");
        result["origin"] = query.value("origin");
        result["grade"] = query.value("grade");
        result["year"] = query.value("year");
        result["part"] = query.value("part");
        result["type"] = query.value("type");
        result["collect_date"] = query.value("collect_date");
        result["detect_date"] = query.value("detect_date");
        result["created_at"] = query.value("created_at");

        // 兜底处理：若配置 SQL 中未包含 detect_date/created_at，单独再查一次时间字段
        bool needFetchTime = false;
        if (!result.contains("detect_date") || !result.value("detect_date").isValid()) {
            needFetchTime = true;
        }
        if (!result.contains("created_at") || !result.value("created_at").isValid()) {
            needFetchTime = true;
        }

        if (needFetchTime) {
            QSqlQuery timeQuery(DatabaseManager::instance().database());
            QString timeSql = QStringLiteral(
                "SELECT detect_date, created_at "
                "FROM single_tobacco_sample WHERE id = :sample_id");
            timeQuery.prepare(timeSql);
            timeQuery.bindValue(":sample_id", sampleId);
            if (timeQuery.exec() && timeQuery.next()) {
                QVariant detectDate = timeQuery.value("detect_date");
                QVariant createdAt = timeQuery.value("created_at");
                if (detectDate.isValid()) {
                    result["detect_date"] = detectDate;
                }
                if (createdAt.isValid()) {
                    result["created_at"] = createdAt;
                }
            }
        }
    }
    
    return result;
}
