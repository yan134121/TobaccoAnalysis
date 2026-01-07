#include "SensorDataImportConfigDialog.h"
#include "./ui_sensordatainportconfigdialog.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QJsonObject>
#include <QJsonDocument>
#include <QDebug>
#include <QSpinBox>
#include <QLabel>
#include <QFormLayout> // 确保 QFormLayout 被包含
#include <QStandardItem> // <-- 


#include "AppInitializer.h" // 



SensorDataImportConfigDialog::SensorDataImportConfigDialog(const QString& sampleCode, const QString& sensorType, FileHandlerFactory* factory, QWidget *parent) // <-- 关键修改：接收 factory 参数
    : QDialog(parent)
    , ui(new Ui::SensorDataImportConfigDialog)
    , m_sampleCode(sampleCode)
    , m_sensorType(sensorType)
    , m_fileHandlerFactory(factory) // <-- 关键修改：初始化成员变量
{
    ui->setupUi(this);
    setWindowTitle(QString("导入 %1 数据配置 - 样本: %2").arg(sensorType, sampleCode));

    ui->currentSampleCodeLabel->setText(QString("当前样本编码: %1").arg(m_sampleCode));
    ui->currentSensorTypeLabel->setText(QString("数据类型: %1").arg(m_sensorType));

    ui->startDataRowSpinBox->setRange(1, 9999);
    ui->startDataRowSpinBox->setValue(3); // 默认从第2行开始

    ui->startDataColSpinBox->setRange(1, 999);
    ui->startDataColSpinBox->setValue(1); // 默认从第1列开始

    ui->replicateNoSpinBox->setRange(1, 99);
    ui->replicateNoSpinBox->setValue(1);

    // connect(ui->browseFileButton, &QPushButton::clicked, this, &SensorDataImportConfigDialog::on_browseFileButton_clicked);
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &SensorDataImportConfigDialog::on_buttonBox_accepted);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &SensorDataImportConfigDialog::on_buttonBox_rejected);
    // connect(ui->previewButton, &QPushButton::clicked, this, &SensorDataImportConfigDialog::on_previewButton_clicked); // <-- 连接预览按钮

    // --- ：连接 QLineEdit/QSpinBox 的信号到预览刷新槽函数 (可选) ---
    connect(ui->filePathLineEdit, &QLineEdit::textChanged, this, &SensorDataImportConfigDialog::on_filePathLineEdit_textChanged);
    connect(ui->startDataRowSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &SensorDataImportConfigDialog::on_startDataRowSpinBox_valueChanged);
    connect(ui->startDataColSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &SensorDataImportConfigDialog::on_startDataColSpinBox_valueChanged);
    // --- 结束 ---

    // 初始化文件处理器工厂 (从 AppInitializer 获取)
    // m_fileHandlerFactory = AppInitializer::getInstance().getFileHandlerFactory();

    // 初始化预览表格
    m_previewModel = new QStandardItemModel(this);
    ui->previewTableView->setModel(m_previewModel);
    ui->previewTableView->horizontalHeader()->setStretchLastSection(true);
    ui->previewTableView->setEditTriggers(QAbstractItemView::NoEditTriggers); // 不可编辑
    ui->previewTableView->setSelectionBehavior(QAbstractItemView::SelectRows); // 整行选择
    ui->previewTableView->verticalHeader()->setVisible(false); // 隐藏行号，因为预览时行号可能不准确

    // 确保 ui->mappingLayout 已经在 Designer 中被设置为 QFormLayout 的 objectName
    // 不再需要手动创建布局
    setupMappingUI(); // 动态设置列映射区域
}

SensorDataImportConfigDialog::~SensorDataImportConfigDialog()
{
    delete ui;
}

