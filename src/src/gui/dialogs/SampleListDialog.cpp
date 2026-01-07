#include "SampleListDialog.h"
#include "ui_samplelistdialog.h"
#include "core/sql/SqlConfigLoader.h"
#include "data_access/DatabaseManager.h"
#include "data_access/NavigatorDAO.h"
#include "gui/views/ChartView.h"
#include <QComboBox>
#include <QLineEdit>
#include <QCompleter>
#include <QStringListModel>
#include <QVBoxLayout>
#include <QSqlQuery>
#include <QSqlError>
#include <QHeaderView>
#include <QTableView>
#include <QItemSelectionModel>
#include <QMessageBox>

SampleListDialog::SampleListDialog(QWidget* parent, AppInitializer* appInitializer)
    : QDialog(parent), ui(new Ui::SampleListDialog),
      m_bigModel(new QStandardItemModel(this)),
      m_smallModel(new QStandardItemModel(this)),
      m_chromModel(new QStandardItemModel(this)),
      m_processBigModel(new QStandardItemModel(this)),
      m_appInitializer(appInitializer)
{
    ui->setupUi(this);
    setupModels();
    // 连接分级下拉与搜索框逻辑
    connect(ui->tabWidget, &QTabWidget::currentChanged, this, [this](int){ buildIndexForCurrentModel(); populateProjectCombo(); });
    connect(ui->projectCombo, &QComboBox::currentTextChanged, this, &SampleListDialog::on_projectCombo_currentIndexChanged);
    connect(ui->batchCombo, &QComboBox::currentTextChanged, this, &SampleListDialog::on_batchCombo_currentIndexChanged);
    connect(ui->sampleCombo, &QComboBox::currentTextChanged, this, &SampleListDialog::on_sampleCombo_currentIndexChanged);
    connect(ui->searchEdit, &QLineEdit::returnPressed, this, &SampleListDialog::on_searchEdit_returnPressed);
    
    loadBigSamples();
    loadSmallSamples();
    loadChromSamples();
    loadProcessBigSamples();
    // 初始构建索引与填充下拉
    buildIndexForCurrentModel();
    populateProjectCombo();
}

SampleListDialog::~SampleListDialog()
{
    delete ui;
}

void SampleListDialog::setupModels()
{
    // 统一设置每个表头
    QStringList headers = {"项目", "批次", "短码", "平行号", "样本名", "数据点数"};
    m_bigModel->setHorizontalHeaderLabels(headers);
    m_smallModel->setHorizontalHeaderLabels(headers);
    m_chromModel->setHorizontalHeaderLabels(headers);
    m_processBigModel->setHorizontalHeaderLabels(headers);

    ui->bigTableView->setModel(m_bigModel);
    ui->smallTableView->setModel(m_smallModel);
    ui->chromTableView->setModel(m_chromModel);
    ui->processBigTableView->setModel(m_processBigModel);
    ui->bigTableView->horizontalHeader()->setStretchLastSection(true);
    ui->smallTableView->horizontalHeader()->setStretchLastSection(true);
    ui->chromTableView->horizontalHeader()->setStretchLastSection(true);
    ui->processBigTableView->horizontalHeader()->setStretchLastSection(true);
    ui->bigTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->smallTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->chromTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->processBigTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->bigTableView->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->smallTableView->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->chromTableView->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->processBigTableView->setSelectionMode(QAbstractItemView::SingleSelection);
}

