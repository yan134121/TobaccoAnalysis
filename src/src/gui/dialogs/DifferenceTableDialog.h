#ifndef DIFFERENCETABLEDIALOG_H
#define DIFFERENCETABLEDIALOG_H

#include <QDialog>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QHeaderView>

class DifferenceTableDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DifferenceTableDialog(QWidget *parent = nullptr);
    ~DifferenceTableDialog();

private:
    QTableWidget *tableWidget;
};

#endif // DIFFERENCETABLEDIALOG_H