// 根据传感器类型动态设置列映射区域的UI
void SensorDataImportConfigDialog::setupMappingUI()
{
    // 清除旧的映射控件
    QLayoutItem *item;
    // 直接访问 ui->mappingLayout，确保它在 .ui 文件中是 QFormLayout
    while ((item = ui->mappingLayout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            item->widget()->deleteLater(); // 销毁 widget
        }
        delete item; // 销毁 layout item
    }

    // 辅助 lambda 函数用于创建和配置 QSpinBox
    auto createAndAddSpinBox = [&](const QString& labelText, const QString& objectName) {
        QSpinBox* spinBox = new QSpinBox(this);
        spinBox->setObjectName(objectName);
        spinBox->setRange(-1, 999); // 允许 -1 表示该列不从文件读取
        spinBox->setValue(-1);     // 默认值 -1
        ui->mappingLayout->addRow(new QLabel(labelText, this), spinBox);
    };

    // 添加新的映射控件
    if (m_sensorType == "TG_BIG") {
        createAndAddSpinBox("序号列 (必填):", "tgBigSerialNoColIndexIndexSpinBox");
        createAndAddSpinBox("热重值列 (必填):", "tgBigTgValueColIndexSpinBox");
        createAndAddSpinBox("热重微分列 (必填):", "tgBigDtgValueColIndexSpinBox");
        
        // createAndAddSpinBox("序号列 (可选):", "tgBigSeqNoColIndexSpinBox");
        createAndAddSpinBox("源文件名列 (可选):", "tgBigSourceNameColIndexSpinBox");
    } else if (m_sensorType == "TG_SMALL") {
        createAndAddSpinBox("温度列 (必填):", "tgSmallTemperatureColIndexSpinBox");
        createAndAddSpinBox("热重值列 (必填):", "tgSmallTgValueColIndexSpinBox");
        createAndAddSpinBox("热重微分列 (必填):", "tgSmallDtgValueColIndexSpinBox");
        // createAndAddSpinBox("平行样号列 (可选):", "tgSmallReplicateNoColIndexSpinBox");
        createAndAddSpinBox("源文件名列 (可选):", "tgSmallSourceNameColIndexSpinBox");
    } else if (m_sensorType == "CHROMATOGRAPHY") {
        createAndAddSpinBox("保留时间列 (必填):", "chromaRetentionTimeColIndexSpinBox");
        createAndAddSpinBox("响应值列 (必填):", "chromaResponseValueColIndexSpinBox");
        // createAndAddSpinBox("平行样号列 (可选):", "chromaReplicateNoColIndexSpinBox");
        createAndAddSpinBox("源文件名列 (可选):", "chromaSourceNameColIndexSpinBox");
    }
}

void SensorDataImportConfigDialog::on_browseFileButton_clicked()
{
    QString filter = "数据文件 (*.csv *.xlsx *.xls);;所有文件 (*.*)";
    QString filePath = QFileDialog::getOpenFileName(this, "选择传感器数据文件", "", filter);
    if (!filePath.isEmpty()) {
        ui->filePathLineEdit->setText(filePath);
        // updateFilePreview(); // 文件路径改变后自动更新预览 (如果已经连接了 textChanged 信号，这里可以不需要手动调用)
    }
}

