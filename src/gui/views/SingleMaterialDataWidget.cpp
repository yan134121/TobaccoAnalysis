// src/ui/SingleMaterialDataWidget.cpp
#include "src/gui/views/SingleMaterialDataWidget.h"
#include "ui_singlematerialdatawidget.h"  // 由 uic 自动生成的 UI 文件

// --------------------【核心功能类与全局对象】--------------------
#include "src/core/AppInitializer.h"
#include "Logger.h"

// --------------------【实体类（entities）】--------------------
#include "src/core/entities/SingleTobaccoSampleData.h"
#include "src/core/entities/TgBigData.h"
#include "src/core/entities/TgSmallData.h"
#include "src/core/entities/ChromatographyData.h"

// --------------------【数据模型（models）】--------------------
#include "src/core/models/SingleTobaccoSampleTableModel.h"
#include "src/core/models/TgBigDataTableModel.h"
#include "src/core/models/TgSmallDataTableModel.h"
#include "src/core/models/ChromatographyDataTableModel.h"

// --------------------【DAO 数据访问层】--------------------
#include "src/data_access/DatabaseConnector.h"
#include "src/data_access/SingleTobaccoSampleDAO.h"
#include "src/data_access/TgBigDataDAO.h"
#include "src/data_access/TgSmallDataDAO.h"

// --------------------【Service 层】--------------------
#include "src/services/SingleTobaccoSampleService.h"

// --------------------【GUI 组件（界面和对话框）】--------------------
#include "src/gui/dialogs/SensorDataImportConfigDialog.h"
#include "src/gui/dialogs/SingleMaterialEditDialog.h"
#include "src/gui/dialogs/ModelListDialog.h"
#include "src/gui/dialogs/BatchListDialog.h"
#include "src/gui/dialogs/DictionaryListDialog.h"
#include "src/gui/dialogs/SampleListDialog.h"
#include "src/gui/charts/LineChartView.h"

// --------------------【Qt 模块基础库】--------------------
#include <QFileDialog>
#include <QMessageBox>
#include <QProgressDialog>
#include <QInputDialog>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QDialogButtonBox>
#include <QDirIterator>
#include <QFileInfo>
#include <QDateTime>
#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QTextStream>
#include <QDateEdit>

// --------------------【外部库】--------------------
#include "third_party/QXlsx/header/xlsxdocument.h"

// SingleMaterialDataWidget::SingleMaterialDataWidget(SingleTobaccoSampleService* service, QWidget *parent)
//     : QWidget(parent)
//     , ui(new Ui::SingleMaterialDataWidget)
//     , m_service(service)
//     , m_selectedTobacco()
// --- 关键修改：第一个构造函数的实现签名必须与 .h 声明一致 ---
SingleMaterialDataWidget::SingleMaterialDataWidget(SingleTobaccoSampleService* service, AppInitializer* initializer, QWidget *parent) // <-- 必须是这个签名
    : QWidget(parent)
    , ui(new Ui::SingleMaterialDataWidget)
    , m_service(service)
    , m_selectedTobacco()
    , m_appInitializer(initializer) // <-- 初始化 m_appInitializer
    , m_singleTobaccoSampleDAO(initializer ? initializer->getSingleTobaccoSampleDAO() : nullptr) // 初始化单料烟样本DAO
{
    ui->setupUi(this);
    setWindowTitle("单料烟实采信息数据库"); // 设置窗口标题

    if (!m_service) {
        QMessageBox::critical(this, "错误", "SingleTobaccoSampleService 未初始化，请检查应用程序启动配置。", QMessageBox::Ok);
        emit statusMessage("错误: 单料烟服务未初始化。", 5000);
        return; // 无法继续，提前返回
    }

    m_tableModel = new SingleTobaccoSampleTableModel(this); // 创建数据模型实例
    setupTableView(); // 设置表格视图属性

    // 连接所有 UI 控件的信号到槽函数，注意ui也会自动绑定槽函数。重复绑定会触发两次
    // connect(ui->queryButton, &QPushButton::clicked, this, &SingleMaterialDataWidget::on_queryButton_clicked);
    // connect(ui->resetQueryButton, &QPushButton::clicked, this, &SingleMaterialDataWidget::on_resetQueryButton_clicked);
    // connect(ui->importButton, &QPushButton::clicked, this, &SingleMaterialDataWidget::on_importButton_clicked);
    // connect(ui->addButton, &QPushButton::clicked, this, &SingleMaterialDataWidget::on_addButton_clicked);
    // connect(ui->editButton, &QPushButton::clicked, this, &SingleMaterialDataWidget::on_editButton_clicked);
    // connect(ui->deleteButton, &QPushButton::clicked, this, &SingleMaterialDataWidget::on_deleteButton_clicked);
    // connect(ui->processButton, &QPushButton::clicked, this, &SingleMaterialDataWidget::on_processButton_clicked);
    // connect(ui->viewDetailsButton, &QPushButton::clicked, this, &SingleMaterialDataWidget::on_viewDetailsButton_clicked);
    // connect(ui->exportButton, &QPushButton::clicked, this, &SingleMaterialDataWidget::on_exportButton_clicked);
    // ... 如果有其他按钮，连接它们的槽函数

    // 填充组合框（如果需要，例如"类型"）
    ui->typeComboBox->addItem("",""); // 第一个空选项，值为QString()
    ui->typeComboBox->addItem("单打", "单打");
    ui->typeComboBox->addItem("混打", "混打");
    // ... 填充其他QComboBox如 gradeComboBox

    loadData(); // 初始加载数据
    emit statusMessage("单料烟数据界面已加载。", 3000);

    // 初始化传感器导入区域的 ComboBox
    ui->sensorTypeComboBox->addItem("大热重", "TG_BIG"); // <-- 确保这些行存在
    ui->sensorTypeComboBox->addItem("小热重", "TG_SMALL");
    ui->sensorTypeComboBox->addItem("色谱", "CHROMATOGRAPHY");

    // --- 连接新增 UI 控件信号 ---
    connect(ui->tableView, &QTableView::clicked, this, &SingleMaterialDataWidget::on_tableView_clicked);
    // connect(ui->importSensorDataButton, &QPushButton::clicked, this, &SingleMaterialDataWidget::on_importSensorDataButton_clicked);
    // connect(ui->loadSensorDataButton, &QPushButton::clicked, this, &SingleMaterialDataWidget::on_loadSensorDataButton_clicked); // <-- 连接按钮

    // // 初始化传感器数据模型
    // m_tgBigDataTableModel = new TgBigDataTableModel(this);
    // ui->tgBigTableView->setModel(m_tgBigDataTableModel);
    // ui->tgBigTableView->horizontalHeader()->setStretchLastSection(true);
    // ui->tgBigTableView->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // m_tgSmallDataTableModel = new TgSmallDataTableModel(this);
    // ui->tgSmallTableView->setModel(m_tgSmallDataTableModel);
    // ui->tgSmallTableView->horizontalHeader()->setStretchLastSection(true);
    // ui->tgSmallTableView->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // m_chromatographyDataTableModel = new ChromatographyDataTableModel(this);
    // ui->chromaTableView->setModel(m_chromatographyDataTableModel);
    // ui->chromaTableView->horizontalHeader()->setStretchLastSection(true);
    // ui->chromaTableView->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // 初始加载基础数据
    loadData();
    updateSelectedTobaccoInfo(); // 初始更新选中样本信息
    emit statusMessage("单料烟数据界面已加载。", 3000);
}

