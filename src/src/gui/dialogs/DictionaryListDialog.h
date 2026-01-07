#ifndef DICTIONARYLISTDIALOG_H
#define DICTIONARYLISTDIALOG_H

#include <QDialog>
#include <QStandardItemModel>

class DictionaryOptionService;

namespace Ui { class DictionaryListDialog; }

class DictionaryListDialog : public QDialog {
    Q_OBJECT
public:
    explicit DictionaryListDialog(DictionaryOptionService* service, QWidget* parent = nullptr);
    ~DictionaryListDialog();

private:
    void loadAllOptions();

private:
    Ui::DictionaryListDialog* ui;
    QStandardItemModel* m_tableModel;
    DictionaryOptionService* m_service;
};

#endif // DICTIONARYLISTDIALOG_H