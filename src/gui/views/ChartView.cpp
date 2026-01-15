#include "ChartView.h"
#include "Logger.h"
#include "ColorUtils.h"
#include <QMessageBox>
#include <algorithm>
    #include <QElapsedTimer>
// ：图标着色与自定义光标需要的绘图类
#include <QPainter>
#include <QPixmap>
#include <QColor>
// 选择工具需要鼠标事件与橡皮筋矩形框
#include <QMouseEvent>
#include <QRubberBand>
#include <QSignalBlocker> // 阻断按钮信号，避免程序化勾选触发切换事件

ChartView::ChartView(QWidget *parent) : QWidget(parent)
{
    DEBUG_LOG << "ChartView::ChartView - Constructor called";
    
    // 设置背景颜色以便于调试
    this->setStyleSheet("background-color: #f0f0f0;");
    
    // 创建 QCustomPlot 对象
    m_plot = new QCustomPlot(this);
    
    DEBUG_LOG << "ChartView::ChartView - QCustomPlot created:" << m_plot;
    
    // 确保 QCustomPlot 对象可见
    if (m_plot) {
        m_plot->setVisible(true);
        DEBUG_LOG << "ChartView::ChartView - QCustomPlot visibility set to true";
    }

    // 启用右键菜单
    m_plot->setContextMenuPolicy(Qt::CustomContextMenu);
    // connect(m_plot, &QCustomPlot::customContextMenuRequested,
    //         this, &ChartView::showContextMenu);

    // connect(m_plot, &QCustomPlot::customContextMenuRequested,
    //     this, [this](const QPoint &pos){
    //         QMenu menu(this);
    //         QAction* addCurveAction = menu.addAction(tr("添加曲线..."));
    //         // QAction* addMultiCurveAction = menu.addAction(tr("添加多条曲线..."));
    //         QAction* selectedAction = menu.exec(m_plot->mapToGlobal(pos));
    //         if (selectedAction == addCurveAction) emit requestAddCurve();
    //         // else if (selectedAction == addMultiCurveAction) emit requestAddMultipleCurves();
    //     });
    
    // // 创建工具栏
    // m_toolBar = new QToolBar(this);
    // m_toolBar->setIconSize(QSize(16, 16));
    // m_toolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    // m_toolBar->setFloatable(false);
    // m_toolBar->setMovable(false);
    
    // // 创建工具按钮
    // QAction* zoomInAction = m_toolBar->addAction(QIcon(":/icons/zoom_in.png"), tr("放大"));
    // QAction* zoomOutAction = m_toolBar->addAction(QIcon(":/icons/zoom_out.png"), tr("缩小"));
    // QAction* resetZoomAction = m_toolBar->addAction(QIcon(":/icons/reset.png"), tr("重置"));
    // QAction* panAction = m_toolBar->addAction(QIcon(":/icons/drag.png"), tr("拖动"));
    // m_toolBar->addSeparator();
    // QAction* exportImageAction = m_toolBar->addAction(QIcon(":/icons/export_image.png"), tr("导出图像"));
    // QAction* exportDataAction = m_toolBar->addAction(QIcon(":/icons/export_data.png"), tr("导出数据"));
    
    // // 连接信号和槽
    // connect(zoomInAction, &QAction::triggered, this, &ChartView::zoomIn);
    // connect(zoomOutAction, &QAction::triggered, this, &ChartView::zoomOut);
    // connect(resetZoomAction, &QAction::triggered, this, &ChartView::zoomReset);
    // connect(panAction, &QAction::triggered, this, &ChartView::panMode);
    // connect(exportImageAction, &QAction::triggered, this, &ChartView::exportImage);
    // connect(exportDataAction, &QAction::triggered, this, &ChartView::exportData);
    
    // // 使用QVBoxLayout让工具栏悬浮在绘图区域上
    // QVBoxLayout* layout = new QVBoxLayout(this);
    // layout->addWidget(m_plot);
    // layout->setContentsMargins(0,0,0,0);
    // layout->setSpacing(0);
    // this->setLayout(layout);
    
    // // 设置工具栏悬浮在绘图区域中间上方
    // m_toolBar->setParent(this);
    // m_toolBar->setStyleSheet("background-color: rgba(240, 240, 240, 180);"); // 半透明背景
    
    // // 确保所有工具按钮都显示
    // m_toolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    // m_toolBar->setIconSize(QSize(16, 16));
    
    // // 计算工具栏位置，放在中间上方
    // QSize toolBarSize = m_toolBar->sizeHint();
    // int x = (width() - toolBarSize.width()) / 2;
    // int y = 5; // 距离顶部5像素
    // m_toolBar->move(x, y);
    // m_toolBar->raise(); // 确保工具栏在最上层
    
    // // 添加窗口大小变化事件处理，以便重新计算工具栏位置
    // connect(this, &ChartView::resized, this, [this]() {
    //     if (m_toolBar && m_toolBar->isVisible()) {
    //         QSize toolBarSize = m_toolBar->sizeHint();
    //         int x = (width() - toolBarSize.width()) / 2;
    //         int y = 5;
    //         m_toolBar->move(x, y);
    //     }
    // });
    
    // // 初始隐藏工具栏
    // m_toolBar->hide();

    // 创建自定义工具栏容器
m_toolBar = new QWidget(this);
QHBoxLayout* toolBarLayout = new QHBoxLayout(m_toolBar);
toolBarLayout->setContentsMargins(5, 3, 5, 3);
toolBarLayout->setSpacing(3);

// 创建工具按钮的辅助函数
auto createButton = [](const QString& iconPath, const QString& tooltip) -> QToolButton* {
    QToolButton* btn = new QToolButton();
    btn->setIcon(QIcon(iconPath));
    btn->setIconSize(QSize(16, 16));
    btn->setToolTip(tooltip);
    btn->setAutoRaise(true);
    btn->setFixedSize(24, 24);
    return btn;
};

// 创建所有工具按钮（提升为成员，便于状态机统一刷新）
m_zoomInBtn = createButton(":/icons/zoom_in.png", tr("放大"));
m_zoomOutBtn = createButton(":/icons/zoom_out.png", tr("缩小"));
m_resetZoomBtn = createButton(":/icons/reset.png", tr("重置"));
m_panBtn = createButton(":/icons/drag.png", tr("拖动"));
m_exportImageBtn = createButton(":/icons/export_image.png", tr("导出图像"));
m_exportDataBtn = createButton(":/icons/export_data.png", tr("导出数据"));
// ：选择工具按钮（点击后进入箭头光标与矩形框模式）
m_selectToolBtn = createButton(":/icons/tool-select.png", tr("选择工具"));
m_selectToolBtn->setCheckable(true);
m_selectToolBtn->setAutoExclusive(true); // 启用互斥，避免二次点击取消选中
// ：标记模式按钮（选中后启用点击标记）
m_markPointBtn = createButton(":/icons/mark_tool.png", tr("标记模式"));
m_markPointBtn->setCheckable(true); // 可切换选中状态
// 标记模式允许再次点击取消选中，因此不使用互斥
m_markPointBtn->setAutoExclusive(false);

// --- 生成选中/未选中两种状态的图标，并设置初始图标 ---
{
    // 加载基础图标，并创建着色后的激活态图标，使选中时图标有明显区别
    QPixmap basePixmap(":/icons/mark_tool.png");
    QPixmap activePixmap = basePixmap.copy();
    {
        QPainter p(&activePixmap);
        p.setRenderHint(QPainter::Antialiasing, true);
        // 使用 SourceIn 组合模式，用绿色半透明填充原图形区域，实现着色效果
        p.setCompositionMode(QPainter::CompositionMode_SourceIn);
        p.fillRect(activePixmap.rect(), QColor(0, 170, 0, 220)); // 绿色着色
        p.end();
    }
    m_markIcon = QIcon(basePixmap);
    m_markIconActive = QIcon(activePixmap);
    m_markPointBtn->setIcon(m_markIcon); // 初始未选中状态
}

// 添加按钮到布局
toolBarLayout->addWidget(m_zoomInBtn);
toolBarLayout->addWidget(m_zoomOutBtn);
toolBarLayout->addWidget(m_resetZoomBtn);
toolBarLayout->addWidget(m_panBtn);
toolBarLayout->addWidget(m_selectToolBtn); // 将选择工具按钮加入工具栏
toolBarLayout->addWidget(m_markPointBtn); // 将标记模式按钮加入工具栏

// 添加分隔线
QFrame* separator = new QFrame();
separator->setFrameShape(QFrame::VLine);
separator->setFrameShadow(QFrame::Sunken);
separator->setFixedHeight(20);
toolBarLayout->addWidget(separator);

toolBarLayout->addWidget(m_exportImageBtn);
toolBarLayout->addWidget(m_exportDataBtn);

// 连接信号和槽
// 放大为可切换模式，选中进入放大模式，再次点击退出
m_zoomInBtn->setCheckable(true);
// 放大按钮支持再次点击取消选中，因此不使用互斥模式
// zoomInBtn->setAutoExclusive(false);
// 修复lambda捕获列表，成员变量应通过this访问，不能直接捕获
// 放大按钮改为事件驱动 + 统一渲染
connect(m_zoomInBtn, &QToolButton::toggled, this, [this](bool checked){
    applyEvent(checked ? ToolEvent::ToggleZoomRectOn : ToolEvent::ToggleZoomRectOff);
    renderState();
});
// 缩小为一次性动作；点击时临时加深背景（样式已在 :pressed），同时保持当前工具状态
connect(m_zoomOutBtn, &QToolButton::clicked, this, [this](){
    applyEvent(ToolEvent::ClickZoomOut);
    renderState(); // 非必须，但保持按钮状态一致
});
// 重置为一次性动作；点击时临时加深背景；保持当前工具状态
connect(m_resetZoomBtn, &QToolButton::clicked, this, [this](){
    applyEvent(ToolEvent::ClickResetZoom);
    renderState();
});
// 拖动为可切换模式；选中进入拖动，再次点击退出
m_panBtn->setCheckable(true);
// 拖动按钮支持再次点击取消选中，因此不使用互斥模式
// panBtn->setAutoExclusive(false);
connect(m_panBtn, &QToolButton::toggled, this, [this](bool checked){
    applyEvent(checked ? ToolEvent::TogglePanOn : ToolEvent::TogglePanOff);
    renderState();
});
connect(m_exportImageBtn, &QToolButton::clicked, this, &ChartView::exportImage);
connect(m_exportDataBtn, &QToolButton::clicked, this, &ChartView::exportData);
// ：选择工具切换逻辑
// 选择工具点击即选中，不支持再次点击取消；同时自动取消其他模式并恢复其图标与背景
connect(m_selectToolBtn, &QToolButton::clicked, this, [this]() {
    applyEvent(ToolEvent::ClickSelect);
    renderState();
});
// 当因选择其他互斥按钮导致取消选中时，收尾处理
connect(m_selectToolBtn, &QToolButton::toggled, this, [this](bool checked){
    if (!checked) {
        m_selectModeActive = false;         // 退出选择模式
        if (m_plot) {
            m_plot->removeEventFilter(this);
        }
        if (m_rubberBand) m_rubberBand->hide();
        // 不强制切换到拖动，由其他工具逻辑自行处理
    }
});
// 标记模式按钮无需连接槽函数，点击曲线时根据其选中状态决定是否响应
// ：标记模式切换时，切换图标与光标；未选中时清除所有标记与坐标
// 标记按钮允许再次点击取消选中，因此不使用互斥模式
// m_markPointBtn->setAutoExclusive(false);
connect(m_markPointBtn, &QToolButton::toggled, this, [this](bool checked){
    applyEvent(checked ? ToolEvent::ToggleMarkOn : ToolEvent::ToggleMarkOff);
    renderState();
});

// 使用QVBoxLayout让工具栏悬浮在绘图区域上
QVBoxLayout* layout = new QVBoxLayout(this);
layout->addWidget(m_plot);
layout->setContentsMargins(0, 0, 0, 0);
layout->setSpacing(0);
this->setLayout(layout);

// 初始化后统一渲染一次，确保按钮与交互/光标与过滤器与图标一致
renderState();

// 设置工具栏悬浮在绘图区域中间上方
m_toolBar->setParent(this);
m_toolBar->setStyleSheet(
    "QWidget { background-color: rgba(240, 240, 240, 180); border-radius: 4px; }"
    "QToolButton { border: none; border-radius: 2px; }"
    "QToolButton:hover { background-color: rgba(200, 200, 200, 180); }"
    "QToolButton:pressed { background-color: rgba(180, 180, 180, 200); }"
    "QToolButton:checked { background-color: rgba(160, 160, 160, 220); }" // 选中时背景加深突出显示
);

// 调整大小以适应内容
m_toolBar->adjustSize();

// 计算工具栏位置，放在中间上方
QSize toolBarSize = m_toolBar->sizeHint();
int x = (width() - toolBarSize.width()) / 2;
int y = 5; // 距离顶部5像素
m_toolBar->move(x, y);
m_toolBar->raise(); // 确保工具栏在最上层

// 添加窗口大小变化事件处理，以便重新计算工具栏位置
connect(this, &ChartView::resized, this, [this]() {
    if (m_toolBar && m_toolBar->isVisible()) {
        QSize toolBarSize = m_toolBar->sizeHint();
        int x = (width() - toolBarSize.width()) / 2;
        int y = 5;
        m_toolBar->move(x, y);
    }
});

// 初始隐藏工具栏
m_toolBar->hide();
    
    // 初始化绘图区域交互设置
    m_plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectAxes | QCP::iSelectLegend | QCP::iSelectPlottables);
    
    // 创建导出工具
    m_plotExporter = new PlotExporter(this);
    
    DEBUG_LOG << "ChartView::ChartView - Layout set with QCustomPlot";
    
    // 确保 QCustomPlot 可见
    m_plot->setVisible(true);
    this->setVisible(true);


    // 开启鼠标跟踪并连接移动事件，用于选择模式下悬停显示曲线图例
    m_plot->setMouseTracking(true);
    connect(m_plot, &QCustomPlot::mouseMove, this, &ChartView::onPlotMouseMove);

    DEBUG_LOG;
    // 初始化成员变量
    m_currentToolMode = ChartView::Select;
    DEBUG_LOG;
    
    // 初始化橡皮筋选择框为空指针
    m_rubberBand = nullptr;

    // 基础交互设置
    // m_plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables);
    // 禁用滚轮缩放，只保留拖拽或选中曲线（按需）
    m_plot->setInteractions(QCP::iSelectPlottables | QCP::iRangeDrag);

    // --- ：连接曲线点击事件以标记点并显示坐标 ---
    // 当点击到某条可点击的曲线（QCPGraph）时，QCustomPlot 会发射 plottableClick 信号
    // 我们在此捕获该事件，并在点击位置添加/更新标记与坐标文本
    connect(m_plot, &QCustomPlot::plottableClick,
            this, [this](QCPAbstractPlottable* plottable, int dataIndex, QMouseEvent* event){
        // 仅处理折线图 QCPGraph 的点击
        QCPGraph* graph = qobject_cast<QCPGraph*>(plottable);
        if (!graph) return;
        // 仅在“标记模式”开启时，才响应点击标记（状态机开关）
        if (!m_markOn) return;
        const bool append = (event && (event->modifiers() & Qt::ControlModifier));
        // 更新点击标记与坐标显示；按下 Ctrl 时追加多个点，否则只保留一个点
        updateClickMarker(graph, dataIndex, append);
        // 避免点击后曲线进入选中态导致颜色变化，立即清除选中状态
        if (m_plot) {
            m_plot->deselectAll();
            m_plot->replot();
        }
    });
    
    // 确保图例可见
    m_plot->legend->setVisible(true);
    DEBUG_LOG << "ChartView::ChartView - Legend visibility set to:" << m_plot->legend->visible();
    
    // 设置大小策略
    m_plot->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    // 设置最小尺寸
    m_plot->setMinimumSize(300, 200);
    this->setMinimumSize(300, 200);
    
    DEBUG_LOG << "ChartView::ChartView - Size policies and minimum sizes set";
    
    // 设置背景颜色以便于调试
    // m_plot->setBackground(QBrush(QColor(240, 240, 240)));
    m_plot->setBackground(QBrush(ColorUtils::ChartView_plotBackground()));
    DEBUG_LOG << "ChartView::ChartView - Background color set for debugging";


    // showMaximized();


}

