#include "TgBigData.h"

TgBigData::TgBigData()
    : m_id(-1), m_sampleId(-1), m_serialNo(0), m_temperature(0), m_tgValue(0.0), m_dtgValue(0.0)
{
}

// TgBigData::TgBigData(int id, int sampleId, int replicateNo, int seqNo, double tgValue,
//                      double dtgValue, const QString& sourceName, const QDateTime& createdAt)
//     : m_id(id), m_sampleId(sampleId), m_replicateNo(replicateNo), m_seqNo(seqNo),
//       m_tgValue(tgValue), m_dtgValue(dtgValue), m_sourceName(sourceName), m_createdAt(createdAt)
// {
// }

TgBigData::TgBigData(int id, int sampleId, int serialNo, double temperature, double weight,
                         double tgValue, double dtgValue, const QString& sourceName, const QDateTime& createdAt)
    : m_id(id),
    m_sampleId(sampleId),
    m_serialNo(serialNo),
    m_temperature(temperature),
    m_weight(weight),
    m_tgValue(tgValue),
    m_dtgValue(dtgValue),
    m_sourceName(sourceName),
    m_createdAt(createdAt)
{
}


int TgBigData::getId() const { return m_id; }
// int TgBigData::getSampleId() const { return m_sampleId; }
// int TgBigData::getReplicateNo() const { return m_replicateNo; }
// int TgBigData::getSeqNo() const { return m_seqNo; }
// double TgBigData::getTgValue() const { return m_tgValue; }
// double TgBigData::getDtgValue() const { return m_dtgValue; }
// QString TgBigData::getSourceName() const { return m_sourceName; }
int TgBigData::getSampleId() const { return m_sampleId; }
int TgBigData::getSerialNo() const { return m_serialNo; }
double TgBigData::getTemperature() const { return m_temperature; }
double TgBigData::getWeight() const { return m_weight; }
double TgBigData::getTgValue() const { return m_tgValue; }
double TgBigData::getDtgValue() const { return m_dtgValue; }
QString TgBigData::getSourceName() const { return m_sourceName; }
QDateTime TgBigData::getCreatedAt() const { return m_createdAt; }

void TgBigData::setId(int newId) { m_id = newId; }
// void TgBigData::setSampleId(int newSampleId) { m_sampleId = newSampleId; }
// void TgBigData::setReplicateNo(int newReplicateNo) { m_replicateNo = newReplicateNo; }
// void TgBigData::setSeqNo(int newSeqNo) { m_seqNo = newSeqNo; }
// void TgBigData::setTgValue(double newTgValue) { m_tgValue = newTgValue; }
// void TgBigData::setDtgValue(double newDtgValue) { m_dtgValue = newDtgValue; }
// // void setSourceName(const QString& newSourceName) { m_sourceName = newSourceName; }
// void TgBigData::setSourceName(const QString& newSourceName) { m_sourceName = newSourceName; } // <-- 修改为这样
void TgBigData::setSampleId(int newSampleId) { m_sampleId = newSampleId; }
void TgBigData::setSerialNo(int newSerialNo) { m_serialNo = newSerialNo; }
void TgBigData::setTemperature(double newTemperature) { m_temperature = newTemperature; }
void TgBigData::setWeight(double newWeight) { m_weight = newWeight; }
void TgBigData::setTgValue(double newTgValue) { m_tgValue = newTgValue; }
void TgBigData::setDtgValue(double newDtgValue) { m_dtgValue = newDtgValue; }
void TgBigData::setSourceName(const QString& newSourceName) { m_sourceName = newSourceName; }
void TgBigData::setCreatedAt(const QDateTime& newCreatedAt) { m_createdAt = newCreatedAt; }
