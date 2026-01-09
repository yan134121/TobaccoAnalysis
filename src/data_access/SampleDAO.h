// 跨表查询、批量数据处理、图表数据获取、分析报表类查询
#ifndef SAMPLEDATAACCESS_H
#define SAMPLEDATAACCESS_H

#include <QList>
#include <QString>
#include <QVector>
#include <QPointF>
#include <QVariantMap>
#include "common.h"

class SingleTobaccoSample;

class SampleDAO
{
public:
    SampleDAO();

    // 根据批次ID, short_code, 获取所有平行样的样本信息
    QList<SingleTobaccoSample*> fetchParallelSamplesInfo(const QString &projectName, const QString &batchCode, 
     const QString &shortCode, const int &parallelNo, QString &error);

    // 根据样本ID(sample_id)和数据类型，获取图表数据
    QVector<QPointF> fetchChartDataForSample(int sampleId, const DataType dataType, QString& error);
    // 获取小热重原始数据（serial_no, weight）
    QVector<QPointF> fetchSmallRawWeightData(int sampleId, QString& error);
    // 获取小热重原始DTG数据（temperature, dtg_value）
    QVector<QPointF> fetchSmallRawDtgData(int sampleId, QString& error);

    QList<QVariantMap> getSamplesByDataType(const QString &dataType);
    
    // 根据样本ID获取样本信息
    QVariantMap getSampleById(int sampleId);
};

#endif // SAMPLEDATAACCESS_H
