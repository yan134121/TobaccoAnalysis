#include "SampleDataTableDialog.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QPushButton>
#include <QMessageBox>

SampleDataTableDialog::SampleDataTableDialog(QWidget *parent) :
    QWidget(parent)
{
    // 设置窗口标题 - 基础标题，将在loadSampleData中更新
    setWindowTitle("样本数据表格");
    
    // 设置窗口属性，允许作为MDI子窗口
    setAttribute(Qt::WA_DeleteOnClose);
    
    // 创建表格控件
    m_tableWidget = new QTableWidget(this);
    m_tableWidget->setAlternatingRowColors(true);
    m_tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    
    // 创建布局
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(m_tableWidget);
    setLayout(mainLayout);
    
    // 设置窗口大小
    resize(600, 400);
}

SampleDataTableDialog::~SampleDataTableDialog()
{
    // 析构函数，不需要删除ui
}

void SampleDataTableDialog::loadSampleData(const struct NavigatorNodeInfo& info)
{
    // 清空表格
    m_tableWidget->clear();
    m_tableWidget->setRowCount(0);
    
    // 更新窗口标题，显示样本的详细信息
    QString title = QString("样本数据表格 -  %1-%2-%3-(%4), %5")
                    .arg(info.projectName)
                    .arg(info.batchCode)
                    .arg(info.shortCode)
                    .arg(info.parallelNo)
                    .arg(info.dataType);
    setWindowTitle(title);
    
    // 根据数据类型设置表头和获取数据
    QString dataType = info.dataType;
    QString error;
    
    if (dataType == "大热重" || dataType == "工序大热重") {
        // 设置表头
        m_tableWidget->setColumnCount(2);
        QStringList headers;
        headers << "序号" << "重量";
        m_tableWidget->setHorizontalHeaderLabels(headers);
        
        // 获取数据
        auto curveData = m_dao.getSampleCurveData(info.id, dataType, error);
        if (!error.isEmpty()) {
            QMessageBox::warning(this, "错误", "获取样本数据失败: " + error);
            return;
        }
        
        // 填充表格
        m_tableWidget->setRowCount(curveData.size());
        for (int i = 0; i < curveData.size(); ++i) {
            m_tableWidget->setItem(i, 0, new QTableWidgetItem(QString::number(curveData[i].x())));
            m_tableWidget->setItem(i, 1, new QTableWidgetItem(QString::number(curveData[i].y())));
        }
    } else if (dataType == "小热重" || dataType == "小热重（原始数据）") {
        // 设置表头
        m_tableWidget->setColumnCount(2);
        QStringList headers;
        headers << "温度" << "DTG值";
        m_tableWidget->setHorizontalHeaderLabels(headers);
        
        // 获取数据
        auto curveData = m_dao.getSampleCurveData(info.id, dataType, error);
        if (!error.isEmpty()) {
            QMessageBox::warning(this, "错误", "获取样本数据失败: " + error);
            return;
        }
        
        // 填充表格
        m_tableWidget->setRowCount(curveData.size());
        for (int i = 0; i < curveData.size(); ++i) {
            m_tableWidget->setItem(i, 0, new QTableWidgetItem(QString::number(curveData[i].x())));
            m_tableWidget->setItem(i, 1, new QTableWidgetItem(QString::number(curveData[i].y())));
        }
    } else if (dataType == "色谱") {
        // 设置表头
        m_tableWidget->setColumnCount(2);
        QStringList headers;
        headers << "保留时间" << "响应值";
        m_tableWidget->setHorizontalHeaderLabels(headers);
        
        // 获取数据
        auto curveData = m_dao.getSampleCurveData(info.id, dataType, error);
        if (!error.isEmpty()) {
            QMessageBox::warning(this, "错误", "获取样本数据失败: " + error);
            return;
        }
        
        // 填充表格
        m_tableWidget->setRowCount(curveData.size());
        for (int i = 0; i < curveData.size(); ++i) {
            m_tableWidget->setItem(i, 0, new QTableWidgetItem(QString::number(curveData[i].x())));
            m_tableWidget->setItem(i, 1, new QTableWidgetItem(QString::number(curveData[i].y())));
        }
    } else {
        QMessageBox::information(this, "提示", "不支持的数据类型: " + dataType);
        return;
    }
    
    // 调整列宽
    m_tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
}
