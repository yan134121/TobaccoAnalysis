

// AddCurveDialog.h
#pragma once
#include <QDialog>
#include <QTreeWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QSplitter>
#include <QMdiArea>
#include <QMdiSubWindow>
#include "DataNavigator.h"  // 包含导航树头文件

class AddCurveDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AddCurveDialog(const QString &dataType, QWidget *parent = nullptr);

        // 在 AddCurveDialog.h 中添加
    QMap<int, QString> getSelectedSampleNames() const;

    void setExistingCurves(const QList<int>& curveIds);

private slots:
    void onNavigatorItemClicked(QTreeWidgetItem *item, int column);
    void updateRightPanel(const QVariantMap &sampleData);

private:
    void setupLeftNavigator(const QString &dataType);
    void setupRightPanel();
    void clearRightPanel();
    
    QMdiArea *m_mdiArea = nullptr;
    QSplitter *m_mainSplitter = nullptr;
    DataNavigator *m_dataNavigator = nullptr;  // 使用 DataNavigator
    QTreeWidget *m_treeWidgetRight = nullptr;
    QList<QVariantMap> m_samplesData;

    QTreeWidgetItem* m_rootItem = nullptr;

    QSet<int> m_existingCurveIds; // 存储已经绘制的曲线 ID
};