// 【关键】在这里添加新方法的完整实现
void ChartView::addCurve(QSharedPointer<Curve> curve)
{
    // 安全检查：确保智能指针有效
    if (curve.isNull()) {
        qWarning() << "ChartView::addCurve was called with a null QSharedPointer<Curve>.";
        return;
    }

    QElapsedTimer timer;  //  先声明
    timer.restart();

    // 1. 从 Curve 对象中提取数据
    const QVector<QPointF>& dataPoints = curve->data();
    if (dataPoints.isEmpty()) {
        qWarning() << "ChartView::addCurve: Curve" << curve->name() << "has no data points.";
        return; // 不绘制空曲线
    }

    
    
    QVector<double> xData, yData;
    xData.reserve(dataPoints.size());
    yData.reserve(dataPoints.size());
    for (const auto& point : dataPoints) {
        xData.append(point.x());
        yData.append(point.y());
    }

    // 2. 复用已有的 addGraph 方法进行底层绘图
    //    addGraph 负责处理所有与 QCustomPlot 相关的细节
    this->addGraph(xData, yData, curve->name(), curve->color(), curve->sampleId());

    // DEBUG_LOG << "data points. Preparation took" << timer.elapsed() << "ms.";
    // 应用曲线线型（例如虚线），避免覆盖“基线”原点样式
    if (m_plot && m_plot->graphCount() > 0) {
        QCPGraph* lastGraph = m_plot->graph(m_plot->graphCount() - 1);
        if (lastGraph && !lastGraph->name().contains(QStringLiteral("基线"))) {
            QPen pen = lastGraph->pen();
            pen.setStyle(curve->lineStyle());
            lastGraph->setPen(pen);
        }
    }
}


// // 提供一个新的、更高效的 addCurve 版本
// void ChartView::addCurve(QSharedPointer<Curve> curve)
// {
//     if (curve.isNull() || curve->data().isEmpty()) {
//         return;
//     }