void SampleListDialog::loadBigSamples()
{
    // 加载有大热重数据的样本并统计点数
    m_bigModel->setRowCount(0);
    QSqlQuery q(DatabaseManager::instance().database());
    QString sql = SqlConfigLoader::getInstance().getSqlOperation("SampleDAO", "get_samples_by_data_type_big").sql;
    if (sql.isEmpty()) {
        sql = "SELECT s.id, s.project_name, b.batch_code, s.short_code, s.parallel_no, s.sample_name "
              "FROM single_tobacco_sample s LEFT JOIN tobacco_batch b ON s.batch_id = b.id "
              "WHERE EXISTS (SELECT 1 FROM tg_big_data tbd WHERE tbd.sample_id = s.id) "
              "ORDER BY s.project_name, b.batch_code, s.short_code, s.parallel_no";
    }
    if (!q.exec(sql)) return;
    while (q.next()) {
        int sampleId = q.value(0).toInt();
        QList<QStandardItem*> row;
        QStandardItem* projectItem = new QStandardItem(q.value(1).toString());
        projectItem->setData(sampleId, Qt::UserRole); // 在项目列的用户数据中存储样本ID
        row << projectItem;
        row << new QStandardItem(q.value(2).toString());
        row << new QStandardItem(q.value(3).toString());
        row << new QStandardItem(q.value(4).toString());
        row << new QStandardItem(q.value(5).toString());
        row << new QStandardItem(QString::number(countBigPoints(sampleId)));
        m_bigModel->appendRow(row);
    }
}

void SampleListDialog::loadSmallSamples()
{
    m_smallModel->setRowCount(0);
    QSqlQuery q(DatabaseManager::instance().database());
    QString sql = SqlConfigLoader::getInstance().getSqlOperation("SampleDAO", "get_samples_by_data_type_small").sql;
    if (sql.isEmpty()) {
        sql = "SELECT s.id, s.project_name, b.batch_code, s.short_code, s.parallel_no, s.sample_name "
              "FROM single_tobacco_sample s LEFT JOIN tobacco_batch b ON s.batch_id = b.id "
              "WHERE EXISTS (SELECT 1 FROM tg_small_data tsd WHERE tsd.sample_id = s.id) "
              "ORDER BY s.project_name, b.batch_code, s.short_code, s.parallel_no";
    }
    if (!q.exec(sql)) return;
    while (q.next()) {
        int sampleId = q.value(0).toInt();
        QList<QStandardItem*> row;
        QStandardItem* projectItem = new QStandardItem(q.value(1).toString());
        projectItem->setData(sampleId, Qt::UserRole); // 在项目列的用户数据中存储样本ID
        row << projectItem;
        row << new QStandardItem(q.value(2).toString());
        row << new QStandardItem(q.value(3).toString());
        row << new QStandardItem(q.value(4).toString());
        row << new QStandardItem(q.value(5).toString());
        row << new QStandardItem(QString::number(countSmallPoints(sampleId)));
        m_smallModel->appendRow(row);
    }
}

void SampleListDialog::loadChromSamples()
{
    m_chromModel->setRowCount(0);
    QSqlQuery q(DatabaseManager::instance().database());
    QString sql = SqlConfigLoader::getInstance().getSqlOperation("SampleDAO", "get_samples_by_data_type_chrom").sql;
    if (sql.isEmpty()) {
        sql = "SELECT s.id, s.project_name, b.batch_code, s.short_code, s.parallel_no, s.sample_name "
              "FROM single_tobacco_sample s LEFT JOIN tobacco_batch b ON s.batch_id = b.id "
              "WHERE EXISTS (SELECT 1 FROM chromatography_data cd WHERE cd.sample_id = s.id) "
              "ORDER BY s.project_name, b.batch_code, s.short_code, s.parallel_no";
    }
    if (!q.exec(sql)) return;
    while (q.next()) {
        int sampleId = q.value(0).toInt();
        QList<QStandardItem*> row;
        QStandardItem* projectItem = new QStandardItem(q.value(1).toString());
        projectItem->setData(sampleId, Qt::UserRole); // 在项目列的用户数据中存储样本ID
        row << projectItem;
        row << new QStandardItem(q.value(2).toString());
        row << new QStandardItem(q.value(3).toString());
        row << new QStandardItem(q.value(4).toString());
        row << new QStandardItem(q.value(5).toString());
        row << new QStandardItem(QString::number(countChromPoints(sampleId)));
        m_chromModel->appendRow(row);
    }
}

