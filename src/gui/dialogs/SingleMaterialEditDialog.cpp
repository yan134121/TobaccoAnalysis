// src/ui/dialogs/SingleMaterialEditDialog.cpp
#include "SingleMaterialEditDialog.h"
#include "./ui_singlematerialeditdialog.h" // 由 uic 工具自动生成
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDate> // 用于日期设置
#include "Logger.h"

// --- 头文件 ---
#include <QDialogButtonBox> // <-- 添加这行
// --- 结束 ---

// SingleMaterialEditDialog::SingleMaterialEditDialog(QWidget *parent)
//     : QDialog(parent)
//     , ui(new Ui::SingleMaterialEditDialog)
//     , m_currentTobacco() // 初始化为一个默认（无效）的 SingleMaterialTobacco 对象
//     , m_service(service) // <-- 初始化 Service
SingleMaterialEditDialog::SingleMaterialEditDialog(SingleTobaccoSampleService* service, QWidget *parent) // <-- 修改为接收 service 参数
    : QDialog(parent)
    , ui(new Ui::SingleMaterialEditDialog)
    , m_currentTobacco()
    , m_service(service) // <-- 初始化 m_service
{
    
    ui->setupUi(this);
    setWindowTitle("新增单料烟数据");

    
    if (!m_service) { // 防御性检查
        QMessageBox::critical(this, "错误", "SingleMaterialTobaccoService 未初始化。", QMessageBox::Ok);
        return;
    }

    
    // // 初始化 ComboBox 选项
    // ui->typeComboBox->addItem("单打");
    // ui->typeComboBox->addItem("混打");

    
    
    loadComboBoxOptions(); // <-- 加载产地、部位、等级、类型(ENUM)的选项

    // 初始化 QSpinBox 的范围和默认值
    ui->yearSpinBox->setRange(1900, QDate::currentDate().year() + 10); // 年份范围
    ui->yearSpinBox->setValue(QDate::currentDate().year());

    // 初始化 QDateEdit 的默认日期
    ui->collectDateEdit->setDate(QDate::currentDate());
    ui->detectDateEdit->setDate(QDate::currentDate());

        // === 默认配置 ===
    ui->projectNameComboBox->setCurrentText("88");
    ui->batchCodeComboBox->setCurrentText("88");
    ui->shortCodeComboBox->setCurrentText("88");
    ui->parallelNoComboBox->setCurrentText("88");
    ui->sampleNameComboBox->setCurrentText("88");
    ui->noteComboBox->setCurrentText("88");

    ui->yearSpinBox->setValue(QDate::currentDate().year());
    ui->codeComboBox->setCurrentText("88");
    ui->originComboBox->setCurrentText("88");
    ui->partComboBox->setCurrentText("88");
    ui->gradeComboBox->setCurrentText("88");
    ui->typeComboBox->setCurrentText("单打");
    ui->collectDateEdit->setDate(QDate::currentDate());
    ui->detectDateEdit->setDate(QDate::currentDate());
    ui->dataJsonPlainTextEdit->setPlainText("{}");


    // 将按钮盒的信号连接到对话框的槽函数
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &SingleMaterialEditDialog::on_buttonBox_accepted);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &SingleMaterialEditDialog::on_buttonBox_rejected);
    
}

// SingleMaterialEditDialog::SingleMaterialEditDialog(const SingleMaterialTobacco& tobacco, QWidget *parent)
//     : SingleMaterialEditDialog(parent) // 调用默认构造函数进行基本设置
SingleMaterialEditDialog::SingleMaterialEditDialog(SingleTobaccoSampleService* service, const SingleTobaccoSampleData& tobacco, QWidget *parent)
    : SingleMaterialEditDialog(service, parent) // 调用新的默认构造函数
{
    setWindowTitle("编辑单料烟数据");
    m_currentTobacco = tobacco; // 保存当前正在编辑的数据
    DEBUG_LOG;
    setupUiForEdit(tobacco);    // 使用传入的数据填充UI
}

SingleMaterialEditDialog::~SingleMaterialEditDialog()
{
    delete ui;
}

