#include "SamplePropertiesDialog.h"
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QFrame>
#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
#include <QByteArray>
#include "data_access/DatabaseConnector.h"
#include "Logger.h"

namespace {

QString primaryTableForDataType(const QString& dataType)
{
    if (dataType.contains(QStringLiteral("工序大热重")) || dataType.contains(QStringLiteral("工序"))) {
        return QStringLiteral("process_tg_big_data");
    }
    if (dataType.contains(QStringLiteral("小热重（原始"))) {
        return QStringLiteral("tg_small_raw_data");
    }
    if (dataType.contains(QStringLiteral("小热重"))) {
        return QStringLiteral("tg_small_data");
    }
    if (dataType.contains(QStringLiteral("色谱"))) {
        return QStringLiteral("chromatography_data");
    }
    return QStringLiteral("tg_big_data");
}

bool tableHasImportAttributesColumn(QSqlDatabase& db, const QString& tableName)
{
    QSqlQuery checkQ(db);
    const QString checkSql = QStringLiteral(
        "SELECT 1 FROM information_schema.COLUMNS "
        "WHERE TABLE_SCHEMA = DATABASE() "
        "AND TABLE_NAME = '%1' AND COLUMN_NAME = 'import_attributes'").arg(tableName);
    if (checkQ.exec(checkSql) && checkQ.next()) {
        return true;
    }
    // 回退：部分环境 information_schema 权限/大小写导致检测失败，SHOW COLUMNS 仍可用
    const QString showSql = QStringLiteral("SHOW COLUMNS FROM `%1` LIKE 'import_attributes'").arg(tableName);
    if (checkQ.exec(showSql) && checkQ.next()) {
        return true;
    }
    return false;
}

/** MySQL JSON 列在 QMYSQL 下 toByteArray() 常为空，需优先 toString 或 CAST 查询结果 */
static QString variantMysqlJsonToUtf8String(const QVariant& v)
{
    if (v.isNull()) {
        return QString();
    }
    QString s = v.toString();
    if (!s.isEmpty()) {
        return s;
    }
    const QByteArray b = v.toByteArray();
    if (!b.isEmpty()) {
        return QString::fromUtf8(b);
    }
    return QString();
}

} // namespace

SamplePropertiesDialog::SamplePropertiesDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("样本属性"));

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(6);

    // --- 基础信息区 ---
    QLabel* basicTitle = new QLabel(tr("<b>基础信息</b>"), this);
    mainLayout->addWidget(basicTitle);

    QFormLayout* basicForm = new QFormLayout();
    basicForm->setLabelAlignment(Qt::AlignRight);

    m_projectNameLabel = new QLabel(this);
    m_batchCodeLabel   = new QLabel(this);
    m_shortCodeLabel   = new QLabel(this);
    m_parallelNoLabel  = new QLabel(this);
    m_dataTypeLabel    = new QLabel(this);
    m_sampleID         = new QLabel(this);

    basicForm->addRow(tr("烟牌号:"),      m_projectNameLabel);
    basicForm->addRow(tr("批次编码:"),    m_batchCodeLabel);
    basicForm->addRow(tr("短码:"),        m_shortCodeLabel);
    basicForm->addRow(tr("平行样编号:"),  m_parallelNoLabel);
    basicForm->addRow(tr("数据类型:"),    m_dataTypeLabel);
    basicForm->addRow(tr("样本ID:"),      m_sampleID);
    mainLayout->addLayout(basicForm);

    // 分割线
    QFrame* line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(line);

    // --- 导入属性区 ---
    QLabel* attrTitle = new QLabel(tr("<b>导入属性</b>"), this);
    mainLayout->addWidget(attrTitle);

    QFormLayout* attrForm = new QFormLayout();
    attrForm->setLabelAlignment(Qt::AlignRight);

    m_codeLabel         = new QLabel(this);
    m_yearLabel         = new QLabel(this);
    m_originLabel       = new QLabel(this);
    m_partLabel         = new QLabel(this);
    m_gradeLabel        = new QLabel(this);
    m_stemLeafModeLabel = new QLabel(this);
    m_detectDateLabel   = new QLabel(this);
    m_factoryLabel      = new QLabel(this);

    attrForm->addRow(tr("编码:"),         m_codeLabel);
    attrForm->addRow(tr("年份:"),         m_yearLabel);
    attrForm->addRow(tr("产地:"),         m_originLabel);
    attrForm->addRow(tr("部位:"),         m_partLabel);
    attrForm->addRow(tr("等级:"),         m_gradeLabel);
    attrForm->addRow(tr("叶梗分离方式:"), m_stemLeafModeLabel);
    attrForm->addRow(tr("检测日期:"),     m_detectDateLabel);
    attrForm->addRow(tr("分厂:"),         m_factoryLabel);
    mainLayout->addLayout(attrForm);

    // 确定按钮
    QPushButton* okButton = new QPushButton(tr("确定"), this);
    connect(okButton, &QPushButton::clicked, this, &QDialog::accept);
    mainLayout->addWidget(okButton, 0, Qt::AlignRight);

    setLayout(mainLayout);
    resize(420, 380);
}

