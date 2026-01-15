#ifndef EXCELPREVIEWDIALOG_H
#define EXCELPREVIEWDIALOG_H

#include <QDialog>
#include <QStandardItemModel>
#include <QTableView>
#include <QComboBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

class ExcelPreviewDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ExcelPreviewDialog(const QString& filePath, QWidget *parent = nullptr);
    ~ExcelPreviewDialog() override;

private slots:
    void on_sheetComboBox_currentIndexChanged(int index);
    void on_refreshButton_clicked();

private:
    void loadExcelFile();
    void loadSheet(int sheetIndex);
    QString m_filePath;
    QComboBox* m_sheetComboBox;
    QTableView* m_tableView;
    QStandardItemModel* m_model;
    QPushButton* m_refreshButton;
    QLabel* m_statusLabel;
    
    // 存储工作表名称列表
    QStringList m_sheetNames;
};

#endif // EXCELPREVIEWDIALOG_H