void SingleMaterialEditDialog::loadComboBoxOptions()
{
    
    // --- 关键修改：从 Service 获取所有独特选项 ---
    QStringList origins = m_service->getAllUniqueOrigins();
    
    QStringList parts = m_service->getAllUniqueParts();
    
    QStringList grades = m_service->getAllUniqueGrades();
    
    QStringList blendTypes = m_service->getAllUniqueBlendTypes(); // 获取 '单打/混打' 选项

    

    // 填充到 QComboBox
    // 对于可编辑的 QComboBox，通常先 clear()，再 addItems()
    // 确保添加一个空选项
    ui->originComboBox->clear();
    ui->originComboBox->addItem("");
    ui->originComboBox->addItems(origins);

    ui->partComboBox->clear();
    ui->partComboBox->addItem("");
    ui->partComboBox->addItems(parts);

    ui->gradeComboBox->clear();
    ui->gradeComboBox->addItem("");
    ui->gradeComboBox->addItems(grades);

    

    // 对于 'typeComboBox'，其选项是 ENUM，不是动态添加的，但也可以通过字典填充
    // 如果 typeComboBox 已经被硬编码添加了 "单打", "混打"，可以跳过这里
    // 如果希望它也从字典管理，则：
    ui->typeComboBox->clear();
    ui->typeComboBox->addItem("");
    ui->typeComboBox->addItems(blendTypes);
    // --- 结束关键修改 ---
    
}

// // 不需要 saveNewComboBoxOption 了，因为 Service 层会根据 addTobacco/updateTobacco 逻辑来处理新值
// // 但在 UI 中，用户输入的新值会自动成为 QComboBox 的当前文本
// void SingleMaterialEditDialog::setupUiForEdit(const SingleTobaccoSampleData& tobacco)
// {
//     ui->codeLineEdit->setText(tobacco.getCode());
//     ui->yearSpinBox->setValue(tobacco.getYear());
//     ui->originComboBox->setCurrentText(tobacco.getOrigin());
//     ui->partComboBox->setCurrentText(tobacco.getPart());
//     ui->gradeComboBox->setCurrentText(tobacco.getGrade());
//     ui->typeComboBox->setCurrentText(tobacco.getType()); // 如果 typeComboBox 是字典管理，这里也用 currentText
//     ui->collectDateEdit->setDate(tobacco.getCollectDate());
//     ui->detectDateEdit->setDate(tobacco.getDetectDate());
//     ui->dataJsonPlainTextEdit->setPlainText(tobacco.getDataJson().toJson(QJsonDocument::Indented));
// }

void SingleMaterialEditDialog::setupUiForEdit(const SingleTobaccoSampleData& sample)
{
    DEBUG_LOG << "SingleMaterialEditDialog::setupUiForEdit(const SingleTobaccoSampleData& sample)";
    // // === 默认配置 ===
    // ui->projectNameComboBox->setCurrentText("默认项目");
    // ui->batchCodeComboBox->setCurrentText("默认批次");
    // ui->shortCodeComboBox->setCurrentText("默认短码");
    // ui->parallelNoComboBox->setCurrentText("1");
    // ui->sampleNameComboBox->setCurrentText("默认样品");
    // ui->noteComboBox->setCurrentText("无备注");

    // ui->yearSpinBox->setValue(QDate::currentDate().year());
    // ui->originComboBox->setCurrentText("默认产地");
    // ui->partComboBox->setCurrentText("默认部位");
    // ui->gradeComboBox->setCurrentText("默认等级");
    // ui->typeComboBox->setCurrentText("默认类型");
    // ui->collectDateEdit->setDate(QDate::currentDate());
    // ui->detectDateEdit->setDate(QDate::currentDate());
    // ui->dataJsonPlainTextEdit->setPlainText("{}");

    // === 覆盖 sample 的值 ===
    auto setComboBoxText = [](QComboBox* box, const QString& value) {
        if (!value.isEmpty()) {
            if (box->findText(value) == -1)
                box->addItem(value);
            box->setCurrentText(value);
        }
    };

    setComboBoxText(ui->projectNameComboBox, sample.getProjectName());
    setComboBoxText(ui->batchCodeComboBox, sample.getBatchCode());
    // Note: batchId is now an integer foreign key, not a string field for UI display
    setComboBoxText(ui->shortCodeComboBox, sample.getShortCode());
    if (sample.getParallelNo() > 0)
        setComboBoxText(ui->parallelNoComboBox, QString::number(sample.getParallelNo()));
    setComboBoxText(ui->sampleNameComboBox, sample.getSampleName());
    setComboBoxText(ui->noteComboBox, sample.getNote());

    if (sample.getYear() > 0)
        ui->yearSpinBox->setValue(sample.getYear());
    setComboBoxText(ui->originComboBox, sample.getOrigin());
    setComboBoxText(ui->partComboBox, sample.getPart());
    setComboBoxText(ui->gradeComboBox, sample.getGrade());
    setComboBoxText(ui->typeComboBox, sample.getType());

    if (sample.getCollectDate().isValid())
        ui->collectDateEdit->setDate(sample.getCollectDate());
    if (sample.getDetectDate().isValid())
        ui->detectDateEdit->setDate(sample.getDetectDate());

    if (!sample.getDataJson().isEmpty())
        ui->dataJsonPlainTextEdit->setPlainText(
            sample.getDataJson().toJson(QJsonDocument::Indented));
}