SamplePropertiesDialog::~SamplePropertiesDialog() {}

void SamplePropertiesDialog::setSampleInfo(const struct NavigatorNodeInfo& info)
{
    m_projectNameLabel->setText(info.projectName);
    m_batchCodeLabel->setText(info.batchCode);
    m_shortCodeLabel->setText(info.shortCode);
    m_parallelNoLabel->setText(QString::number(info.parallelNo));
    m_sampleID->setText(QString::number(info.id));

    QString dataTypeText = info.dataType;
    if (dataTypeText.contains("工序")) {
        m_dataTypeLabel->setText(dataTypeText + tr(" (工序数据)"));
    } else {
        m_dataTypeLabel->setText(dataTypeText + tr(" (样本数据)"));
    }

    // 默认置空
    const QString na = tr("—");
    m_codeLabel->setText(na);
    m_yearLabel->setText(na);
    m_originLabel->setText(na);
    m_partLabel->setText(na);
    m_gradeLabel->setText(na);
    m_stemLeafModeLabel->setText(na);
    m_detectDateLabel->setText(na);
    m_factoryLabel->setText(na);

    if (info.id > 0) {
        loadImportAttributes(info.id, info.dataType);
    }
}

void SamplePropertiesDialog::loadImportAttributes(int sampleId, const QString& dataType)
{
    QSqlDatabase db = DatabaseConnector::getInstance().getDatabase();
    if (!db.isOpen()) {
        WARNING_LOG << "SamplePropertiesDialog: 数据库未打开";
        return;
    }
    // 确保本连接使用 utf8mb4，防止中文乱码
    { QSqlQuery setNames(db); setNames.exec("SET NAMES utf8mb4"); }

    const QString primary = primaryTableForDataType(dataType);
    static const QStringList kAllTables = {
        QStringLiteral("tg_big_data"),
        QStringLiteral("tg_small_data"),
        QStringLiteral("tg_small_raw_data"),
        QStringLiteral("process_tg_big_data"),
        QStringLiteral("chromatography_data"),
    };
    QStringList tryTables;
    tryTables << primary;
    for (const QString& t : kAllTables) {
        if (t != primary) {
            tryTables << t;
        }
    }

    WARNING_LOG << "[属性查询] sampleId=" << sampleId << " dataType=" << dataType
                << " primaryTable=" << primary << " tryOrder=" << tryTables.join(QLatin1Char(','));

    bool anyColumn = false;
    QString attrStr;
    QByteArray attrRawBytes;
    for (const QString& tableName : tryTables) {
        if (!tableHasImportAttributesColumn(db, tableName)) {
            continue;
        }
        anyColumn = true;
        // CAST 为 CHAR，避免驱动把 JSON 列映射成 QVariant 后 toByteArray 为空导致读不到内容
        QSqlQuery q(db);
        const QString sqlCast = QStringLiteral(
            "SELECT CAST(`import_attributes` AS CHAR(16384)) AS ja "
            "FROM `%1` "
            "WHERE sample_id = :sid AND import_attributes IS NOT NULL "
            "ORDER BY `id` DESC LIMIT 1").arg(tableName);
        const QString sqlPlain = QStringLiteral(
            "SELECT `import_attributes` AS ja "
            "FROM `%1` "
            "WHERE sample_id = :sid AND import_attributes IS NOT NULL "
            "ORDER BY `id` DESC LIMIT 1").arg(tableName);
        q.prepare(sqlCast);
        q.bindValue(QStringLiteral(":sid"), sampleId);

        if (!q.exec()) {
            WARNING_LOG << "[属性查询] CAST 查询失败，回退为直接读 JSON 列 table=" << tableName
                        << q.lastError().text();
            q.prepare(sqlPlain);
            q.bindValue(QStringLiteral(":sid"), sampleId);
            if (!q.exec()) {
                WARNING_LOG << "[属性查询] 查询执行失败 table=" << tableName << q.lastError().text();
                continue;
            }
        }
        if (!q.next()) {
            continue;
        }
        attrStr = variantMysqlJsonToUtf8String(q.value(QStringLiteral("ja")));
        if (attrStr.isEmpty()) {
            attrStr = variantMysqlJsonToUtf8String(q.value(0));
        }
        attrRawBytes = attrStr.toUtf8();
        if (!attrStr.trimmed().isEmpty()) {
            WARNING_LOG << "[属性查询] 命中表=" << tableName << " 字符串长度=" << attrStr.size();
            break;
        }
    }

    if (!anyColumn) {
        WARNING_LOG << "[属性查询] 各表均无 import_attributes 列";
        m_codeLabel->setText(tr("（请先执行数据库迁移并重新导入）"));
        return;
    }
    if (attrStr.isEmpty()) {
        WARNING_LOG << "[属性查询] sample_id=" << sampleId << " 各候选表中无有效属性 JSON";
        m_codeLabel->setText(tr("（该样本暂无导入属性记录）"));
        return;
    }

    // SET NAMES utf8mb4 已在查询前执行，直接用 toString() 拿到正确 UTF-8 QString
    WARNING_LOG << "[属性查询] 读取字符串长度=" << attrStr.size() << " 内容(前80)=" << attrStr.left(80);

    // 先直接解析
    QJsonDocument doc = QJsonDocument::fromJson(attrStr.toUtf8());
    // 若失败，尝试 toLocal8Bit（兼容连接字符集未完全生效的情况）
    if (doc.isNull() || !doc.isObject()) {
        doc = QJsonDocument::fromJson(attrStr.toLocal8Bit());
    }
    // 再兜底：使用驱动返回的原始字节（与 toString 在部分 JSON 列类型下可能不一致）
    if (doc.isNull() || !doc.isObject()) {
        doc = QJsonDocument::fromJson(attrRawBytes);
    }
    WARNING_LOG << "[属性查询] JSON解析: isNull=" << doc.isNull() << " isObject=" << doc.isObject();
    if (doc.isNull() || !doc.isObject()) {
        WARNING_LOG << "[属性查询] JSON解析彻底失败，原始=" << attrStr.left(60);
        m_codeLabel->setText(tr("（属性数据格式异常）"));
        return;
    }

    const QJsonObject obj = doc.object();
    WARNING_LOG << "[属性查询] 解析成功，keys=" << obj.keys();
    const QString na = tr("—");

    m_codeLabel->setText(obj.value("code").toString(na));
    const int year = obj.value("year").toInt(0);
    m_yearLabel->setText(year > 0 ? QString::number(year) : na);
    m_originLabel->setText(obj.value("origin").toString(na));
    m_partLabel->setText(obj.value("part").toString(na));
    m_gradeLabel->setText(obj.value("grade").toString(na));
    m_stemLeafModeLabel->setText(obj.value("stemLeafMode").toString(na));
    m_detectDateLabel->setText(obj.value("detectDate").toString(na));
    m_factoryLabel->setText(obj.value("factory").toString(na));
    WARNING_LOG << "[属性查询] 界面填充完成";
}
