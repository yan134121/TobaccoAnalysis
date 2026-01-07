#include "DictionaryListDialog.h"
#include "ui_dictionarylistdialog.h"
#include "services/DictionaryOptionService.h"
#include <QHeaderView>

DictionaryListDialog::DictionaryListDialog(DictionaryOptionService* service, QWidget* parent)
    : QDialog(parent), ui(new Ui::DictionaryListDialog), m_tableModel(new QStandardItemModel(this)), m_service(service)
{
    ui->setupUi(this);
    ui->tableView->setModel(m_tableModel);
    m_tableModel->setHorizontalHeaderLabels({"分类", "值", "描述"});
    ui->tableView->horizontalHeader()->setStretchLastSection(true);
    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    loadAllOptions();
}

DictionaryListDialog::~DictionaryListDialog() { delete ui; }

void DictionaryListDialog::loadAllOptions()
{
    if (!m_service || !m_service->getDAO()) return;
    auto list = m_service->getDAO()->getAll();
    m_tableModel->setRowCount(0);
    for (const auto& opt : list) {
        QList<QStandardItem*> row;
        row << new QStandardItem(opt.getCategory());
        row << new QStandardItem(opt.getValue());
        row << new QStandardItem(opt.getDescription());
        m_tableModel->appendRow(row);
    }
}