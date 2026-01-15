#include "CsvPreviewDialog.h"
#include "utils/file_handler/CsvParser.h"
#include <QHeaderView>
#include <QMessageBox>
#include <QFileInfo>
#include <QTimer>
#include <QApplication>
#include <QDirIterator>
#include <QDir>

CsvPreviewDialog::CsvPreviewDialog(const QString& dirPath, QWidget *parent)
    : QDialog(parent)
    , m_dirPath(dirPath)
{
    setWindowTitle(tr("CSV文件预览 - %1").arg(QDir(dirPath).dirName()));
    setMinimumSize(800, 600);
    resize(1000, 700);
    // 设置为非模态对话框，允许在选择列对话框打开时继续操作
    setWindowModality(Qt::NonModal);

    // 主布局
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // 顶部工具栏布局
    QHBoxLayout* toolbarLayout = new QHBoxLayout();
    
    QLabel* fileLabel = new QLabel(tr("CSV文件:"), this);
    m_fileComboBox = new QComboBox(this);
    m_fileComboBox->setMinimumWidth(300);
    
    m_refreshButton = new QPushButton(tr("刷新"), this);
    
    m_statusLabel = new QLabel(this);
    
    toolbarLayout->addWidget(fileLabel);
    toolbarLayout->addWidget(m_fileComboBox);
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
    connect(m_fileComboBox, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &CsvPreviewDialog::on_fileComboBox_currentIndexChanged);
    connect(m_refreshButton, &QPushButton::clicked,
            this, &CsvPreviewDialog::on_refreshButton_clicked);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);

    // 延迟扫描和加载CSV文件，避免阻塞UI响应
    m_statusLabel->setText(tr("正在扫描文件夹..."));
    QTimer::singleShot(0, this, [this]() {
        scanCsvFiles();
    });
}

CsvPreviewDialog::~CsvPreviewDialog()
{
    // 无需特殊清理
}

void CsvPreviewDialog::scanCsvFiles()
{
    m_csvFilePaths.clear();
    m_fileComboBox->clear();
    
    // 使用 QDirIterator 支持递归扫描
    QDirIterator it(m_dirPath, QStringList() << "*.csv", QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString filePath = it.next();
        m_csvFilePaths.append(filePath);
        
        // 显示相对路径，更易读
        QDir dir(m_dirPath);
        QString relativePath = dir.relativeFilePath(filePath);
        m_fileComboBox->addItem(relativePath);
    }
    
    if (m_csvFilePaths.isEmpty()) {
        QMessageBox::warning(this, tr("警告"), tr("文件夹中没有找到CSV文件。"));
        m_statusLabel->setText(tr("未找到CSV文件"));
        return;
    }
    
    // 加载第一个CSV文件
    if (m_fileComboBox->count() > 0) {
        loadCsvFile(m_csvFilePaths.at(0));
    }
}

void CsvPreviewDialog::loadCsvFile(const QString& filePath)
{
    // 创建CSV解析器
    CsvParser parser;
    QString errorMessage;
    
    // 更新状态，显示正在加载
    QFileInfo fileInfo(filePath);
    m_statusLabel->setText(tr("正在加载文件: %1...").arg(fileInfo.fileName()));
    QApplication::processEvents(); // 更新UI显示
    
    // 预览文件头部几行，总是从行1列1开始解析
    QList<QVariantList> rawPreviewData = parser.parseFile(filePath, errorMessage, 1, 1);
    
    if (!errorMessage.isEmpty() || rawPreviewData.isEmpty()) {
        if (errorMessage.isEmpty()) {
            errorMessage = tr("CSV文件为空或无法读取");
        }
        QMessageBox::critical(this, tr("错误"), tr("无法打开CSV文件:\n%1\n%2").arg(filePath, errorMessage));
        m_statusLabel->setText(tr("无法加载文件"));
        return;
    }

    // 固定显示前50行
    const int maxPreviewRows = 50;
    
    // 设置列数 (取最宽一行的列数，或限制最大列数)
    int maxCols = 0;
    for(const QVariantList& row : rawPreviewData) {
        if (row.size() > maxCols) maxCols = row.size();
    }
    // 限制最大列数为10
    const int maxPreviewCols = qMin(maxCols, 10);
    
    // 清空模型
    m_model->clear();

    // 固定设置列数和行数
    m_model->setColumnCount(maxPreviewCols);
    m_model->setRowCount(maxPreviewRows);

    // 设置列标题为列号（从1开始）
    QStringList headers;
    for (int col = 1; col <= maxPreviewCols; ++col) {
        headers << QString("列 %1").arg(col);
    }
    m_model->setHorizontalHeaderLabels(headers);

    // 填充模型，限制预览行数
    int actualRowCount = 0;
    int actualColCount = 0;
    
    for (int rowIdx = 0; rowIdx < qMin(rawPreviewData.size(), maxPreviewRows); ++rowIdx) {
        const QVariantList& rowData = rawPreviewData.at(rowIdx);
        bool hasDataInRow = false;
        int maxColInRow = 0;
        
        for (int colIdx = 0; colIdx < qMin(rowData.size(), maxPreviewCols); ++colIdx) {
            QString cellText = rowData.at(colIdx).toString();
            
            if (!cellText.isEmpty()) {
                hasDataInRow = true;
                maxColInRow = qMax(maxColInRow, colIdx + 1);
            }
            
            QStandardItem* item = new QStandardItem(cellText);
            m_model->setItem(rowIdx, colIdx, item);
        }
        
        if (hasDataInRow) {
            actualRowCount = rowIdx + 1;
            actualColCount = qMax(actualColCount, maxColInRow);
        }
    }

    // 设置行标题为行号（从1开始），固定50行
    for (int row = 0; row < maxPreviewRows; ++row) {
        m_model->setVerticalHeaderItem(row, new QStandardItem(QString::number(row + 1)));
    }

    // 自动调整列宽
    m_tableView->resizeColumnsToContents();

    // 更新状态标签
    QString fileName = fileInfo.fileName();
    if (actualRowCount > 0) {
        if (rawPreviewData.size() >= maxPreviewRows || maxCols >= maxPreviewCols) {
            m_statusLabel->setText(tr("文件: %1 | 显示前 %2 行, 前 %3 列（文件共有 %4 行, %5 列，可能还有更多数据）")
                                   .arg(fileName)
                                   .arg(maxPreviewRows)
                                   .arg(maxPreviewCols)
                                   .arg(rawPreviewData.size())
                                   .arg(maxCols));
        } else {
            m_statusLabel->setText(tr("文件: %1 | 共 %2 行, %3 列")
                                   .arg(fileName)
                                   .arg(actualRowCount)
                                   .arg(actualColCount));
        }
    } else {
        // 文件为空
        m_statusLabel->setText(tr("文件 '%1' 为空或无效").arg(fileName));
    }
}

void CsvPreviewDialog::on_fileComboBox_currentIndexChanged(int index)
{
    if (index >= 0 && index < m_csvFilePaths.size()) {
        loadCsvFile(m_csvFilePaths.at(index));
    }
}

void CsvPreviewDialog::on_refreshButton_clicked()
{
    scanCsvFiles();
}

#include "CsvPreviewDialog.moc"
