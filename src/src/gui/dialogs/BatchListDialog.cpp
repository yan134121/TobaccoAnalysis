#include "BatchListDialog.h"
#include "ui_batchlistdialog.h"
#include "data_access/NavigatorDAO.h"
#include <QHeaderView>
#include <QMessageBox>
#include <QCompleter>
#include <QStringListModel>
#include <QItemSelectionModel>

BatchListDialog::BatchListDialog(QWidget* parent)
    : QDialog(parent), ui(new Ui::BatchListDialog), m_batchModel(new QStandardItemModel(this))
{
    ui->setupUi(this);
    ui->batchTableView->setModel(m_batchModel);
    m_batchModel->setHorizontalHeaderLabels({"批次代码", "批次ID", "样本数"});
    ui->batchTableView->horizontalHeader()->setStretchLastSection(true);
    ui->batchTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->batchTableView->setSelectionMode(QAbstractItemView::SingleSelection);
    // 为型号下拉设置可编辑与自动补全（前缀匹配）
    ui->modelComboBox->setEditable(true);
    connect(ui->modelComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onModelChanged(int)));
    // 批次类型切换信号连接，切换后按当前型号刷新
    connect(ui->batchTypeComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onBatchTypeChanged(int)));
    // 批次下拉与搜索框联动
    connect(ui->batchComboBox, &QComboBox::currentTextChanged, this, &BatchListDialog::on_batchComboBox_currentTextChanged);
    connect(ui->searchEdit, &QLineEdit::returnPressed, this, &BatchListDialog::on_searchEdit_returnPressed);
    loadModels();
}

BatchListDialog::~BatchListDialog() { delete ui; }

void BatchListDialog::loadModels()
{
    NavigatorDAO dao;
    QString error;
    m_models = dao.fetchAllModels(error);
    ui->modelComboBox->clear();
    for (const auto& m : m_models) {
        ui->modelComboBox->addItem(m.first, m.second);
    }
    // 型号自动补全
    QStringList modelCodes;
    for (const auto& m : m_models) modelCodes << m.first;
    auto* modelCompleter = new QCompleter(modelCodes, ui->modelComboBox);
    modelCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    ui->modelComboBox->setCompleter(modelCompleter);

    if (!m_models.isEmpty()) {
        loadBatches(m_models.first().second);
    }
}

void BatchListDialog::onModelChanged(int index)
{
    if (index < 0 || index >= m_models.size()) return;
    int modelId = m_models[index].second;
    loadBatches(modelId);
}

void BatchListDialog::onBatchTypeChanged(int index)
{
    Q_UNUSED(index);
    // 批次类型切换后，按当前型号刷新批次列表
    int currentModelIndex = ui->modelComboBox->currentIndex();
    if (currentModelIndex < 0 || currentModelIndex >= m_models.size()) return;
    loadBatches(m_models[currentModelIndex].second);
}

void BatchListDialog::loadBatches(int modelId)
{
    NavigatorDAO dao;
    QString error;
    // 根据批次类型选择筛选 NORMAL/PROCESS
    QString batchType = (ui->batchTypeComboBox->currentText() == "正常批次") ? "NORMAL" : "PROCESS";
    auto batches = dao.fetchBatchesForModel(modelId, error, batchType);
    m_batchModel->setRowCount(0);
    for (const auto& b : batches) {
        QString errCount;
        int sampleCount = dao.countSamplesForBatch(b.second, errCount);
        if (!errCount.isEmpty()) sampleCount = 0;
        QList<QStandardItem*> row;
        row << new QStandardItem(b.first);
        row << new QStandardItem(QString::number(b.second));
        row << new QStandardItem(QString::number(sampleCount));
        m_batchModel->appendRow(row);
    }
    // 重建批次索引与自动补全
    buildBatchIndex();
}

void BatchListDialog::on_addButton_clicked()
{
    // 新增批次功能占位
    QMessageBox::information(this, "提示", "新增功能暂未实现");
}

void BatchListDialog::on_editButton_clicked()
{
    // 编辑批次功能占位
    QMessageBox::information(this, "提示", "编辑功能暂未实现");
}

