
// 建议文件名: src/entity/SingleTobaccoSampleData.h
#ifndef SINGLETOBACCOSAMPLEDATA_H 
#define SINGLETOBACCOSAMPLEDATA_H

#include <QString>
#include <QDate>
#include <QDateTime>
#include <QJsonDocument>

#include "common.h"

// 修改: 类名更新
class SingleTobaccoSampleData
{
public:
    // 构造函数
    SingleTobaccoSampleData();
    // 带有所有字段的构造函数
    SingleTobaccoSampleData(int id, int batchId, const QString& projectName, const QString& batchCode,
                          const QString& shortCode, int parallelNo, const QString& sampleName, const QString& note,
                          int year, const QString& origin, const QString& part, const QString& grade,
                          const QString& type, const QDate& collectDate, const QDate& detectDate,
                          const QJsonDocument& dataJson, const QDateTime& createdAt);

    // --- Getters ---
    int getId() const;
    // 新增 Getters
    int getBatchId() const;
    QString getProjectName() const;
    QString getBatchCode() const;
    QString getShortCode() const; 
    int getParallelNo() const;
    QString getSampleName() const;
    QString getNote() const;
    // 保留的 Getters
    int getYear() const;
    QString getOrigin() const;
    QString getPart() const;
    QString getGrade() const;
    QString getType() const;
    QDate getCollectDate() const;
    QDate getDetectDate() const;
    QJsonDocument getDataJson() const;
    QDateTime getCreatedAt() const;

    // --- Setters ---
    void setId(int newId);
    // 新增 Setters
    void setBatchId(int newBatchId);
    void setProjectName(const QString& newProjectName);
    void setBatchCode(const QString& newBatchCode);
    void setShortCode(const QString& newShortCode); 
    void setParallelNo(int newParallelNo);
    void setSampleName(const QString& newSampleName);
    void setNote(const QString& newNote);
    // 保留的 Setters
    void setYear(int newYear);
    void setOrigin(const QString& newOrigin);
    void setPart(const QString& newPart);
    void setGrade(const QString& newGrade);
    void setType(const QString& newType);
    void setCollectDate(const QDate& newCollectDate);
    void setDetectDate(const QDate& newDetectDate);
    void setDataJson(const QJsonDocument& newDataJson);
    void setCreatedAt(const QDateTime& newCreatedAt);

    BatchType getBatchType() const { return m_batchType; }
    void setBatchType(BatchType type) { m_batchType = type; }


private:
    int m_id;
    // 新增字段
    int m_batchId;
    QString m_projectName;
    QString m_batchCode;
    QString m_shortCode; 
    int m_parallelNo;
    QString m_sampleName;
    QString m_note;
    // 保留字段
    int m_year;
    QString m_origin;
    QString m_part;
    QString m_grade;
    QString m_type;
    QDate m_collectDate;
    QDate m_detectDate;
    QJsonDocument m_dataJson;
    QDateTime m_createdAt;

    BatchType m_batchType = BatchType::NORMAL;
};

#endif // SINGLETOBACCOSAMPLEDATA_H