// --- 槽函数和辅助函数实现 ---
void SensorDataImportConfigDialog::updateFilePreview()
{
    m_previewModel->clear(); // 清空旧数据
    QString filePath = ui->filePathLineEdit->text().trimmed();
    int startRowForPreview = ui->startDataRowSpinBox->value();
    int startColForPreview = ui->startDataColSpinBox->value();

    if (filePath.isEmpty() || !m_fileHandlerFactory) {
        m_previewModel->setHorizontalHeaderLabels({"无文件或工厂未初始化"});
        return;
    }

    QString errorMessage;
    AbstractFileParser* parser = m_fileHandlerFactory->getParser(filePath, errorMessage);
    if (!parser) {
        m_previewModel->setHorizontalHeaderLabels({errorMessage});
        return;
    }

    // 预览文件头部几行 (例如 100 行)，总是从行1列1开始解析，不考虑 startDataRow/startDataCol 的最终值
    // 因为预览是为了帮助用户设置这些值，所以应该显示原始文件最完整的头部信息
    QList<QVariantList> rawPreviewData = parser->parseFile(filePath, errorMessage, 1, 1); // 总是从 1,1 开始预览
    if (rawPreviewData.isEmpty()) {
        m_previewModel->setHorizontalHeaderLabels({errorMessage});
        return;
    }

    // 设置列数 (取最宽一行的列数，或限制最大列数)
    int maxCols = 0;
    for(const QVariantList& row : rawPreviewData) {
        if (row.size() > maxCols) maxCols = row.size();
    }
    m_previewModel->setColumnCount(maxCols);

    // 设置表头为列号
    QStringList headerLabels;
    for(int i = 1; i <= maxCols; ++i) {
        headerLabels << QString("列 %1").arg(i);
    }
    m_previewModel->setHorizontalHeaderLabels(headerLabels);


    // 填充模型，限制预览行数
    int rowIdx = 0;
    for(const QVariantList& rowData : rawPreviewData) {
        for(int colIdx = 0; colIdx < rowData.size(); ++colIdx) {
            m_previewModel->setItem(rowIdx, colIdx, new QStandardItem(rowData.at(colIdx).toString()));
        }
        rowIdx++;
        if (rowIdx >= 100) break; // 限制预览行数
    }

    // --- ：显示行号 ---
    for (int i = 0; i < m_previewModel->rowCount(); ++i) {
        m_previewModel->setVerticalHeaderItem(i, new QStandardItem(QString::number(i + 1)));
    }

    
// 确保垂直表头显示
ui->previewTableView->verticalHeader()->setVisible(true);

    // 自动调整列宽
    ui->previewTableView->resizeColumnsToContents();
    ui->previewTableView->horizontalHeader()->setStretchLastSection(true);
}

void SensorDataImportConfigDialog::on_previewButton_clicked()
{
    updateFilePreview();
    if (m_previewModel->rowCount() == 0) {
        QMessageBox::warning(this, "预览失败", "未能加载文件预览，请检查文件路径和内容。", QMessageBox::Ok);
    }
}

void SensorDataImportConfigDialog::on_filePathLineEdit_textChanged(const QString& text)
{
    Q_UNUSED(text);
    updateFilePreview(); // 文件路径改变后自动更新预览
}

void SensorDataImportConfigDialog::on_startDataRowSpinBox_valueChanged(int value)
{
    Q_UNUSED(value);
    // updateFilePreview(); // 如果预览不需要根据 startDataRow 变化，则无需在这里调用
}

void SensorDataImportConfigDialog::on_startDataColSpinBox_valueChanged(int value)
{
    Q_UNUSED(value);
    // updateFilePreview(); // 如果预览不需要根据 startDataCol 变化，则无需在这里调用
}

// 从 UI 控件的值填充 m_mapping
// void SensorDataImportConfigDialog::populateMappingFromUI()
// {
//     m_mapping = DataColumnMapping(); // 先清空，确保是新的

//     // 使用 findChild 查找动态创建的 QSpinBox
//     // 注意：findChild 默认查找子对象，如果这些 SpinBox 的 parent 是 this (dialog)，则 findChild(this, "objectName") 就可以找到
//     // 如果它们的 parent 是 ui->mappingGroupBox，则需要 findChild<QSpinBox*>("objectName", ui->mappingGroupBox);

//     if (m_sensorType == "TG_BIG") {
//         m_mapping.tgBigTgValueColIndex = findChild<QSpinBox*>("tgBigTgValueColIndexSpinBox")->value();
//         m_mapping.tgBigDtgValueColIndex = findChild<QSpinBox*>("tgBigDtgValueColIndexSpinBox")->value();
//         m_mapping.tgBigReplicateNoColIndex = findChild<QSpinBox*>("tgBigSerialNoColIndexIndexSpinBox")->value();
//         m_mapping.tgBigSeqNoColIndex = findChild<QSpinBox*>("tgBigSeqNoColIndexSpinBox")->value();
//         m_mapping.tgBigSourceNameColIndex = findChild<QSpinBox*>("tgBigSourceNameColIndexSpinBox")->value();
//     } else if (m_sensorType == "TG_SMALL") {
//         m_mapping.tgSmallTemperatureColIndex = findChild<QSpinBox*>("tgSmallTemperatureColIndexSpinBox")->value();
//         m_mapping.tgSmallTgValueColIndex = findChild<QSpinBox*>("tgSmallTgValueColIndexSpinBox")->value();
//         m_mapping.tgSmallDtgValueColIndex = findChild<QSpinBox*>("tgSmallDtgValueColIndexSpinBox")->value();
//         m_mapping.tgSmallReplicateNoColIndex = findChild<QSpinBox*>("tgSmallReplicateNoColIndexSpinBox")->value();
//         m_mapping.tgSmallSourceNameColIndex = findChild<QSpinBox*>("tgSmallSourceNameColIndexSpinBox")->value();
//     } else if (m_sensorType == "CHROMATOGRAPHY") {
//         m_mapping.chromaRetentionTimeColIndex = findChild<QSpinBox*>("chromaRetentionTimeColIndexSpinBox")->value();
//         m_mapping.chromaResponseValueColIndex = findChild<QSpinBox*>("chromaResponseValueColIndexSpinBox")->value();
//         m_mapping.chromaReplicateNoColIndex = findChild<QSpinBox*>("chromaReplicateNoColIndexSpinBox")->value();
//         m_mapping.chromaSourceNameColIndex = findChild<QSpinBox*>("chromaSourceNameColIndexSpinBox")->value();
//     }
// }

