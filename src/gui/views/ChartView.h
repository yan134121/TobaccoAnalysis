#ifndef CHARTVIEW_H
#define CHARTVIEW_H

#include "third_party/qcustomplot/qcustomplot.h"
#include <QWidget>
#include <QContextMenuEvent> 
#include <QToolBar>
#include <QAction>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVector>
#include <QCursor>
#include <QMouseEvent>

#include "core/entities/Curve.h"
#include "PlotExporter.h"
#include "third_party/QXlsx/header/xlsxdocument.h"

class ChartView : public QWidget
{
    Q_OBJECT
signals:
    void resized(); // 窗口大小变化信号

public:
    explicit ChartView(QWidget *parent = nullptr);
    ~ChartView();

    static constexpr int CURVEWIDTH       = 2;//曲线宽度

    // --- 【关键】添加新的公共接口 ---
    void addCurve(QSharedPointer<Curve> curve);

    // void addGraph(const QVector<double>& x, const QVector<double>& y, const QString& name, const QColor& color = Qt::blue, int sampleId);
    void addGraph(const QVector<double>& x, const QVector<double>& y, const QString& name, const QColor& color = Qt::blue, int sampleId =-1);
    void highlightGraph(const QString& name);
    void clearGraphs();
    void setLabels(const QString& xLabel, const QString& yLabel);
    void setPlotTitle(const QString& titleText);
    void setLegendName(const QString &legendName);
    void setLegendVisible(bool visible);
    // 取指定样本曲线的实际颜色（用于自定义图例与曲线一致）
    QColor getCurveColor(int sampleId) const;
    void replot();
    // 新增：添加散点集合（用于显示坏点）
    void addScatterPoints(const QVector<double>& x, const QVector<double>& y, const QString& name, const QColor& color = Qt::red, int pointSize = 6);

    // 【关键】在这里定义枚举类型（旧的工具模式，仍保留以兼容）
    enum ToolMode { Select, Pan, ZoomRect };

    // 轻量状态机的主状态与事件类型
    enum class PrimaryState { Select, Pan, ZoomRect };          // 唯一主状态（三选一）
    enum class ToolEvent {
        ClickSelect,                                            // 选择工具（点击）
        TogglePanOn, TogglePanOff,                              // 拖动模式开启/关闭
        ToggleZoomRectOn, ToggleZoomRectOff,                    // 放大矩形模式开启/关闭
        ToggleMarkOn, ToggleMarkOff,                            // 标记模式开启/关闭（并行开关）
        ClickZoomOut, ClickResetZoom                            // 一次性动作：缩小/重置
    };

    QList<int> getExistingCurveIds() const;
    
    // 获取曲线数量
    int getCurveCount() const;
    
    // 获取曲线数据
    QVector<QPair<QVector<double>, QVector<double>>> getCurvesData() const;
    
    // 获取选中曲线的数据
    QVector<QPair<QVector<double>, QVector<double>>> getSelectedCurvesData() const;
    
    // 获取选中曲线及其样本ID（返回 <样本ID, <x数据, y数据>>）
    QList<QPair<int, QPair<QVector<double>, QVector<double>>>> getSelectedCurvesWithSampleIds() const;
    
    // 导出曲线数据到CSV文件
    bool exportDataToCSV(const QString& filePath) const;
    
    // 导出选中曲线数据到CSV文件
    bool exportSelectedDataToCSV(const QString& filePath) const;
    
    // 导出曲线数据到XLSX文件
    bool exportDataToXLSX(const QString& filePath) const;
    
    // 导出选中曲线数据到XLSX文件
    bool exportSelectedDataToXLSX(const QString& filePath) const;

    bool exportPlot(const QString& filePath) const;
    
    // 新增工具功能
    void panMode();
    void exportImage();
    void exportData();
public slots:
    void setToolMode(const QString& toolId);
    void zoomIn();
    void zoomOut();
    void zoomReset();

    void zoomOutFixed();

    void showContextMenu(const QPoint &pos);
    

protected:
    void contextMenuEvent(QContextMenuEvent *event) override;  // 声明在 protected
    void enterEvent(QEvent *event) override;  // 鼠标进入事件
    void leaveEvent(QEvent *event) override;  // 鼠标离开事件
    void resizeEvent(QResizeEvent *event) override;  // 窗口大小变化事件
    // // 【必须声明这三个函数来重写 QWidget 的虚函数】
    // void mousePressEvent(QMouseEvent *event) override;
    // void mouseMoveEvent(QMouseEvent *event) override;
    // void mouseReleaseEvent(QMouseEvent *event) override;

signals:
    void requestAddCurve();
    void requestAddMultipleCurves();

private:
    QCustomPlot * m_plot = nullptr;
    QCPTextElement* m_titleElement = nullptr;  // 追踪标题元素
    // 【关键】在这里声明私有成员变量
    ToolMode m_currentToolMode;
    ToolMode m_prevToolMode = Select; // 记录上一个工具模式，用于退出时恢复

