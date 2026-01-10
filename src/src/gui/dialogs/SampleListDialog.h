#ifndef SAMPLELISTDIALOG_H
#define SAMPLELISTDIALOG_H

#include <QDialog>
#include <QStandardItemModel>

class QTableView; // 前置声明，避免在头文件中包含完整QTableView头
class AppInitializer; // 前置声明应用初始化器
class QComboBox;     // 前置声明下拉框
class QLineEdit;     // 前置声明搜索框
class QCompleter;    // 前置声明自动补全器

namespace Ui { class SampleListDialog; }

class SampleListDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SampleListDialog(QWidget* parent = nullptr, AppInitializer* appInitializer = nullptr);
    ~SampleListDialog();

private:
    void setupModels();
    void loadBigSamples();
    void loadSmallSamples();
    void loadSmallRawSamples();
    void loadChromSamples();
    void loadProcessBigSamples();
    // 获取当前页签对应的表格视图与模型
    QTableView* currentTableView() const;
    QStandardItemModel* currentModel() const;
    // 根据当前页签推断数据类型与分支
    QString currentDataType() const;
    bool currentIsProcessBranch() const;
    int countBigPoints(int sampleId);
    int countSmallPoints(int sampleId);
    int countSmallRawPoints(int sampleId);
    int countChromPoints(int sampleId);
    int countProcessBigPoints(int sampleId);
    // 构建当前页签的三级索引与搜索缓存
    void buildIndexForCurrentModel();
    void populateProjectCombo();
    void populateBatchCombo(const QString& project);
    void populateSampleCombo(const QString& project, const QString& batch);
    void selectRow(int row);
    void selectByPathString(const QString& path);

private slots:
    // 删除当前选中样本
    void on_deleteButton_clicked();
    // 预览当前选中样本曲线
    void on_previewButton_clicked();
    // 分级联动槽函数
    void on_projectCombo_currentIndexChanged(const QString& project);
    void on_batchCombo_currentIndexChanged(const QString& batch);
    void on_sampleCombo_currentIndexChanged(const QString& sampleDisplay);
    // 搜索与自动补全
    void on_searchEdit_returnPressed();

private:
    Ui::SampleListDialog* ui;
    QStandardItemModel* m_bigModel;
    QStandardItemModel* m_smallModel;
    QStandardItemModel* m_smallRawModel;
    QStandardItemModel* m_chromModel;
    QStandardItemModel* m_processBigModel;
    AppInitializer* m_appInitializer = nullptr; // 保存应用初始化器，用于创建工序大热重处理对话框
    
    // 索引与缓存（支持上万数据量的快速检索）
    struct SampleIndexItem {
        int sampleId;
        QString project;
        QString batch;
        QString shortCode;
        int parallelNo;
        int row; // 所在行
    };
    QVector<SampleIndexItem> m_indexItems;              // 当前页签所有样本条目
    QHash<QString, QSet<QString>> m_projToBatches;      // 项目->批次集合
    QHash<QString, QList<SampleIndexItem>> m_projBatchToSamples; // key=project||batch -> 样本列表
    QHash<QString, int> m_pathToRow;                    // 显示路径->行索引
    QCompleter* m_searchCompleter = nullptr;            // 搜索自动补全
};

#endif // SAMPLELISTDIALOG_H
