#ifndef TGSMALLDATA_H
#define TGSMALLDATA_H

#include <QObject>
#include <QString>
#include <QDateTime>

class TgSmallData
{
public:
    TgSmallData();
    // TgSmallData(int id, int sampleId, int replicateNo, int temperature, double tgValue,
    //           double dtgValue, const QString& sourceName, const QDateTime& createdAt);
    TgSmallData(int id, int sampleId, int serialNo, double temperature, double weight,
                             double tgValue, double dtgValue, const QString& sourceName, const QDateTime& createdAt);

    // // Getters
    int getId() const;
    // int getSampleId() const;
    // int getReplicateNo() const;
    // double getTemperature() const; 
    // double getTgValue() const;
    // double getDtgValue() const;
    // QString getSourceName() const;
    QDateTime getCreatedAt() const;

    // // Setters
    void setId(int newId);
    // void setSampleId(int newSampleId);
    // void setReplicateNo(int newReplicateNo);
    // void setTemperature(double newTemperature);
    // void setTgValue(double newTgValue);
    // void setDtgValue(double newDtgValue);
    // void setSourceName(const QString& newSourceName);
    void setCreatedAt(const QDateTime& newCreatedAt);

    // int getSampleId() const;
    // // int getSerialNo() const;
    // // double getTemperature() const;
    // // double getWeight() const;
    // // double getTgValue() const;
    // // double getDTgValue() const;
    // QString getSourceName() const;
    int getSampleId() const;
    int getSerialNo() const;
    double getTemperature() const;
    double getWeight() const;
    double getTgValue() const;
    double getDtgValue() const;
    QString getSourceName() const;

    // void setSampleId(int id);
    // void setSerialNo(int no);
    // void setTemperature(double temp);
    // void setWeight(double w);
    // void setTgValue(double val);
    // void setDTgValue(double val);
    // void setSourceName(const QString &name);
    void setSampleId(int newSampleId);
    void setSerialNo(int newSerialNo);
    void setTemperature(double newTemperature);
    void setWeight(double newWeight);
    void setTgValue(double newTgValue);
    void setDtgValue(double newDtgValue);
    void setSourceName(const QString& newSourceName);

private:
    int m_id;
    // int m_sampleId;
    // int m_replicateNo;
    // double m_temperature;   // <-- 确保有这个成员变量声明
    // double m_tgValue;
    // double m_dtgValue;
    // QString m_sourceName;
    int m_sampleId;        // sample_id
    int m_serialNo;        // serial_no
    double m_temperature;  // temperature
    double m_weight;       // weight
    double m_tgValue;      // tg_value
    double m_dtgValue;     // dtg_value
    QString m_sourceName;  // source_filename
    QDateTime m_createdAt;
};

#endif // TGSMALLDATA_H
