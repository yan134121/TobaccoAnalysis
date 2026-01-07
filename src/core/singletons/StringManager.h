
#ifndef STRINGMANAGER_H
#define STRINGMANAGER_H

#include <QObject>
#include <QString>
#include <QMap>
#include <QSet>
#include <QHash>

#define STR(key) StringManager::instance()->getString(key)

class StringManager : public QObject
{
    Q_OBJECT
public:
    static StringManager* instance();
    QString getString(const QString& key) const;
//     void reloadStrings();

//     // 样本集合管理（按数据类型）
//     void setSamplesForType(const QString& dataType, const QSet<int>& sampleIds);
//     void addSample(const QString& dataType, int sampleId);
//     void removeSample(const QString& dataType, int sampleId);
//     QSet<int> samplesByType(const QString& dataType) const;

// signals:
//     void stringsUpdated();
//     void samplesUpdated(const QString& dataType);

private:
    StringManager(QObject *parent = nullptr);
    void loadStrings();
    QMap<QString, QString> m_stringMap;
    // QHash<QString, QSet<int>> m_samplesByType;
};

#endif // STRINGMANAGER_H
