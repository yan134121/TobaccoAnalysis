#include "SamplePropertiesDialog.h"
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

SamplePropertiesDialog::SamplePropertiesDialog(QWidget *parent) :
    QDialog(parent)
{
    // 创建UI
    
    // 设置窗口标题
    setWindowTitle("样本属性");
    
    // 创建布局
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    QFormLayout* formLayout = new QFormLayout();
    
    // 添加表单字段
    m_projectNameLabel = new QLabel(this);
    m_batchCodeLabel = new QLabel(this);
    m_shortCodeLabel = new QLabel(this);
    m_parallelNoLabel = new QLabel(this);
    m_dataTypeLabel = new QLabel(this);
    
    formLayout->addRow("烟牌号:", m_projectNameLabel);
    formLayout->addRow("批次编码:", m_batchCodeLabel);
    formLayout->addRow("短码:", m_shortCodeLabel);
    formLayout->addRow("平行样编号:", m_parallelNoLabel);
    formLayout->addRow("数据类型:", m_dataTypeLabel);
    
    m_sampleID = new QLabel(this);
    formLayout->addRow("样本ID:", m_sampleID);
    
    // 添加确定按钮
    QPushButton* okButton = new QPushButton("确定", this);
    connect(okButton, &QPushButton::clicked, this, &QDialog::accept);
    
    // 设置布局
    mainLayout->addLayout(formLayout);
    mainLayout->addWidget(okButton, 0, Qt::AlignRight);
    setLayout(mainLayout);
    
    // 不再需要保存到ui中，因为我们已经使用成员变量
    
    // 设置窗口大小
    resize(400, 200);
}

SamplePropertiesDialog::~SamplePropertiesDialog()
{
    // 析构函数，不需要删除ui
}

void SamplePropertiesDialog::setSampleInfo(const struct NavigatorNodeInfo& info)
{
    // 设置样本信息
    m_projectNameLabel->setText(info.projectName);
    m_batchCodeLabel->setText(info.batchCode);
    m_shortCodeLabel->setText(info.shortCode);
    m_parallelNoLabel->setText(QString::number(info.parallelNo));
    m_sampleID->setText(QString::number(info.id));
    
    // 设置数据类型，并区分样本数据和工序数据
    QString dataTypeText = info.dataType;
    if (dataTypeText.contains("工序")) {
        m_dataTypeLabel->setText(dataTypeText + " (工序数据)");
    } else {
        m_dataTypeLabel->setText(dataTypeText + " (样本数据)");
    }
}