void SampleListDialog::loadProcessBigSamples()
{
    m_processBigModel->setRowCount(0);
    QSqlQuery q(DatabaseManager::instance().database());
    QString sql = SqlConfigLoader::getInstance().getSqlOperation("SampleDAO", "get_samples_by_data_type_process_big").sql;
    if (sql.isEmpty()) {
        sql = "SELECT s.id, s.project_name, b.batch_code, s.short_code, s.parallel_no, s.sample_name "
              "FROM single_tobacco_sample s LEFT JOIN tobacco_batch b ON s.batch_id = b.id "
              "WHERE EXISTS (SELECT 1 FROM process_tg_big_data ptbd WHERE ptbd.sample_id = s.id) "
              "ORDER BY s.project_name, b.batch_code, s.short_code, s.parallel_no";
    }
    if (!q.exec(sql)) return;
    while (q.next()) {
        int sampleId = q.value(0).toInt();
        QList<QStandardItem*> row;
        QStandardItem* projectItem = new QStandardItem(q.value(1).toString());
        projectItem->setData(sampleId, Qt::UserRole); // 在项目列的用户数据中存储样本ID
        row << projectItem;
        row << new QStandardItem(q.value(2).toString());
        row << new QStandardItem(q.value(3).toString());
        row << new QStandardItem(q.value(4).toString());
        row << new QStandardItem(q.value(5).toString());
        row << new QStandardItem(QString::number(countProcessBigPoints(sampleId)));
        m_processBigModel->appendRow(row);
    }
}

int SampleListDialog::countBigPoints(int sampleId)
{
    QSqlQuery q(DatabaseManager::instance().database());
    QString sql = SqlConfigLoader::getInstance().getSqlOperation("SampleDAO", "count_data_points_big").sql;
    if (sql.isEmpty()) {
        sql = "SELECT COUNT(*) FROM tg_big_data WHERE sample_id = :sample_id";
    }
    q.prepare(sql);
    q.bindValue(":sample_id", sampleId);
    return (q.exec() && q.next()) ? q.value(0).toInt() : 0;
}

int SampleListDialog::countSmallPoints(int sampleId)
{
    QSqlQuery q(DatabaseManager::instance().database());
    QString sql = SqlConfigLoader::getInstance().getSqlOperation("SampleDAO", "count_data_points_small").sql;
    if (sql.isEmpty()) {
        sql = "SELECT COUNT(*) FROM tg_small_data WHERE sample_id = :sample_id";
    }
    q.prepare(sql);
    q.bindValue(":sample_id", sampleId);
    return (q.exec() && q.next()) ? q.value(0).toInt() : 0;
}

int SampleListDialog::countChromPoints(int sampleId)
{
    QSqlQuery q(DatabaseManager::instance().database());
    QString sql = SqlConfigLoader::getInstance().getSqlOperation("SampleDAO", "count_data_points_chrom").sql;
    if (sql.isEmpty()) {
        sql = "SELECT COUNT(*) FROM chromatography_data WHERE sample_id = :sample_id";
    }
    q.prepare(sql);
    q.bindValue(":sample_id", sampleId);
    return (q.exec() && q.next()) ? q.value(0).toInt() : 0;
}

int SampleListDialog::countProcessBigPoints(int sampleId)
{
    QSqlQuery q(DatabaseManager::instance().database());
    QString sql = SqlConfigLoader::getInstance().getSqlOperation("SampleDAO", "count_data_points_process_big").sql;
    if (sql.isEmpty()) {
        sql = "SELECT COUNT(*) FROM process_tg_big_data WHERE sample_id = :sample_id";
    }
    q.prepare(sql);
    q.bindValue(":sample_id", sampleId);
    return (q.exec() && q.next()) ? q.value(0).toInt() : 0;
}

// 获取当前页签对应的表格视图
QTableView* SampleListDialog::currentTableView() const
{
    int idx = ui->tabWidget->currentIndex();
    if (idx == 0) return ui->bigTableView;
    if (idx == 1) return ui->smallTableView;
    if (idx == 2) return ui->chromTableView;
    return ui->processBigTableView;
}

// 获取当前页签对应的数据模型
QStandardItemModel* SampleListDialog::currentModel() const
{
    int idx = ui->tabWidget->currentIndex();
    if (idx == 0) return m_bigModel;
    if (idx == 1) return m_smallModel;
    if (idx == 2) return m_chromModel;
    return m_processBigModel;
}

