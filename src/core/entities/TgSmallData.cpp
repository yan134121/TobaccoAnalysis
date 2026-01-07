#include "TgSmallData.h"

TgSmallData::TgSmallData()
    : m_id(-1), m_sampleId(-1), m_serialNo(0), m_temperature(0), m_tgValue(0.0), m_dtgValue(0.0)
{
}

// TgSmallData::TgSmallData(int id, int sampleId, int replicateNo, int seqNo, double tgValue,
//                      double dtgValue, const QString& sourceName, const QDateTime& createdAt)
//     : m_id(id), m_sampleId(sampleId), m_replicateNo(replicateNo), m_temperature(seqNo),
//       m_tgValue(tgValue), m_dtgValue(dtgValue), m_sourceName(sourceName), m_createdAt(createdAt)
// {
// }

TgSmallData::TgSmallData(int id, int sampleId, int serialNo, double temperature, double weight,
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

int TgSmallData::getId() const { return m_id; }
// int TgSmallData::getSampleId() const { return m_sampleId; }
// int TgSmallData::getReplicateNo() const { return m_replicateNo; }
// double TgSmallData::getTemperature() const { return m_temperature; }
// double TgSmallData::getTgValue() const { return m_tgValue; }
// double TgSmallData::getDtgValue() const { return m_dtgValue; }
// QString TgSmallData::getSourceName() const { return m_sourceName; }
int TgSmallData::getSampleId() const { return m_sampleId; }
int TgSmallData::getSerialNo() const { return m_serialNo; }
double TgSmallData::getTemperature() const { return m_temperature; }
double TgSmallData::getWeight() const { return m_weight; }
double TgSmallData::getTgValue() const { return m_tgValue; }
double TgSmallData::getDtgValue() const { return m_dtgValue; }
QString TgSmallData::getSourceName() const { return m_sourceName; }
QDateTime TgSmallData::getCreatedAt() const { return m_createdAt; }

void TgSmallData::setId(int newId) { m_id = newId; }
// void TgSmallData::setSampleId(int newSampleId) { m_sampleId = newSampleId; }
// void TgSmallData::setReplicateNo(int newReplicateNo) { m_replicateNo = newReplicateNo; }
// void TgSmallData::setTemperature(double newTemperature) { m_temperature = newTemperature; }
// void TgSmallData::setTgValue(double newTgValue) { m_tgValue = newTgValue; }
// void TgSmallData::setDtgValue(double newDtgValue) { m_dtgValue = newDtgValue; }
// void TgSmallData::setSourceName(const QString& newSourceName) { m_sourceName = newSourceName; }
void TgSmallData::setSampleId(int newSampleId) { m_sampleId = newSampleId; }
void TgSmallData::setSerialNo(int newSerialNo) { m_serialNo = newSerialNo; }
void TgSmallData::setTemperature(double newTemperature) { m_temperature = newTemperature; }
void TgSmallData::setWeight(double newWeight) { m_weight = newWeight; }
void TgSmallData::setTgValue(double newTgValue) { m_tgValue = newTgValue; }
void TgSmallData::setDtgValue(double newDtgValue) { m_dtgValue = newDtgValue; }
void TgSmallData::setSourceName(const QString& newSourceName) { m_sourceName = newSourceName; }
void TgSmallData::setCreatedAt(const QDateTime& newCreatedAt) { m_createdAt = newCreatedAt; }