SingleMaterialDataWidget::~SingleMaterialDataWidget()
{
    // 停止并清理工作线程
    if (m_tgSmallDataImportWorker) {
        if (m_tgSmallDataImportWorker->isRunning()) {
            m_tgSmallDataImportWorker->stop();
            m_tgSmallDataImportWorker->wait();
        }
        delete m_tgSmallDataImportWorker;
        m_tgSmallDataImportWorker = nullptr;
    }
    
    // 清理进度对话框
    if (m_importProgressDialog) {
        delete m_importProgressDialog;
        m_importProgressDialog = nullptr;
    }
    
    delete ui;
}

void SingleMaterialDataWidget::setupTableView()
{
    ui->tableView->setModel(m_tableModel); // 将模型设置给视图
    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows); // 整行选择
    ui->tableView->setSelectionMode(QAbstractItemView::ExtendedSelection); // 支持多选
    ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers); // 默认不允许直接编辑，通过对话框编辑

    // 自动调整列宽
    ui->tableView->horizontalHeader()->setStretchLastSection(true); // 最后一列拉伸
    ui->tableView->resizeColumnsToContents(); // 根据内容调整列宽
}

void SingleMaterialDataWidget::loadData()
{
    DEBUG_LOG << "UI: 正在加载单料烟数据...";
    QList<SingleTobaccoSampleData> tobaccos = m_service->getAllTobaccos(); // 从 Service 层获取所有数据
    m_tableModel->setSamples(tobaccos); // 更新模型数据，视图会自动刷新
    emit statusMessage(QString("已加载 %1 条单料烟记录。").arg(tobaccos.size()));
}

// 根据 UI 输入构建查询条件映射
QMap<QString, QVariant> SingleMaterialDataWidget::buildQueryConditions() const
{
    QMap<QString, QVariant> conditions;
    QString code = ui->codeLineEdit->text().trimmed();
    if (!code.isEmpty()) conditions["code"] = code + "%"; // 模糊查询

    int year = ui->yearLineEdit->text().toInt(); // toInt() 如果文本为空或无效，返回0
    if (year > 0) conditions["year"] = year;

    QString origin = ui->originLineEdit->text().trimmed();
    if (!origin.isEmpty()) conditions["origin"] = origin + "%";

    QString type = ui->typeComboBox->currentData().toString(); // 使用 currentData 获取实际值
    if (!type.isEmpty()) conditions["type"] = type;

    // TODO: 添加其他查询条件，例如部位、等级、日期范围等
    // 日期范围查询可能需要特殊的处理，例如 WHERE collect_date BETWEEN :start_date AND :end_date

    return conditions;
}

// 获取当前选中的单行数据
SingleTobaccoSampleData SingleMaterialDataWidget::getSelectedTobacco() const
{
    QModelIndexList selectedRows = ui->tableView->selectionModel()->selectedRows();
    if (!selectedRows.isEmpty()) {
        int row = selectedRows.first().row(); // 只取第一行
        return m_tableModel->getSampleAt(row);
    }
    return SingleTobaccoSampleData(); // 返回一个默认构造的无效对象 (id 为 -1)
}

// 获取所有选中行的 ID 列表
QList<int> SingleMaterialDataWidget::getSelectedTobaccoIds() const
{
    QList<int> ids;
    QModelIndexList selectedRows = ui->tableView->selectionModel()->selectedRows();
    for (const QModelIndex& index : selectedRows) {
        // 从模型中获取指定行的 ID 列数据
        ids.append(m_tableModel->data(m_tableModel->index(index.row(), SingleTobaccoSampleTableModel::Id)).toInt());
    }
    return ids;
}


// --- 槽函数实现 ---

void SingleMaterialDataWidget::on_queryButton_clicked()
{
    DEBUG_LOG << "UI: 查询按钮被点击。";
    QMap<QString, QVariant> conditions = buildQueryConditions(); // 构建查询条件
    QList<SingleTobaccoSampleData> tobaccos = m_service->queryTobaccos(conditions); // 调用 Service 查询数据
    m_tableModel->setSamples(tobaccos); // 更新模型，显示查询结果
    emit statusMessage(QString("查询到 %1 条记录。").arg(tobaccos.size()));
}

void SingleMaterialDataWidget::on_resetQueryButton_clicked()
{
    // 清空所有查询条件输入框和组合框
    ui->codeLineEdit->clear();
    ui->yearLineEdit->clear();
    ui->originLineEdit->clear();
    ui->typeComboBox->setCurrentIndex(0); // 假设第一个是空选项
    // ... 清空其他查询条件控件

    loadData(); // 重新加载所有数据
    emit statusMessage("查询条件已重置，并加载所有数据。", 3000);
}

void SingleMaterialDataWidget::on_importButton_clicked()
{
    QString filter = "Excel Files (*.xlsx *.xls);;CSV Files (*.csv);;All Files (*.*)";
    QString filePath = QFileDialog::getOpenFileName(this, "选择数据文件", "", filter);
    if (filePath.isEmpty()) {
        emit statusMessage("文件导入操作已取消。", 2000);
        return;
    }

    QString errorMessage;
    // if (m_service->importFullSampleData(filePath, errorMessage)) { // 调用 Service 导入文件
    //     QMessageBox::information(this, "导入成功", "数据已成功导入。", QMessageBox::Ok);
    //     loadData(); // 重新加载数据以显示新导入的记录
    //     emit statusMessage("数据导入成功。", 3000);
    // } else {
    //     QMessageBox::critical(this, "导入失败", errorMessage, QMessageBox::Ok);
    //     emit statusMessage(QString("数据导入失败: %1").arg(errorMessage), 5000);
    // }
}



void SingleMaterialDataWidget::on_addButton_clicked()
{
    // DEBUG_LOG << "on_addButton_clicked called";

    // SingleMaterialEditDialog dialog(this); // 创建新增对话框
    SingleMaterialEditDialog dialog(m_service, this);
    // DEBUG_LOG;
    if (dialog.exec() == QDialog::Accepted) { // 模态显示，如果用户点击了OK
        
        SingleTobaccoSampleData newTobacco = dialog.getTobacco(); // 从对话框获取新数据
        QString errorMessage;
        if (m_service->addTobacco(newTobacco, errorMessage)) { // 调用 Service 添加数据
            QMessageBox::information(this, "新增成功", "新数据已添加。", QMessageBox::Ok);
            loadData(); // 重新加载数据
            emit statusMessage("新数据添加成功。", 3000);
        } else {
            
            QMessageBox::critical(this, "新增失败", errorMessage, QMessageBox::Ok);
            emit statusMessage(QString("新增失败: %1").arg(errorMessage), 5000);
        }
    } else {
        emit statusMessage("新增操作已取消。", 2000);
    }
}

