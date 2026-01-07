#include "ProcessTgBigData.h"

ProcessTgBigData::ProcessTgBigData()
    : m_id(0), m_sampleId(0), m_serialNo(0), m_temperature(0.0), m_weight(0.0),
      m_tgValue(0.0), m_dtgValue(0.0), m_sourceName("")
{
}

ProcessTgBigData::ProcessTgBigData(int id, int sampleId, int serialNo, double temperature, double weight,
                         double tgValue, double dtgValue, const QString& sourceName, const QDateTime& createdAt)
    : m_id(id), m_sampleId(sampleId), m_serialNo(serialNo), m_temperature(temperature), m_weight(weight),
      m_tgValue(tgValue), m_dtgValue(dtgValue), m_sourceName(sourceName), m_createdAt(createdAt)
{
}

int ProcessTgBigData::getId() const
{
    return m_id;
}

int ProcessTgBigData::getSampleId() const
{
    return m_sampleId;
}

int ProcessTgBigData::getSerialNo() const
{
    return m_serialNo;
}

double ProcessTgBigData::getTemperature() const
{
    return m_temperature;
}

double ProcessTgBigData::getWeight() const
{
    return m_weight;
}

double ProcessTgBigData::getTgValue() const
{
    return m_tgValue;
}

double ProcessTgBigData::getDtgValue() const
{
    return m_dtgValue;
}

QString ProcessTgBigData::getSourceName() const
{
    return m_sourceName;
}

QDateTime ProcessTgBigData::getCreatedAt() const
{
    return m_createdAt;
}

void ProcessTgBigData::setId(int newId)
{
    m_id = newId;
}

void ProcessTgBigData::setSampleId(int id)
{
    m_sampleId = id;
}

void ProcessTgBigData::setSerialNo(int no)
{
    m_serialNo = no;
}

void ProcessTgBigData::setTemperature(double temp)
{
    m_temperature = temp;
}

void ProcessTgBigData::setWeight(double w)
{
    m_weight = w;
}

void ProcessTgBigData::setTgValue(double val)
{
    m_tgValue = val;
}

void ProcessTgBigData::setDtgValue(double val)
{
    m_dtgValue = val;
}

void ProcessTgBigData::setSourceName(const QString &name)
{
    m_sourceName = name;
}

void ProcessTgBigData::setCreatedAt(const QDateTime& newCreatedAt)
{
    m_createdAt = newCreatedAt;
}