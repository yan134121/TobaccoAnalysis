#include "DifferenceTableDialog.h"

DifferenceTableDialog::DifferenceTableDialog(QWidget *parent) :
    QDialog(parent)
{
    setWindowTitle("差异度分析");
    setMinimumSize(800, 600); // 设置一个合适的初始大小

    tableWidget = new QTableWidget(this);
    tableWidget->setColumnCount(4); // 四列：样本名字, 算法名字, 算法名字, 排序

    // 设置表头
    QStringList headerLabels;
    headerLabels << "sample" << "algorithm1" << "algorithm2" << "排序";
    tableWidget->setHorizontalHeaderLabels(headerLabels);

    // 允许列宽自动调整
    tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    // 布局
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(tableWidget);
    setLayout(mainLayout);

    // 示例数据 (你可以根据实际需求填充数据)
    // tableWidget->setRowCount(3);
    // tableWidget->setItem(0, 0, new QTableWidgetItem("样本A"));
    // tableWidget->setItem(0, 1, new QTableWidgetItem("算法X"));
    // tableWidget->setItem(0, 2, new QTableWidgetItem("算法Y"));
    // tableWidget->setItem(0, 3, new QTableWidgetItem("1"));

    // tableWidget->setItem(1, 0, new QTableWidgetItem("样本B"));
    // tableWidget->setItem(1, 1, new QTableWidgetItem("算法X"));
    // tableWidget->setItem(1, 2, new QTableWidgetItem("算法Z"));
    // tableWidget->setItem(1, 3, new QTableWidgetItem("2"));

    // tableWidget->setItem(2, 0, new QTableWidgetItem("样本C"));
    // tableWidget->setItem(2, 1, new QTableWidgetItem("算法Y"));
    // tableWidget->setItem(2, 2, new QTableWidgetItem("算法Z"));
    // tableWidget->setItem(2, 3, new QTableWidgetItem("3"));

        tableWidget->setRowCount(3);
    tableWidget->setItem(0, 0, new QTableWidgetItem(""));
    tableWidget->setItem(0, 1, new QTableWidgetItem(""));
    tableWidget->setItem(0, 2, new QTableWidgetItem(""));
    tableWidget->setItem(0, 3, new QTableWidgetItem("1"));

    tableWidget->setItem(1, 0, new QTableWidgetItem(""));
    tableWidget->setItem(1, 1, new QTableWidgetItem(""));
    tableWidget->setItem(1, 2, new QTableWidgetItem(""));
    tableWidget->setItem(1, 3, new QTableWidgetItem("2"));

    tableWidget->setItem(2, 0, new QTableWidgetItem(""));
    tableWidget->setItem(2, 1, new QTableWidgetItem(""));
    tableWidget->setItem(2, 2, new QTableWidgetItem(""));
    tableWidget->setItem(2, 3, new QTableWidgetItem("3"));
}

DifferenceTableDialog::~DifferenceTableDialog()
{
}