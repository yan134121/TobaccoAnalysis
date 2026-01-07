#ifndef SENSORDATAIMPORTCONFIGDIALOG_H
#define SENSORDATAIMPORTCONFIGDIALOG_H

#include <QDialog>
#include <QMap>
#include <QVariant>
#include <QStandardItemModel> // <-- 
#include "services/SingleTobaccoSampleService.h" // 引入 DataColumnMapping 结构体

#include "utils/file_handler/FileHandlerFactory.h" // <-- 
#include "utils/file_handler/AbstractFileParser.h" // <-- 

QT_BEGIN_NAMESPACE
namespace Ui { class SensorDataImportConfigDialog; }
QT_END_NAMESPACE

class SensorDataImportConfigDialog : public QDialog
{
    Q_OBJECT

public:
//     // 构造函数，传入当前选中的样本编码和要导入的数据类型
//     explicit SensorDataImportConfigDialog(const QString& sampleCode, const QString& sensorType, QWidget *parent = nullptr);
    // --- 关键修改：构造函数接收 FileHandlerFactory* 参数 ---
    explicit SensorDataImportConfigDialog(const QString& sampleCode, const QString& sensorType, FileHandlerFactory* factory, QWidget *parent = nullptr);
    ~SensorDataImportConfigDialog() override;

    // 获取用户配置的参数
    QString getFilePath() const;
    int getStartDataRow() const;
    int getStartDataCol() const;
    int getReplicateNo() const;
    DataColumnMapping getDataColumnMapping() const;

private slots:
    void on_browseFileButton_clicked();
    void on_buttonBox_accepted();
    void on_buttonBox_rejected();

    void on_previewButton_clicked(); // <-- 槽函数

    // 可以在这里连接 QLineEdit/QSpinBox 的 textChanged/valueChanged 信号
    // 当这些值改变时自动触发预览更新 (可选，但很方便)
    void on_filePathLineEdit_textChanged(const QString& text); // <-- 
    void on_startDataRowSpinBox_valueChanged(int value);    // <-- 
    void on_startDataColSpinBox_valueChanged(int value);    // <-- 

private:
    Ui::SensorDataImportConfigDialog *ui;
    QString m_sampleCode;
    QString m_sensorType;
    DataColumnMapping m_mapping; // 内部存储映射配置

    QStandardItemModel* m_previewModel = nullptr; // <-- ：预览表格的模型
    FileHandlerFactory* m_fileHandlerFactory = nullptr; // <-- ：用于预览的解析器工厂

    void setupMappingUI(); // 根据传感器类型动态设置列映射区域的UI
    void populateMappingFromUI(); // 从 UI 控件的值填充 m_mapping
    bool validateInput(QString& errorMessage) const; // 验证用户输入

    void updateFilePreview(); // <-- 辅助函数，用于更新预览表格

};

#endif // SENSORDATAIMPORTCONFIGDIALOG_H
