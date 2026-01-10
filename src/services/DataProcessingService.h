#ifndef DATAPROCESSINGSERVICE_H
#define DATAPROCESSINGSERVICE_H

#include <QObject>
#include "core/common.h"
#include "gui/dialogs/TgBigParameterSettingsDialog.h" // 包含 ProcessingParameters

// 前置声明
class IProcessingStep;
class Curve;
class AppInitializer; // 前置声明 AppInitializer，便于在服务层使用其 Getter


// Q_DECLARE_METATYPE(BatchMultiStageData)

class DataProcessingService : public QObject
{
    Q_OBJECT
public:
    explicit DataProcessingService(QObject *parent = nullptr);
    ~DataProcessingService();

public slots: 
    BatchGroupData runTgBigPipelineForMultiple(const QList<int>& sampleIds, const ProcessingParameters& params);

    BatchGroupData runTgSmallPipelineForMultiple(const QList<int> &sampleIds, const ProcessingParameters &params);
    BatchGroupData runTgSmallRawPipelineForMultiple(const QList<int> &sampleIds, const ProcessingParameters &params);
    BatchGroupData runChromatographPipelineForMultiple(const QList<int> &sampleIds, const ProcessingParameters &params);
    BatchGroupData runProcessTgBigPipelineForMultiple(const QList<int> &sampleIds, const ProcessingParameters &params);

    // 这个方法将在后台线程中被调用
    SampleDataFlexible runTgBigPipeline(int sampleId, const ProcessingParameters& params);
    SampleDataFlexible runTgSmallPipeline(int sampleId, const ProcessingParameters &params);
    SampleDataFlexible runTgSmallRawPipeline(int sampleId, const ProcessingParameters &params);
    SampleDataFlexible runChromatographPipeline(int sampleId, const ProcessingParameters& params);
    SampleDataFlexible runProcessTgBigPipeline(int sampleId, const ProcessingParameters& params);

private:
    void registerSteps();
    SampleDataFlexible runTgBigLikePipeline(DataType dataType, int sampleId, const ProcessingParameters& params);
    QMap<QString, IProcessingStep*> m_registeredSteps;
    AppInitializer* m_appInitializer = nullptr; // 
};

#endif // DATAPROCESSINGSERVICE_H
