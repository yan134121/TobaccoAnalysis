#ifndef SAMPLEPROPERTIESDIALOG_H
#define SAMPLEPROPERTIESDIALOG_H

#include <QDialog>
#include <QLabel>
#include "data_access/NavigatorDAO.h"
#include "core/common.h"

class SamplePropertiesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SamplePropertiesDialog(QWidget *parent = nullptr);
    ~SamplePropertiesDialog();

    void setSampleInfo(const struct NavigatorNodeInfo& info);

private:
    // 基础样本信息
    QLabel* m_projectNameLabel;
    QLabel* m_batchCodeLabel;
    QLabel* m_shortCodeLabel;
    QLabel* m_parallelNoLabel;
    QLabel* m_dataTypeLabel;
    QLabel* m_sampleID;

    // 导入属性字段
    QLabel* m_codeLabel;
    QLabel* m_yearLabel;
    QLabel* m_originLabel;
    QLabel* m_partLabel;
    QLabel* m_gradeLabel;
    QLabel* m_stemLeafModeLabel;
    QLabel* m_detectDateLabel;
    QLabel* m_factoryLabel;

    // 从数据库中查询该样本的 import_attributes
    void loadImportAttributes(int sampleId, const QString& dataType);
};

#endif // SAMPLEPROPERTIESDIALOG_H
