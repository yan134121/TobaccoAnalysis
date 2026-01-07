#include "SqlConfigValidator.h"
#include "SqlConfigLoader.h"
#include <QDebug>
#include "Logger.h"

bool SqlConfigValidator::validateConfig()
{
    INFO_LOG << "开始验证SQL配置...";
    
    bool valid = true;
    
    if (!validateDaoOperations()) {
        FATAL_LOG << "DAO操作验证失败";
        valid = false;
    }
    
    if (!validateCreateTableSqls()) {
        FATAL_LOG << "建表SQL验证失败";
        valid = false;
    }
    
    if (valid) {
        INFO_LOG << "SQL配置验证通过";
    } else {
        FATAL_LOG << "SQL配置验证失败";
    }
    
    return valid;
}

bool SqlConfigValidator::validateDaoOperations()
{
    QStringList missing = getMissingOperations();
    if (!missing.isEmpty()) {
        FATAL_LOG << "缺少必需的DAO操作:" << missing;
        return false;
    }
    return true;
}

bool SqlConfigValidator::validateCreateTableSqls()
{
    QStringList missing = getMissingTables();
    if (!missing.isEmpty()) {
        FATAL_LOG << "缺少必需的建表SQL:" << missing;
        return false;
    }
    return true;
}

QStringList SqlConfigValidator::getMissingOperations()
{
    SqlConfigLoader& loader = SqlConfigLoader::getInstance();
    QStringList required = getRequiredOperations();
    QStringList missing;
    
    for (const QString& operation : required) {
        QStringList parts = operation.split("::");
        if (parts.size() == 2) {
            QString daoName = parts[0];
            QString opName = parts[1];
            
            if (loader.getSqlOperation(daoName, opName).sql.isEmpty()) {
                missing.append(operation);
            }
        }
    }
    
    return missing;
}

QStringList SqlConfigValidator::getMissingTables()
{
    SqlConfigLoader& loader = SqlConfigLoader::getInstance();
    QStringList required = getRequiredTables();
    QStringList missing;
    
    for (const QString& tableName : required) {
        if (loader.getCreateTableSql(tableName).isEmpty()) {
            missing.append(tableName);
        }
    }
    
    return missing;
}

QStringList SqlConfigValidator::getRequiredOperations()
{
    return {
        "TgSmallDataDAO::insert",
        "TgSmallDataDAO::insert_batch",
        "TgSmallDataDAO::select_by_sample_id",
        "TgSmallDataDAO::delete_by_sample_id",
        "TgBigDataDAO::insert",
        "TgBigDataDAO::insert_batch",
        "TgBigDataDAO::select_by_sample_id",
        "TgBigDataDAO::delete_by_sample_id",
        "ChromatographyDataDAO::insert",
        "ChromatographyDataDAO::insert_batch",
        "ChromatographyDataDAO::select_by_sample_id",
        "ChromatographyDataDAO::delete_by_sample_id"
        // 添加更多必需的操作
    };
}

QStringList SqlConfigValidator::getRequiredTables()
{
    return {
        "tobacco_model",
        "single_tobacco_sample",
        "tg_big_data",
        "tg_small_data",
        "chromatography_data",
        "algo_results",
        "dictionary_option",
        "leaf_group",
        "fitted_leaf_group",
        "user"
    };
}
