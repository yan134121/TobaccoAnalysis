#include "SqlConfigLoader.h"
#include <QJsonArray>
#include <QJsonValue>
#include <QTextStream>
#include <QDir>
#include <QCoreApplication>
#include "Logger.h"

SqlConfigLoader* SqlConfigLoader::instance = nullptr;

SqlConfigLoader::SqlConfigLoader()
    : isLoaded(false), configType(JSON_CONFIG)
{
}

SqlConfigLoader& SqlConfigLoader::getInstance()
{
    if (instance == nullptr) {
        instance = new SqlConfigLoader();
    }
    return *instance;
}

SqlConfigLoader::~SqlConfigLoader()
{
    clearConfig();
}

bool SqlConfigLoader::loadConfig(const QString& filePath, ConfigType type)
{
    configFilePath = filePath;
    configType = type;
    
    // 清空之前的配置
    clearConfig();
    
    // 检查文件是否存在
    QFile file(filePath);
    if (!file.exists()) {
        WARNING_LOG << "SQL配置文件不存在:" << filePath;
        return false;
    }
    
    bool success = false;
    switch (type) {
        case JSON_CONFIG:
            success = loadFromJson(filePath);
            break;
        case XML_CONFIG:
            success = loadFromXml(filePath);
            break;
        default:
            WARNING_LOG << "不支持的配置文件类型";
            return false;
    }
    
    if (success) {
        isLoaded = true;
        DEBUG_LOG << "SQL配置文件加载成功:" << filePath;
    } else {
        WARNING_LOG << "SQL配置文件加载失败:" << filePath;
    }
    
    return success;
}

bool SqlConfigLoader::loadFromJson(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        WARNING_LOG << "无法打开JSON配置文件:" << filePath;
        return false;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    
    if (error.error != QJsonParseError::NoError) {
        WARNING_LOG << "JSON解析错误:" << error.errorString();
        return false;
    }
    
    QJsonObject root = doc.object();
    
    // 解析数据库架构
    if (root.contains("database_schema")) {
        QJsonObject dbSchema = root["database_schema"].toObject();
        if (dbSchema.contains("create_tables")) {
            parseJsonCreateTables(dbSchema["create_tables"].toObject());
        }
    }
    
    // 解析DAO操作
    if (root.contains("dao_operations")) {
        parseJsonDaoOperations(root["dao_operations"].toObject());
    }
    
    return true;
}

bool SqlConfigLoader::loadFromXml(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        WARNING_LOG << "无法打开XML配置文件:" << filePath;
        return false;
    }
    
    QDomDocument doc;
    QString errorMsg;
    int errorLine, errorColumn;
    
    if (!doc.setContent(&file, &errorMsg, &errorLine, &errorColumn)) {
        WARNING_LOG << "XML解析错误:" << errorMsg << "行:" << errorLine << "列:" << errorColumn;
        file.close();
        return false;
    }
    
    file.close();
    
    QDomElement root = doc.documentElement();
    if (root.tagName() != "sql_configuration") {
        WARNING_LOG << "XML根元素不正确，期望sql_configuration";
        return false;
    }
    
    // 解析数据库架构
    QDomElement dbSchema = root.firstChildElement("database_schema");
    if (!dbSchema.isNull()) {
        QDomElement createTables = dbSchema.firstChildElement("create_tables");
        if (!createTables.isNull()) {
            parseXmlCreateTables(createTables);
        }
    }
    
    // 解析DAO操作
    QDomElement daoOperations = root.firstChildElement("dao_operations");
    if (!daoOperations.isNull()) {
        parseXmlDaoOperations(daoOperations);
    }
    
    return true;
}

void SqlConfigLoader::parseJsonCreateTables(const QJsonObject& createTablesJson)
{
    for (auto it = createTablesJson.begin(); it != createTablesJson.end(); ++it) {
        QString tableName = it.key();
        QString sql = it.value().toString();
        createTableSqls[tableName] = sql;
    }
}

