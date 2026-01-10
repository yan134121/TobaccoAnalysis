#include "ModelListDialog.h"
#include "ui_modellistdialog.h"
#include "data_access/NavigatorDAO.h"
#include <QHeaderView>
#include <QMessageBox>
#include <QInputDialog>

ModelListDialog::ModelListDialog(QWidget* parent)
    : QDialog(parent), ui(new Ui::ModelListDialog), m_tableModel(new QStandardItemModel(this))
{
    ui->setupUi(this);
    ui->tableView->setModel(m_tableModel);
    m_tableModel->setHorizontalHeaderLabels({"型号代码", "型号ID", "批次数", "样本数"});
    ui->tableView->horizontalHeader()->setStretchLastSection(true);
    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    loadModels();
}

ModelListDialog::~ModelListDialog() { delete ui; }

void ModelListDialog::loadModels()
{
    NavigatorDAO dao;
    QString error;
    auto models = dao.fetchAllModels(error);
    m_tableModel->setRowCount(0);
    for (const auto& pair : models) {
        QString errSample;
        int sampleCount = dao.countSamplesForModel(pair.second, errSample);
        if (!errSample.isEmpty()) {
            sampleCount = 0; // 计数失败时显示0
        }
        QString errBatch;
        int batchCount = dao.countBatchesForModel(pair.second, errBatch);
        if (!errBatch.isEmpty()) {
            batchCount = 0;
        }
        QList<QStandardItem*> row;
        row << new QStandardItem(pair.first);
        row << new QStandardItem(QString::number(pair.second));
        row << new QStandardItem(QString::number(batchCount));
        row << new QStandardItem(QString::number(sampleCount));
        m_tableModel->appendRow(row);
    }
}

void ModelListDialog::on_addButton_clicked()
{
    // 新增型号功能占位，后续可打开专用对话框录入
    QMessageBox::information(this, "提示", "新增功能暂未实现");
}

void ModelListDialog::on_editButton_clicked()
{
    // 编辑型号功能占位，后续可打开专用对话框编辑
    QMessageBox::information(this, "提示", "编辑功能暂未实现");
}

void ModelListDialog::on_deleteButton_clicked()
{
    // 获取当前选择的型号
    auto sel = ui->tableView->selectionModel()->selectedRows();
    if (sel.isEmpty()) {
        QMessageBox::warning(this, "提示", "请先选择要删除的型号");
        return;
    }
    int row = sel.first().row();
    QString modelCode = m_tableModel->item(row, 0)->text(); // 第一列为型号代码

    // 按烟牌号（样本project_name）删除，弹窗让用户确认并输入/确认烟牌号
    bool okInput = false;
    QString brandName = QInputDialog::getText(
        this,
        "按烟牌号删除",
        QString("请输入要删除的烟牌号（项目名）：\n（建议与样本的 project_name 完全一致）\n\n默认值：%1").arg(modelCode),
        QLineEdit::Normal,
        modelCode,
        &okInput
    );
    if (!okInput || brandName.trimmed().isEmpty()) {
        QMessageBox::information(this, "提示", "已取消按烟牌号删除操作");
        return;
    }

    // 二次确认，提示会级联删除批次和样本数据
    QString text = QString("将按烟牌号（项目名） [%1] 删除其下所有批次与样本数据。\n"
                           "包含：大热重、小热重、小热重（原始数据）、色谱数据。\n"
                           "操作不可撤销，是否继续？").arg(brandName);
    auto ret = QMessageBox::question(this, "级联删除确认", text, QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ret != QMessageBox::Yes) return;

    // 调用级联删除（样本数据分支）
    NavigatorDAO dao;
    QString error;
    bool ok = dao.deleteProjectCascade(brandName, /*processBranch*/ false, error);
    if (!ok) {
        QMessageBox::critical(this, "删除失败", "数据库错误或未找到匹配数据：" + error);
        return;
    }

    // 删除成功后刷新列表
    loadModels();
    QMessageBox::information(this, "完成", "删除成功");
}

void ModelListDialog::on_searchButton_clicked()
{
    // 查询型号功能占位，后续可实现筛选
    QMessageBox::information(this, "提示", "查询功能暂未实现");
}