//     // 检查 m_plot 指针是否有效，这是一个非常好的编程习惯
//     if (!m_plot) {
//         qWarning() << "ChartView::addCurve - m_plot is null!";
//         return;
//     }

//     // 直接在 ChartView 中创建 graph，并让它共享 Curve 的数据
//     // QCPGraph* graph = this->addGraph();
//     // QCPGraph* graph = QCustomPlot::addGraph();
//     // QCPGraph* graph = this->QCustomPlot::addGraph();
//     QCPGraph* graph = m_plot->addGraph();

//     // 检查图层是否成功创建
//     if (!graph) {
//         qWarning() << "m_plot->addGraph() 创建图层失败。";
//         return;
//     }
    
//     // QCPGraph::data() 返回一个 QSharedPointer<QCPGraphDataContainer>
//     // 我们可以直接用 Curve 的数据替换掉它内部的数据容器
//     // 这样就实现了零复制（Zero-Copy）
//     graph->setData(curve->getDataContainer()); // 假设 Curve 类提供这样一个接口
    
//     graph->setName(curve->name());
//     graph->setPen(QPen(curve->color()));
//     graph->setAdaptiveSampling(true);
// }


// void ChartView::addGraph(const QVector<double> &x, const QVector<double> &y, const QString &name, const QColor& color)
void ChartView::addGraph(const QVector<double>& x, const QVector<double>& y, 
                        const QString& name, const QColor& color, int sampleId)
{
    // DEBUG_LOG << "ChartView::addGraph - Adding graph with" << x.size() << "points, name:" << name << "sampleId:" << sampleId;
    
    if (!m_plot) {
        DEBUG_LOG << "ChartView::addGraph - ERROR: m_plot is null!";
        return;
    }
    
    if (x.isEmpty() || y.isEmpty()) {
        DEBUG_LOG << "ChartView::addGraph - ERROR: Empty data vectors!";
        return;
    }
    
    // 计算数据范围
    double xMin = *std::min_element(x.begin(), x.end());
    double xMax = *std::max_element(x.begin(), x.end());
    double yMin = *std::min_element(y.begin(), y.end());
    double yMax = *std::max_element(y.begin(), y.end());
    
    // DEBUG_LOG << "ChartView::addGraph - X range:" << xMin << "to" << xMax;
    // DEBUG_LOG << "ChartView::addGraph - Y range:" << yMin << "to" << yMax;
    
    // 创建新图表
    QCPGraph* graph = m_plot->addGraph();
    if (!graph) {
        DEBUG_LOG << "ChartView::addGraph - ERROR: Failed to create graph!";
        return;
    }
    
    // 设置数据
    graph->setData(x, y);
    
    // 确保坐标轴范围包含所有数据点
    if (m_plot->graphCount() == 1) {
        // 第一条曲线，直接设置范围
        m_plot->xAxis->setRange(xMin, xMax);
        m_plot->yAxis->setRange(yMin, yMax);
        // DEBUG_LOG << "ChartView::addGraph - Setting initial axis ranges";
    } else {
        // 后续曲线，扩展范围
        double currentXMin = m_plot->xAxis->range().lower;
        double currentXMax = m_plot->xAxis->range().upper;
        double currentYMin = m_plot->yAxis->range().lower;
        double currentYMax = m_plot->yAxis->range().upper;
        
        m_plot->xAxis->setRange(qMin(currentXMin, xMin), qMax(currentXMax, xMax));
        m_plot->yAxis->setRange(qMin(currentYMin, yMin), qMax(currentYMax, yMax));
        // DEBUG_LOG << "ChartView::addGraph - Extending axis ranges";
    }
    // 设置曲线名称和颜色
    graph->setName(name);
    QPen pen(color);
    pen.setWidth(CURVEWIDTH);
    graph->setPen(pen);
    // 基线原点样式：名称包含“基线”时，使用仅散点圆形样式
    if (name.contains(QStringLiteral("基线"))) {
        graph->setLineStyle(QCPGraph::lsNone);
        QCPScatterStyle scatter(QCPScatterStyle::ssCircle, QPen(color), QBrush(color), 3);
        graph->setScatterStyle(scatter);
    }
    
    // 确保曲线可见
    graph->setVisible(true);
    
    // 保存样本 ID 和图形对象的对应关系
    if (sampleId >= 0) {
        m_sampleGraphs[sampleId] = graph;
        m_sampleNames[sampleId] = name; // 同步保存完整显示名称（project-batch-short-parallel）
    }
    
    // // 输出调试信息
    // DEBUG_LOG << "ChartView::addGraph - Graph added successfully, total graphs:" << m_plot->graphCount();
    // DEBUG_LOG << "ChartView::addGraph - Graph visible:" << graph->visible();
    // DEBUG_LOG << "ChartView::addGraph - Graph name:" << graph->name();
    // DEBUG_LOG << "ChartView::addGraph - Graph data count:" << graph->dataCount();
    
    // 立即重绘以显示新添加的曲线
    m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void ChartView::highlightGraph(const QString &name)
{
    for (int i = 0; i < m_plot->graphCount(); ++i) {
        QCPGraph* graph = m_plot->graph(i);
        if (graph->name() == name) {
            QPen pen = graph->pen();
            pen.setWidth(3); // 高亮曲线加粗
            graph->setPen(pen);
        } else {
            QPen pen = graph->pen();
            pen.setWidth(1); // 其他曲线恢复正常
            graph->setPen(pen);
        }
    }
    m_plot->replot();
}

void ChartView::clearGraphs()
{
    DEBUG_LOG << "ChartView::clearGraphs - Clearing all graphs";
    
    if (!m_plot) {
        DEBUG_LOG << "ChartView::clearGraphs - ERROR: m_plot is null!";
        return;
    }
    
    m_plot->clearGraphs();
    m_sampleGraphs.clear(); // 清除样本ID和图形的映射关系
    m_sampleNames.clear();  // 清除样本ID与显示名称的映射
    // 清理点击标记与坐标文本，避免悬挂
    clearClickMarker();
    m_plot->replot();
    
    DEBUG_LOG << "ChartView::clearGraphs - All graphs cleared";
}

// --- ：更新点击点标记与坐标文本 ---
// 根据被点击的图形与数据索引，在曲线上标记一个点并显示其坐标
void ChartView::updateClickMarker(QCPGraph* graph, int dataIndex, bool append)
{
    if (!m_plot || !graph) return;

    // 安全获取数据容器与索引
    QSharedPointer<QCPGraphDataContainer> dataContainer = graph->data();
    if (!dataContainer) return;
    if (dataIndex < 0 || dataIndex >= dataContainer->size()) return;

    // QCPDataContainer::at 返回的是 const_iterator（指向数据的指针），需解引用
    auto it = dataContainer->at(dataIndex);
    if (it == dataContainer->constEnd()) return; // 越界保护
    const double x = it->key;
    const double y = it->value;

    if (!append) {
        // 非 Ctrl 模式：只保留一个点，先清除所有已有标记
        clearClickMarker();

        // 初始化或更新单一追踪标记
        if (!m_clickTracer) {
            m_clickTracer = new QCPItemTracer(m_plot);
            m_clickTracer->setStyle(QCPItemTracer::tsCircle); // 圆形标记
            m_clickTracer->setSize(7);                        // 标记大小
            m_clickTracer->setPen(QPen(Qt::red, 2));          // 红色边框
            m_clickTracer->setBrush(QBrush(QColor(255, 0, 0, 60))); // 半透明填充
            m_clickTracer->setInterpolating(true);
        }
        m_clickTracer->setGraph(graph);
        m_clickTracer->setGraphKey(x);

        // 初始化或更新单一坐标文本
        if (!m_coordText) {
            m_coordText = new QCPItemText(m_plot);
            m_coordText->setClipAxisRect(m_plot->axisRect());
            m_coordText->setPadding(QMargins(4, 2, 4, 2));
            m_coordText->setBrush(QBrush(QColor(255, 255, 255, 200))); // 半透明白底
            m_coordText->setPen(QPen(Qt::black));
            m_coordText->setPositionAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            m_coordText->position->setType(QCPItemPosition::ptAbsolute);
            m_coordText->position->setCoords(QPointF(10, -10)); // 右上偏移，避免遮挡标记
        }
        // 文本跟随当前单一标记
        m_coordText->position->setParentAnchor(m_clickTracer->position);
        m_coordText->setText(QString("x: %1\ny: %2")
                             .arg(QString::number(x, 'f', 4))
                             .arg(QString::number(y, 'f', 4)));
    } else {
        // Ctrl 模式：追加一个点标记与文本
        QCPItemTracer* tracer = new QCPItemTracer(m_plot);
        tracer->setStyle(QCPItemTracer::tsCircle);
        tracer->setSize(7);
        tracer->setPen(QPen(Qt::red, 2));
        tracer->setBrush(QBrush(QColor(255, 0, 0, 60)));
        tracer->setInterpolating(true);
        tracer->setGraph(graph);
        tracer->setGraphKey(x);
        m_extraTracers.push_back(tracer);

        QCPItemText* text = new QCPItemText(m_plot);
        text->setClipAxisRect(m_plot->axisRect());
        text->setPadding(QMargins(4, 2, 4, 2));
        text->setBrush(QBrush(QColor(255, 255, 255, 200)));
        text->setPen(QPen(Qt::black));
        text->setPositionAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        text->position->setType(QCPItemPosition::ptAbsolute);
        text->position->setCoords(QPointF(10, -10));
        text->position->setParentAnchor(tracer->position);
        text->setText(QString("x: %1\ny: %2")
                      .arg(QString::number(x, 'f', 4))
                      .arg(QString::number(y, 'f', 4)));
        m_extraTexts.push_back(text);
    }

    // 触发重绘以显示最新标记与文本
    m_plot->replot(QCustomPlot::rpQueuedReplot);
}

// --- ：清理点击标记与坐标文本 ---
void ChartView::clearClickMarker()
{
    if (!m_plot) return;
    if (m_clickTracer) {
        m_plot->removeItem(m_clickTracer);
        m_clickTracer = nullptr;
    }
    if (m_coordText) {
        m_plot->removeItem(m_coordText);
        m_coordText = nullptr;
    }
    // 同时清除所有附加标记与文本
    for (auto* tracer : m_extraTracers) {
        if (tracer) m_plot->removeItem(tracer);
    }
    m_extraTracers.clear();
    for (auto* text : m_extraTexts) {
        if (text) m_plot->removeItem(text);
    }
    m_extraTexts.clear();
}

// 选择工具模式下，鼠标悬停到某条曲线时，显示该曲线的图例名称
void ChartView::onPlotMouseMove(QMouseEvent* event)
{
    if (!m_plot) return;

    // 仅在“选择工具模式”下启用，且标记模式未开启，避免与标记模式冲突
    if (m_primaryState != PrimaryState::Select || m_markOn) {
        if (m_hoverTracer) { m_plot->removeItem(m_hoverTracer); m_hoverTracer = nullptr; }
        if (m_hoverLegendText) { m_plot->removeItem(m_hoverLegendText); m_hoverLegendText = nullptr; }
        m_lastHoverGraphName.clear();
        return;
    }

    // 遍历所有曲线，使用 selectTest 评估距离，找到最近曲线
    QCPGraph* nearestGraph = nullptr;
    double bestDist = std::numeric_limits<double>::max();
    const double threshold = 12.0; // 像素距离阈值

    for (int i = 0; i < m_plot->graphCount(); ++i) {
        QCPGraph* g = m_plot->graph(i);
        if (!g || !g->visible()) continue;
        double d = g->selectTest(event->pos(), false, nullptr);
        if (d >= 0 && d < bestDist) {
            bestDist = d;
            nearestGraph = g;
        }
    }

    if (nearestGraph && bestDist < threshold) {
        const QString graphName = nearestGraph->name();
        if (m_lastHoverGraphName != graphName) m_lastHoverGraphName = graphName;

        // 根据图形对象反查 sampleId，再取完整显示名称（project-batch-short-parallel）
        int matchedSampleId = -1;
        for (auto it = m_sampleGraphs.constBegin(); it != m_sampleGraphs.constEnd(); ++it) {
            if (it.value() == nearestGraph) { matchedSampleId = it.key(); break; }
        }
        QString displayName = graphName;
        if (matchedSampleId >= 0 && m_sampleNames.contains(matchedSampleId)) {
            QString n = m_sampleNames.value(matchedSampleId).trimmed();
            if (!n.isEmpty()) displayName = n;
        }

        // 创建/更新悬停标记与图例文本
        double xCoord = m_plot->xAxis->pixelToCoord(event->pos().x());

        if (!m_hoverTracer) {
            m_hoverTracer = new QCPItemTracer(m_plot);
            m_hoverTracer->setStyle(QCPItemTracer::tsCircle);
            m_hoverTracer->setSize(4); // 更小的标记，降低存在感
            m_hoverTracer->setPen(QPen(QColor(120,120,120), 1)); // 细灰色边框
            m_hoverTracer->setBrush(QBrush(QColor(180, 180, 180, 40))); // 更淡的半透明
            m_hoverTracer->setInterpolating(true);
        }
        m_hoverTracer->setGraph(nearestGraph);
        m_hoverTracer->setGraphKey(xCoord);

        if (!m_hoverLegendText) {
            m_hoverLegendText = new QCPItemText(m_plot);
            m_hoverLegendText->setClipAxisRect(m_plot->axisRect());
            m_hoverLegendText->setPadding(QMargins(4, 2, 4, 2)); // 更小的内边距
            m_hoverLegendText->setBrush(QBrush(QColor(255, 255, 255, 160))); // 更淡的白底
            m_hoverLegendText->setPen(QPen(QColor(80,80,80))); // 深灰色文字框
            m_hoverLegendText->setPositionAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            m_hoverLegendText->position->setType(QCPItemPosition::ptAbsolute);
            // m_hoverLegendText->position->setCoords(QPointF(10, -10)); // 更小偏移
            m_hoverLegendText->position->setCoords(event->pos());

            QFont f; f.setPointSize(9); // 更小字号
            m_hoverLegendText->setFont(f);
        }
        m_hoverLegendText->position->setParentAnchor(m_hoverTracer->position);
        m_hoverLegendText->setText(displayName.isEmpty() ? QStringLiteral("曲线") : displayName);

        m_plot->replot(QCustomPlot::rpQueuedReplot);
    } else {
        // 远离任何曲线，清理悬停标记与文本
        if (m_hoverTracer) { m_plot->removeItem(m_hoverTracer); m_hoverTracer = nullptr; }
        if (m_hoverLegendText) { m_plot->removeItem(m_hoverLegendText); m_hoverLegendText = nullptr; }
        m_lastHoverGraphName.clear();
        m_plot->replot(QCustomPlot::rpQueuedReplot);
    }
}

void ChartView::setLabels(const QString &xLabel, const QString &yLabel)
{
    m_plot->xAxis->setLabel(xLabel);
    m_plot->yAxis->setLabel(yLabel);
}

// 假设在 ChartView 类中封装
// void ChartView::setPlotTitle(const QString& titleText)
// {
//     // if (!m_plot) return;

//     // 删除原来的标题行（如果存在）
//     if (m_plot->plotLayout()->rowCount() > 0)
//         m_plot->plotLayout()->removeAt(0);

//     // // 创建新的标题
//     // QCPTextElement* title = new QCPTextElement(m_plot, titleText, QFont("Arial", 14, QFont::Bold));
//     // m_plot->plotLayout()->insertRow(0);
//     // m_plot->plotLayout()->addElement(0, 0, title);

//     QCPTextElement* m_title;
//     m_plot->plotLayout()->insertRow(0);
// m_title = new QCPTextElement(m_plot, titleText);
// m_plot->plotLayout()->addElement(0, 0, m_title);



// }
 void ChartView::setPlotTitle(const QString& titleText)
{
    if (!m_plot) return;

    // 如果已有标题，直接更新文本
    if (m_titleElement) {
        m_titleElement->setText(titleText);
        m_plot->replot();
        return;
    }

    // 首次创建标题
    m_plot->plotLayout()->insertRow(0);
    // m_titleElement = new QCPTextElement(m_plot, titleText, 
    //                                     QFont("Arial", 14, QFont::Bold));
    m_titleElement = new QCPTextElement(m_plot, titleText);
                                        
    m_plot->plotLayout()->addElement(0, 0, m_titleElement);
    m_plot->replot();
}

void ChartView::setLegendName(const QString &legendName)
{
    if (!m_plot) return;

    // 确保图例可见
    if (!m_plot->legend->visible()) {
        m_plot->legend->setVisible(true);
        m_plot->legend->setFont(QFont("Arial", 10));
    }

    // 添加一个空曲线用于显示图例
    QCPGraph *dummyGraph = m_plot->addGraph();
    dummyGraph->setName(legendName);

    // 数据为空，不绘制曲线
    dummyGraph->setData(QVector<double>(), QVector<double>());

    // 刷新图表
    m_plot->replot();
}

void ChartView::setLegendVisible(bool visible)
{
    if (!m_plot) return;

    m_plot->legend->setVisible(visible);

    // 强制重绘
    m_plot->replot();
}

QColor ChartView::getCurveColor(int sampleId) const
{
    if (!m_plot) return QColor();
    auto it = m_sampleGraphs.find(sampleId);
    if (it == m_sampleGraphs.end()) return QColor();
    QCPGraph* g = it.value();
    if (!g) return QColor();
    return g->pen().color();
}


void ChartView::replot()
{
    QElapsedTimer timer;
    timer.start();

    DEBUG_LOG << "ChartView::replot - Rescaling axes and replotting";
    if (!m_plot) {
        DEBUG_LOG << "ChartView::replot - ERROR: m_plot is null!";
        return;
    }


    
    DEBUG_LOG << "ChartView::replot - Graph count:" << m_plot->graphCount();
    
    // 如果没有图表，直接返回
    if (m_plot->graphCount() == 0) {
        DEBUG_LOG << "ChartView::replot - No graphs to plot, returning";
        return;
    }
    
    // 确保所有图表可见
    for (int i = 0; i < m_plot->graphCount(); ++i) {
        if (m_plot->graph(i)) {
            m_plot->graph(i)->setVisible(true);
            // DEBUG_LOG << "ChartView::replot - Graph" << i << "is visible:" << m_plot->graph(i)->visible();
            
            // 检查图表是否有数据
            // DEBUG_LOG << "ChartView::replot - Graph" << i << "data points:" << m_plot->graph(i)->dataCount();
        }
    }
    
    // 确保坐标轴范围正确设置
    // m_plot->rescaleAxes(true); // true 参数表示只扩展范围，不缩小范围
    m_plot->rescaleAxes(false); // 或者直接 rescaleAxes()，不要 onlyEnlarge=true
    
    // 获取并输出当前坐标轴范围
    QCPRange xRange = m_plot->xAxis->range();
    QCPRange yRange = m_plot->yAxis->range();
    DEBUG_LOG << "ChartView::replot - X axis range:" << xRange.lower << "to" << xRange.upper;
    DEBUG_LOG << "ChartView::replot - Y axis range:" << yRange.lower << "to" << yRange.upper;
    
    // // 如果范围太小，设置一个合理的默认范围
    // if (xRange.size() < 0.1) {
    //     double center = (xRange.lower + xRange.upper) / 2;
    //     m_plot->xAxis->setRange(center - 50, center + 50);
    //     DEBUG_LOG << "ChartView::replot - X axis range too small, setting default range";
    // }
    // if (yRange.size() < 0.1) {
    //     double center = (yRange.lower + yRange.upper) / 2;
    //     m_plot->yAxis->setRange(center - 50, center + 50);
    //     DEBUG_LOG << "ChartView::replot - Y axis range too small, setting default range";
    // }
    
    // 强制重绘
    m_plot->replot(QCustomPlot::rpQueuedReplot);
    
    // 强制更新界面
    QApplication::processEvents();
    
    DEBUG_LOG << "ChartView::replot - Replot completed";

    DEBUG_LOG << "ChartView::绘图replot - Time taken (ms):" << timer.elapsed();
}

// void ChartView::setToolMode(const QString &toolId)
// {
//     if (toolId == "select") {
//         m_currentToolMode = Select;
//         m_plot->setInteraction(QCP::iSelectPlottables, true);
//         m_plot->setInteraction(QCP::iRangeDrag, false);
//         m_plot->setSelectionRectMode(QCP::srmNone);
//         this->setCursor(Qt::ArrowCursor);
//     } 
//     else if (toolId == "pan") {
//         m_currentToolMode = Pan;
//         m_plot->setInteraction(QCP::iSelectPlottables, false);
//         m_plot->setInteraction(QCP::iRangeDrag, true);
//         this->setCursor(Qt::OpenHandCursor);
//     } 
//     else if (toolId == "zoom_rect") {
//         m_currentToolMode = ZoomRect;
//         m_plot->setInteraction(QCP::iSelectPlottables, false);
//         m_plot->setInteraction(QCP::iRangeDrag, false);
//         m_plot->setSelectionRectMode(QCP::srmZoom);
//         this->setCursor(Qt::CrossCursor);
//     }
// }


// void ChartView::setToolMode(const QString &toolId)
// {
//     DEBUG_LOG;

//     // ---- 清理之前的框选缩放状态 ----
//     if (m_currentToolMode == ZoomRect && m_rubberBand) {
//         m_rubberBand->hide();
//         delete m_rubberBand;
//         m_rubberBand = nullptr;
//         m_plot->setSelectionRectMode(QCP::srmNone);  // 确保退出框选
//     }
    
//     if (toolId == "select") {
//         // 【修正】使用类名作用域来访问枚举值
//         m_currentToolMode = ChartView::Select;
//         m_plot->setInteraction(QCP::iSelectPlottables, true);
//         m_plot->setInteraction(QCP::iRangeDrag, false);
//         m_plot->setSelectionRectMode(QCP::srmNone);
//         this->setCursor(Qt::ArrowCursor);
//     } 
//     else if (toolId == "pan") {
//         m_currentToolMode = ChartView::Pan;
//         m_plot->setInteraction(QCP::iSelectPlottables, false);
//         m_plot->setInteraction(QCP::iRangeDrag, true);
//         this->setCursor(Qt::OpenHandCursor);
//     } 
//     // else if (toolId == "zoom_rect") {
//     //     m_currentToolMode = ChartView::ZoomRect;
//     //     m_plot->setInteraction(QCP::iSelectPlottables, false);
//     //     m_plot->setInteraction(QCP::iRangeDrag, false);
//     //     m_plot->setSelectionRectMode(QCP::srmZoom);
//     //     this->setCursor(Qt::CrossCursor);
//     // }
//     else if (toolId == "zoom_rect") {
//         m_currentToolMode = ChartView::ZoomRect;
//         m_plot->setInteraction(QCP::iSelectPlottables, false);
//         m_plot->setInteraction(QCP::iRangeDrag, false);
//         m_plot->setSelectionRectMode(QCP::srmZoom);  // 关键
//         this->setCursor(Qt::CrossCursor);
//     }
//     DEBUG_LOG;
// }

void ChartView::setToolMode(const QString &toolId)
{
    if (!m_plot) return;

    // 将外部旧接口统一映射到轻量状态机事件，避免与内部按钮逻辑不一致
    if (toolId == "select") {
        applyEvent(ToolEvent::ClickSelect);
    } else if (toolId == "pan") {
        applyEvent(ToolEvent::TogglePanOn);
    } else if (toolId == "zoom_rect") {
        applyEvent(ToolEvent::ToggleZoomRectOn);
    }
    // 统一渲染当前状态，刷新按钮背景、交互模式、光标与过滤器
    renderState();
}

// ===================== 轻量状态机实现 =====================
// 应用事件，纯修改内部状态，不直接更新UI
void ChartView::applyEvent(ToolEvent e)
{
    switch (e) {
    case ToolEvent::ClickSelect:
        m_primaryState = PrimaryState::Select;
        // 策略：进入选择时关闭标记，避免光标冲突
        m_markOn = false;
        break;

    case ToolEvent::TogglePanOn:
        m_primaryState = PrimaryState::Pan;
        m_markOn = false;
        break;
    case ToolEvent::TogglePanOff:
        // 退出拖动统一回到选择
        m_primaryState = PrimaryState::Select;
        break;

    case ToolEvent::ToggleZoomRectOn:
        m_primaryState = PrimaryState::ZoomRect;
        m_markOn = false;
        break;
    case ToolEvent::ToggleZoomRectOff:
        m_primaryState = PrimaryState::Select;
        break;

    case ToolEvent::ToggleMarkOn:
        m_markOn = true;
        break;
    case ToolEvent::ToggleMarkOff:
        m_markOn = false;
        // 关闭标记时清理标记与坐标文本
        clearClickMarker();
        break;

    case ToolEvent::ClickZoomOut:
        zoomOut();
        break;
    case ToolEvent::ClickResetZoom:
        zoomReset();
        break;
    }
}

// 根据当前状态统一刷新UI（按钮、交互、光标、图标、过滤器）
void ChartView::renderState()
{
    if (!m_plot) return;

    // 1) 同步按钮勾选（使用 QSignalBlocker 阻断信号，避免程序化勾选触发 toggled 槽导致状态被覆盖）
    {
        QSignalBlocker b1(m_selectToolBtn);
        QSignalBlocker b2(m_panBtn);
        QSignalBlocker b3(m_zoomInBtn);
        QSignalBlocker b4(m_markPointBtn);
        if (m_selectToolBtn) m_selectToolBtn->setChecked(m_primaryState == PrimaryState::Select);
        if (m_panBtn)        m_panBtn->setChecked(m_primaryState == PrimaryState::Pan);
        if (m_zoomInBtn)     m_zoomInBtn->setChecked(m_primaryState == PrimaryState::ZoomRect);
        if (m_markPointBtn)  m_markPointBtn->setChecked(m_markOn);
    }

    // 2) 交互与选择矩形、事件过滤器与选择框开关
    if (m_primaryState == PrimaryState::Select) {
        m_plot->setInteraction(QCP::iSelectPlottables, true);
        m_plot->setInteraction(QCP::iRangeDrag, false);
        m_plot->setSelectionRectMode(QCP::srmNone);
        // 启用选择模式相关过滤器
        m_selectModeActive = true;
        m_plot->installEventFilter(this);
        if (!m_rubberBand) m_rubberBand = new QRubberBand(QRubberBand::Rectangle, m_plot);
    } else if (m_primaryState == PrimaryState::Pan) {
        m_plot->setInteraction(QCP::iSelectPlottables, false);
        m_plot->setInteraction(QCP::iRangeDrag, true);
        m_plot->setSelectionRectMode(QCP::srmNone);
        m_selectModeActive = false;
        // 保留事件过滤器以捕获键盘快捷键（如 Delete/Undo），但不处理鼠标左键
        m_plot->installEventFilter(this);
        if (m_rubberBand) m_rubberBand->hide();
    } else { // ZoomRect
        m_plot->setInteraction(QCP::iSelectPlottables, false);
        m_plot->setInteraction(QCP::iRangeDrag, false);
        m_plot->setSelectionRectMode(QCP::srmZoom);
        m_selectModeActive = false;
        // 保留事件过滤器以捕获键盘快捷键（如 Delete/Undo），但不处理鼠标左键
        m_plot->installEventFilter(this);
        if (m_rubberBand) m_rubberBand->hide();
    }

    // 3) 光标与标记模式优先级（标记开启则覆盖光标与交互）
    if (m_markOn) {
        // 标记模式下应允许点击曲线（用于触发 plottableClick），但不允许拖动与矩形选择，以避免不必要的交互冲突
        m_plot->setInteraction(QCP::iSelectPlottables, true);   // 保持点击事件可用
        m_plot->setInteraction(QCP::iRangeDrag, false);         // 禁用拖动
        m_plot->setSelectionRectMode(QCP::srmNone);             // 禁用矩形选择

        // 标记模式关闭选择框行为，但安装事件过滤器以处理右键删除与键盘清除
        m_selectModeActive = false;
        m_plot->installEventFilter(this);
        if (m_rubberBand) m_rubberBand->hide();

        // 进入标记模式时，视觉上清除其它工具按钮的勾选背景（避免多按钮同时高亮）
        {
            QSignalBlocker b1(m_selectToolBtn);
            QSignalBlocker b2(m_panBtn);
            QSignalBlocker b3(m_zoomInBtn);
            if (m_selectToolBtn) m_selectToolBtn->setChecked(false);
            if (m_panBtn)        m_panBtn->setChecked(false);
            if (m_zoomInBtn)     m_zoomInBtn->setChecked(false);
        }
        // 创建并应用标记专用光标
        if (m_markCursor.shape() == Qt::ArrowCursor) {
            QPixmap px(32, 32);
            px.fill(Qt::transparent);
            QPainter painter(&px);
            painter.setRenderHint(QPainter::Antialiasing, true);
            QPen pen(QColor(0, 0, 0), 2);
            painter.setPen(pen);
            painter.drawEllipse(QRect(8, 8, 16, 16));
            painter.drawLine(QPoint(16, 4), QPoint(16, 10));
            painter.drawLine(QPoint(16, 22), QPoint(16, 28));
            painter.drawLine(QPoint(4, 16), QPoint(10, 16));
            painter.drawLine(QPoint(22, 16), QPoint(28, 16));
            painter.end();
            m_markCursor = QCursor(px, 16, 16);
        }
        m_plot->setCursor(m_markCursor);
    } else {
        // 根据主状态恢复光标
        if (m_primaryState == PrimaryState::Select) {
            m_plot->setCursor(Qt::ArrowCursor);
        } else if (m_primaryState == PrimaryState::Pan) {
            m_plot->setCursor(Qt::OpenHandCursor);
        } else {
            m_plot->setCursor(Qt::CrossCursor);
        }
    }

    // 4) 图标同步（标记图标）
    if (m_markPointBtn) m_markPointBtn->setIcon(m_markOn ? m_markIconActive : m_markIcon);
}


// 事件过滤器，用于在选择模式下用左键拖动绘制矩形框
bool ChartView::eventFilter(QObject* obj, QEvent* event)
{
    // 仅在绘图区域处理事件
    if (obj == m_plot) {
        // 当处于标记模式时，处理右键删除与键盘快捷键
        if (m_markOn) {
            switch (event->type()) {
            case QEvent::MouseButtonPress: {
                QMouseEvent* me = static_cast<QMouseEvent*>(event);
                if (me->button() == Qt::RightButton) {
                    // 右键删除最近标记
                    deleteNearestMarker(me->pos());
                    return true; // 事件已处理
                }
                break;
            }
            case QEvent::KeyPress: {
                QKeyEvent* ke = static_cast<QKeyEvent*>(event);
                if (ke->key() == Qt::Key_Delete) {
                    // Delete 清除全部标记
                    clearClickMarker();
                    return true;
                }
                if ((ke->modifiers() & Qt::ControlModifier) && ke->key() == Qt::Key_Z) {
                    // Ctrl+Z 撤销最后一个标记
                    undoLastMarker();
                    return true;
                }
                break;
            }
            default:
                break;
            }
        }

        // 选择模式下的矩形框绘制
        if (m_selectModeActive) {
        switch (event->type()) {
        case QEvent::MouseButtonPress: {
            QMouseEvent* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                m_zoomStart = me->pos();
                if (!m_rubberBand) {
                    m_rubberBand = new QRubberBand(QRubberBand::Rectangle, m_plot);
                }
                m_rubberBand->setGeometry(QRect(m_zoomStart, QSize()));
                m_rubberBand->show();
                return true; // 事件已处理
            }
            break;
        }
        case QEvent::MouseMove: {
            QMouseEvent* me = static_cast<QMouseEvent*>(event);
            if (me->buttons() & Qt::LeftButton) {
                if (m_rubberBand) {
                    QRect rect = QRect(m_zoomStart, me->pos()).normalized();
                    m_rubberBand->setGeometry(rect);
                    return true; // 事件已处理
                }
            }
            break;
        }
        case QEvent::MouseButtonRelease: {
            QMouseEvent* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                if (m_rubberBand) {
                    m_rubberBand->hide();
                }
                return true; // 事件已处理
            }
            break;
        }
        default:
            break;
        }
        }
    }
    // 其他情况走默认处理
    return QWidget::eventFilter(obj, event);
}

