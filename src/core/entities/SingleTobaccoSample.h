#ifndef SINGLETOBACCOSAMPLE_H
#define SINGLETOBACCOSAMPLE_H

#include <QObject>
#include <QString>
#include <QDate>

class SingleTobaccoSample : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int id READ id WRITE setId)
    Q_PROPERTY(QString projectName READ projectName WRITE setProjectName)
    Q_PROPERTY(QString batchCode READ batchCode WRITE setBatchCode)
    Q_PROPERTY(QString shortCode READ shortCode WRITE setShortCode)
    Q_PROPERTY(int parallelNo READ parallelNo WRITE setParallelNo)
    Q_PROPERTY(QString sampleName READ sampleName WRITE setSampleName)
    Q_PROPERTY(QString origin READ origin WRITE setOrigin)
    Q_PROPERTY(QString grade READ grade WRITE setGrade)

public:
    explicit SingleTobaccoSample(QObject* parent = nullptr) : QObject(parent) {}
    
    // Getters
    int id() const { return m_id; }
    QString projectName() const { return m_projectName; }
    QString batchCode() const { return m_batchCode; }
    QString shortCode() const { return m_shortCode; }
    int parallelNo() const { return m_parallelNo; }
    QString sampleName() const { return m_sampleName; }
    QString origin() const { return m_origin; }
    QString grade() const { return m_grade; }
    
    // Setters
    void setId(int id) { m_id = id; }
    void setProjectName(const QString &name) { m_projectName = name; }
    void setBatchCode(const QString &code) { m_batchCode = code; }
    void setShortCode(const QString &code) { m_shortCode = code; }
    void setParallelNo(int no) { m_parallelNo = no; }
    void setSampleName(const QString &name) { m_sampleName = name; }
    void setOrigin(const QString &origin) { m_origin = origin; }
    void setGrade(const QString &grade) { m_grade = grade; }

private:
    int m_id;
    QString m_projectName;
    QString m_batchCode;
    QString m_shortCode;
    int m_parallelNo;
    QString m_sampleName;
    QString m_origin;
    QString m_grade;
};

#endif // SINGLETOBACCOSAMPLE_H