#ifndef DICTIONARYOPTION_H
#define DICTIONARYOPTION_H

#include <QString>
#include <QDateTime>

class DictionaryOption
{
public:
    DictionaryOption();
    DictionaryOption(int id, const QString& category, const QString& value, const QString& description, const QDateTime& createdAt);

    // Getters
    int getId() const;
    QString getCategory() const;
    QString getValue() const;
    QString getDescription() const;
    QDateTime getCreatedAt() const;

    // Setters
    void setId(int newId);
    void setCategory(const QString& newCategory);
    void setValue(const QString& newValue);
    void setDescription(const QString& newDescription);
    void setCreatedAt(const QDateTime& newCreatedAt);

private:
    int m_id;
    QString m_category;
    QString m_value;
    QString m_description;
    QDateTime m_createdAt;
};

#endif // DICTIONARYOPTION_H