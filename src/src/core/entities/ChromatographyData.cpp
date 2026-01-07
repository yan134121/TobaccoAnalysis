// #include "ChromatographyData.h"

// ChromatographyData::ChromatographyData()
//     : m_id(-1), m_sampleId(-1), m_replicateNo(0), m_seqNo(0), m_tgValue(0.0), m_dtgValue(0.0)
// {
// }

// ChromatographyData::ChromatographyData(int id, int sampleId, int replicateNo, int seqNo, double tgValue,
//                      double dtgValue, const QString& sourceName, const QDateTime& createdAt)
//     : m_id(id), m_sampleId(sampleId), m_replicateNo(replicateNo), m_seqNo(seqNo),
//       m_tgValue(tgValue), m_dtgValue(dtgValue), m_sourceName(sourceName), m_createdAt(createdAt)
// {
// }

// int ChromatographyData::getId() const { return m_id; }
// int ChromatographyData::getSampleId() const { return m_sampleId; }
// int ChromatographyData::getReplicateNo() const { return m_replicateNo; }
// int ChromatographyData::getSeqNo() const { return m_seqNo; }
// double ChromatographyData::getTgValue() const { return m_tgValue; }
// double ChromatographyData::getDtgValue() const { return m_dtgValue; }
// QString ChromatographyData::getSourceName() const { return m_sourceName; }
// QDateTime ChromatographyData::getCreatedAt() const { return m_createdAt; }

// void ChromatographyData::setId(int newId) { m_id = newId; }
// void ChromatographyData::setSampleId(int newSampleId) { m_sampleId = newSampleId; }
// void ChromatographyData::setReplicateNo(int newReplicateNo) { m_replicateNo = newReplicateNo; }
// void ChromatographyData::setSeqNo(int newSeqNo) { m_seqNo = newSeqNo; }
// void ChromatographyData::setTgValue(double newTgValue) { m_tgValue = newTgValue; }
// void ChromatographyData::setDtgValue(double newDtgValue) { m_dtgValue = newDtgValue; }
// void setSourceName(const QString& newSourceName) { m_sourceName = newSourceName; }
// void ChromatographyData::setCreatedAt(const QDateTime& newCreatedAt) { m_createdAt = newCreatedAt; }

// src/entity/ChromatographyData.cpp
#include "ChromatographyData.h"

ChromatographyData::ChromatographyData()
    : m_id(-1), m_sampleId(-1), m_replicateNo(0), m_retentionTime(0.0), m_responseValue(0.0) // <-- 初始化新成员
{
}

ChromatographyData::ChromatographyData(int id, int sampleId, int replicateNo, double retentionTime,
                                       double responseValue, const QString& sourceName, const QDateTime& createdAt)
    : m_id(id), m_sampleId(sampleId), m_replicateNo(replicateNo),
      m_retentionTime(retentionTime), m_responseValue(responseValue), // <-- 初始化新成员
      m_sourceName(sourceName), m_createdAt(createdAt)
{
}

int ChromatographyData::getId() const { return m_id; }
int ChromatographyData::getSampleId() const { return m_sampleId; }
int ChromatographyData::getReplicateNo() const { return m_replicateNo; }
double ChromatographyData::getRetentionTime() const { return m_retentionTime; }     // <-- 实现 Getter
double ChromatographyData::getResponseValue() const { return m_responseValue; }     // <-- 实现 Getter
QString ChromatographyData::getSourceName() const { return m_sourceName; }
QDateTime ChromatographyData::getCreatedAt() const { return m_createdAt; }

void ChromatographyData::setId(int newId) { m_id = newId; }
void ChromatographyData::setSampleId(int newSampleId) { m_sampleId = newSampleId; }
void ChromatographyData::setReplicateNo(int newReplicateNo) { m_replicateNo = newReplicateNo; }
void ChromatographyData::setRetentionTime(double newRetentionTime) { m_retentionTime = newRetentionTime; } // <-- 实现 Setter
void ChromatographyData::setResponseValue(double newResponseValue) { m_responseValue = newResponseValue; } // <-- 实现 Setter
void ChromatographyData::setSourceName(const QString& newSourceName) { m_sourceName = newSourceName; }
void ChromatographyData::setCreatedAt(const QDateTime& newCreatedAt) { m_createdAt = newCreatedAt; }