void SingleMaterialDataWidget::on_editButton_clicked()
{
    SingleTobaccoSampleData selectedTobacco = getSelectedTobacco(); // 获取选中的单条记录
    if (selectedTobacco.getId() == -1) { // 检查是否选中有效记录
        QMessageBox::warning(this, "提示", "请选择一条要编辑的记录。", QMessageBox::Ok);
        return;
    }

    // SingleMaterialEditDialog dialog(selectedTobacco, this); // 创建编辑对话框，传入要编辑的数据
    SingleMaterialEditDialog dialog(m_service, selectedTobacco, this);
    if (dialog.exec() == QDialog::Accepted) { // 模态显示，如果用户点击了OK
        SingleTobaccoSampleData updatedTobacco = dialog.getTobacco(); // 从对话框获取修改后的数据
        // 确保 ID 保持不变，否则Service无法知道更新哪条记录
        updatedTobacco.setId(selectedTobacco.getId());

        QString errorMessage;
        if (m_service->updateTobacco(updatedTobacco, errorMessage)) { // 调用 Service 更新数据
            QMessageBox::information(this, "更新成功", "数据已更新。", QMessageBox::Ok);
            loadData(); // 重新加载数据以反映更新
            emit statusMessage("数据更新成功。", 3000);
        } else {
            QMessageBox::critical(this, "更新失败", errorMessage, QMessageBox::Ok);
            emit statusMessage(QString("更新失败: %1").arg(errorMessage), 5000);
        }
    } else {
        emit statusMessage("编辑操作已取消。", 2000);
    }
}

