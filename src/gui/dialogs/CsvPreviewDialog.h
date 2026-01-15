#ifndef CSVPREVIEWDIALOG_H
#define CSVPREVIEWDIALOG_H

#include <QDialog>
#include <QStandardItemModel>
#include <QTableView>
#include <QComboBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

class CsvPreviewDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CsvPreviewDialog(const QString& dirPath, QWidget *parent = nullptr);
    ~CsvPreviewDialog() override;

private slots:
    void on_fileComboBox_currentIndexChanged(int index);
    void on_refreshButton_clicked();

private:
    void scanCsvFiles();
    void loadCsvFile(const QString& filePath);
    QString m_dirPath;
    QComboBox* m_fileComboBox;
    QTableView* m_tableView;
    QStandardItemModel* m_model;
    QPushButton* m_refreshButton;
    QLabel* m_statusLabel;
    
    // 存储CSV文件路径列表
    QStringList m_csvFilePaths;
};

#endif // CSVPREVIEWDIALOG_H
