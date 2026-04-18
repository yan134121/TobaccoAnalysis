#ifndef NAVIGATORATTRIBUTEFILTERDIALOG_H
#define NAVIGATORATTRIBUTEFILTERDIALOG_H

#include <QDialog>
#include <QJsonObject>

class QComboBox;
class QLineEdit;
class QDialogButtonBox;

/// 数据导航「属性分类」：按样本表字段筛选，留空的项不参与筛选
class NavigatorAttributeFilterDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NavigatorAttributeFilterDialog(const QString& dataTypeTitle, QWidget* parent = nullptr);

    void setFilter(const QJsonObject& filter);
    QJsonObject filter() const;

private:
    void setupUi();

    QComboBox* m_yearCombo = nullptr;
    QLineEdit* m_originEdit = nullptr;
    QComboBox* m_partCombo = nullptr;
    QComboBox* m_gradeCombo = nullptr;
    QLineEdit* m_typeEdit = nullptr;
    QLineEdit* m_projectEdit = nullptr;
    QLineEdit* m_batchCodeEdit = nullptr;
    QDialogButtonBox* m_buttonBox = nullptr;
};

#endif
