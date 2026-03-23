#ifndef TWOCURVEPICKDIALOG_H
#define TWOCURVEPICKDIALOG_H

#include <QDialog>
#include <QPair>
#include <QString>
#include <QList>

class TwoCurvePickDialog : public QDialog
{
    Q_OBJECT
public:
    // items: (sampleId, displayName)，按当前可见曲线顺序传入
    // 成功返回 true，并写入 id1、id2（互不相同）
    static bool pickTwo(QWidget* parent, const QList<QPair<int, QString>>& items, int& id1, int& id2);
};

#endif