void SingleMaterialDataWidget::on_deleteButton_clicked()
{
    QList<int> selectedIds = getSelectedTobaccoIds(); // 获取所有选中行的 ID 列表
    if (selectedIds.isEmpty()) {
        QMessageBox::warning(this, "提示", "请选择至少一条要删除的记录。", QMessageBox::Ok);
        return;
    }

    // 确认删除
    if (QMessageBox::question(this, "确认删除", QString("确定要删除选中的 %1 条记录吗？").arg(selectedIds.size()),
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::No) {
        emit statusMessage("删除操作已取消。", 2000);
        return;
    }

    QString errorMessage;
    if (m_service->removeTobaccos(selectedIds, errorMessage)) { // 调用 Service 删除数据
        QMessageBox::information(this, "删除成功", "选中的记录已删除。", QMessageBox::Ok);
        loadData(); // 重新加载数据
        emit statusMessage("数据删除成功。", 3000);
    } else {
        QMessageBox::critical(this, "删除失败", errorMessage, QMessageBox::Ok);
        emit statusMessage(QString("删除失败: %1").arg(errorMessage), 5000);
    }
}

void SingleMaterialDataWidget::on_processButton_clicked()
{
    SingleTobaccoSampleData selectedTobacco = getSelectedTobacco(); // 获取选中的单条记录
    if (selectedTobacco.getId() == -1) {
        QMessageBox::warning(this, "提示", "请选择一条要处理的记录。", QMessageBox::Ok);
        return;
    }

    // 实际中可能需要一个对话框来选择处理算法和参数
    QVariantMap params;
    params["algorithm_type"] = "smoothing"; // 示例参数
    params["smoothing_strength"] = 0.5;

    QString errorMessage;
    if (m_service->processTobaccoData(selectedTobacco.getId(), params, errorMessage)) { // 调用 Service 处理数据
        QMessageBox::information(this, "数据处理", "数据已成功处理。", QMessageBox::Ok);
        loadData(); // 重新加载数据以显示处理后的更新（如果处理结果反映在表格中）
        emit statusMessage("数据处理成功。", 3000);
    } else {
        QMessageBox::critical(this, "数据处理失败", errorMessage, QMessageBox::Ok);
        emit statusMessage(QString("数据处理失败: %1").arg(errorMessage), 5000);
    }
}

void SingleMaterialDataWidget::on_viewDetailsButton_clicked()
{
    SingleTobaccoSampleData selectedTobacco = getSelectedTobacco(); // 获取选中的单条记录
    if (selectedTobacco.getId() == -1) {
        QMessageBox::warning(this, "提示", "请选择一条要查看详情的记录。", QMessageBox::Ok);
        return;
    }

    // 示例：显示 JSON 数据的简单方式
    QString details = QString("ID: %1\n编码: \n年份: %3\n产地: %4\n"
                              "部位: %5\n等级: %6\n类型: %7\n"
                              "数采日期: %8\n检测日期: %9\n"
                              "创建时间: %10\n"
                              "原始数据JSON:\n%11")
                          .arg(selectedTobacco.getId())
                          // .arg(selectedTobacco.getCode())
                          .arg(selectedTobacco.getYear())
                          .arg(selectedTobacco.getOrigin())
                          .arg(selectedTobacco.getPart())
                          .arg(selectedTobacco.getGrade())
                          .arg(selectedTobacco.getType())
                          .arg(selectedTobacco.getCollectDate().toString(Qt::ISODate))
                          .arg(selectedTobacco.getDetectDate().toString(Qt::ISODate))
                          .arg(selectedTobacco.getCreatedAt().toString(Qt::ISODate))
                          .arg(QString(selectedTobacco.getDataJson().toJson(QJsonDocument::Indented))); // 格式化JSON

    QMessageBox::information(this, QString("单料烟详情 - %1").arg(selectedTobacco.getId()), details, QMessageBox::Ok);

    // 实际中，这里应该弹出一个更复杂的对话框，甚至包含图形可视化组件
    // SingleMaterialDetailDialog* detailDialog = new SingleMaterialDetailDialog(selectedTobacco, m_service, this);
    // detailDialog->exec();
}

void SingleMaterialDataWidget::on_openModelListButton_clicked()
{
    ModelListDialog dlg(this);
    dlg.exec();
}

void SingleMaterialDataWidget::on_openBatchListButton_clicked()
{
    BatchListDialog dlg(this);
    dlg.exec();
}

void SingleMaterialDataWidget::on_openDictionaryListButton_clicked()
{
    if (!m_appInitializer) {
        QMessageBox::critical(this, "错误", "AppInitializer 未初始化。", QMessageBox::Ok);
        return;
    }
    auto dictService = m_appInitializer->getDictionaryOptionService();
    if (!dictService) {
        QMessageBox::critical(this, "错误", "字典选项服务未初始化。", QMessageBox::Ok);
        return;
    }
    DictionaryListDialog dlg(dictService, this);
    dlg.exec();
}

void SingleMaterialDataWidget::on_openSampleListButton_clicked()
{
    // 打开样本列表对话框，按四类数据分别显示并统计数据点数
    SampleListDialog dlg(this, m_appInitializer);
    dlg.exec();
}

void SingleMaterialDataWidget::on_exportButton_clicked()
{
    QString filter = "Excel Files (*.xlsx);;CSV Files (*.csv);;All Files (*.*)";
    QString filePath = QFileDialog::getSaveFileName(this, "保存数据到文件", "", filter);
    if (filePath.isEmpty()) {
        emit statusMessage("数据导出操作已取消。", 2000);
        return;
    }

    // 获取要导出的数据 (例如，当前显示的所有数据)
    QList<SingleTobaccoSampleData> dataToExport = m_tableModel->getSamples();
    if (dataToExport.isEmpty()) {
        QMessageBox::warning(this, "导出失败", "当前没有数据可导出。", QMessageBox::Ok);
        emit statusMessage("没有数据可导出。", 3000);
        return;
    }

    QString errorMessage;
    if (m_service->exportDataToFile(filePath, dataToExport, errorMessage)) { // 调用 Service 导出数据
        QMessageBox::information(this, "导出成功", "数据已成功导出。", QMessageBox::Ok);
        emit statusMessage("数据导出成功。", 3000);
    } else {
        QMessageBox::critical(this, "导出失败", errorMessage, QMessageBox::Ok);
        emit statusMessage(QString("数据导出失败: %1").arg(errorMessage), 5000);
    }
}


// --- 新的导入传感器数据槽函数 ---
void SingleMaterialDataWidget::on_importSensorDataButton_clicked()
{
    if (m_selectedTobacco.getId() == -1) {
        QMessageBox::warning(this, "提示", "请先在表格中选择一条单料烟基础信息记录。", QMessageBox::Ok);
        return;
    }

    QString sensorType = ui->sensorTypeComboBox->currentData().toString();
    if (sensorType.isEmpty()) {
        QMessageBox::warning(this, "提示", "请选择要导入的传感器数据类型。", QMessageBox::Ok);
        return;
    }

    // --- 关键修改：将 m_appInitializer->getFileHandlerFactory() 传递给对话框 ---
    // SingleMaterialDataWidget 需要知道 AppInitializer 才能获取文件工厂，
    // 所以 SingleMaterialDataWidget 的构造函数也需要接收 AppInitializer*
    // 为了简化，我们假设 m_appInitializer 是 SingleMaterialDataWidget 的成员，
    // 且已在构造函数中初始化。
    if (!m_appInitializer) { // 防御性检查
        QMessageBox::critical(this, "错误", "AppInitializer 未初始化，无法获取文件工厂。", QMessageBox::Ok);
        return;
    }
    FileHandlerFactory* factory = m_appInitializer->getFileHandlerFactory();
    if (!factory) { // 防御性检查
        QMessageBox::critical(this, "错误", "文件处理器工厂未初始化。", QMessageBox::Ok);
        return;
    }

    SensorDataImportConfigDialog configDialog(m_selectedTobacco.getShortCode(), sensorType, factory, this); // <-- 传入 factory
    // --- 结束关键修改 ---

    // 弹出导入配置对话框
    // SensorDataImportConfigDialog configDialog(m_selectedTobacco.getCode(), sensorType, this);
    if (configDialog.exec() == QDialog::Accepted) {
        QString filePath = configDialog.getFilePath();
        int startDataRow = configDialog.getStartDataRow();
        int startDataCol = configDialog.getStartDataCol();
        int replicateNo = configDialog.getReplicateNo();
        DataColumnMapping mapping = configDialog.getDataColumnMapping(); // 获取用户配置的列映射

        QString errorMessage;
        bool success = false;

        if (sensorType == "TG_BIG") {
            success = m_service->importTgBigDataForSample(m_selectedTobacco.getId(), filePath, replicateNo, startDataRow, startDataCol, mapping, errorMessage);
        } else if (sensorType == "TG_SMALL") {
            success = m_service->importTgSmallDataForSample(m_selectedTobacco.getId(), filePath, replicateNo, startDataRow, startDataCol, mapping, errorMessage);
        } else if (sensorType == "CHROMATOGRAPHY") {
            success = m_service->importChromatographyDataForSample(m_selectedTobacco.getId(), filePath, replicateNo, startDataRow, startDataCol, mapping, errorMessage);
        } else {
            QMessageBox::critical(this, "错误", "未知传感器数据类型。", QMessageBox::Ok);
            return;
        }

        if (success) {
            QMessageBox::information(this, "导入成功", QString("%1 数据已成功导入。").arg(ui->sensorTypeComboBox->currentText()), QMessageBox::Ok);
            emit statusMessage(QString("%1 数据导入成功。").arg(ui->sensorTypeComboBox->currentText()), 3000);
            // 导入成功后发射数据导入完成信号，用于通知主窗口自动刷新导航树
            emit dataImportFinished(sensorType);
        } else {
            QMessageBox::critical(this, "导入失败", errorMessage, QMessageBox::Ok);
            emit statusMessage(QString("%1 数据导入失败: %2").arg(ui->sensorTypeComboBox->currentText(), errorMessage), 5000);
        }
    } else {
        emit statusMessage("导入配置已取消。", 2000);
    }
}


void SingleMaterialDataWidget::on_tableView_clicked(const QModelIndex& index)
{
    // DEBUG_LOG << "on_tableView_clicked triggered for row:" << index.row();
    // if (index.isValid()) {
    //     m_selectedTobacco = m_tableModel->getSampleAt(index.row());
    //     DEBUG_LOG << "Selected Tobacco ID:" << m_selectedTobacco.getId() << ", Code:" << m_selectedTobacco.getCode();
    //     updateSelectedTobaccoInfo(); // 更新 UI 显示
    //     loadSensorDataForSelectedTobacco(); // <-- 选中新样本时自动加载传感器数据
    // } else {
    //     DEBUG_LOG << "Invalid index clicked.";
    //     m_selectedTobacco = SingleTobaccoSampleData(); // 清空选择
    //     updateSelectedTobaccoInfo();
    // }
    DEBUG_LOG << "on_tableView_clicked triggered for row:" << index.row();
    if (index.isValid()) {
        m_selectedTobacco = m_tableModel->getSampleAt(index.row());
        // DEBUG_LOG << "Selected Tobacco ID:" << m_selectedTobacco.getId() << ", Code:" << m_selectedTobacco.getCode();
        updateSelectedTobaccoInfo(); // 更新 UI 显示
        loadSensorDataForSelectedTobacco(); // <-- 选中新样本时自动加载传感器数据
    } else {
        DEBUG_LOG << "Invalid index clicked.";
        m_selectedTobacco = SingleTobaccoSampleData(); // 清空选择
        updateSelectedTobaccoInfo();
        loadSensorDataForSelectedTobacco(); // 清空传感器数据
    }
}


#include <QElapsedTimer>

// --- 新增辅助函数：加载并显示传感器数据 ---
void SingleMaterialDataWidget::loadSensorDataForSelectedTobacco()
{
    // // 清空所有旧数据和图表
    // m_tgBigDataTableModel->setTgBigData({});
    // ui->tgBigChartView->clearChart();
    // m_tgSmallDataTableModel->setTgSmallData({});
    // ui->tgSmallChartView->clearChart();
    // m_chromatographyDataTableModel->setChromatographyData({});
    // ui->chromaChartView->clearChart();

    if (m_selectedTobacco.getId() == -1) {
        emit statusMessage("未选择单料烟，传感器数据已清空。", 2000);
        return;
    }

    int sampleId = m_selectedTobacco.getId();
    emit statusMessage(QString("正在加载样本ID %1 的传感器数据...").arg(sampleId), 0); // 0 表示一直显示

    // 加载大热重数据
    QList<TgBigData> bigData = m_service->getTgBigDataForSample(sampleId);
    m_tgBigDataTableModel->setTgBigData(bigData);
    if (!bigData.isEmpty()) {
        QList<double> xValues, yValues;
        for(const TgBigData& item : bigData) {
            xValues.append(item.getSerialNo()); // 假设序号是 X 轴
            yValues.append(item.getDtgValue()); // TgValue 是 Y 轴
        }
        // ui->tgBigChartView->setChartData(xValues, yValues, "序号", "热重值", "大热重");
        emit statusMessage(QString("已加载 %1 条大热重数据。").arg(bigData.size()), 3000);
    } else {
        emit statusMessage("未找到大热重数据。", 2000);
    }

    // 加载小热重数据
    QList<TgSmallData> smallData = m_service->getTgSmallDataForSample(sampleId);
    m_tgSmallDataTableModel->setTgSmallData(smallData);
    if (!smallData.isEmpty()) {
        QList<double> xValues, yValues;
        for(const TgSmallData& item : smallData) {
            xValues.append(item.getTemperature()); // 温度是 X 轴
            yValues.append(item.getDtgValue()); // TgValue 是 Y 轴
        }
        // ui->tgSmallChartView->setChartData(xValues, yValues, "温度", "热重值", "小热重");
        emit statusMessage(QString("已加载 %1 条小热重数据。").arg(smallData.size()), 3000);
    } else {
        emit statusMessage("未找到小热重数据。", 2000);
    }

    

    QElapsedTimer timer;  //  先声明
    timer.restart();

    // 加载色谱数据
    QList<ChromatographyData> chromaData = m_service->getChromatographyDataForSample(sampleId);

    DEBUG_LOG << "色谱数据加载 took:" << timer.elapsed() << "ms";
    timer.restart();

    m_chromatographyDataTableModel->setChromatographyData(chromaData);
    if (!chromaData.isEmpty()) {
        QList<double> xValues, yValues;
        for(const ChromatographyData& item : chromaData) {
            xValues.append(item.getRetentionTime()); // 保留时间是 X 轴
            yValues.append(item.getResponseValue()); // 响应值是 Y 轴
        }
        // ui->chromaChartView->setChartData(xValues, yValues, "保留时间", "响应值", "色谱");
        emit statusMessage(QString("已加载 %1 条色谱数据。").arg(chromaData.size()), 3000);

        DEBUG_LOG << " 色谱绘图 took:" << timer.elapsed() << "ms";

    } else {
        emit statusMessage("未找到色谱数据。", 2000);
    }
}


// --- 槽函数：点击加载传感器数据按钮 ---
void SingleMaterialDataWidget::on_loadSensorDataButton_clicked()
{
    loadSensorDataForSelectedTobacco();
}


void SingleMaterialDataWidget::updateSelectedTobaccoInfo()
{
    if (m_selectedTobacco.getId() != -1) {
        // ui->selectedSampleCodeLabel->setText(QString("当前样本: %1").arg(m_selectedTobacco.getShortCode()));
        // 可以根据需要禁用/启用某些按钮
        ui->importSensorDataButton->setEnabled(true);
        ui->processButton->setEnabled(true);
        // ... (根据您的 UI 设计，如果有其他需要更新的标签或按钮状态) ...
    } else {
        // ui->selectedSampleCodeLabel->setText("当前样本: 未选择");
        ui->importSensorDataButton->setEnabled(false); // 没有选择样本时，禁用导入
        ui->processButton->setEnabled(false); // 没有选择样本时，禁用处理
        // ...
    }
}


// #include <QProgressDialog>
// #include <QFileInfo>

//     QString m_dirPath;
//     AppInitializer* m_appInitializer;
//     bool m_stopped;
//     QMutex m_mutex;
//     QSqlDatabase m_threadDb;
//     TgBigDataDAO* m_tgBigDataDao;
//     SingleTobaccoSampleDAO* m_singleTobaccoSampleDao;
// // }

void SingleMaterialDataWidget::on_importTgBigDataButton_clicked()
{
    DEBUG_LOG << "点击了导入大热重数据按钮。";
    // 1) 选择目录
    QString dirPath = QFileDialog::getExistingDirectory(this, tr("选择大热重数据文件夹"), QString(), QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (dirPath.isEmpty()) {
        emit statusMessage(tr("未选择文件夹，操作已取消。"), 3000);
        return;
    }

    // 2. 创建输入对话框获取批次代码和检测日期（烟牌号固定为“大热重”）
    QDialog inputDialog(this);
    inputDialog.setWindowTitle(tr("输入大热重样本信息"));
    
    QVBoxLayout* layout = new QVBoxLayout(&inputDialog);
    
    QFormLayout* formLayout = new QFormLayout();
    
    QLineEdit batchCodeEdit;
    batchCodeEdit.setText("1"); // 默认批次代码
    formLayout->addRow(tr("批次代码:"), &batchCodeEdit);

    // 新增检测日期选择框
    QDateEdit detectDateEdit;
    detectDateEdit.setCalendarPopup(true);
    detectDateEdit.setDate(QDate::currentDate());
    formLayout->addRow(tr("检测日期:"), &detectDateEdit);
    
    // QSpinBox parallelNoSpinBox;
    // parallelNoSpinBox.setRange(1, 100);
    // parallelNoSpinBox.setValue(1); // 默认值为1
    // formLayout->addRow(tr("平行样编号:"), &parallelNoSpinBox);
    
    layout->addLayout(formLayout);
    
    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, &inputDialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &inputDialog, &QDialog::reject);
    layout->addWidget(buttonBox);
    
    if (inputDialog.exec() != QDialog::Accepted) {
        emit statusMessage(tr("用户取消了操作。"), 3000);
        return;
    }

    // 3. 获取用户输入的信息（烟牌号统一设为“大热重”）
    QString projectName = QStringLiteral("大热重");
    QString batchCode = batchCodeEdit.text();
    QDate detectDate = detectDateEdit.date();
    // int parallelNo = parallelNoSpinBox.value();

    // 如果已经有一个导入进程在运行，先停止它
    if (m_tgBigDataImportWorker && m_tgBigDataImportWorker->isRunning()) {
        m_tgBigDataImportWorker->stop();
        m_tgBigDataImportWorker->wait();
    }
    
    // 创建进度对话框（非模态，不阻塞界面其他操作）
    if (!m_importProgressDialog) {
        m_importProgressDialog = new QProgressDialog(this);
        m_importProgressDialog->setWindowModality(Qt::NonModal); // 设置为非模态
        m_importProgressDialog->setMinimumDuration(0);
        m_importProgressDialog->setCancelButtonText(tr("取消"));
        m_importProgressDialog->setAutoClose(false);
        m_importProgressDialog->setAutoReset(false);
    }
    
    m_importProgressDialog->setWindowTitle(tr("导入大热重数据"));
    m_importProgressDialog->setLabelText(tr("准备导入数据..."));
    m_importProgressDialog->setValue(0);
    m_importProgressDialog->setRange(0, 100);
    m_importProgressDialog->show();
    
    // 创建工作线程
    if (!m_tgBigDataImportWorker) {
        m_tgBigDataImportWorker = new TgBigDataImportWorker(this);
        
        // 连接信号和槽
        connect(m_tgBigDataImportWorker, &TgBigDataImportWorker::progressChanged, 
                this, [this](int current, int total) {
                    if (m_importProgressDialog) {
                        m_importProgressDialog->setRange(0, total);
                        m_importProgressDialog->setValue(current);
                    }
                });
        
        connect(m_tgBigDataImportWorker, &TgBigDataImportWorker::progressMessage,
                this, [this](const QString& message) {
                    if (m_importProgressDialog) {
                        m_importProgressDialog->setLabelText(message);
                    }
                    emit statusMessage(message, 3000);
                });
        
        connect(m_tgBigDataImportWorker, &TgBigDataImportWorker::importFinished,
                this, [this](int successCount, int totalDataCount) {
                    if (m_importProgressDialog) {
                        m_importProgressDialog->close();
                    }
                    
                    if (successCount > 0) {
                        QString msg = tr("导入成功：%1 个文件，共 %2 条数据。").arg(successCount).arg(totalDataCount);
                        QMessageBox::information(this, tr("导入完成"), msg);
                        emit statusMessage(msg, 5000);
                        
                        // 刷新图表
                        refreshCharts();
                        // 通知主窗口刷新导航树（数据源）
                        emit dataImportFinished("TG_BIG");
                    } else {
                        QString msg = tr("导入失败：未从CSV文件中提取到有效数据。");
                        QMessageBox::warning(this, tr("导入错误"), msg);
                        emit statusMessage(msg, 5000);
                    }
                });
        
        connect(m_tgBigDataImportWorker, &TgBigDataImportWorker::importError,
                this, [this](const QString& errorMessage) {
                    if (m_importProgressDialog) {
                        m_importProgressDialog->close();
                    }
                    
                    QMessageBox::critical(this, tr("导入错误"), errorMessage);
                    emit statusMessage(tr("大热重数据导入失败: %1").arg(errorMessage), 5000);
                });
        
        // 连接进度对话框的取消按钮
        connect(m_importProgressDialog, &QProgressDialog::canceled,
                m_tgBigDataImportWorker, &TgBigDataImportWorker::stop);
    }
    
    // 设置工作线程参数并启动
    // m_tgBigDataImportWorker->setParameters(dirPath, m_appInitializer);
    m_tgBigDataImportWorker->setParameters(dirPath, projectName, batchCode, detectDate, m_appInitializer);
    m_tgBigDataImportWorker->start();
    
    emit statusMessage(tr("正在后台导入大热重数据..."), 3000);
}
    
void SingleMaterialDataWidget::on_importProcessTgBigDataButton_clicked()
{
    // 1) 选择目录
    QString dirPath = QFileDialog::getExistingDirectory(this, tr("选择工序大热重原始数据文件夹"), QString(), QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (dirPath.isEmpty()) {
        emit statusMessage(tr("未选择文件夹，操作已取消。"), 3000);
        return;
    }

    // 2) 解析默认项目与批次（从首个CSV文件名推断），并弹出对话框允许用户修改
    QString defaultProjectName;
    QString defaultBatchCode;
    {
        // 扫描所选目录下的第一个CSV文件，尝试从文件名中拆分出“烟牌号-批次代码”
        QDirIterator it(dirPath, QStringList() << "*.csv" << "*.CSV", QDir::Files, QDirIterator::Subdirectories);
        if (it.hasNext()) {
            QString firstCsvPath = it.next();
            QFileInfo info(firstCsvPath);
            QString baseName = info.fileName();
            baseName = baseName.split(".").first(); // 去掉扩展名
            QStringList parts = baseName.split("-");
            if (parts.size() >= 2) {
                defaultProjectName = parts.at(0).trimmed();
                defaultBatchCode = parts.at(1).trimmed();
            }
        }
    }
    // 构造输入对话框，预填默认的项目与批次，用户可编辑确认
    QDialog inputDialog(this);
    inputDialog.setWindowTitle(tr("确认或修改项目与批次信息"));
    QVBoxLayout* layout = new QVBoxLayout(&inputDialog);
    QFormLayout* formLayout = new QFormLayout();
    QLineEdit projectNameEdit;
    projectNameEdit.setText(defaultProjectName);
    formLayout->addRow(tr("烟牌号:"), &projectNameEdit);
    QLineEdit batchCodeEdit;
    batchCodeEdit.setText(defaultBatchCode);
    formLayout->addRow(tr("批次代码:"), &batchCodeEdit);
    layout->addLayout(formLayout);
    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttonBox);
    connect(buttonBox, &QDialogButtonBox::accepted, &inputDialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &inputDialog, &QDialog::reject);
    if (inputDialog.exec() != QDialog::Accepted) {
        emit statusMessage(tr("已取消导入操作。"), 3000);
        return;
    }
    const QString projectName = projectNameEdit.text().trimmed();
    const QString batchCode = batchCodeEdit.text().trimmed();
    if (projectName.isEmpty() || batchCode.isEmpty()) {
        QMessageBox::warning(this, tr("输入不完整"), tr("请填写完整的烟牌号与批次代码。"));
        return;
    }

    // 如果已经有一个导入进程在运行，先停止它
    if (m_processTgBigDataImportWorker && m_processTgBigDataImportWorker->isRunning()) {
        m_processTgBigDataImportWorker->stop();
        m_processTgBigDataImportWorker->wait();
    }
    
    // 创建进度对话框（非模态，不阻塞界面其他操作）
    if (!m_importProgressDialog) {
        m_importProgressDialog = new QProgressDialog(this);
        m_importProgressDialog->setWindowModality(Qt::NonModal); // 设置为非模态
        m_importProgressDialog->setMinimumDuration(0);
        m_importProgressDialog->setCancelButtonText(tr("取消"));
        m_importProgressDialog->setAutoClose(false);
        m_importProgressDialog->setAutoReset(false);
    }
    
    m_importProgressDialog->setWindowTitle(tr("导入工序大热重数据"));
    m_importProgressDialog->setLabelText(tr("准备导入数据..."));
    m_importProgressDialog->setValue(0);
    m_importProgressDialog->setRange(0, 100);
    m_importProgressDialog->show();
    
    // 创建工作线程
    if (!m_processTgBigDataImportWorker) {
        m_processTgBigDataImportWorker = new ProcessTgBigDataImportWorker(this);
        
        // 连接信号和槽
        connect(m_processTgBigDataImportWorker, &ProcessTgBigDataImportWorker::progressChanged, 
                this, [this](int current, int total) {
                    if (m_importProgressDialog) {
                        m_importProgressDialog->setRange(0, total);
                        m_importProgressDialog->setValue(current);
                    }
                });
        
        connect(m_processTgBigDataImportWorker, &ProcessTgBigDataImportWorker::progressMessage,
                this, [this](const QString& message) {
                    if (m_importProgressDialog) {
                        m_importProgressDialog->setLabelText(message);
                    }
                    emit statusMessage(message, 3000);
                });
        
        connect(m_processTgBigDataImportWorker, &ProcessTgBigDataImportWorker::importFinished,
                this, [this](int successCount, int totalDataCount) {
                    if (m_importProgressDialog) {
                        m_importProgressDialog->close();
                    }
                    
                    if (successCount > 0) {
                        QString msg = tr("导入成功：%1 个文件，共 %2 条数据。").arg(successCount).arg(totalDataCount);
                        QMessageBox::information(this, tr("导入完成"), msg);
                        emit statusMessage(msg, 5000);
                        
                        // 刷新图表
                        refreshCharts();
                        // 通知主窗口刷新导航树（工序大热重）
                        emit dataImportFinished("PROCESS_TG_BIG");
                    } else {
                        QString msg = tr("导入失败：未从CSV文件中提取到有效数据。");
                        QMessageBox::warning(this, tr("导入错误"), msg);
                        emit statusMessage(msg, 5000);
                    }
                });
        
        connect(m_processTgBigDataImportWorker, &ProcessTgBigDataImportWorker::importError,
                this, [this](const QString& errorMessage) {
                    if (m_importProgressDialog) {
                        m_importProgressDialog->close();
                    }
                    
                    QMessageBox::critical(this, tr("导入错误"), errorMessage);
                    emit statusMessage(tr("工序大热重数据导入失败: %1").arg(errorMessage), 5000);
                });
        
        // 连接进度对话框的取消按钮
        connect(m_importProgressDialog, &QProgressDialog::canceled,
                m_processTgBigDataImportWorker, &ProcessTgBigDataImportWorker::stop);
    }
    
    // 设置工作线程参数并启动（传递用户确认的项目与批次，用于覆盖解析值）
    m_processTgBigDataImportWorker->setParameters(dirPath, projectName, batchCode, m_appInitializer);

    m_processTgBigDataImportWorker->start();
    
    emit statusMessage(tr("正在后台导入工序大热重数据..."), 3000);
}

void SingleMaterialDataWidget::on_importTgSmallDataButton_clicked()
{
    // 1. 打开文件选择对话框
    QString filePath = QFileDialog::getOpenFileName(this, tr("选择小热重数据文件"), 
                                                   QDir::homePath(), 
                                                   tr("CSV文件 (*.csv)"));
    if (filePath.isEmpty()) {
        emit statusMessage(tr("未选择文件，操作已取消。"), 3000);
        return;
    }

    // 2. 创建输入对话框获取温度列与DTG列
    QDialog columnDialog(this);
    columnDialog.setWindowTitle(tr("指定小热重数据列"));

    QVBoxLayout* columnLayout = new QVBoxLayout(&columnDialog);
    QFormLayout* columnFormLayout = new QFormLayout();

    QSpinBox temperatureColumnSpinBox;
    temperatureColumnSpinBox.setRange(1, 1000);
    temperatureColumnSpinBox.setValue(1);
    columnFormLayout->addRow(tr("温度列(从1开始):"), &temperatureColumnSpinBox);

    QSpinBox dtgColumnSpinBox;
    dtgColumnSpinBox.setRange(1, 1000);
    dtgColumnSpinBox.setValue(3);
    columnFormLayout->addRow(tr("DTG列(从1开始):"), &dtgColumnSpinBox);

    columnLayout->addLayout(columnFormLayout);

    QDialogButtonBox* columnButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(columnButtonBox, &QDialogButtonBox::accepted, &columnDialog, &QDialog::accept);
    connect(columnButtonBox, &QDialogButtonBox::rejected, &columnDialog, &QDialog::reject);
    columnLayout->addWidget(columnButtonBox);

    if (columnDialog.exec() != QDialog::Accepted) {
        emit statusMessage(tr("用户取消了操作。"), 3000);
        return;
    }

    int temperatureColumn = temperatureColumnSpinBox.value();
    int dtgColumn = dtgColumnSpinBox.value();

    // 3. 创建输入对话框获取批次代码和检测日期（烟牌号固定为“小热重”）
    QDialog inputDialog(this);
    inputDialog.setWindowTitle(tr("输入小热重样本信息"));
    
    QVBoxLayout* layout = new QVBoxLayout(&inputDialog);
    
    QFormLayout* formLayout = new QFormLayout();
    
    QLineEdit batchCodeEdit;
    batchCodeEdit.setText("1"); // 默认批次代码
    formLayout->addRow(tr("批次代码:"), &batchCodeEdit);

    // 新增检测日期选择框
    QDateEdit detectDateEdit;
    detectDateEdit.setCalendarPopup(true);
    detectDateEdit.setDate(QDate::currentDate());
    formLayout->addRow(tr("检测日期:"), &detectDateEdit);
    
    QSpinBox parallelNoSpinBox;
    parallelNoSpinBox.setRange(1, 100);
    parallelNoSpinBox.setValue(1); // 默认值为1
    // formLayout->addRow(tr("平行样编号:"), &parallelNoSpinBox);
    
    layout->addLayout(formLayout);
    
    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, &inputDialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &inputDialog, &QDialog::reject);
    layout->addWidget(buttonBox);
    
    if (inputDialog.exec() != QDialog::Accepted) {
        emit statusMessage(tr("用户取消了操作。"), 3000);
        return;
    }
    
    // 4. 获取用户输入的信息（烟牌号统一设为“小热重”）
    QString projectName = QStringLiteral("小热重");
    QString batchCode = batchCodeEdit.text();
    QDate detectDate = detectDateEdit.date();
    int parallelNo = parallelNoSpinBox.value();
    
    // 5. 如果已经有一个导入进程在运行，先停止它
    if (m_tgSmallDataImportWorker && m_tgSmallDataImportWorker->isRunning()) {
        m_tgSmallDataImportWorker->stop();
        m_tgSmallDataImportWorker->wait();
    }
    
    // 6. 创建进度对话框（非模态，不阻塞界面其他操作）
    if (!m_importProgressDialog) {
        m_importProgressDialog = new QProgressDialog(this);
        m_importProgressDialog->setWindowModality(Qt::NonModal); // 设置为非模态
        m_importProgressDialog->setMinimumDuration(0);
        m_importProgressDialog->setCancelButtonText(tr("取消"));
        m_importProgressDialog->setAutoClose(false);
        m_importProgressDialog->setAutoReset(false);
    }
    
    m_importProgressDialog->setWindowTitle(tr("导入小热重数据"));
    m_importProgressDialog->setLabelText(tr("准备导入数据..."));
    m_importProgressDialog->setValue(0);
    m_importProgressDialog->setRange(0, 100);
    m_importProgressDialog->show();
    
    // 7. 创建工作线程
    if (!m_tgSmallDataImportWorker) {
        m_tgSmallDataImportWorker = new TgSmallDataImportWorker(this);
        
        // 连接信号和槽
        connect(m_tgSmallDataImportWorker, &TgSmallDataImportWorker::progressChanged, 
                this, [this](int current, int total) {
                    if (m_importProgressDialog) {
                        m_importProgressDialog->setRange(0, total);
                        m_importProgressDialog->setValue(current);
                    }
                });
        
        connect(m_tgSmallDataImportWorker, &TgSmallDataImportWorker::progressMessage,
                this, [this](const QString& message) {
                    if (m_importProgressDialog) {
                        m_importProgressDialog->setLabelText(message);
                    }
                    emit statusMessage(message, 3000);
                });
        
        connect(m_tgSmallDataImportWorker, &TgSmallDataImportWorker::importFinished,
                this, [this](int successCount, int totalDataCount) {
                    if (m_importProgressDialog) {
                        m_importProgressDialog->close();
                    }
                    
                    if (successCount > 0) {
                        QString msg = tr("导入成功：%1 个文件，共 %2 条数据。").arg(successCount).arg(totalDataCount);
                        QMessageBox::information(this, tr("导入完成"), msg);
                        emit statusMessage(msg, 5000);
                        
                        // 刷新图表
                        refreshCharts();
                        // 通知主窗口刷新导航树（数据源）
                        emit dataImportFinished("TG_SMALL");
                    } else {
                        QString detailedMsg = tr("导入失败：未从CSV文件中提取到有效数据。\n\n可能的原因：\n"
                                              "1. CSV文件格式不正确\n"
                                              "2. 文件名未包含有效的样本编号\n"
                                              "3. 温度列或DTG列设置错误\n"
                                              "4. 数据不是有效的数字格式\n\n"
                                              "请检查CSV文件与列设置后重试。");
                        QMessageBox::warning(this, tr("导入错误"), detailedMsg);
                        emit statusMessage(tr("导入失败：未从CSV文件中提取到有效数据"), 5000);
                    }
                });
        
        connect(m_tgSmallDataImportWorker, &TgSmallDataImportWorker::importError,
                this, [this](const QString& errorMessage) {
                    if (m_importProgressDialog) {
                        m_importProgressDialog->close();
                    }
                    
                    // 显示错误信息
                    QMessageBox::critical(this, tr("导入错误"), errorMessage);
                    emit statusMessage(tr("导入错误: ") + errorMessage, 5000);
                });
        
        // 连接进度对话框的取消按钮
        connect(m_importProgressDialog, &QProgressDialog::canceled,
                m_tgSmallDataImportWorker, &TgSmallDataImportWorker::stop);
    }
    
    // 8. 设置工作线程参数
    m_tgSmallDataImportWorker->setParameters(filePath, projectName, batchCode, detectDate, parallelNo, temperatureColumn, dtgColumn, m_appInitializer);
    
    // 9. 启动工作线程
    m_tgSmallDataImportWorker->start();
    
    emit statusMessage(tr("正在后台导入小热重数据..."), 3000);
}


