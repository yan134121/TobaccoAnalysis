

// AddCurveDialog.cpp
#include "AddCurveDialog.h"
#include <QDebug>
#include "Logger.h"
#include <QSplitter>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QHBoxLayout>
#include <QApplication>
#include <QStyle>
#include <QMessageBox>

AddCurveDialog::AddCurveDialog(const QString &dataType, QWidget *parent)
    : QDialog(parent)
{
    // setWindowTitle(tr("选择要添加的曲线数据"));
    setWindowTitle(tr("-"));
    resize(800, 600);

    // 主布局
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // 创建MDI区域
    m_mdiArea = new QMdiArea(this);
    m_mdiArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_mdiArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    // 创建分割器
    m_mainSplitter = new QSplitter(Qt::Horizontal, m_mdiArea);
    
    // 设置左右面板
    setupLeftNavigator(dataType);
    setupRightPanel();

    // 将分割器添加到MDI区域
    QMdiSubWindow *subWindow = m_mdiArea->addSubWindow(m_mainSplitter);
    if (dataType == "大热重") {
        subWindow->setWindowTitle(tr("大热重数据选择"));
    } else if (dataType == "小热重") {
        subWindow->setWindowTitle(tr("小热重数据选择"));
    } else if (dataType == "小热重（原始数据）") {
        subWindow->setWindowTitle(tr("小热重（原始数据）选择"));
    } else if (dataType == "色谱") {
        subWindow->setWindowTitle(tr("色谱数据选择"));
    } else {
        subWindow->setWindowTitle(tr("曲线数据选择"));
    }
    // subWindow->setWindowTitle(tr("曲线数据选择"));
    subWindow->setAttribute(Qt::WA_DeleteOnClose, false);
    
    // 设置分割比例
    m_mainSplitter->setStretchFactor(0, 1); // 左边导航树占1份
    m_mainSplitter->setStretchFactor(1, 2); // 右边详细信息占2份
    
    // 确保子窗口填满MDI区域
    subWindow->showMaximized();

    mainLayout->addWidget(m_mdiArea);

    // 底部按钮
    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    mainLayout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    
    // 连接导航树的信号
    connect(m_dataNavigator, &DataNavigator::itemClicked, 
            this, &AddCurveDialog::onNavigatorItemClicked);
}



void AddCurveDialog::setupRightPanel()
{
    m_treeWidgetRight = new QTreeWidget(m_mainSplitter);
    m_treeWidgetRight->setColumnCount(1);
    m_treeWidgetRight->setHeaderLabels({tr("已选样本")});
    m_treeWidgetRight->setSelectionMode(QAbstractItemView::NoSelection);
    m_treeWidgetRight->setExpandsOnDoubleClick(false);
    
    m_mainSplitter->addWidget(m_treeWidgetRight);
}

void AddCurveDialog::onNavigatorItemClicked(QTreeWidgetItem *item, int column)
{
    if (!item) return;

    // 获取样本 ID
    bool ok;
    int sampleId = item->data(0, Qt::UserRole).toInt(&ok);
    
    if (ok && item->checkState(0) == Qt::Checked) {
        // 检查是否已经绘制
        if (m_existingCurveIds.contains(sampleId)) {
            // 如果已经绘制，显示提示并取消选中
            QMessageBox::information(this, tr("提示"), 
                tr("样本 %1 已经被绘制，不能重复选择").arg(sampleId));
            item->setCheckState(0, Qt::Unchecked);
            return;
        }
    
    // 检查是否是样本项（有sample_id数据）
    QVariant sampleIdVar = item->data(0, Qt::UserRole);
    if (sampleIdVar.isValid()) {
        // 如果是样本项，根据选中状态更新右侧面板
        if (item->checkState(0) == Qt::Checked) {
            // 样本被选中，添加到右侧面板
            int sampleId = sampleIdVar.toInt();
            
            // 查找对应的样本数据
            for (const auto &sample : m_samplesData) {
                if (sample["sample_id"].toInt() == sampleId) {
                    updateRightPanel(sample);
                    break;
                }
            }
        } else {
            // 样本被取消选中，从右侧面板移除
            int sampleId = sampleIdVar.toInt();
            
            // 遍历右侧面板中的所有项，移除匹配的样本
            for (int i = 0; i < m_treeWidgetRight->topLevelItemCount(); i++) {
                QTreeWidgetItem *rightItem = m_treeWidgetRight->topLevelItem(i);
                if (rightItem->data(0, Qt::UserRole).toInt() == sampleId) {
                    delete m_treeWidgetRight->takeTopLevelItem(i);
                    break;
                }
            }
        }
    } else {
        // 如果是文件夹节点，不做任何操作
        // 用户可以通过选择/取消选择样本来管理右侧面板
    }

}
}

