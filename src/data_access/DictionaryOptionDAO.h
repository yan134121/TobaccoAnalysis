#ifndef DICTIONARYOPTIONDAO_H
#define DICTIONARYOPTIONDAO_H

#include "DictionaryOption.h"
#include <QList>
#include <QStringList>
#include <QSqlQuery>

class DictionaryOptionDAO {
public:
    bool insert(DictionaryOption& option);
    bool update(const DictionaryOption& option);
    bool remove(int id);
    DictionaryOption getById(int id);
    QList<DictionaryOption> getAll();
    QList<DictionaryOption> getByCategory(const QString& category);
    QStringList getValuesByCategory(const QString& category); // <-- 关键方法
    bool exists(const QString& category, const QString& value); // <-- 关键方法
private:
    DictionaryOption createOptionFromQuery(QSqlQuery& query);
};

#endif // DICTIONARYOPTIONDAO_H