void SqlConfigLoader::parseJsonDaoOperations(const QJsonObject& daoOperationsJson)
{
    for (auto daoIt = daoOperationsJson.begin(); daoIt != daoOperationsJson.end(); ++daoIt) {
        QString daoName = daoIt.key();
        QJsonObject daoObj = daoIt.value().toObject();
        
        QMap<QString, SqlOperation> operations;
        
        for (auto opIt = daoObj.begin(); opIt != daoObj.end(); ++opIt) {
            QString operationName = opIt.key();
            QJsonObject opObj = opIt.value().toObject();
            
            SqlOperation operation;
            operation.sql = opObj["sql"].toString();
            operation.description = opObj["description"].toString();
            
            // 解析参数列表
            if (opObj.contains("parameters")) {
                QJsonArray paramsArray = opObj["parameters"].toArray();
                for (const QJsonValue& param : paramsArray) {
                    operation.parameters.append(param.toString());
                }
            }
            
            operations[operationName] = operation;
        }
        
        daoOperations[daoName] = operations;
    }
}

void SqlConfigLoader::parseXmlCreateTables(const QDomElement& createTablesElement)
{
    QDomNodeList tableNodes = createTablesElement.elementsByTagName("table");
    
    for (int i = 0; i < tableNodes.count(); ++i) {
        QDomElement tableElement = tableNodes.at(i).toElement();
        QString tableName = tableElement.attribute("name");
        QString sql = tableElement.firstChildElement("sql").text();
        
        if (!tableName.isEmpty() && !sql.isEmpty()) {
            createTableSqls[tableName] = sql;
        }
    }
}

void SqlConfigLoader::parseXmlDaoOperations(const QDomElement& daoOperationsElement)
{
    QDomNodeList daoNodes = daoOperationsElement.elementsByTagName("dao");
    
    for (int i = 0; i < daoNodes.count(); ++i) {
        QDomElement daoElement = daoNodes.at(i).toElement();
        QString daoName = daoElement.attribute("name");
        
        QMap<QString, SqlOperation> operations;
        QDomNodeList operationNodes = daoElement.elementsByTagName("operation");
        
        for (int j = 0; j < operationNodes.count(); ++j) {
            QDomElement opElement = operationNodes.at(j).toElement();
            QString operationName = opElement.attribute("name");
            
            SqlOperation operation;
            operation.sql = opElement.firstChildElement("sql").text();
            operation.description = opElement.firstChildElement("description").text();
            
            // 解析参数列表
            QDomElement parametersElement = opElement.firstChildElement("parameters");
            if (!parametersElement.isNull()) {
                QDomNodeList paramNodes = parametersElement.elementsByTagName("parameter");
                for (int k = 0; k < paramNodes.count(); ++k) {
                    operation.parameters.append(paramNodes.at(k).toElement().text());
                }
            }
            
            operations[operationName] = operation;
        }
        
        daoOperations[daoName] = operations;
    }
}

SqlConfigLoader::SqlOperation SqlConfigLoader::getSqlOperation(const QString& daoName, const QString& operationName) const
{
    if (!isLoaded) {
        WARNING_LOG << "SQL配置尚未加载";
        return SqlOperation();
    }
    
    if (!daoOperations.contains(daoName)) {
        WARNING_LOG << "未找到DAO:" << daoName;
        return SqlOperation();
    }
    
    const QMap<QString, SqlOperation>& operations = daoOperations[daoName];
    if (!operations.contains(operationName)) {
        WARNING_LOG << "未找到操作:" << daoName << "::" << operationName;
        return SqlOperation();
    }
    
    return operations[operationName];
}

QString SqlConfigLoader::getCreateTableSql(const QString& tableName) const
{
    if (!isLoaded) {
        WARNING_LOG << "SQL配置尚未加载";
        return QString();
    }
    
    if (!createTableSqls.contains(tableName)) {
        WARNING_LOG << "未找到建表SQL:" << tableName;
        return QString();
    }
    
    return createTableSqls[tableName];
}

QStringList SqlConfigLoader::getDaoNames() const
{
    return daoOperations.keys();
}

QStringList SqlConfigLoader::getOperationNames(const QString& daoName) const
{
    if (!daoOperations.contains(daoName)) {
        return QStringList();
    }
    
    return daoOperations[daoName].keys();
}

QStringList SqlConfigLoader::getTableNames() const
{
    return createTableSqls.keys();
}

bool SqlConfigLoader::reloadConfig()
{
    if (configFilePath.isEmpty()) {
        WARNING_LOG << "没有配置文件路径可以重新加载";
        return false;
    }
    
    return loadConfig(configFilePath, configType);
}

void SqlConfigLoader::clearConfig()
{
    daoOperations.clear();
    createTableSqls.clear();
    isLoaded = false;
}