// 删除距离鼠标位置最近的标记点（主标记 + 附加标记）
void ChartView::deleteNearestMarker(const QPoint& mousePos)
{
    if (!m_plot) return;

    struct Candidate { QCPItemTracer* tracer; QCPItemText* text; double dist2; int index; bool isMain; };
    Candidate best{nullptr, nullptr, std::numeric_limits<double>::max(), -1, false};

    // 主标记
    if (m_clickTracer) {
        QPointF px = m_clickTracer->position->pixelPosition();
        double d2 = std::pow(px.x() - mousePos.x(), 2) + std::pow(px.y() - mousePos.y(), 2);
        best = { m_clickTracer, m_coordText, d2, -1, true };
    }

    // 附加标记
    for (int i = 0; i < m_extraTracers.size(); ++i) {
        QCPItemTracer* t = m_extraTracers[i];
        if (!t) continue;
        QPointF px = t->position->pixelPosition();
        double d2 = std::pow(px.x() - mousePos.x(), 2) + std::pow(px.y() - mousePos.y(), 2);
        if (d2 < best.dist2) {
            best = { t, (i < m_extraTexts.size() ? m_extraTexts[i] : nullptr), d2, i, false };
        }
    }

    if (!best.tracer) return; // 无标记

    // 从图上移除
    m_plot->removeItem(best.tracer);
    if (best.text) m_plot->removeItem(best.text);

    // 从容器移除
    if (best.isMain) {
        m_clickTracer = nullptr;
        m_coordText = nullptr;
    } else if (best.index >= 0 && best.index < m_extraTracers.size()) {
        m_extraTracers.remove(best.index);
        if (best.index < m_extraTexts.size()) m_extraTexts.remove(best.index);
    }

    m_plot->replot();
}

