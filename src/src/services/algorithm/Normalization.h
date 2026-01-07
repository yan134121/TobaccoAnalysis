#ifndef NORMALIZATION_H
#define NORMALIZATION_H

#include "services/algorithm/processing/IProcessingStep.h"

class Normalization : public IProcessingStep
{
public:
    QString stepName() const override;
    QString userVisibleName() const override;
    QVariantMap defaultParameters() const override;
    ProcessingResult process(const QList<Curve*>& inputCurves, 
                             const QVariantMap& params, 
                             QString& error) override;
};
#endif // NORMALIZATION_H
