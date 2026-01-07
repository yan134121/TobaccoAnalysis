// src/ui/dialogs/SingleMaterialEditDialog.h
#ifndef SINGLEMATERIALEDITDIALOG_H
#define SINGLEMATERIALEDITDIALOG_H

#include <QDialog>
#include "SingleTobaccoSampleData.h" // 引入数据实体
#include "services/SingleTobaccoSampleService.h" // <-- 引入 Service，用于获取现有选项

QT_BEGIN_NAMESPACE
namespace Ui { class SingleMaterialEditDialog; }
QT_END_NAMESPACE

class SingleMaterialEditDialog : public QDialog
{
    Q_OBJECT

public:

    // explicit SingleMaterialEditDialog(const SingleMaterialTobacco& tobacco, QWidget *parent = nullptr);
    explicit SingleMaterialEditDialog(SingleTobaccoSampleService* service, QWidget *parent = nullptr);
    explicit SingleMaterialEditDialog(SingleTobaccoSampleService* service, const SingleTobaccoSampleData& tobacco, QWidget *parent = nullptr);
    
    ~SingleMaterialEditDialog() override; // 虚析构函数

    // 获取对话框中用户输入或修改后的 SingleMaterialTobacco 数据
    SingleTobaccoSampleData getTobacco() const;

private slots:
    // 对应 QDialogButtonBox 的 accepted 信号
    void on_buttonBox_accepted();
    // 对应 QDialogButtonBox 的 rejected 信号
    void on_buttonBox_rejected();

private:
    Ui::SingleMaterialEditDialog *ui;
    SingleTobaccoSampleData m_currentTobacco; // 
    SingleTobaccoSampleService* m_service; // 


    // 辅助函数，用于根据传入的 tobacco 对象初始化UI控件
    void setupUiForEdit(const SingleTobaccoSampleData& tobacco);
    // 辅助函数，用于输入校验
    bool validateInput(QString& errorMessage) const;
    void loadComboBoxOptions();
    // void saveNewComboBoxOption(const QString& propertyName, const QString& newValue);
};

#endif // SINGLEMATERIALEDITDIALOG_H
