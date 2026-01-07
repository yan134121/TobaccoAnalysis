#ifndef MODELLISTDIALOG_H
#define MODELLISTDIALOG_H

#include <QDialog>
#include <QStandardItemModel>

namespace Ui { class ModelListDialog; }

class ModelListDialog : public QDialog {
    Q_OBJECT
public:
    explicit ModelListDialog(QWidget* parent = nullptr);
    ~ModelListDialog();

private slots:
    // 新增按钮点击，预留后续实现
    void on_addButton_clicked();
    // 编辑按钮点击，预留后续实现
    void on_editButton_clicked();
    // 删除按钮点击，执行级联删除并二次确认
    void on_deleteButton_clicked();
    // 查询按钮点击，预留后续实现
    void on_searchButton_clicked();

private:
    void loadModels();

private:
    Ui::ModelListDialog* ui;
    QStandardItemModel* m_tableModel;
};

#endif // MODELLISTDIALOG_H