// 撤销最后一个标记（优先删除附加标记）
void ChartView::undoLastMarker()
{
    if (!m_plot) return;
    if (!m_extraTracers.isEmpty()) {
        QCPItemTracer* t = m_extraTracers.takeLast();
        QCPItemText* tx = m_extraTexts.isEmpty() ? nullptr : m_extraTexts.takeLast();
        if (t) m_plot->removeItem(t);
        if (tx) m_plot->removeItem(tx);
    } else if (m_clickTracer) {
        m_plot->removeItem(m_clickTracer);
        if (m_coordText) m_plot->removeItem(m_coordText);
        m_clickTracer = nullptr;
        m_coordText = nullptr;
    }
    m_plot->replot();
}



void ChartView::zoomIn()
{
    if (m_plot) {
        // 设置为放大模式，使用QCP::srmZoom
        m_plot->setSelectionRectMode(QCP::srmZoom);
        m_plot->setCursor(Qt::CrossCursor); // 设置鼠标为十字形状
        m_currentToolMode = ZoomRect;
        
        // 禁用拖动功能
        m_plot->setInteraction(QCP::iRangeDrag, false);
    }
}

void ChartView::zoomOut()
{
    if (m_plot) {
        // 缩小图表，缩放因子为1.2（扩大坐标范围）
        m_plot->xAxis->scaleRange(1.2, m_plot->xAxis->range().center());
        m_plot->yAxis->scaleRange(1.2, m_plot->yAxis->range().center());
        m_plot->replot();
    }
}

