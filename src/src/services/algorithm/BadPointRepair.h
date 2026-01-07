#ifndef BADPOINTREPAIR_H
#define BADPOINTREPAIR_H

#include "services/algorithm/processing/IProcessingStep.h"

class BadPointRepair : public IProcessingStep
{
public:
    QString stepName() const override;
    QString userVisibleName() const override;
    QVariantMap defaultParameters() const override;

    ProcessingResult process(const QList<Curve*>& inputCurves,
                             const QVariantMap& params,
                             QString& error) override;

private:
    bool isBadPoint(double v);
    bool isLocalOutlier(const QVector<double>& y, int idx, int win, double nSigma);
    void localLinearRepair(QVector<double>& y, const QVector<bool>& badMask);
};

#endif // BADPOINTREPAIR_H