// 根据当前页签返回数据类型文本
QString SampleListDialog::currentDataType() const
{
    int idx = ui->tabWidget->currentIndex();
    if (idx == 0) return QStringLiteral("大热重");
    if (idx == 1) return QStringLiteral("小热重");
    if (idx == 2) return QStringLiteral("色谱");
    return QStringLiteral("工序大热重");
}

// 当前是否为工序分支
bool SampleListDialog::currentIsProcessBranch() const
{
    return ui->tabWidget->currentIndex() == 3;
}

// 删除当前选中样本（级联删除）
void SampleListDialog::on_deleteButton_clicked()
{
    QTableView* tv = currentTableView();
    QStandardItemModel* model = currentModel();
    if (!tv || !model) return;
    QModelIndex idx = tv->currentIndex();
    if (!idx.isValid()) {
        QMessageBox::information(this, tr("提示"), tr("请先选择要删除的样本"));
        return;
    }
    int row = idx.row();
    // 样本ID存放在第0列的UserRole
    int sampleId = model->item(row, 0)->data(Qt::UserRole).toInt();
    QString sampleName = model->item(row, 4)->text();
    bool processBranch = currentIsProcessBranch();

    QString dataType = currentDataType();
    QString confirmText = tr("确定删除样本 [%1]（类型：%2）吗？该操作不可撤销。").arg(sampleName, dataType);
    if (QMessageBox::question(this, tr("确认删除"), confirmText) != QMessageBox::Yes) {
        return;
    }

    NavigatorDAO dao;
    QString error;
    bool ok = dao.deleteSampleCascade(sampleId, processBranch, error);
    if (!ok) {
        QMessageBox::warning(this, tr("删除失败"), tr("删除样本失败：%1").arg(error));
        return;
    }

    // 删除成功后刷新当前页签数据
    if (ui->tabWidget->currentIndex() == 0) {
        loadBigSamples();
    } else if (ui->tabWidget->currentIndex() == 1) {
        loadSmallSamples();
    } else if (ui->tabWidget->currentIndex() == 2) {
        loadChromSamples();
    } else {
        loadProcessBigSamples();
    }
    QMessageBox::information(this, tr("删除成功"), tr("样本已删除"));
}

// 预览当前选中样本曲线（在新界面）
void SampleListDialog::on_previewButton_clicked()
{
    QTableView* tv = currentTableView();
    QStandardItemModel* model = currentModel();
    if (!tv || !model) return;
    QModelIndex idx = tv->currentIndex();
    if (!idx.isValid()) {
        QMessageBox::information(this, tr("提示"), tr("请先选择要预览的样本"));
        return;
    }
    int row = idx.row();
    // 从表格中读取样本信息
    QString projectName = model->item(row, 0)->text();
    QString batchCode   = model->item(row, 1)->text();
    QString shortCode   = model->item(row, 2)->text();
    int parallelNo      = model->item(row, 3)->text().toInt();
    QString sampleName  = model->item(row, 4)->text();
    int sampleId        = model->item(row, 0)->data(Qt::UserRole).toInt();
    QString dataType    = currentDataType();

    // 统一使用轻量预览对话框，非模态显示一张曲线图
    QDialog* dlg = new QDialog(this);
    dlg->setWindowTitle(tr("预览 - %1/%2/%3 平行样%4 - %5")
                        .arg(projectName)
                        .arg(batchCode)
                        .arg(shortCode)
                        .arg(parallelNo)
                        .arg(dataType));
    QVBoxLayout* layout = new QVBoxLayout(dlg);
    ChartView* chart = new ChartView(dlg);
    layout->addWidget(chart);
    
    // 获取曲线数据并绘制
    NavigatorDAO dao;
    QString error;
    QVector<QPointF> points = dao.getSampleCurveData(sampleId, dataType, error);
    if (!error.isEmpty()) {
        QMessageBox::warning(this, tr("错误"), tr("获取曲线数据失败：%1").arg(error));
    } else {
        QVector<double> xData, yData;
        xData.reserve(points.size());
        yData.reserve(points.size());
        for (const auto& p : points) { xData.append(p.x()); yData.append(p.y()); }
        chart->addGraph(xData, yData, sampleName, QColor::fromHsv(QRandomGenerator::global()->bounded(360), 200, 200), sampleId);
        if (dataType == QStringLiteral("大热重") || dataType == QStringLiteral("工序大热重")) {
            chart->setLabels(tr("数据序号"), tr("质量/重量"));
        } else if (dataType == QStringLiteral("小热重")) {
            chart->setLabels(tr("温度 (°C)"), tr("热重微分值 (DTG)"));
        } else if (dataType == QStringLiteral("色谱")) {
            chart->setLabels(tr("保留时间 (min)"), tr("响应值"));
        }
        chart->replot();
    }
    dlg->show(); // 非模态展示
}

