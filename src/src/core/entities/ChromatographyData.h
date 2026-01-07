// #ifndef CHROMATOGRAPHYDATA_H
// #define CHROMATOGRAPHYDATA_H

// #include <QObject>
// #include <QString>
// #include <QDateTime>

// class ChromatographyData
// {
// public:
//     ChromatographyData();
//     ChromatographyData(int id, int sampleId, int replicateNo, int seqNo, double tgValue,
//               double dtgValue, const QString& sourceName, const QDateTime& createdAt);

//     // Getters
//     int getId() const;
//     int getSampleId() const;
//     int getReplicateNo() const;
//     int getSeqNo() const;
//     double getTgValue() const;
//     double getDtgValue() const;
//     QString getSourceName() const;
//     QDateTime getCreatedAt() const;

//     // Setters
//     void setId(int newId);
//     void setSampleId(int newSampleId);
//     void setReplicateNo(int newReplicateNo);
//     void setSeqNo(int newSeqNo);
//     void setTgValue(double newTgValue);
//     void setDtgValue(double newDtgValue);
//     void setSourceName(const QString& newSourceName);
//     void setCreatedAt(const QDateTime& newCreatedAt);

// private:
//     int m_id;
//     int m_sampleId;
//     int m_replicateNo;
//     int m_seqNo;
//     double m_tgValue;
//     double m_dtgValue;
//     QString m_sourceName;
//     QDateTime m_createdAt;
// };

// #endif // CHROMATOGRAPHYDATA_H

// src/entity/ChromatographyData.h
#ifndef CHROMATOGRAPHYDATA_H
#define CHROMATOGRAPHYDATA_H

#include <QObject>
#include <QString>
#include <QDateTime>

class ChromatographyData
{
public:
    ChromatographyData();
    ChromatographyData(int id, int sampleId, int replicateNo, double retentionTime,
                       double responseValue, const QString& sourceName, const QDateTime& createdAt);

    // Getters
    int getId() const;
    int getSampleId() const;
    int getReplicateNo() const;
    double getRetentionTime() const;   // <-- 新增
    double getResponseValue() const;   // <-- 新增
    QString getSourceName() const;
    QDateTime getCreatedAt() const;

    // Setters
    void setId(int newId);
    void setSampleId(int newSampleId);
    void setReplicateNo(int newReplicateNo);
    void setRetentionTime(double newRetentionTime); // <-- 新增
    void setResponseValue(double newResponseValue); // <-- 新增
    void setSourceName(const QString& newSourceName);
    void setCreatedAt(const QDateTime& newCreatedAt);

private:
    int m_id;
    int m_sampleId;
    int m_replicateNo;
    double m_retentionTime;   // <-- 成员变量
    double m_responseValue;   // <-- 成员变量
    QString m_sourceName;
    QDateTime m_createdAt;
};

#endif // CHROMATOGRAPHYDATA_H