void ChartView::zoomReset()
{
    if (m_plot) {
        // 重置图表的缩放级别
        m_plot->rescaleAxes();
        m_plot->replot();
    }
}


// void ChartView::mousePressEvent(QMouseEvent *event) {
//     if (m_currentToolMode == ZoomRect && event->button() == Qt::LeftButton) {
//         m_zoomStart = event->pos(); // 记录起点
//         m_rubberBand = new QRubberBand(QRubberBand::Rectangle, this);
//         m_rubberBand->setGeometry(QRect(m_zoomStart, QSize()));
//         m_rubberBand->show();
//     }
//     QWidget::mousePressEvent(event);
// }

// void ChartView::mouseMoveEvent(QMouseEvent *event) {
//     if (m_currentToolMode == ZoomRect && m_rubberBand) {
//         m_rubberBand->setGeometry(QRect(m_zoomStart, event->pos()).normalized());
//     }
//     QWidget::mouseMoveEvent(event);
// }

// void ChartView::mouseReleaseEvent(QMouseEvent *event) {
//     if (m_currentToolMode == ZoomRect && event->button() == Qt::LeftButton && m_rubberBand) {
//         QRect zoomRect = m_rubberBand->geometry();
//         m_rubberBand->hide();
//         delete m_rubberBand;
//         m_rubberBand = nullptr;

