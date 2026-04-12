#ifndef WEIGHTEDCURVESUMDIALOG_H
#define WEIGHTEDCURVESUMDIALOG_H

#include <QDialog>
#include <QList>
#include <QString>
#include <QPair>
#include <QVector>

class WeightedCurveSumDialog : public QDialog
{
    Q_OBJECT
public:
    // items: (displayName, defaultWeight)
    // 返回 true 表示用户确认；weights 与 items 顺序一致
    static bool pickWeights(QWidget* parent,
                            const QList<QPair<QString, double>>& items,
                            QVector<double>& weights);
};

#endif // WEIGHTEDCURVESUMDIALOG_H