// 构建当前页签的索引缓存（项目->批次->样本），并准备搜索路径
void SampleListDialog::buildIndexForCurrentModel()
{
    m_indexItems.clear();
    m_projToBatches.clear();
    m_projBatchToSamples.clear();
    m_pathToRow.clear();
    QString dataType = currentDataType();
    QStandardItemModel* model = currentModel();
    if (!model) return;
    int rows = model->rowCount();
    m_indexItems.reserve(rows);
    for (int r = 0; r < rows; ++r) {
        QString project = model->item(r, 0)->text();
        QString batch   = model->item(r, 1)->text();
        QString shortCode = model->item(r, 2)->text();
        int parallelNo    = model->item(r, 3)->text().toInt();
        int sampleId      = model->item(r, 0)->data(Qt::UserRole).toInt();
        SampleIndexItem item{sampleId, project, batch, shortCode, parallelNo, r};
        m_indexItems.append(item);
        m_projToBatches[project].insert(batch);
        QString key = project + "||" + batch;
        m_projBatchToSamples[key].append(item);
        QString display = QString("%1 / %2 / %3-%4 (%5)").arg(project, batch, shortCode).arg(parallelNo).arg(dataType);
        m_pathToRow[display] = r;
    }
    // 构建搜索自动补全
    QStringList paths = m_pathToRow.keys();
    if (!m_searchCompleter) {
        m_searchCompleter = new QCompleter(this);
        m_searchCompleter->setCaseSensitivity(Qt::CaseInsensitive);
        m_searchCompleter->setFilterMode(Qt::MatchContains); // 模糊匹配（包含）
        connect(m_searchCompleter, QOverload<const QString &>::of(&QCompleter::activated),
                this, &SampleListDialog::selectByPathString);
        ui->searchEdit->setCompleter(m_searchCompleter);
    }
    auto* modelCompleter = new QStringListModel(paths, m_searchCompleter);
    m_searchCompleter->setModel(modelCompleter);
}

// 填充项目下拉
void SampleListDialog::populateProjectCombo()
{
    ui->projectCombo->blockSignals(true);
    ui->batchCombo->blockSignals(true);
    ui->sampleCombo->blockSignals(true);
    ui->projectCombo->clear();
    QStringList projects = m_projToBatches.keys();
    projects.sort();
    ui->projectCombo->addItems(projects);
    ui->projectCombo->blockSignals(false);
    ui->batchCombo->blockSignals(false);
    ui->sampleCombo->blockSignals(false);
    if (!projects.isEmpty()) {
        on_projectCombo_currentIndexChanged(projects.first());
    } else {
        ui->batchCombo->clear();
        ui->sampleCombo->clear();
    }
}

// 填充批次下拉
void SampleListDialog::populateBatchCombo(const QString& project)
{
    ui->batchCombo->blockSignals(true);
    ui->sampleCombo->blockSignals(true);
    ui->batchCombo->clear();
    QStringList batches = m_projToBatches.value(project).values();
    std::sort(batches.begin(), batches.end());
    ui->batchCombo->addItems(batches);
    ui->batchCombo->blockSignals(false);
    ui->sampleCombo->blockSignals(false);
    if (!batches.isEmpty()) {
        on_batchCombo_currentIndexChanged(batches.first());
    } else {
        ui->sampleCombo->clear();
    }
}