    // 轻量状态机当前状态与标记开关
    PrimaryState m_primaryState = PrimaryState::Select; // 初始为选择
    bool m_markOn = false;                              // 标记开关（与主状态并行）
    // QToolBar* m_toolBar = nullptr;
    QWidget* m_toolBar = nullptr;  // 改为 QWidget*
    PlotExporter* m_plotExporter = nullptr;

    // 鼠标框选放大相关
    QPoint m_zoomStart;          // 记录鼠标按下起点
    QRubberBand* m_rubberBand;   // 显示选中矩形

    QMap<int, QCPGraph*> m_sampleGraphs; // 存储样本 ID 和对应的图形对象
    QMap<int, QString>   m_sampleNames;  // 存储样本 ID 与完整显示名称，用于悬停提示

    // --- 新增：点击点标记与坐标文本 ---
    // 使用 QCPItemTracer 作为曲线上点的标记，使用 QCPItemText 显示坐标
    QCPItemTracer* m_clickTracer = nullptr;   // 点击点的跟踪标记
    QCPItemText*   m_coordText  = nullptr;    // 显示坐标的文本

    // --- 新增：标记模式按钮与多点标记容器 ---
    QToolButton* m_markPointBtn = nullptr;    // 工具栏中的“标记模式”按钮（选中后才启用点击标记）
    QVector<QCPItemTracer*> m_extraTracers;   // Ctrl 多选附加标记集合
    QVector<QCPItemText*>   m_extraTexts;     // Ctrl 多选附加坐标文本集合

    // --- 新增：不同状态的图标与自定义光标 ---
    QIcon   m_markIcon;          // 未选中时的图标
    QIcon   m_markIconActive;    // 选中时的图标（着色以示区别）
    QCursor m_markCursor;        // 标记模式下的自定义瞄准光标（空心正方形）

    // --- 新增：选择工具（矩形框） ---
    QToolButton* m_selectToolBtn = nullptr;   // “选择工具”按钮，图标使用 tool-select.png
    bool m_selectModeActive = false;          // 选择模式是否激活

    // --- 新增：将局部按钮提升为成员，便于统一刷新 ---
    QToolButton* m_zoomInBtn = nullptr;       // 放大（矩形）
    QToolButton* m_zoomOutBtn = nullptr;      // 缩小（一次性动作）
    QToolButton* m_resetZoomBtn = nullptr;    // 重置（一次性动作）
    QToolButton* m_panBtn = nullptr;          // 拖动
    QToolButton* m_exportImageBtn = nullptr;  // 导出图像
    QToolButton* m_exportDataBtn = nullptr;   // 导出数据

    // --- 新增：辅助方法 ---
    // 根据被点击的图形与数据索引，更新标记位置与坐标文本
    void updateClickMarker(QCPGraph* graph, int dataIndex, bool append);
    // 清理现有标记与文本（包含所有附加标记）
    void clearClickMarker();
    // 删除距离鼠标位置最近的标记点（含主标记与附加标记）
    void deleteNearestMarker(const QPoint& mousePos);
    // 撤销最后一个标记（优先删除附加标记，最后删除主标记）
    void undoLastMarker();

    // --- 新增：悬停图例相关成员 ---
    QCPItemTracer* m_hoverTracer = nullptr;      // 悬停跟踪标记
    QCPItemText*   m_hoverLegendText = nullptr;  // 悬停图例文本
    QString        m_lastHoverGraphName;         // 最近悬停曲线名

    // --- 新增：选择模式下悬停显示曲线名 ---
    void onPlotMouseMove(QMouseEvent* event);

    // --- 新增：事件过滤器处理绘图区域的鼠标事件（用于矩形框绘制） ---
protected:
    bool eventFilter(QObject* obj, QEvent* event) override; // 监听 m_plot 的鼠标事件

    // --- 新增：轻量状态机的内部方法 ---
private:
    // 应用事件，仅修改内部状态，不直接更新UI
    void applyEvent(ToolEvent e);
    // 根据当前状态统一刷新UI（按钮、交互、光标、图标、过滤器）
    void renderState();

    // 根据 sampleId 为曲线应用可区分的样式（线型/点型/宽度）
    void applyDistinctStyle(QCPGraph* graph, int sampleId, const QColor& baseColor);
    
};

#endif // CHARTVIEW_H