void SingleMaterialDataWidget::on_importChromatographDataButton_clicked()
{
    // 选择包含色谱数据的文件夹
    QString dirPath = QFileDialog::getExistingDirectory(this, tr("选择色谱数据文件夹"), 
                                                      QDir::homePath(),
                                                      QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (dirPath.isEmpty()) {
        return;
    }

    // 变量声明（烟牌号固定为“色谱”）
    QString projectName = QStringLiteral("色谱");
    QString batchCode = "1";
    QString shortCode = "CHROM";
    int parallelNo = 1;
    
    // 创建输入对话框获取项目名称、批次代码和平行样编号
    QDialog inputDialog(this);
    inputDialog.setWindowTitle(tr("输入色谱样本信息"));
    
    QVBoxLayout* layout = new QVBoxLayout(&inputDialog);
    
    QFormLayout* formLayout = new QFormLayout();
    
    QLineEdit batchCodeEdit;
    batchCodeEdit.setText(batchCode); // 默认批次代码
    formLayout->addRow(tr("批次代码:"), &batchCodeEdit);

    // 新增检测日期选择框
    QDateEdit detectDateEdit;
    detectDateEdit.setCalendarPopup(true);
    detectDateEdit.setDate(QDate::currentDate());
    formLayout->addRow(tr("检测日期:"), &detectDateEdit);
    
    QLineEdit shortCodeEdit;
    shortCodeEdit.setText(shortCode); // 默认值
    // formLayout->addRow(tr("样本编码:"), &shortCodeEdit);
    
    QSpinBox parallelNoSpinBox;
    parallelNoSpinBox.setRange(1, 100);
    parallelNoSpinBox.setValue(1); // 默认值为1
    // formLayout->addRow(tr("平行样编号:"), &parallelNoSpinBox);
    
    layout->addLayout(formLayout);
    
    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, &inputDialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &inputDialog, &QDialog::reject);
    layout->addWidget(buttonBox);
    
    if (inputDialog.exec() != QDialog::Accepted) {
        emit statusMessage(tr("用户取消了操作。"), 3000);
        return;
    }
    
    // 获取用户输入的信息（烟牌号统一设为“色谱”）
    projectName = QStringLiteral("色谱");
    batchCode = batchCodeEdit.text();
    QDate detectDate = detectDateEdit.date();
    shortCode = shortCodeEdit.text();
    parallelNo = parallelNoSpinBox.value();
    
    // 如果已经有一个导入进程在运行，先停止它
    if (m_chromatographDataImportWorker && m_chromatographDataImportWorker->isRunning()) {
        m_chromatographDataImportWorker->stop();
        m_chromatographDataImportWorker->wait();
    }
    
    // 创建进度对话框（非模态，不阻塞界面其他操作）
    if (!m_importProgressDialog) {
        m_importProgressDialog = new QProgressDialog(this);
        m_importProgressDialog->setWindowModality(Qt::NonModal); // 设置为非模态
        m_importProgressDialog->setMinimumDuration(0);
        m_importProgressDialog->setCancelButtonText(tr("取消"));
        m_importProgressDialog->setAutoClose(false);
        m_importProgressDialog->setAutoReset(false);
    }
    
    m_importProgressDialog->setWindowTitle(tr("导入色谱数据"));
    m_importProgressDialog->setLabelText(tr("准备导入数据..."));
    m_importProgressDialog->setValue(0);
    m_importProgressDialog->setRange(0, 100);
    m_importProgressDialog->show();
    
    // 创建工作线程
    if (!m_chromatographDataImportWorker) {
        m_chromatographDataImportWorker = new ChromatographDataImportWorker(this);
        
        // 连接信号和槽
        connect(m_chromatographDataImportWorker, &ChromatographDataImportWorker::progressChanged, 
                this, [this](int current, int total) {
                    if (m_importProgressDialog) {
                        m_importProgressDialog->setRange(0, total);
                        m_importProgressDialog->setValue(current);
                    }
                });
        
        connect(m_chromatographDataImportWorker, &ChromatographDataImportWorker::progressMessage,
                this, [this](const QString& message) {
                    if (m_importProgressDialog) {
                        m_importProgressDialog->setLabelText(message);
                    }
                    emit statusMessage(message, 3000);
                });
        
        connect(m_chromatographDataImportWorker, &ChromatographDataImportWorker::importFinished,
                this, [this](int successCount, int totalDataCount) {
                    if (m_importProgressDialog) {
                        m_importProgressDialog->close();
                    }
                    
                    if (successCount > 0) {
                        QString msg = tr("导入成功：%1 个文件，共 %2 条数据。").arg(successCount).arg(totalDataCount);
                        QMessageBox::information(this, tr("导入完成"), msg);
                        emit statusMessage(msg, 5000);
                        
                        // 刷新色谱数据表格
                        // refreshChromatographyDataTable();
                        // 通知主窗口刷新导航树（数据源）
                        emit dataImportFinished("CHROMATOGRAPHY");
                    } else {
                        QString msg = tr("导入失败：未找到有效的色谱数据文件。");
                        QMessageBox::warning(this, tr("导入错误"), msg);
                        emit statusMessage(msg, 5000);
                    }
                });
        
        connect(m_chromatographDataImportWorker, &ChromatographDataImportWorker::importError,
                this, [this](const QString& errorMessage) {
                    if (m_importProgressDialog) {
                        m_importProgressDialog->close();
                    }
                    
                    QMessageBox::critical(this, tr("导入错误"), errorMessage);
                    emit statusMessage(tr("色谱数据导入失败: %1").arg(errorMessage), 5000);
                });
        
        // 连接进度对话框的取消按钮
        connect(m_importProgressDialog, &QProgressDialog::canceled,
                m_chromatographDataImportWorker, &ChromatographDataImportWorker::stop);
    }
    
    // 设置工作线程参数并启动
    m_chromatographDataImportWorker->setParameters(dirPath, projectName, batchCode, detectDate, shortCode, parallelNo, m_appInitializer);
    m_chromatographDataImportWorker->start();
}

// 刷新图表
void SingleMaterialDataWidget::refreshCharts()
{
    // 刷新数据显示
    if (m_selectedTobacco.getId() != -1) {
        loadSensorDataForSelectedTobacco();
    }
}