// 返回类型已更新为 SingleTobaccoSampleData
SingleTobaccoSampleData SingleMaterialEditDialog::getTobacco() const
{
    SingleTobaccoSampleData sample; // 修改: 变量类型和名称

    // 保留ID的逻辑，用于区分是新建还是更新
    if (m_currentTobacco.getId() != -1) { // 假设 m_currentTobacco 也已更新为 SingleTobaccoSampleData 类型
        sample.setId(m_currentTobacco.getId());
    }

    // 假设 batch_id 是从 m_currentTobacco 或其他地方传入的
    // 因为用户通常不直接编辑这个外键ID
    sample.setBatchId(m_currentTobacco.getBatchId());

    // --- 从UI控件读取新增字段的值 ---
    sample.setProjectName(ui->projectNameComboBox->currentText().trimmed());
    sample.setBatchCode(ui->batchCodeComboBox->currentText().trimmed());
    sample.setShortCode(ui->shortCodeComboBox->currentText().trimmed());
    sample.setParallelNo(ui->parallelNoComboBox->currentText().toInt());
    sample.setSampleName(ui->sampleNameComboBox->currentText().trimmed());
    sample.setNote(ui->noteComboBox->currentText().trimmed());

    // --- 从UI控件读取保留字段的值 (逻辑不变) ---
    sample.setYear(ui->yearSpinBox->value());
    sample.setOrigin(ui->originComboBox->currentText().trimmed());
    sample.setPart(ui->partComboBox->currentText().trimmed());
    sample.setGrade(ui->gradeComboBox->currentText().trimmed());
    sample.setType(ui->typeComboBox->currentText().trimmed());

    sample.setCollectDate(ui->collectDateEdit->date());
    sample.setDetectDate(ui->detectDateEdit->date());

    // Note: batchId should be set from tobacco_batch table relationship, not from UI
    // sample.setBatchId(ui->batchIdComboBox->currentText().toInt());

    // JSON处理逻辑不变
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(ui->dataJsonPlainTextEdit->toPlainText().toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError && !ui->dataJsonPlainTextEdit->toPlainText().isEmpty()) {
        WARNING_LOG << "用户输入的JSON格式有误:" << parseError.errorString();
    }
    sample.setDataJson(doc);

    // 移除: tobacco.setCode(ui->codeLineEdit->text()); 这一行不再需要

    return sample; // 返回新的 sample 对象
}

bool SingleMaterialEditDialog::validateInput(QString& errorMessage) const
{
    // if (ui->codeComboBox->currentText().trimmed().isEmpty()) { errorMessage = "编码不能为空。"; return false; }
    // if (ui->yearSpinBox->value() <= 0) { errorMessage = "年份必须大于0。"; return false; }
    // if (ui->originComboBox->currentText().trimmed().isEmpty()) { // <-- 校验 ComboBox 文本
    //     errorMessage = "产地不能为空。"; return false;
    // }
    // if (ui->typeComboBox->currentText().trimmed().isEmpty()) { // <-- 校验 ComboBox 文本
    //     errorMessage = "类型不能为空。"; return false;
    // }

    QString jsonText = ui->dataJsonPlainTextEdit->toPlainText().trimmed();
    if (!jsonText.isEmpty()) { /* ... */ }
    return true;
}

void SingleMaterialEditDialog::on_buttonBox_accepted()
{
    QString errorMessage;
    if (validateInput(errorMessage)) {
        // 在 Service::addTobacco 或 Service::updateTobacco 中，
        // 如果值是新的，Service 会负责将其添加到 dictionary_option 表中。
        // 这里不需要额外的 saveNewComboBoxOption 调用。
        accept();
    } else {
        QMessageBox::warning(this, "输入校验失败", errorMessage);
    }
}


// Cancel 按钮被点击的槽函数
void SingleMaterialEditDialog::on_buttonBox_rejected()
{
    reject(); // 关闭对话框并返回 QDialog::Rejected
}
