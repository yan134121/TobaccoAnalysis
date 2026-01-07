#ifndef DATABASEINITIALIZER_H
#define DATABASEINITIALIZER_H

#include <QString>

class DatabaseInitializer
{
public:
    static bool initializeDatabase();
    static bool createTables();
    static bool insertInitialData();
    
private:
    static bool executeCreateTableSql(const QString& tableName);
};

#endif // DATABASEINITIALIZER_H