void SensorDataImportConfigDialog::populateMappingFromUI()
{
    m_mapping = DataColumnMapping(); // 先清空，确保是新的

    // --- 关键修改：创建一个 SingleReplicateColumnMapping 对象，并填充它 ---
    SingleReplicateColumnMapping singleMapping; // <-- 创建一个单组映射对象

    if (m_sensorType == "TG_BIG") {
        singleMapping.tgBigSerialNoColIndex = findChild<QSpinBox*>("tgBigSerialNoColIndexIndexSpinBox")->value();
        singleMapping.tgBigTgValueColIndex = findChild<QSpinBox*>("tgBigTgValueColIndexSpinBox")->value();
        singleMapping.tgBigDtgValueColIndex = findChild<QSpinBox*>("tgBigDtgValueColIndexSpinBox")->value();        
        // singleMapping.seqNoColIndex = findChild<QSpinBox*>("tgBigSeqNoColIndexSpinBox")->value();
        // singleMapping.sourceNameColIndex = findChild<QSpinBox*>("tgBigSourceNameColIndexSpinBox")->value();
        singleMapping.sourceNameColIndex = findChild<QSpinBox*>("tgBigSourceNameColIndexSpinBox")->value();
    } else if (m_sensorType == "TG_SMALL") {
        singleMapping.tgSmallTemperatureColIndex = findChild<QSpinBox*>("tgSmallTemperatureColIndexSpinBox")->value();
        singleMapping.tgSmallTgValueColIndex = findChild<QSpinBox*>("tgSmallTgValueColIndexSpinBox")->value();
        singleMapping.tgSmallDtgValueColIndex = findChild<QSpinBox*>("tgSmallDtgValueColIndexSpinBox")->value();
        // singleMapping.replicateNoColIndex = findChild<QSpinBox*>("tgSmallReplicateNoColIndexSpinBox")->value();
        // singleMapping.tgSmallSourceNameColIndex = findChild<QSpinBox*>("tgSmallSourceNameColIndexSpinBox")->value();
        singleMapping.sourceNameColIndex = findChild<QSpinBox*>("tgSmallSourceNameColIndexSpinBox")->value();
        // DEBUG_LOG << "singleMapping.tgSmallTemperatureColIndex" << singleMapping.tgSmallTemperatureColIndex;
        // DEBUG_LOG << "singleMapping.tgSmallTgValueColIndex" << singleMapping.tgSmallTgValueColIndex;
        // DEBUG_LOG << "singleMapping.tgSmallDtgValueColIndex" << singleMapping.tgSmallDtgValueColIndex;
    } else if (m_sensorType == "CHROMATOGRAPHY") {
        singleMapping.chromaRetentionTimeColIndex = findChild<QSpinBox*>("chromaRetentionTimeColIndexSpinBox")->value();
        singleMapping.chromaResponseValueColIndex = findChild<QSpinBox*>("chromaResponseValueColIndexSpinBox")->value();
        // singleMapping.replicateNoColIndex = findChild<QSpinBox*>("chromaReplicateNoColIndexSpinBox")->value();
        // singleMapping.chromaSourceNameColIndex = findChild<QSpinBox*>("chromaSourceNameColIndexSpinBox")->value();
        singleMapping.sourceNameColIndex = findChild<QSpinBox*>("chromaSourceNameColIndexSpinBox")->value();
    }

    // 将这个单组映射添加到 DataColumnMapping 的列表中
    m_mapping.replicateMappings.append(singleMapping);
    // --- 结束关键修改 ---
}


