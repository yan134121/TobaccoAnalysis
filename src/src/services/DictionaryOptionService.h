#ifndef DICTIONARYOPTIONSERVICE_H
#define DICTIONARYOPTIONSERVICE_H

#include <QObject>
#include <QStringList>
#include "DictionaryOptionDAO.h" // 引入 DAO

class DictionaryOptionService : public QObject
{
    Q_OBJECT
public:
    explicit DictionaryOptionService(DictionaryOptionDAO* dao, QObject *parent = nullptr);
    ~DictionaryOptionService() override = default;

    QStringList getOptionsByCategory(const QString& category);
    // bool addOption(const QString& category, const QString& value, const QString& description = QString(), QString& errorMessage);
    bool addOption(const QString& category, const QString& value, QString& errorMessage, const QString& description = QString()); 
    bool removeOption(int id, QString& errorMessage);
    // ... 其他管理方法

    DictionaryOptionDAO* getDAO() const { return m_dao; }
private:
    DictionaryOptionDAO* m_dao;
};

#endif // DICTIONARYOPTIONSERVICE_H
