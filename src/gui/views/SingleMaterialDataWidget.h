// src/ui/SingleMaterialDataWidget.h
#ifndef SINGLEMATERIALDATAWIDGET_H
#define SINGLEMATERIALDATAWIDGET_H
#include <QWidget>
#include <QMessageBox>
#include <QList>
#include <QMap>
#include <QVariant>
#include <QStringList>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QSqlDatabase>

// 数据实体类
#include "SingleTobaccoSampleData.h"
#include "TgBigData.h"
#include "TgSmallData.h"
#include "ChromatographyData.h"

// 数据访问对象
#include "data_access/TgBigDataDAO.h"

// 数据导入服务
#include "services/data_import/TgBigDataImportWorker.h"
#include "services/data_import/TgSmallDataImportWorker.h"
#include "services/data_import/TgSmallRawDataImportWorker.h"
#include "services/data_import/ProcessTgBigDataImportWorker.h"
#include "services/data_import/ChromatographDataImportWorker.h"

// 前向声明
class SingleTobaccoSampleService;
class SingleTobaccoSampleTableModel;
class SingleTobaccoSampleData;
class TgBigDataTableModel;
class TgSmallDataTableModel;
class ChromatographyDataTableModel;
class LineChartView;
class TgSmallDataDAO;
class SingleTobaccoSampleDAO;
class QProgressDialog;
class AppInitializer;


// --- 结束前向声明 ---

QT_BEGIN_NAMESPACE
namespace Ui { class SingleMaterialDataWidget; } // 由 uic 生成的 UI 类
QT_END_NAMESPACE

class SingleMaterialDataWidget : public QWidget
{
    Q_OBJECT

public:
    // // 构造函数接收 Service 实例
    // explicit SingleMaterialDataWidget(SingleTobaccoSampleService* service, QWidget *parent = nullptr);
    // --- 关键修改：构造函数接收 AppInitializer* ---
    explicit SingleMaterialDataWidget(SingleTobaccoSampleService* service, AppInitializer* initializer, QWidget *parent = nullptr);
    ~SingleMaterialDataWidget() override; // 虚析构函数

signals:
    // 信号，用于向主窗口或其他模块发送状态消息
    void statusMessage(const QString& message, int timeout = 3000);

    // 新增信号：数据导入完成，携带导入类别，用于自动刷新导航树（中文注释）
    // 可选值示例："TG_BIG"、"TG_SMALL"、"TG_SMALL_RAW"、"CHROMATOGRAPHY"、"PROCESS_TG_BIG"
    void dataImportFinished(const QString& category);

private slots:
    // 槽函数，对应UI操作按钮的 clicked() 信号
    void on_queryButton_clicked();
    void on_resetQueryButton_clicked();
    void on_importButton_clicked();
    void on_addButton_clicked();
    void on_editButton_clicked();
    void on_deleteButton_clicked();
    void on_processButton_clicked();
    void on_viewDetailsButton_clicked();
    void on_exportButton_clicked();
    // void on_favoriteButton_clicked(); // 如果有收藏功能
    void on_openModelListButton_clicked();
    void on_openBatchListButton_clicked();
    void on_openDictionaryListButton_clicked();
    // 打开样本列表对话框
    void on_openSampleListButton_clicked();

    void on_importSensorDataButton_clicked(); // <-- 修改逻辑
    void on_tableView_clicked(const QModelIndex& index);

    // --- 新增槽函数：加载传感器数据并显示 ---
    void on_loadSensorDataButton_clicked();
    // --- 结束新增 ---

    void on_importTgBigDataButton_clicked();
    void on_importTgSmallDataButton_clicked();
    void on_importTgSmallRawDataButton_clicked();
    void on_importChromatographDataButton_clicked();
    void on_importProcessTgBigDataButton_clicked();

private:
    Ui::SingleMaterialDataWidget *ui; // UI 界面对象
    SingleTobaccoSampleService* m_service; // Service 层实例
    SingleTobaccoSampleTableModel* m_tableModel;   // 数据模型实例