//         // 转换为坐标轴范围
//         double x1 = m_plot->xAxis->pixelToCoord(zoomRect.left());
//         double x2 = m_plot->xAxis->pixelToCoord(zoomRect.right());
//         double y1 = m_plot->yAxis->pixelToCoord(zoomRect.bottom());
//         double y2 = m_plot->yAxis->pixelToCoord(zoomRect.top());

//         m_plot->xAxis->setRange(qMin(x1, x2), qMax(x1, x2));
//         m_plot->yAxis->setRange(qMin(y1, y2), qMax(y1, y2));
//         m_plot->replot();
//     }
//     QWidget::mouseReleaseEvent(event);
// }


void ChartView::zoomOutFixed()
{
    DEBUG_LOG;
    if (!m_plot) return;

    // 获取当前坐标轴范围中心
    double xCenter = m_plot->xAxis->range().center();
    double yCenter = m_plot->yAxis->range().center();

    // 缩放比例，例如放大 1.2 倍表示缩小视图
    double factor = 1.2;

    m_plot->xAxis->scaleRange(factor, xCenter);
    m_plot->yAxis->scaleRange(factor, yCenter);

    m_plot->replot();
}


void ChartView::contextMenuEvent(QContextMenuEvent *event)
{
//     QMenu menu(this);

//     QAction* addCurveAction = menu.addAction(tr("添加曲线..."));
//     // QAction* addMultiCurveAction = menu.addAction(tr("添加多条曲线..."));

//     QAction* selectedAction = menu.exec(event->globalPos());
//     if (selectedAction == addCurveAction) {
//         emit requestAddCurve();
//     } 
// //     else if (selectedAction == addMultiCurveAction) {
// //         emit requestAddMultipleCurves();
// //     }
}

void ChartView::enterEvent(QEvent *event)
{
    // 鼠标进入时显示工具栏
    if (m_toolBar) {
        m_toolBar->show();
    }
    QWidget::enterEvent(event);
}

void ChartView::leaveEvent(QEvent *event)
{
    // 鼠标离开时隐藏工具栏
    if (m_toolBar) {
        m_toolBar->hide();
    }
    // 鼠标离开绘图区域，清理悬停效果
    if (m_plot) {
        if (m_hoverTracer) { m_plot->removeItem(m_hoverTracer); m_hoverTracer = nullptr; }
        if (m_hoverLegendText) { m_plot->removeItem(m_hoverLegendText); m_hoverLegendText = nullptr; }
        m_lastHoverGraphName.clear();
        m_plot->replot(QCustomPlot::rpQueuedReplot);
    }
    QWidget::leaveEvent(event);
}

void ChartView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    emit resized();
}


void ChartView::showContextMenu(const QPoint &pos) {
    QMenu menu(this);
    menu.addAction("保存为图片", this, SLOT(savePlot()));
    menu.addAction("导出数据", this, SLOT(exportData()));
    menu.exec(m_plot->mapToGlobal(pos));
}

// 在 ChartView.cpp 中添加：
QList<int> ChartView::getExistingCurveIds() const
{
    return m_sampleGraphs.keys();
}

int ChartView::getCurveCount() const
{
    return m_plot->graphCount();
}

QVector<QPair<QVector<double>, QVector<double>>> ChartView::getCurvesData() const
{
    QVector<QPair<QVector<double>, QVector<double>>> allData;
    
    for (int i = 0; i < m_plot->graphCount(); ++i) {
        QCPGraph* graph = m_plot->graph(i);
        QVector<double> xData, yData;
        
        // 获取图形的数据点
        for (int j = 0; j < graph->data()->size(); ++j) {
            xData.append(graph->data()->at(j)->key);
            yData.append(graph->data()->at(j)->value);
        }
        
        allData.append(qMakePair(xData, yData));
    }
    
    return allData;
}

QVector<QPair<QVector<double>, QVector<double>>> ChartView::getSelectedCurvesData() const
{
    QVector<QPair<QVector<double>, QVector<double>>> selectedData;
    
    // 安全检查
    if (!m_plot) {
        WARNING_LOG << "获取选中曲线数据失败：m_plot为空指针";
        return selectedData;
    }
    
    for (int i = 0; i < m_plot->graphCount(); ++i) {
        QCPGraph* graph = m_plot->graph(i);
        if (graph && graph->selected()) {
            QVector<double> xData, yData;
            
            // 获取图形的数据点
            for (int j = 0; j < graph->data()->size(); ++j) {
                xData.append(graph->data()->at(j)->key);
                yData.append(graph->data()->at(j)->value);
            }
            
            selectedData.append(qMakePair(xData, yData));
        }
    }
    
    return selectedData;
}

QList<QPair<int, QPair<QVector<double>, QVector<double>>>> ChartView::getSelectedCurvesWithSampleIds() const
{
    QList<QPair<int, QPair<QVector<double>, QVector<double>>>> selectedData;
    
    // 安全检查
    if (!m_plot) {
        WARNING_LOG << "获取选中曲线数据失败：m_plot为空指针";
        return selectedData;
    }
    
    for (int i = 0; i < m_plot->graphCount(); ++i) {
        QCPGraph* graph = m_plot->graph(i);
        if (graph && graph->selected()) {
            QVector<double> xData, yData;
            
            // 获取图形的数据点
            for (int j = 0; j < graph->data()->size(); ++j) {
                xData.append(graph->data()->at(j)->key);
                yData.append(graph->data()->at(j)->value);
            }
            
            // 查找对应的样本ID
            int sampleId = -1;
            for (auto it = m_sampleGraphs.constBegin(); it != m_sampleGraphs.constEnd(); ++it) {
                if (it.value() == graph) {
                    sampleId = it.key();
                    break;
                }
            }
            
            selectedData.append(qMakePair(sampleId, qMakePair(xData, yData)));
        }
    }
    
    return selectedData;
}

bool ChartView::exportDataToCSV(const QString& filePath) const
{
    DEBUG_LOG;
    DEBUG_LOG << "导出数据到CSV文件" << this;
    // 安全检查：确保m_plot不为空
    if (!this) {
        WARNING_LOG << "导出失败：thist为空指针";
        return false;
    }
    

    if (!m_plot) {
        WARNING_LOG << "导出失败：m_plot为空指针";
        return false;
    }
    
    // 检查是否有曲线
    if (m_plot->graphCount() <= 0) {
        WARNING_LOG << "导出失败：没有可导出的曲线数据";
        return false;
    }
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        WARNING_LOG << "导出失败：无法打开文件" << filePath;
        return false;
    }
    
    QTextStream out(&file);
    
    // 写入表头（Y 列使用对应曲线图例名）
    out << "X";
    for (int i = 0; i < m_plot->graphCount(); ++i) {
        QCPGraph* graph = m_plot->graph(i);
        QString colName;
        if (graph && !graph->name().trimmed().isEmpty()) {
            colName = graph->name();
        } else {
            colName = QString("Y%1").arg(i+1);
        }
        out << "," << colName;
    }
    out << "\n";
    
    // 获取所有曲线数据
    QVector<QPair<QVector<double>, QVector<double>>> allData = getCurvesData();
    
    // 找出最大数据点数量
    int maxPoints = 0;
    for (const auto& data : allData) {
        maxPoints = qMax(maxPoints, data.first.size());
    }
    
    // 写入数据
    for (int i = 0; i < maxPoints; ++i) {
        // 只有第一条曲线有数据时才写入X值
        if (!allData.isEmpty() && i < allData.first().first.size()) {
            out << QString::number(allData.first().first.at(i));
        } else {
            out << "";
        }
        
        // 写入所有曲线的Y值
        for (const auto& data : allData) {
            out << ",";
            if (i < data.second.size()) {
                out << QString::number(data.second.at(i));
            }
        }
        
        out << "\n";
    }
    
    file.close();
    return true;
}