// 填充样本下拉（可编辑 + 自动补全）
void SampleListDialog::populateSampleCombo(const QString& project, const QString& batch)
{
    ui->sampleCombo->blockSignals(true);
    ui->sampleCombo->clear();
    QString key = project + "||" + batch;
    const auto list = m_projBatchToSamples.value(key);
    QStringList samples;
    samples.reserve(list.size());
    for (const auto& s : list) {
        samples << QString("%1-%2").arg(s.shortCode).arg(s.parallelNo);
    }
    samples.sort();
    ui->sampleCombo->addItems(samples);
    ui->sampleCombo->setEditable(true);
    // 为样本下拉设置自动补全（前缀匹配）
    QCompleter* completer = new QCompleter(samples, ui->sampleCombo);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    ui->sampleCombo->setCompleter(completer);
    ui->sampleCombo->blockSignals(false);
}

// 选择表格行并滚动到可见
void SampleListDialog::selectRow(int row)
{
    QTableView* tv = currentTableView();
    if (!tv) return;
    QAbstractItemModel* model = tv->model();
    if (!model) return;
    QModelIndex idx = model->index(row, 0);
    tv->selectionModel()->setCurrentIndex(idx, QItemSelectionModel::Select | QItemSelectionModel::Rows);
    tv->scrollTo(idx, QAbstractItemView::PositionAtCenter);
}

// 通过路径字符串选择
void SampleListDialog::selectByPathString(const QString& path)
{
    if (m_pathToRow.contains(path)) {
        selectRow(m_pathToRow.value(path));
        // 同步下拉框
        // 解析 path: "project / batch / shortCode-parallelNo (type)"
        QString p = path;
        int typePos = p.lastIndexOf(" (");
        if (typePos > 0) p = p.left(typePos);
        QStringList parts = p.split(" / ");
        if (parts.size() == 3) {
            QString project = parts[0];
            QString batch   = parts[1];
            QString sampleDisplay = parts[2];
            ui->projectCombo->setCurrentText(project);
            ui->batchCombo->setCurrentText(batch);
            ui->sampleCombo->setCurrentText(sampleDisplay);
        }
    }
}

// 项目变化 -> 刷新批次
void SampleListDialog::on_projectCombo_currentIndexChanged(const QString& project)
{
    populateBatchCombo(project);
}

// 批次变化 -> 刷新样本
void SampleListDialog::on_batchCombo_currentIndexChanged(const QString& batch)
{
    QString project = ui->projectCombo->currentText();
    populateSampleCombo(project, batch);
    // 默认选中列表中第一个样本并定位
    QString key = project + "||" + batch;
    const auto list = m_projBatchToSamples.value(key);
    if (!list.isEmpty()) selectRow(list.first().row);
}

// 样本变化 -> 在表格中定位
void SampleListDialog::on_sampleCombo_currentIndexChanged(const QString& sampleDisplay)
{
    QString project = ui->projectCombo->currentText();
    QString batch   = ui->batchCombo->currentText();
    QString key = project + "||" + batch;
    const auto list = m_projBatchToSamples.value(key);
    for (const auto& s : list) {
        QString disp = QString("%1-%2").arg(s.shortCode).arg(s.parallelNo);
        if (disp == sampleDisplay) {
            selectRow(s.row);
            break;
        }
    }
}

// 搜索框回车 -> 使用自动补全的文本或直接包含匹配的第一条
void SampleListDialog::on_searchEdit_returnPressed()
{
    QString text = ui->searchEdit->text().trimmed();
    if (text.isEmpty()) return;
    // 优先尝试完整路径匹配
    if (m_pathToRow.contains(text)) {
        selectByPathString(text);
        return;
    }
    // 包含匹配，找到第一条包含关键字的路径
    for (auto it = m_pathToRow.constBegin(); it != m_pathToRow.constEnd(); ++it) {
        if (it.key().contains(text, Qt::CaseInsensitive)) {
            selectByPathString(it.key());
            return;
        }
    }
}
