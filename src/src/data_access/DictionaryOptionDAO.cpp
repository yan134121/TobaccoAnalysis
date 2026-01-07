#include "DictionaryOptionDAO.h"
#include "DatabaseConnector.h"
#include "core/sql/SqlConfigLoader.h"
#include "Logger.h"
#include <QSqlError>
#include <QDebug>
#include <QVariant>

DictionaryOption DictionaryOptionDAO::createOptionFromQuery(QSqlQuery& query) {
    DictionaryOption opt;
    opt.setId(query.value("id").toInt());
    opt.setCategory(query.value("category").toString());
    opt.setValue(query.value("value").toString());
    opt.setDescription(query.value("description").toString());
    opt.setCreatedAt(query.value("created_at").toDateTime());
    return opt;
}

bool DictionaryOptionDAO::insert(DictionaryOption& option) {
    QSqlDatabase db = DatabaseConnector::getInstance().getDatabase();
    if (!db.isOpen()) { WARNING_LOG << "Database not open for DictionaryOption insert."; return false; }
    QSqlQuery query(db);
    
    // 从SqlConfigLoader获取SQL语句，如果不存在则使用默认SQL
    QString sql = SqlConfigLoader::getInstance().getSqlOperation("DictionaryOptionDAO", "insert").sql;
    if (sql.isEmpty()) {
        sql = "INSERT INTO dictionary_option (category, value, description) VALUES (:category, :value, :description)";
        DEBUG_LOG << "Using default SQL for DictionaryOptionDAO.insert";
    }
    
    query.prepare(sql);
    query.bindValue(":category", option.getCategory());
    query.bindValue(":value", option.getValue());
    query.bindValue(":description", option.getDescription());
    if (query.exec()) {
        option.setId(query.lastInsertId().toInt());
        DEBUG_LOG << "Inserted DictionaryOption ID:" << option.getId();
        return true;
    } else { WARNING_LOG << "Insert DictionaryOption failed:" << query.lastError().text(); return false; }
}

bool DictionaryOptionDAO::update(const DictionaryOption& option) {
    if (option.getId() == -1) { WARNING_LOG << "Cannot update DictionaryOption with invalid ID."; return false; }
    QSqlDatabase db = DatabaseConnector::getInstance().getDatabase();
    if (!db.isOpen()) { WARNING_LOG << "Database not open for DictionaryOption update."; return false; }
    QSqlQuery query(db);
    
    // 从SqlConfigLoader获取SQL语句，如果不存在则使用默认SQL
    QString sql = SqlConfigLoader::getInstance().getSqlOperation("DictionaryOptionDAO", "update").sql;
    if (sql.isEmpty()) {
        sql = "UPDATE dictionary_option SET category = :category, value = :value, description = :description WHERE id = :id";
        DEBUG_LOG << "Using default SQL for DictionaryOptionDAO.update";
    }
    
    query.prepare(sql);
    query.bindValue(":category", option.getCategory());
    query.bindValue(":value", option.getValue());
    query.bindValue(":description", option.getDescription());
    query.bindValue(":id", option.getId());
    if (query.exec()) {
        DEBUG_LOG << "Updated DictionaryOption ID:" << option.getId();
        return query.numRowsAffected() > 0;
    } else { WARNING_LOG << "Update DictionaryOption failed:" << query.lastError().text(); return false; }
}

bool DictionaryOptionDAO::remove(int id) {
    if (id == -1) { WARNING_LOG << "Cannot remove DictionaryOption with invalid ID."; return false; }
    QSqlDatabase db = DatabaseConnector::getInstance().getDatabase();
    if (!db.isOpen()) { WARNING_LOG << "Database not open for DictionaryOption remove."; return false; }
    QSqlQuery query(db);
    
    // 从SqlConfigLoader获取SQL语句，如果不存在则使用默认SQL
    QString sql = SqlConfigLoader::getInstance().getSqlOperation("DictionaryOptionDAO", "remove").sql;
    if (sql.isEmpty()) {
        sql = "DELETE FROM dictionary_option WHERE id = :id";
        DEBUG_LOG << "Using default SQL for DictionaryOptionDAO.remove";
    }
    
    query.prepare(sql);
    query.bindValue(":id", id);
    if (query.exec()) {
        DEBUG_LOG << "Removed DictionaryOption ID:" << id;
        return query.numRowsAffected() > 0;
    } else { WARNING_LOG << "Remove DictionaryOption failed:" << query.lastError().text(); return false; }
}

DictionaryOption DictionaryOptionDAO::getById(int id) {
    QSqlDatabase db = DatabaseConnector::getInstance().getDatabase();
    if (!db.isOpen()) { WARNING_LOG << "Database not open for DictionaryOption getById."; return DictionaryOption(); }
    QSqlQuery query(db);
    
    // 从SqlConfigLoader获取SQL语句，如果不存在则使用默认SQL
    QString sql = SqlConfigLoader::getInstance().getSqlOperation("DictionaryOptionDAO", "getById").sql;
    if (sql.isEmpty()) {
        sql = "SELECT * FROM dictionary_option WHERE id = :id";
        DEBUG_LOG << "Using default SQL for DictionaryOptionDAO.getById";
    }
    
    query.prepare(sql);
    query.bindValue(":id", id);
    if (query.exec() && query.next()) {
        return createOptionFromQuery(query);
    } else {
        WARNING_LOG << "Get DictionaryOption by ID failed:" << query.lastError().text();
        return DictionaryOption();
    }
}

