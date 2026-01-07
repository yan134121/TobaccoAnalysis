#ifndef PROCESSTGBIGDATA_H
#define PROCESSTGBIGDATA_H

#include <QObject>
#include <QString>
#include <QDateTime>

class ProcessTgBigData
{
public:
    ProcessTgBigData();
    ProcessTgBigData(int id, int sampleId, int serialNo, double temperature, double weight,
              double tgValue, double dtgValue, const QString& sourceName, const QDateTime& createdAt);

    // Getters
    int getId() const;
    int getSampleId() const;
    int getSerialNo() const;
    double getTemperature() const;
    double getWeight() const;
    double getTgValue() const;
    double getDtgValue() const;
    QString getSourceName() const;
    QDateTime getCreatedAt() const;

    // Setters
    void setId(int newId);
    void setSampleId(int id);
    void setSerialNo(int no);
    void setTemperature(double temp);
    void setWeight(double w);
    void setTgValue(double val);
    void setDtgValue(double val);
    void setSourceName(const QString &name);
    void setCreatedAt(const QDateTime& newCreatedAt);

private:
    int m_id;
    int m_sampleId;        // sample_id
    int m_serialNo;        // serial_no
    double m_temperature;  // temperature
    double m_weight;       // weight
    double m_tgValue;      // tg_value
    double m_dtgValue;     // dtg_value
    QString m_sourceName;  // source_filename
    QDateTime m_createdAt;
};

#endif // PROCESSTGBIGDATA_H