# TobaccoAnalysis

## 项目简介
- TobaccoAnalysis 是一个基于 Qt 的桌面数据分析工具，用于烟草相关数据的导入、处理、分析与可视化。
- 支持四类核心数据处理：
  - 大热重数据处理
  - 小热重数据处理
  - 色谱数据分析（峰检测、COW 对齐等）
  - 工序大热重数据分析
- 主要功能包括：Excel 批量导入、数据裁剪/归一化/平滑/求导、差异度与代表样评估、色谱峰检测与对齐、结果表格与图谱导出。

## 架构概览
- 分层架构（桌面单体）：`GUI → Services → Algorithms → DAO → Core Models`
  - 服务层集中注册与调度算法：`src/services/DataProcessingService.cpp`
  - 统一参数与数据模型：`src/core/common.h`（`ProcessingParameters`、`StageData` 等）
  - Excel 读写：`src/utils/file_handler/XlsxParser.*`、`XlsxWriter.*`
  - 关键第三方：`Qt5`、`QCustomPlot`、`QXlsx`

## 一分钟跑起来（Windows）
1. 安装依赖（已安装可跳过）：
   - 安装 `Visual Studio 2022`（含 `MSVC` 与 `C++ 桌面开发`组件）
   - 安装 `Qt5`（建议 `5.15.x`，MSVC 64 位），记下 `Qt5_DIR` 路径，例如：`C:\Qt\5.15.2\msvc2019_64\lib\cmake\Qt5`
   - 安装 `CMake ≥ 3.16`
   - 安装 MySQL 客户端库，确保存在 `libmysql.dll`
2. 在 PowerShell 执行（路径在项目根目录）：
   - 配置（生成 VS 解决方案）：
     ```powershell
     cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DQt5_DIR="C:\Qt\5.15.2\msvc2019_64\lib\cmake\Qt5"
     ```
   - 构建（Release）：
     ```powershell
     cmake --build build --config Release --target TobaccoAnalysis
     ```
   - 运行：
     ```powershell
     & "build\release\TobaccoAnalysis.exe"
     ```
3. 注意事项：
   - 如未安装在 `D:\Program Files\MySQL\MySQL Server 8.0\lib\libmysql.dll`，请手动将你的 `libmysql.dll` 拷贝到 `build\release\` 可执行文件所在目录。
   - CMake 已自动复制 `config\` 与 `sql\` 到输出目录，首次运行请在 `config\sql_config.json` 中配置数据库连接（如使用导入/查询功能）。

## 环境要求
- 操作系统：Windows 10/11（64 位）
- 编译工具链：Visual Studio 2022（MSVC），CMake ≥ 3.16
- 依赖库：Qt5（Core/Gui/Widgets/Sql/Charts/Xml/Concurrent/PrintSupport），QCustomPlot，QXlsx
- 数据库：MySQL 客户端库（用于数据库访问功能）

## 构建与运行（详细）
- 生成与构建：
  - `cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DQt5_DIR="<你的Qt5_DIR>"`
  - `cmake --build build --config Debug` 或 `Release`
- 运行：
  - `build\release\TobaccoAnalysis.exe` 或 `build\debug\TobaccoAnalysis.exe`
- 资源复制：
  - 构建后会自动复制 `config\` 与 `sql\` 到输出目录；若路径不同的 `libmysql.dll` 未自动复制，请手动拷贝。
- mysql连接问题：
  - 根据自己的数据库修改release中config的mysql密码 

## 常见问题
- 找不到 Qt：检查 `Qt5_DIR` 是否指向 `...\lib\cmake\Qt5`
- 找不到 `libmysql.dll`：将你的 MySQL 客户端 `libmysql.dll` 拷贝到可执行文件目录（`build\release\`）
- 启动后界面空白或数据为空：确认数据库连接配置与导入的 Excel 模板是否符合要求

## 目录结构（简要）
- `src/`：源码（`core/`、`services/`、`data_access/`、`gui/`、`utils/` 等）
- `third_party/`：第三方库（`qcustomplot/`、`QXlsx/`）
- `config/`：数据库 SQL 配置与应用配置（构建后复制到输出目录）
- `sql/`：SQL 脚本与相关文件（构建后复制到输出目录）
- `reports/`：项目报告与分析文档
- `CMakeLists.txt`：CMake 构建配置

## 功能预览（简述）
- 数据导入：结构化 Excel 批量导入与单条新增
- 数据处理：裁剪、归一化、平滑、求导、色谱峰检测与对齐、工序坏点修复
- 差异度分析：NRMSE、RMSE、皮尔逊、欧氏距离，支持权重综合评分
- 可视化与导出：图谱绘制与表格导出（Excel）

