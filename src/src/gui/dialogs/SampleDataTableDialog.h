#ifndef SAMPLEDATATABLEDIALOG_H
#define SAMPLEDATATABLEDIALOG_H

#include <QWidget>
#include <QTableWidget>
#include "data_access/NavigatorDAO.h"
#include "core/common.h"  // 添加NavigatorNodeInfo的完整定义

class SampleDataTableDialog : public QWidget
{
    Q_OBJECT

public:
    explicit SampleDataTableDialog(QWidget *parent = nullptr);
    ~SampleDataTableDialog();

    // 加载样本数据
    void loadSampleData(const struct NavigatorNodeInfo& info);

private:
    QTableWidget* m_tableWidget;
    NavigatorDAO m_dao;
};

#endif // SAMPLEDATATABLEDIALOG_H