void BatchListDialog::on_deleteButton_clicked()
{
    // 获取当前选择的批次与型号
    auto sel = ui->batchTableView->selectionModel()->selectedRows();
    if (sel.isEmpty()) {
        QMessageBox::warning(this, "提示", "请先选择要删除的批次");
        return;
    }
    int row = sel.first().row();
    QString batchCode = m_batchModel->item(row, 0)->text(); // 第一列为批次代码
    QString modelCode = ui->modelComboBox->currentText();    // 下拉框显示为型号代码

    // 二次确认，提示会级联删除其下所有样本及数据
    QString text = QString("将删除型号 [%1] 下批次 [%2] 的所有样本与数据。\n"
                           "包含：大热重、小热重、色谱数据。\n"
                           "操作不可撤销，是否继续？").arg(modelCode, batchCode);
    auto ret = QMessageBox::question(this, "级联删除确认", text, QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ret != QMessageBox::Yes) return;

    // 调用级联删除（样本数据分支）
    NavigatorDAO dao;
    QString error;
    bool ok = dao.deleteBatchCascade(modelCode, batchCode, /*processBranch*/ false, error);
    if (!ok) {
        QMessageBox::critical(this, "删除失败", "数据库错误：" + error);
        return;
    }

    // 删除成功后刷新列表
    int currentModelIndex = ui->modelComboBox->currentIndex();
    if (currentModelIndex < 0 || currentModelIndex >= m_models.size()) return;
    loadBatches(m_models[currentModelIndex].second);
}

// 构建批次索引与自动补全
void BatchListDialog::buildBatchIndex()
{
    m_batchCodes.clear();
    m_codeToRow.clear();
    int rows = m_batchModel->rowCount();
    m_batchCodes.reserve(rows);
    for (int r = 0; r < rows; ++r) {
        QString code = m_batchModel->item(r, 0)->text();
        m_batchCodes << code;
        m_codeToRow.insert(code, r);
    }
    m_batchCodes.sort();
    // 填充批次下拉与自动补全
    ui->batchComboBox->blockSignals(true);
    ui->batchComboBox->clear();
    ui->batchComboBox->addItems(m_batchCodes);
    ui->batchComboBox->setEditable(true);
    if (!m_batchCompleter) {
        m_batchCompleter = new QCompleter(this);
        m_batchCompleter->setCaseSensitivity(Qt::CaseInsensitive);
        ui->batchComboBox->setCompleter(m_batchCompleter);
    }
    auto* completerModel = new QStringListModel(m_batchCodes, m_batchCompleter);
    m_batchCompleter->setModel(completerModel);
    ui->batchComboBox->blockSignals(false);
    // 搜索自动补全（包含匹配）
    if (!m_searchCompleter) {
        m_searchCompleter = new QCompleter(this);
        m_searchCompleter->setCaseSensitivity(Qt::CaseInsensitive);
        m_searchCompleter->setFilterMode(Qt::MatchContains);
        ui->searchEdit->setCompleter(m_searchCompleter);
    }
    auto* searchModel = new QStringListModel(m_batchCodes, m_searchCompleter);
    m_searchCompleter->setModel(searchModel);
}

// 定位指定批次代码所在行并选中
void BatchListDialog::selectRowByBatchCode(const QString& batchCode)
{
    if (!m_codeToRow.contains(batchCode)) return;
    int row = m_codeToRow.value(batchCode);
    QAbstractItemModel* model = ui->batchTableView->model();
    if (!model) return;
    QModelIndex idx = model->index(row, 0);
    ui->batchTableView->selectionModel()->setCurrentIndex(idx, QItemSelectionModel::Select | QItemSelectionModel::Rows);
    ui->batchTableView->scrollTo(idx, QAbstractItemView::PositionAtCenter);
}

// 批次下拉变化时，定位该批次
void BatchListDialog::on_batchComboBox_currentTextChanged(const QString& batchCode)
{
    if (batchCode.isEmpty()) return;
    selectRowByBatchCode(batchCode);
}

// 搜索框回车，模糊匹配第一条并定位
void BatchListDialog::on_searchEdit_returnPressed()
{
    QString text = ui->searchEdit->text().trimmed();
    if (text.isEmpty()) return;
    // 优先按完整批次代码匹配
    if (m_codeToRow.contains(text)) {
        selectRowByBatchCode(text);
        return;
    }
    // 包含匹配，找到第一条包含关键字的批次代码
    for (const QString& code : m_batchCodes) {
        if (code.contains(text, Qt::CaseInsensitive)) {
            selectRowByBatchCode(code);
            return;
        }
    }
}

void BatchListDialog::on_searchButton_clicked()
{
    // 查询批次功能占位
    QMessageBox::information(this, "提示", "查询功能暂未实现");
}
