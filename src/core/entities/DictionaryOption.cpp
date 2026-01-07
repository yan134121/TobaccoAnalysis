#include "DictionaryOption.h"

DictionaryOption::DictionaryOption()
    : m_id(-1)
{
}

DictionaryOption::DictionaryOption(int id, const QString& category, const QString& value, const QString& description, const QDateTime& createdAt)
    : m_id(id),
      m_category(category),
      m_value(value),
      m_description(description),
      m_createdAt(createdAt)
{
}

int DictionaryOption::getId() const { return m_id; }
QString DictionaryOption::getCategory() const { return m_category; }
QString DictionaryOption::getValue() const { return m_value; }
QString DictionaryOption::getDescription() const { return m_description; }
QDateTime DictionaryOption::getCreatedAt() const { return m_createdAt; }

void DictionaryOption::setId(int newId) { m_id = newId; }
void DictionaryOption::setCategory(const QString& newCategory) { m_category = newCategory; }
void DictionaryOption::setValue(const QString& newValue) { m_value = newValue; }
void DictionaryOption::setDescription(const QString& newDescription) { m_description = newDescription; }
void DictionaryOption::setCreatedAt(const QDateTime& newCreatedAt) { m_createdAt = newCreatedAt; }