// 添加清除右侧面板的方法
void AddCurveDialog::clearRightPanel()
{
    m_treeWidgetRight->clear();
}

void AddCurveDialog::updateRightPanel(const QVariantMap &sampleData)
{
    // 创建样本名称，统一为 short_code(parallel_no)-timestamp
    QString shortCode = sampleData.value("short_code").toString();
    int parallelNo = sampleData.value("parallel_no").toInt();
    QDateTime dt;
    QVariant detectVar = sampleData.value("detect_date");
    if (detectVar.isValid()) {
        if (detectVar.canConvert<QDateTime>()) {
            dt = detectVar.toDateTime();
        } else {
            QDate d = detectVar.toDate();
            if (d.isValid()) {
                dt = QDateTime(d);
            }
        }
    }
    if (!dt.isValid()) {
        QVariant createdVar = sampleData.value("created_at");
        if (createdVar.isValid() && createdVar.canConvert<QDateTime>()) {
            dt = createdVar.toDateTime();
        }
    }
    QString timestamp = QStringLiteral("N/A");
    if (dt.isValid()) {
        timestamp = dt.toString("yyyyMMdd_HHmmss");
    }
    QString sampleDisplayName;
    if (!shortCode.isEmpty() && parallelNo > 0) {
        sampleDisplayName = QString("%1(%2)-%3")
                                .arg(shortCode)
                                .arg(parallelNo)
                                .arg(timestamp);
    } else {
        sampleDisplayName = sampleData.value("sample_name").toString();
        if (sampleDisplayName.trimmed().isEmpty()) {
            sampleDisplayName = QString("样本 %1").arg(sampleData.value("sample_id").toInt());
        }
    }
    
    // 添加样本名称
    QTreeWidgetItem *nameItem = new QTreeWidgetItem(m_treeWidgetRight);
    nameItem->setText(0, sampleDisplayName);
    nameItem->setFont(0, QFont("", -1, QFont::Bold));
    nameItem->setData(0, Qt::UserRole, sampleData.value("sample_id"));
    
    // 调整列宽
    m_treeWidgetRight->resizeColumnToContents(0);
}

// QList<int> AddCurveDialog::selectedSampleIds() const
// {
//     QList<int> ids;
    
//     // 遍历导航树中的所有项，找出被选中的样本
//     QTreeWidgetItemIterator it(m_dataNavigator);
//     while (*it) {
//         QTreeWidgetItem *item = *it;
//         QVariant sampleIdVar = item->data(0, Qt::UserRole);
//         if (sampleIdVar.isValid() && item->checkState(0) == Qt::Checked) {
//             ids << sampleIdVar.toInt();
//         }
//         ++it;
//     }
    
//     return ids;
// }


