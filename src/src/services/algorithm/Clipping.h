#ifndef CLIPPING_H
#define CLIPPING_H

#include "services/algorithm/processing/IProcessingStep.h"

class Clipping : public IProcessingStep
{
public:
    QString stepName() const override;
    QString userVisibleName() const override;
    QVariantMap defaultParameters() const override;
    ProcessingResult process(const QList<Curve*>& inputCurves, 
                             const QVariantMap& params, 
                             QString& error) override;
};

#endif // CLIPPING_H
