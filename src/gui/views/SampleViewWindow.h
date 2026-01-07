
#ifndef SAMPLEVIEWWINDOW_H
#define SAMPLEVIEWWINDOW_H
#include <QMdiSubWindow>
#include "MyWidget.h"

#include "gui/IInteractiveView.h"

class QTableView;
class SampleDataModel;
class SampleDAO;
class QSplitter;
class ChartView; // 只需前置声明，不需要包含 ChartView.h

// class SampleViewWindow : public QMdiSubWindow, public IInteractiveView
class SampleViewWindow : public MyWidget, public IInteractiveView
// class SampleViewWindow : public QWidget

{
    Q_OBJECT

    // 【关键】使用 Q_INTERFACES 宏向 MOC 声明此事
    Q_INTERFACES(IInteractiveView) 
    
public:
    // 【关键】构造函数签名改变
    explicit SampleViewWindow( 
        const QString &projectName,
                     const QString &batchCode,
                     const QString &shortCode,
                     int parallelNo, const QString& dataType, QWidget *parent = nullptr);
    ~SampleViewWindow();

    // 公共接口，用于防止窗口重复打开
    QString shortCode() const { return m_shortCode; }
    QString projectName() const { return m_projectName; }
    QString batchCode() const { return m_batchCode; }
    int parallelNo() const { return m_parallelNo; }
    QString dataType() const { return m_dataType; }

    ChartView* activeChartView() const;
    ChartView* getChartView() const { return m_chartView; }

    // 实现接口中声明的纯虚函数
    void setCurrentTool(const QString& toolId) override;
    void zoomIn() override;
    void zoomOut() override;
    void zoomReset() override;

    void selectArrow();

    // void onRequestAddMultipleCurves();


private slots:
    void onTableRowClicked(const QModelIndex& current, const QModelIndex& previous);
    // SampleViewWindow.h
    void onZoomRectToolClicked();

private:
    void setupUi();
    void loadData();

    QString m_shortCode;
    QString m_projectName;
    QString m_batchCode;
    int m_parallelNo;
    QString m_dataType;

    SampleDAO* m_dao;
    SampleDataModel* m_model;
    
    // UI 控件
    QSplitter* m_mainSplitter;
    QTableView* m_tableView;
    ChartView* m_chartView; // **现在只有一个图表视图**
};

#endif // SAMPLEVIEWWINDOW_H