void AddCurveDialog::setupLeftNavigator(const QString &dataType)
{
    // 使用 DataNavigator 作为左侧导航树
    m_dataNavigator = new DataNavigator(m_mainSplitter);
    
    // 清除 DataNavigator 默认创建的节点
    m_dataNavigator->clear();
    
    // 创建根节点
    QTreeWidgetItem* rootItem = nullptr;
    
    // 根据数据类型创建不同的根节点
    if (dataType == "大热重") {
        rootItem = new QTreeWidgetItem(m_dataNavigator, {tr("样本数据")});
    } else if (dataType == "小热重") {
        rootItem = new QTreeWidgetItem(m_dataNavigator, {tr("样本数据")});
    } else if (dataType == "小热重（原始数据）") {
        rootItem = new QTreeWidgetItem(m_dataNavigator, {tr("样本数据")});
    } else if (dataType == "色谱") {
        rootItem = new QTreeWidgetItem(m_dataNavigator, {tr("样本数据")});
    } else {
        rootItem = new QTreeWidgetItem(m_dataNavigator, {tr("数据集")});
    }
    
    // 设置根节点图标
    rootItem->setIcon(0, m_dataNavigator->style()->standardIcon(QStyle::SP_DirOpenIcon));
    
    // 添加根节点到树中
    m_dataNavigator->addTopLevelItem(rootItem);
    
    // 展开根节点
    rootItem->setExpanded(true);
    
    // 保存根节点的引用，以便后续使用
    m_rootItem = rootItem;
    
    m_mainSplitter->addWidget(m_dataNavigator);
}




// 在 AddCurveDialog.cpp 中实现
QMap<int, QString> AddCurveDialog::getSelectedSampleNames() const
{
    QMap<int, QString> result;
    
    // 遍历导航树中的所有项，找出被选中的样本
    QTreeWidgetItemIterator it(m_dataNavigator);
    while (*it) {
        QTreeWidgetItem *item = *it;
        QVariant sampleIdVar = item->data(0, Qt::UserRole);
        if (sampleIdVar.isValid() && item->checkState(0) == Qt::Checked) {
            int sampleId = sampleIdVar.toInt();
            
            // 查找对应的样本数据
            for (const auto &sample : m_samplesData) {
                if (sample["sample_id"].toInt() == sampleId) {
                    // 创建样本名称，统一为 short_code(parallel_no)-timestamp
                    QString shortCode = sample.value("short_code").toString();
                    int parallelNo = sample.value("parallel_no").toInt();
                    QDateTime dt;
                    QVariant detectVar = sample.value("detect_date");
                    if (detectVar.isValid()) {
                        if (detectVar.canConvert<QDateTime>()) {
                            dt = detectVar.toDateTime();
                        } else {
                            QDate d = detectVar.toDate();
                            if (d.isValid()) {
                                dt = QDateTime(d);
                            }
                        }
                    }
                    if (!dt.isValid()) {
                        QVariant createdVar = sample.value("created_at");
                        if (createdVar.isValid() && createdVar.canConvert<QDateTime>()) {
                            dt = createdVar.toDateTime();
                        }
                    }
                    QString timestamp = QStringLiteral("N/A");
                    if (dt.isValid()) {
                        timestamp = dt.toString("yyyyMMdd_HHmmss");
                    }
                    QString sampleName;
                    if (!shortCode.isEmpty() && parallelNo > 0) {
                        sampleName = QString("%1(%2)-%3")
                                         .arg(shortCode)
                                         .arg(parallelNo)
                                         .arg(timestamp);
                    } else {
                        sampleName = sample.value("sample_name").toString();
                        if (sampleName.trimmed().isEmpty()) {
                            sampleName = QString("样本 %1").arg(sampleId);
                        }
                    }
                    result[sampleId] = sampleName;
                    break;
                }
            }
        }
        ++it;
    }
    
    return result;
}

// 在 AddCurveDialog.cpp 中添加：
void AddCurveDialog::setExistingCurves(const QList<int>& curveIds)
{
    m_existingCurveIds.clear();
    for (int id : curveIds) {
        m_existingCurveIds.insert(id);
    }
}
