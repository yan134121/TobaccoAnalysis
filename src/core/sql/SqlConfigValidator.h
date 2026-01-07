#ifndef SQLCONFIGVALIDATOR_H
#define SQLCONFIGVALIDATOR_H

#include <QString>
#include <QStringList>

class SqlConfigValidator
{
public:
    static bool validateConfig();
    static bool validateDaoOperations();
    static bool validateCreateTableSqls();
    static QStringList getMissingOperations();
    static QStringList getMissingTables();
    
private:
    static QStringList getRequiredOperations();
    static QStringList getRequiredTables();
};

#endif // SQLCONFIGVALIDATOR_H