bool SensorDataImportConfigDialog::validateInput(QString& errorMessage) const
{
    if (ui->filePathLineEdit->text().trimmed().isEmpty()) {
        errorMessage = "请选择要导入的数据文件。";
        return false;
    }
    if (ui->startDataRowSpinBox->value() < 1) {
        errorMessage = "数据起始行必须大于等于1。";
        return false;
    }
    if (ui->startDataColSpinBox->value() < 1) {
        errorMessage = "数据起始列必须大于等于1。";
        return false;
    }

    // 校验映射的必填字段
    if (m_sensorType == "TG_BIG") {
        QSpinBox* serialNoValueSpin = findChild<QSpinBox*>("tgBigSerialNoColIndexIndexSpinBox");
        QSpinBox* tgValueSpin = findChild<QSpinBox*>("tgBigTgValueColIndexSpinBox");
        QSpinBox* dtgValueSpin = findChild<QSpinBox*>("tgBigDtgValueColIndexSpinBox");
        if (!serialNoValueSpin || serialNoValueSpin->value() <= 0 ||
        !tgValueSpin || tgValueSpin->value() <= 0 || !dtgValueSpin || dtgValueSpin->value() <= 0) {
            errorMessage = "大热重数据的 '序号' '热重值列' 和 '热重微分列' 必须指定且大于0。";
            return false;
        }
    } else if (m_sensorType == "TG_SMALL") {
        QSpinBox* tempSpin = findChild<QSpinBox*>("tgSmallTemperatureColIndexSpinBox");
        QSpinBox* tgValueSpin = findChild<QSpinBox*>("tgSmallTgValueColIndexSpinBox");
        QSpinBox* dtgValueSpin = findChild<QSpinBox*>("tgSmallDtgValueColIndexSpinBox");
        if (!tempSpin || tempSpin->value() <= 0 || !tgValueSpin || tgValueSpin->value() <= 0 || !dtgValueSpin || dtgValueSpin->value() <= 0) {
            errorMessage = "小热重数据的 '温度列'、'热重值列' 和 '热重微分列' 必须指定且大于0。";
            return false;
        }
    } else if (m_sensorType == "CHROMATOGRAPHY") {
        QSpinBox* rtSpin = findChild<QSpinBox*>("chromaRetentionTimeColIndexSpinBox");
        QSpinBox* respSpin = findChild<QSpinBox*>("chromaResponseValueColIndexSpinBox");
        if (!rtSpin || rtSpin->value() <= 0 || !respSpin || respSpin->value() <= 0) {
            errorMessage = "色谱数据的 '保留时间列' 和 '响应值列' 必须指定且大于0。";
            return false;
        }
    }
    return true;
}

void SensorDataImportConfigDialog::on_buttonBox_accepted()
{
    QString errorMessage;
    if (validateInput(errorMessage)) {
        populateMappingFromUI(); // 从 UI 收集数据到 m_mapping
        accept();
    } else {
        QMessageBox::warning(this, "输入校验失败", errorMessage);
    }
}

void SensorDataImportConfigDialog::on_buttonBox_rejected()
{
    reject();
}


QString SensorDataImportConfigDialog::getFilePath() const { return ui->filePathLineEdit->text(); }
int SensorDataImportConfigDialog::getStartDataRow() const { return ui->startDataRowSpinBox->value(); }
int SensorDataImportConfigDialog::getStartDataCol() const { return ui->startDataColSpinBox->value(); }
int SensorDataImportConfigDialog::getReplicateNo() const { return ui->replicateNoSpinBox->value(); }
DataColumnMapping SensorDataImportConfigDialog::getDataColumnMapping() const { return m_mapping; }
