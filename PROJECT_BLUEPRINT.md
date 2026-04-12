# TobaccoAnalysis 项目框架蓝图

## 项目目标
一个基于 Qt 的烟草数据导入、处理、差异度分析与可视化桌面工具。

## 技术栈
- 语言/标准：C++17
- GUI：Qt5 (Core/Gui/Widgets/Sql/Charts/Xml/Concurrent/PrintSupport)
- 绘图：QCustomPlot
- Excel：QXlsx
- 构建：CMake (>=3.16), MSVC
- 数据库：MySQL（QtSql + libmysql.dll）

## 目录树（核心）
```
CMakeLists.txt
src/
  main.cpp
  core/
    AppInitializer.*
    common.*
    entities/        # 业务实体
    models/          # TableModel/数据模型
    singletons/      # 单例管理
    sql/             # SQL配置加载与校验
  data_access/       # DAO与数据库连接
  services/
    DataProcessingService.*
    analysis/        # 评估/差异度/代表样选择
    algorithm/       # 算法实现与策略
    data_import/     # 数据导入 Worker
  gui/
    mainwindow.*
    dialogs/
    views/
      workbenches/
  utils/
    file_handler/    # Excel/CSV 解析与导出
third_party/
  QXlsx/
  qcustomplot/
config/
  config.json
  sql_config.json
sql/
```

## 结构解析（感知 / 决策 / 执行）
- 感知层（数据获取与输入）
  - `data_access/*DAO`：数据库读取原始曲线与样本信息
  - `services/data_import/*`：Excel/CSV 导入
  - `utils/file_handler/*`：文件解析与导出
- 决策层（参数与算法选择）
  - `core/common.h`：`ProcessingParameters` 统一参数模型
  - `gui/dialogs/*ParameterSettingsDialog`：参数配置入口
  - `services/DataProcessingService`：按参数注册并选择算法步骤
  - `services/analysis/*`：差异度/代表样选择策略
- 执行层（计算与可视化输出）
  - `services/algorithm/*`：具体算法实现（裁剪/归一化/平滑/微分/峰检测/对齐）
  - `gui/views/*` + `workbenches/*`：图表渲染、表格展示、导出

## 核心逻辑流（数据从输入到输出）
1. 应用启动：`main.cpp` 初始化日志/配置/数据库 → `AppInitializer::initialize()`
2. 数据获取：DAO 拉取样本曲线数据或导入文件解析
3. 参数设定：对话框编辑 `ProcessingParameters`
4. 处理流水线：`DataProcessingService` 根据数据类型组装阶段（Raw → Clip → Normalize → Smooth → Derivative → …）
5. 结果组织：输出 `SampleDataFlexible / BatchGroupData`，写入阶段曲线与指标
6. 差异度分析：`SampleComparisonService` / `ParallelSampleAnalysisService` 计算排名与代表样
7. GUI展示：`ChartView` 绘图 + `DifferenceResultModel` 表格 + 导出 Excel/CSV

## 关键接口（类/函数）
- 启动与依赖注入
  - `AppInitializer::initialize()`：加载配置、初始化数据库与服务
- 统一参数模型
  - `ProcessingParameters`：所有处理/算法/对齐参数
- 处理流水线
  - `DataProcessingService::run*Tg*Pipeline(...)`：单样本处理入口
  - `DataProcessingService::run*Tg*PipelineForMultiple(...)`：批量处理入口
  - `DataProcessingService::registerSteps()`：算法策略注册
- 分析服务
  - `SampleComparisonService::calculateRankingFromCurves(...)`：差异度排名
  - `ParallelSampleAnalysisService::selectRepresentativesInBatch(...)`：组内代表样选择
- GUI工作台
  - `TgSmallDifferenceWorkbench::calculateAndDisplay()`：差异度计算 + 绘图 + 表格
  - `ChartView`：曲线渲染/高亮

## 开发约定（从代码中总结）
- 命名习惯
  - 类名 `PascalCase`；函数/变量 `camelCase`；宏/常量 `UPPER_SNAKE_CASE`
  - 数据类型/阶段枚举集中在 `core/common.h`
- 参数与状态
  - 统一使用 `ProcessingParameters` 传参，减少 GUI 与服务耦合
  - 阶段统一封装为 `StageData`，结果以 `SampleDataFlexible`/`BatchGroupData` 传递
- 错误处理与日志
  - 使用 `Logger` 统一日志；严重错误弹 `QMessageBox`
  - 数据缺失时通常返回空结果并警告，不直接崩溃
- 线程与数据库
  - 处理逻辑可能在后台线程执行，必要时克隆数据库连接

## 备注（核心文件索引）
- 入口：`src/main.cpp`
- 初始化：`src/core/AppInitializer.*`
- 统一模型/参数：`src/core/common.h`
- 处理服务：`src/services/DataProcessingService.*`
- 小热重差异工作台：`src/gui/views/workbenches/TgSmallDifferenceWorkbench.*`
- 算法实现：`src/services/algorithm/*`
- DAO/数据访问：`src/data_access/*`