QList<DictionaryOption> DictionaryOptionDAO::getAll() {
    QSqlDatabase db = DatabaseConnector::getInstance().getDatabase();
    QList<DictionaryOption> list;
    if (!db.isOpen()) { WARNING_LOG << "Database not open for DictionaryOption getAll."; return list; }
    
    // 从SqlConfigLoader获取SQL语句，如果不存在则使用默认SQL
    QString sql = SqlConfigLoader::getInstance().getSqlOperation("DictionaryOptionDAO", "getAll").sql;
    if (sql.isEmpty()) {
        sql = "SELECT * FROM dictionary_option ORDER BY category, value";
        DEBUG_LOG << "Using default SQL for DictionaryOptionDAO.getAll";
    }
    
    QSqlQuery query(db);
    query.prepare(sql);
    if (query.exec()) {
        while (query.next()) {
            list.append(createOptionFromQuery(query));
        }
    } else {
        WARNING_LOG << "Get all DictionaryOption failed:" << query.lastError().text();
    }
    return list;
}

QList<DictionaryOption> DictionaryOptionDAO::getByCategory(const QString& category) {
    QSqlDatabase db = DatabaseConnector::getInstance().getDatabase();
    QList<DictionaryOption> list;
    if (!db.isOpen()) { WARNING_LOG << "Database not open for DictionaryOption getByCategory."; return list; }
    
    // 从SqlConfigLoader获取SQL语句，如果不存在则使用默认SQL
    QString sql = SqlConfigLoader::getInstance().getSqlOperation("DictionaryOptionDAO", "getByCategory").sql;
    if (sql.isEmpty()) {
        sql = "SELECT * FROM dictionary_option WHERE category = :category ORDER BY value";
        DEBUG_LOG << "Using default SQL for DictionaryOptionDAO.getByCategory";
    }
    
    QSqlQuery query(db);
    query.prepare(sql);
    query.bindValue(":category", category);
    if (query.exec()) {
        while (query.next()) {
            list.append(createOptionFromQuery(query));
        }
    } else {
        WARNING_LOG << "Get DictionaryOption by category failed:" << query.lastError().text();
    }
    return list;
}

QStringList DictionaryOptionDAO::getValuesByCategory(const QString& category) {
    // DEBUG_LOG << "getValuesByCategory:" << category;
    QSqlDatabase db = DatabaseConnector::getInstance().getDatabase();
    QStringList list;
    if (!db.isOpen()) { WARNING_LOG << "Database not open for DictionaryOption getValuesByCategory."; return list; }
    
    // 从SqlConfigLoader获取SQL语句，如果不存在则使用默认SQL
    QString sql = SqlConfigLoader::getInstance().getSqlOperation("DictionaryOptionDAO", "getValuesByCategory").sql;
    if (sql.isEmpty()) {
        sql = "SELECT value FROM dictionary_option WHERE category = :category ORDER BY value";
        DEBUG_LOG << "Using default SQL for DictionaryOptionDAO.getValuesByCategory";
    }
    
    QSqlQuery query(db);
    query.prepare(sql);
    query.bindValue(":category", category);
    if (query.exec()) {
        while (query.next()) {
            list.append(query.value(0).toString());
        }
    } else {
        WARNING_LOG << "Get values by category failed:" << query.lastError().text();
    }
    return list;
}

// QStringList DictionaryOptionDAO::getValuesByCategory(const QString& category) {
//     DEBUG_LOG << "getValuesByCategory:" << category;

//     QSqlDatabase db = DatabaseConnector::getInstance().getDatabase();
//     DEBUG_LOG << "Connection name:" << db.connectionName()
//          << "Driver:" << db.driverName()
//          << "isValid:" << db.isValid()
//          << "isOpen:" << db.isOpen();

//     QStringList list;
// DEBUG_LOG << "Database connector is valid.";
//     if (!db.isValid()) {
//         WARNING_LOG << "Invalid database connection!";
//         return list;
//     }
// DEBUG_LOG << "Database connection is valid.";
//     if (!db.isOpen()) {
//         if (!db.open()) {
//             WARNING_LOG << "Failed to open database:" << db.lastError().text();
//             return list;
//         }
//     }
// DEBUG_LOG << "Database is open.";
//     QSqlQuery query(db);
//     query.prepare("SELECT value FROM dictionary_option WHERE category = :category ORDER BY value");
//     query.bindValue(":category", category);
// DEBUG_LOG << "Query prepared.";
//     if (query.exec()) {
//         while (query.next()) {
//             list.append(query.value(0).toString());
//         }
//     } else {
//         WARNING_LOG << "SQL execution failed:" << query.lastError().text();
//     }
// DEBUG_LOG << "Query executed.";
//     return list;
// }


bool DictionaryOptionDAO::exists(const QString& category, const QString& value) {
    QSqlDatabase db = DatabaseConnector::getInstance().getDatabase();
    if (!db.isOpen()) { WARNING_LOG << "Database not open for DictionaryOption exists."; return false; }
    
    // 从SqlConfigLoader获取SQL语句，如果不存在则使用默认SQL
    QString sql = SqlConfigLoader::getInstance().getSqlOperation("DictionaryOptionDAO", "exists").sql;
    if (sql.isEmpty()) {
        sql = "SELECT COUNT(*) FROM dictionary_option WHERE category = :category AND value = :value";
        DEBUG_LOG << "Using default SQL for DictionaryOptionDAO.exists";
    }
    
    QSqlQuery query(db);
    query.prepare(sql);
    query.bindValue(":category", category);
    query.bindValue(":value", value);
    if (query.exec() && query.next()) {
        return query.value(0).toInt() > 0;
    } else {
        WARNING_LOG << "Check DictionaryOption exists failed:" << query.lastError().text();
        return false;
    }
}
