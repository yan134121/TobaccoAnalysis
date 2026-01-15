#include "ExcelPreviewDialog.h"
#include "third_party/QXlsx/header/xlsxdocument.h"
#include <QHeaderView>
#include <QMessageBox>
#include <QFileInfo>
#include <QTimer>
#include <QApplication>

ExcelPreviewDialog::ExcelPreviewDialog(const QString& filePath, QWidget *parent)
    : QDialog(parent)
    , m_filePath(filePath)
    , m_xlsxDocument(nullptr)
{
    setWindowTitle(tr("Excel文件预览 - %1").arg(QFileInfo(filePath).fileName()));
    setMinimumSize(800, 600);
    resize(1000, 700);
    // 设置为非模态对话框，允许在选择列对话框打开时继续操作
    setWindowModality(Qt::NonModal);

    // 主布局
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // 顶部工具栏布局
    QHBoxLayout* toolbarLayout = new QHBoxLayout();
    
    QLabel* sheetLabel = new QLabel(tr("工作表:"), this);
    m_sheetComboBox = new QComboBox(this);
    m_sheetComboBox->setMinimumWidth(200);
    
    m_refreshButton = new QPushButton(tr("刷新"), this);
    
    m_statusLabel = new QLabel(this);
    
    toolbarLayout->addWidget(sheetLabel);
    toolbarLayout->addWidget(m_sheetComboBox);
    toolbarLayout->addWidget(m_refreshButton);
    toolbarLayout->addStretch();
    toolbarLayout->addWidget(m_statusLabel);
    
    mainLayout->addLayout(toolbarLayout);

    // 表格视图
    m_tableView = new QTableView(this);
    m_model = new QStandardItemModel(this);
    m_tableView->setModel(m_model);
    m_tableView->setSelectionBehavior(QAbstractItemView::SelectItems);
    m_tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableView->horizontalHeader()->setStretchLastSection(true);
    m_tableView->verticalHeader()->setVisible(true);
    
    mainLayout->addWidget(m_tableView);

    // 底部按钮
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    QPushButton* closeButton = new QPushButton(tr("关闭"), this);
    buttonLayout->addWidget(closeButton);
    mainLayout->addLayout(buttonLayout);

    // 连接信号
    connect(m_sheetComboBox, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &ExcelPreviewDialog::on_sheetComboBox_currentIndexChanged);
    connect(m_refreshButton, &QPushButton::clicked,
            this, &ExcelPreviewDialog::on_refreshButton_clicked);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);

    // 延迟加载Excel文件，避免阻塞UI响应
    m_statusLabel->setText(tr("正在加载文件..."));
    QTimer::singleShot(0, this, [this]() {
        loadExcelFile();
    });
}

ExcelPreviewDialog::~ExcelPreviewDialog()
{
    // 释放缓存的Document对象
    if (m_xlsxDocument) {
        delete m_xlsxDocument;
        m_xlsxDocument = nullptr;
    }
}

void ExcelPreviewDialog::loadExcelFile()
{
    // 如果已经有缓存的Document，先释放
    if (m_xlsxDocument) {
        delete m_xlsxDocument;
        m_xlsxDocument = nullptr;
    }
    
    // 创建并加载Document（只加载一次，后续复用）
    m_xlsxDocument = new QXlsx::Document(m_filePath);
    if (!m_xlsxDocument->load()) {
        QMessageBox::critical(this, tr("错误"), tr("无法打开Excel文件:\n%1").arg(m_filePath));
        m_statusLabel->setText(tr("无法加载文件"));
        delete m_xlsxDocument;
        m_xlsxDocument = nullptr;
        return;
    }

    // 获取所有工作表名称（这个操作很快，只读取文件头信息）
    m_sheetNames = m_xlsxDocument->sheetNames();
    if (m_sheetNames.isEmpty()) {
        QMessageBox::warning(this, tr("警告"), tr("Excel文件中没有工作表。"));
        m_statusLabel->setText(tr("文件中没有工作表"));
        delete m_xlsxDocument;
        m_xlsxDocument = nullptr;
        return;
    }

    // 填充工作表下拉框
    m_sheetComboBox->clear();
    m_sheetComboBox->addItems(m_sheetNames);

    // 加载第一个工作表（使用缓存的Document）
    if (m_sheetComboBox->count() > 0) {
        loadSheet(0);
    }
}

