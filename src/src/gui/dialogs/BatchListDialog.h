#ifndef BATCHLISTDIALOG_H
#define BATCHLISTDIALOG_H

#include <QDialog>
#include <QStandardItemModel>

class QLineEdit;  // 前置声明搜索框
class QComboBox;  // 前置声明下拉框
class QCompleter; // 前置声明自动补全器

namespace Ui { class BatchListDialog; }

class BatchListDialog : public QDialog {
    Q_OBJECT
public:
    explicit BatchListDialog(QWidget* parent = nullptr);
    ~BatchListDialog();

private slots:
    void onModelChanged(int index);
    // 批次类型切换时，按当前选择刷新批次列表
    void onBatchTypeChanged(int index);
    // 批次下拉变化时，定位列表行
    void on_batchComboBox_currentTextChanged(const QString& batchCode);
    // 搜索框回车时，模糊匹配并定位
    void on_searchEdit_returnPressed();
    // 新增批次占位
    void on_addButton_clicked();
    // 编辑批次占位
    void on_editButton_clicked();
    // 删除批次执行级联删除
    void on_deleteButton_clicked();
    // 查询批次占位
    void on_searchButton_clicked();

private:
    void loadModels();
    void loadBatches(int modelId);
    // 构建批次索引与自动补全
    void buildBatchIndex();
    // 定位指定批次代码所在行并选中
    void selectRowByBatchCode(const QString& batchCode);

private:
    Ui::BatchListDialog* ui;
    QStandardItemModel* m_batchModel;
    QList<QPair<QString,int>> m_models;
    // 批次索引与缓存
    QStringList m_batchCodes;                 // 当前型号+类型下的批次代码列表
    QHash<QString, int> m_codeToRow;          // 批次代码 -> 行号
    QCompleter* m_batchCompleter = nullptr;   // 批次下拉自动补全
    QCompleter* m_searchCompleter = nullptr;  // 搜索框自动补全（包含匹配）
};

#endif // BATCHLISTDIALOG_H
