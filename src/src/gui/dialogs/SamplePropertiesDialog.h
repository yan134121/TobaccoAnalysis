#ifndef SAMPLEPROPERTIESDIALOG_H
#define SAMPLEPROPERTIESDIALOG_H

#include <QDialog>
#include <QLabel>
#include "data_access/NavigatorDAO.h"
#include "core/common.h"  // 添加NavigatorNodeInfo的完整定义

class SamplePropertiesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SamplePropertiesDialog(QWidget *parent = nullptr);
    ~SamplePropertiesDialog();

    // 设置样本信息
    void setSampleInfo(const struct NavigatorNodeInfo& info);

private:
    QLabel* m_projectNameLabel;
    QLabel* m_batchCodeLabel;
    QLabel* m_shortCodeLabel;
    QLabel* m_parallelNoLabel;
    QLabel* m_dataTypeLabel;
    QLabel* m_sampleID;
};

#endif // SAMPLEPROPERTIESDIALOG_H