void ExcelPreviewDialog::loadSheet(int sheetIndex)
{
    if (sheetIndex < 0 || sheetIndex >= m_sheetNames.size()) {
        return;
    }

    // 使用缓存的Document，避免重复加载文件
    if (!m_xlsxDocument) {
        m_statusLabel->setText(tr("文件未加载"));
        return;
    }

    // 选择工作表（使用已缓存的Document）
    QString sheetName = m_sheetNames.at(sheetIndex);
    if (!m_xlsxDocument->selectSheet(sheetName)) {
        m_statusLabel->setText(tr("无法选择工作表: %1").arg(sheetName));
        return;
    }

    QXlsx::Worksheet* worksheet = m_xlsxDocument->currentWorksheet();
    if (!worksheet) {
        m_statusLabel->setText(tr("工作表无效"));
        return;
    }
    
    // 更新状态，显示正在加载
    m_statusLabel->setText(tr("正在加载工作表: %1...").arg(sheetName));
    QApplication::processEvents(); // 更新UI显示

    // 固定显示前50行、10列
    const int maxPreviewRows = 50;
    const int maxPreviewCols = 10;
    
    // 清空模型
    m_model->clear();

    // 固定设置列数为10，行数为50
    m_model->setColumnCount(maxPreviewCols);
    m_model->setRowCount(maxPreviewRows);

    // 设置列标题为列号（从1开始）
    QStringList headers;
    for (int col = 1; col <= maxPreviewCols; ++col) {
        headers << QString("列 %1").arg(col);
    }
    m_model->setHorizontalHeaderLabels(headers);

    // 直接读取前50行数据，不调用dimension()以避免扫描整个工作表
    int actualRowCount = 0;
    int actualColCount = 0;
    
    for (int row = 1; row <= maxPreviewRows; ++row) {
        bool hasDataInRow = false;
        int maxColInRow = 0;
        
        for (int col = 1; col <= maxPreviewCols; ++col) {
            QVariant cellValue = worksheet->read(row, col);
            QString cellText = cellValue.toString();
            
            if (!cellText.isEmpty()) {
                hasDataInRow = true;
                maxColInRow = qMax(maxColInRow, col);
            }
            
            QStandardItem* item = new QStandardItem(cellText);
            m_model->setItem(row - 1, col - 1, item);
        }
        
        if (hasDataInRow) {
            actualRowCount = row;
            actualColCount = qMax(actualColCount, maxColInRow);
        } else if (row > 1) {
            // 如果遇到空行且不是第一行，可以提前停止（可选优化）
            // 但为了保持一致性，仍然读取到50行
        }
    }

    // 设置行标题为行号（从1开始），固定50行
    for (int row = 0; row < maxPreviewRows; ++row) {
        m_model->setVerticalHeaderItem(row, new QStandardItem(QString::number(row + 1)));
    }

    // 自动调整列宽
    m_tableView->resizeColumnsToContents();

    // 更新状态标签
    if (actualRowCount > 0) {
        if (actualRowCount >= maxPreviewRows || actualColCount >= maxPreviewCols) {
            m_statusLabel->setText(tr("工作表: %1 | 显示前 %2 行, 前 %3 列（可能还有更多数据）")
                                   .arg(sheetName)
                                   .arg(maxPreviewRows)
                                   .arg(maxPreviewCols));
        } else {
            m_statusLabel->setText(tr("工作表: %1 | 共 %2 行, %3 列")
                                   .arg(sheetName)
                                   .arg(actualRowCount)
                                   .arg(actualColCount));
        }
    } else {
        // 工作表为空
        m_statusLabel->setText(tr("工作表 '%1' 为空或无效").arg(sheetName));
    }
}

void ExcelPreviewDialog::on_sheetComboBox_currentIndexChanged(int index)
{
    if (index >= 0) {
        loadSheet(index);
    }
}

void ExcelPreviewDialog::on_refreshButton_clicked()
{
    loadExcelFile();
}

#include "ExcelPreviewDialog.moc"