    SingleTobaccoSampleData m_selectedTobacco; // 存储当前选中的基础信息

    // --- 新增传感器数据模型 ---
    TgBigDataTableModel* m_tgBigDataTableModel = nullptr;
    TgSmallDataTableModel* m_tgSmallDataTableModel = nullptr;
    ChromatographyDataTableModel* m_chromatographyDataTableModel = nullptr;
    TgSmallDataDAO* m_tgSmallDataDao = nullptr; // 小热重DAO
    SingleTobaccoSampleDAO* m_singleTobaccoSampleDAO = nullptr; // 单料烟样本DAO成员变量
    // --- 结束新增 ---
    
    // 小热重数据导入工作线程
    TgSmallDataImportWorker* m_tgSmallDataImportWorker = nullptr;
    TgSmallRawDataImportWorker* m_tgSmallRawDataImportWorker = nullptr;
    // 大热重数据导入工作线程
    TgBigDataImportWorker* m_tgBigDataImportWorker = nullptr;
    //工序大热重数据导入工作线程
    ProcessTgBigDataImportWorker* m_processTgBigDataImportWorker = nullptr;
    // 色谱数据导入工作线程
    ChromatographDataImportWorker* m_chromatographDataImportWorker = nullptr;
    
    // 色谱数据导入相关函数
    QString findTicBackCsv(const QString& dirPath);
    bool parseDataFolderName(const QString& folderName, QString& shortCode, int& parallelNo);
    bool processCsvFile(const QString& csvPath, int sampleId, const QString& shortCode, int parallelNo, QSqlDatabase& db);
    void refreshChromatographyDataTable();
    QProgressDialog* m_importProgressDialog = nullptr;
    
    QMap<int, QColor> m_colorMap;
    QMap<int, QString> m_sampleNameMap;
    QMap<int, bool> m_sampleVisibilityMap;
    
    // 从sheet名称中提取四位数字作为short_code
    QString extractShortCodeFromSheetName(const QString &sheetName);
    
    // 创建或获取样本ID
    int createOrGetSample(const SingleTobaccoSampleData &sampleData);

    void loadData(); // 从 Service 加载数据并更新表格
    void setupTableView(); // 设置 QTableView 的属性和外观
    SingleTobaccoSampleData getSelectedTobacco() const; // 获取当前选中的单行数据
    QList<int> getSelectedTobaccoIds() const; // 获取所有选中行的 ID 列表
    QMap<QString, QVariant> buildQueryConditions() const; // 根据 UI 输入构建查询条件映射
    void updateSelectedTobaccoInfo(); // 更新 UI 显示

    // --- 新增辅助函数：加载并显示传感器数据 ---
    void loadSensorDataForSelectedTobacco();
    void displayTgBigData(const QList<TgBigData>& data);
    void displayTgSmallData(const QList<TgSmallData>& data);
    void displayChromatographyData(const QList<ChromatographyData>& data);
    // --- 结束新增 ---

    // --- 新增辅助函数：导入TG大数据目录扫描与分组 ---
    // 解析文件名生成分组标识
    QString parseGroupIdFromFilename(const QString& filename);
    
    // 从文件名解析样本信息
    SingleTobaccoSampleData* parseSampleInfoFromFilename(const QString& filename);
    
    // 创建或获取样本ID
    int createOrGetSample(const QString& filename);
    
    // 从CSV文本流中读取TgBigData数据
    QList<TgBigData> readTgBigDataFromCsv(QTextStream& in, const QString& filePath, int sampleId);
    
    // 刷新图表
    void refreshCharts();
    QMap<QString, QStringList> buildGroupedCsvPaths(const QString& directoryPath);
    // --- 结束新增 ---

    AppInitializer* m_appInitializer = nullptr; // <-- 新增成员变量


    QMap<QString, QStringList> m_tgBigDataFileGroups;
};

#endif // SINGLEMATERIALDATAWIDGET_H