bool ChartView::exportSelectedDataToCSV(const QString& filePath) const
{
    // 安全检查：确保m_plot不为空
    if (!m_plot) {
        WARNING_LOG << "导出失败：m_plot为空指针";
        return false;
    }
    
    // 获取选中的曲线数据
    QVector<QPair<QVector<double>, QVector<double>>> selectedData = getSelectedCurvesData();
    
    // 检查是否有选中的曲线
    if (selectedData.isEmpty()) {
        WARNING_LOG << "导出失败：没有选中的曲线数据";
        return false;
    }
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        WARNING_LOG << "导出失败：无法打开文件" << filePath;
        return false;
    }
    
    QTextStream out(&file);
    
    // 写入表头（仅选中曲线，列名为图例名）
    out << "X";
    for (int i = 0; i < m_plot->graphCount(); ++i) {
        QCPGraph* graph = m_plot->graph(i);
        if (graph && graph->selected()) {
            QString colName = graph->name().trimmed().isEmpty() ? QString("Y") : graph->name();
            out << "," << colName;
        }
    }
    out << "\n";
    
    // 找出最大数据点数量
    int maxPoints = 0;
    for (const auto& data : selectedData) {
        maxPoints = qMax(maxPoints, data.first.size());
    }
    
    // 写入数据
    for (int i = 0; i < maxPoints; ++i) {
        // 只有第一条曲线有数据时才写入X值
        if (!selectedData.isEmpty() && i < selectedData.first().first.size()) {
            out << QString::number(selectedData.first().first.at(i));
        } else {
            out << "";
        }
        
        // 写入所有选中曲线的Y值
        for (const auto& data : selectedData) {
            out << ",";
            if (i < data.second.size()) {
                out << QString::number(data.second.at(i));
            }
        }
        
        out << "\n";
    }
    
    file.close();
    return true;
}

bool ChartView::exportDataToXLSX(const QString& filePath) const
{
    DEBUG_LOG;
    DEBUG_LOG << "导出数据到XLSX文件" << this;
    // 安全检查：确保m_plot不为空
    if (!this) {
        WARNING_LOG << "导出失败：this为空指针";
        return false;
    }
    
    if (!m_plot) {
        WARNING_LOG << "导出失败：m_plot为空指针";
        return false;
    }
    
    // 检查是否有曲线
    if (m_plot->graphCount() <= 0) {
        WARNING_LOG << "导出失败：没有可导出的曲线数据";
        return false;
    }
    
    // 创建XLSX文档
    QXlsx::Document xlsx;
    
    // 写入表头（Y 列名使用图例名）
    xlsx.write(1, 1, "X");
    for (int i = 0; i < m_plot->graphCount(); ++i) {
        QCPGraph* graph = m_plot->graph(i);
        QString colName;
        if (graph && !graph->name().trimmed().isEmpty()) {
            colName = graph->name();
        } else {
            colName = QString("Y%1").arg(i+1);
        }
        xlsx.write(1, i+2, colName);
    }
    
    // 获取所有曲线数据
    QVector<QPair<QVector<double>, QVector<double>>> allData = getCurvesData();
    
    // 找出最大数据点数量
    int maxPoints = 0;
    for (const auto& data : allData) {
        maxPoints = qMax(maxPoints, data.first.size());
    }
    
    // 写入数据
    for (int i = 0; i < maxPoints; ++i) {
        // 只有第一条曲线有数据时才写入X值
        if (!allData.isEmpty() && i < allData.first().first.size()) {
            xlsx.write(i+2, 1, allData.first().first.at(i));
        }
        
        // 写入所有曲线的Y值
        for (int j = 0; j < allData.size(); ++j) {
            const auto& data = allData[j];
            if (i < data.second.size()) {
                xlsx.write(i+2, j+2, data.second.at(i));
            }
        }
    }
    
    // 保存文件
    bool success = xlsx.saveAs(filePath);
    if (!success) {
        WARNING_LOG << "导出失败：无法保存XLSX文件" << filePath;
    } else {
        DEBUG_LOG << "导出成功：" << filePath;
    }
    
    return success;
}

bool ChartView::exportSelectedDataToXLSX(const QString& filePath) const
{
    // 安全检查：确保m_plot不为空
    if (!m_plot) {
        WARNING_LOG << "导出失败：m_plot为空指针";
        return false;
    }
    
    // 获取选中的曲线数据
    QVector<QPair<QVector<double>, QVector<double>>> selectedData = getSelectedCurvesData();
    
    // 检查是否有选中的曲线
    if (selectedData.isEmpty()) {
        WARNING_LOG << "导出失败：没有选中的曲线数据";
        return false;
    }
    
    // 创建XLSX文档
    QXlsx::Document xlsx;
    
    // 写入表头（仅选中曲线，列名为图例名）
    xlsx.write(1, 1, "X");
    int col = 2;
    for (int i = 0; i < m_plot->graphCount(); ++i) {
        QCPGraph* graph = m_plot->graph(i);
        if (graph && graph->selected()) {
            QString colName = graph->name().trimmed().isEmpty() ? QString("Y") : graph->name();
            xlsx.write(1, col, colName);
            col++;
        }
    }
    
    // 找出最大数据点数量
    int maxPoints = 0;
    for (const auto& data : selectedData) {
        maxPoints = qMax(maxPoints, data.first.size());
    }
    
    // 写入数据
    for (int i = 0; i < maxPoints; ++i) {
        // 只有第一条曲线有数据时才写入X值
        if (!selectedData.isEmpty() && i < selectedData.first().first.size()) {
            xlsx.write(i+2, 1, selectedData.first().first.at(i));
        }
        
        // 写入所有选中曲线的Y值
        for (int j = 0; j < selectedData.size(); ++j) {
            const auto& data = selectedData[j];
            if (i < data.second.size()) {
                xlsx.write(i+2, j+2, data.second.at(i));
            }
        }
    }
    
    // 保存文件
    bool success = xlsx.saveAs(filePath);
    if (!success) {
        WARNING_LOG << "导出失败：无法保存XLSX文件" << filePath;
    } else {
        DEBUG_LOG << "导出成功：" << filePath;
    }
    
    return success;
}


bool ChartView::exportPlot(const QString& filePath) const
{
    if (!m_plot)
    {
        WARNING_LOG << "QCustomPlot 指针为空，无法导出！";
        return false;
    }

    // 保证图形是最新绘制的
    m_plot->replot();

    // 当前控件大小
    int width  = m_plot->width();
    int height = m_plot->height();

    QFileInfo fi(filePath);
    if (!fi.exists() && !fi.dir().exists())
    {
        WARNING_LOG << "导出路径不存在：" << filePath;
        return false;
    }

    bool ok = false;

    if (filePath.endsWith(".png", Qt::CaseInsensitive))
    {
        ok = m_plot->savePng(filePath, width, height, 96); // dpi 可按需调整
        // QImage img = m_plot->toPixmap(width, height).toImage().convertToFormat(QImage::Format_RGB32);
        // ok = img.save(filePath, "PNG");
    }
    else if (filePath.endsWith(".pdf", Qt::CaseInsensitive))
    {
        ok = m_plot->savePdf(filePath, width, height);
    }
    // else if (filePath.endsWith(".svg", Qt::CaseInsensitive))
    // {
    //     ok = m_plot->saveSvg(filePath, width, height);
    // }
    else
    {
        WARNING_LOG << "不支持的文件类型：" << filePath;
        return false;
    }

    if (!ok)
    {
        WARNING_LOG << "导出失败：" << filePath;
        return false;
    }

    DEBUG_LOG << "导出成功：" << filePath;
    return true;
}

void ChartView::panMode()
{
    // 设置为拖动模式
    m_currentToolMode = Pan;
    
    // 确保启用拖动交互
    m_plot->setInteraction(QCP::iRangeDrag, true);
    m_plot->setInteraction(QCP::iSelectPlottables, false);
    
    // 取消放大模式
    m_plot->setSelectionRectMode(QCP::srmNone);
    m_plot->setCursor(Qt::OpenHandCursor);
}

void ChartView::exportImage()
{
    if (m_plot && m_plotExporter) {
        m_plotExporter->exportImage(m_plot);
    }
}

void ChartView::exportData()
{
    if (m_plot && m_plotExporter) {
        m_plotExporter->exportData(m_plot);
    }
}

ChartView::~ChartView()
{
    if (m_plotExporter) {
        delete m_plotExporter;
        m_plotExporter = nullptr;
    }
}

void ChartView::addScatterPoints(const QVector<double>& x, const QVector<double>& y, const QString& name, const QColor& color, int pointSize)
{
    if (!m_plot) {
        WARNING_LOG << "ChartView::addScatterPoints - m_plot 为空";
        return;
    }
    if (x.isEmpty() || y.isEmpty() || x.size() != y.size()) {
        WARNING_LOG << "ChartView::addScatterPoints - 输入数据为空或长度不一致";
        return;
    }
    QCPGraph* graph = m_plot->addGraph();
    if (!graph) {
        WARNING_LOG << "ChartView::addScatterPoints - 创建图形失败";
        return;
    }
    graph->setData(x, y);
    graph->setName(name);
    QPen pen(color);
    pen.setWidth(1);
    graph->setPen(pen);
    graph->setLineStyle(QCPGraph::lsNone);
    graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, color, color, pointSize));
    // 扩展坐标轴范围
    double xMin = *std::min_element(x.begin(), x.end());
    double xMax = *std::max_element(x.begin(), x.end());
    double yMin = *std::min_element(y.begin(), y.end());
    double yMax = *std::max_element(y.begin(), y.end());
    if (m_plot->graphCount() == 1) {
        m_plot->xAxis->setRange(xMin, xMax);
        m_plot->yAxis->setRange(yMin, yMax);
    } else {
        m_plot->xAxis->setRange(qMin(m_plot->xAxis->range().lower, xMin), qMax(m_plot->xAxis->range().upper, xMax));
        m_plot->yAxis->setRange(qMin(m_plot->yAxis->range().lower, yMin), qMax(m_plot->yAxis->range().upper, yMax));
    }
}
