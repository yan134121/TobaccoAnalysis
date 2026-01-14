# DTG 界面选项与状态行为报告

## 概要
- 界面文件：`DTG_Group_Viewer_GUI.m`，顶层窗口与三栏布局创建于 e:\gz_tobacco\收发\界面和算法\Copy_of_V2.2\Copy_of_V2.2\DTG_Group_Viewer_GUI.m:40–45。
- 选项控件位于右栏面板的网格布局中（29×2），创建与布局见 e:\gz_tobacco\收发\界面和算法\Copy_of_V2.2\Copy_of_V2.2\DTG_Group_Viewer_GUI.m:101–107。
- 绘图执行入口：`onPlotSelected` 将所有选项组装为 `plotSettings` 并调用核心绘图函数 `plotGroupDataOntoAxes`，参见 e:\gz_tobacco\收发\界面和算法\Copy_of_V2.2\Copy_of_V2.2\DTG_Group_Viewer_GUI.m:839–868、674–837。

## 选项与状态
- `Show mean curve`
  - 控件：`cbMean` 勾选与否 e:\gz_tobacco\收发\界面和算法\Copy_of_V2.2\Copy_of_V2.2\DTG_Group_Viewer_GUI.m:128–130。
  - 行为：若开启，跨 Q/Z/H 三阶段收集所选步骤数据，统一长度后求平均并在每个子图叠加平均曲线；图例仅添加一次。
    - 平均数据收集与计算：e:\gz_tobacco\收发\界面和算法\Copy_of_V2.2\Copy_of_V2.2\DTG_Group_Viewer_GUI.m:701–735。
    - 平均曲线绘制与图例：e:\gz_tobacco\收发\界面和算法\Copy_of_V2.2\Copy_of_V2.2\DTG_Group_Viewer_GUI.m:807–825。

- `Show Raw (pre-repair) overlay`
  - 控件：`cbShowRaw` e:\gz_tobacco\收发\界面和算法\Copy_of_V2.2\Copy_of_V2.2\DTG_Group_Viewer_GUI.m:136–138。
  - 行为：若开启，在当前所选步骤曲线的基础上叠加原始读取序列（`rawSeg` 或 `cleanedRaw`）用于参考比对；在核心绘图循环中依据 `plotSettings.showRaw` 控制。该叠加绘制位于绘图循环中（代码处有保留注释“showBad, showRaw 等代码保持不变”）。
    - 绘图循环入口：e:\gz_tobacco\收发\界面和算法\Copy_of_V2.2\Copy_of_V2.2\DTG_Group_Viewer_GUI.m:737–806。
    - 说明：原始段来源于 `steps.rawSeg`/`steps.cleanedRaw`，参见读取与封装 e:\gz_tobacco\收发\界面和算法\Copy_of_V2.2\Copy_of_V2.2\DTG_Group_Viewer_GUI.m:582–590。

- `interpolate NaN for plotting`
  - 控件：`cbInterp` e:\gz_tobacco\收发\界面和算法\Copy_of_V2.2\Copy_of_V2.2\DTG_Group_Viewer_GUI.m:133–135。
  - 行为：若开启，在绘图前对当前序列进行线性插值以填补 NaN；两处应用：
    - 对样本曲线：`if interpNaN && any(isnan(y)) ... y = fillmissing(y,'linear')` e:\gz_tobacco\收发\界面和算法\Copy_of_V2.2\Copy_of_V2.2\DTG_Group_Viewer_GUI.m:772–774。
    - 对平均曲线：同样线性插值 e:\gz_tobacco\收发\界面和算法\Copy_of_V2.2\Copy_of_V2.2\DTG_Group_Viewer_GUI.m:809–811。

- `Show Bad Points (removed)`
  - 控件：`cbShowBad` e:\gz_tobacco\收发\界面和算法\Copy_of_V2.2\Copy_of_V2.2\DTG_Group_Viewer_GUI.m:140–142。
  - 行为：
    - 选择列表中显示每个样本的“坏点数量”标签（`(bad:N)`），计算方法对比 `rawSeg` 与 `cleanedRaw` 的 NaN 掩码，参见 e:\gz_tobacco\收发\界面和算法\Copy_of_V2.2\Copy_of_V2.2\DTG_Group_Viewer_GUI.m:620–626, 648–655, 660–663。
    - 绘图时的坏点标记叠加在绘图循环中，与 `plotSettings.showBad` 联动（具体绘制段位于同一循环中，参考注释 e:\gz_tobacco\收发\界面和算法\Copy_of_V2.2\Copy_of_V2.2\DTG_Group_Viewer_GUI.m:804）。

- `Display Step`
  - 控件：`ddSteps`（选项：Raw (pre-repair)/Truncated/Normalized/Smoothed/DTG (derivative)） e:\gz_tobacco\收发\界面和算法\Copy_of_V2.2\Copy_of_V2.2\DTG_Group_Viewer_GUI.m:162–168。
  - 行为：根据所选步骤在 `getStepData` 中提取对应字段与标签：
    - Raw：优先 `rawSeg`，缺省回退 `cleanedRaw` e:\gz_tobacco\收发\界面和算法\Copy_of_V2.2\Copy_of_V2.2\DTG_Group_Viewer_GUI.m:1531–1533。
    - Truncated：`truncated` e:\gz_tobacco\收发\界面和算法\Copy_of_V2.2\Copy_of_V2.2\DTG_Group_Viewer_GUI.m:1534。
    - Normalized：`normalized` e:\gz_tobacco\收发\界面和算法\Copy_of_V2.2\Copy_of_V2.2\DTG_Group_Viewer_GUI.m:1535–1536。
    - Smoothed：受 `smoothDisplay`（下列选项）影响选择 `smoothed_sg/smoothed_loess/smoothed` 并设置标签后缀；若为空则回退到 LOESS 或 SG e:\gz_tobacco\收发\界面和算法\Copy_of_V2.2\Copy_of_V2.2\DTG_Group_Viewer_GUI.m:1538–1561。
    - DTG (derivative)：先确定基线平滑（`app.smoothMethod`），再依据 `app.diffMethod` 选择 SG 导数或一阶差分（失败回退差分），并设置标签后缀 e:\gz_tobacco\收发\界面和算法\Copy_of_V2.2\Copy_of_V2.2\DTG_Group_Viewer_GUI.m:1562–1593。

- `Smooth Display (only for Smoothed step)`
  - 控件：`ddSmoothDisplay`（SG (only)/LOESS (only)/Both/None） e:\gz_tobacco\收发\界面和算法\Copy_of_V2.2\Copy_of_V2.2\DTG_Group_Viewer_GUI.m:145–149。
  - 行为：仅当 `Display Step=Smoothed` 时生效，在 `getStepData` 内决定显示 `smoothed_sg` 或 `smoothed_loess`，`Both` 情况下默认以 LOESS 作为曲线基础；若 `None` 使用聚合字段 `smoothed`；缺省回退优先 LOESS e:\gz_tobacco\收发\界面和算法\Copy_of_V2.2\Copy_of_V2.2\DTG_Group_Viewer_GUI.m:1538–1561。

- `Plot Type`
  - 控件：`ddPlotType`（Line/Scatter） e:\gz_tobacco\收发\界面和算法\Copy_of_V2.2\Copy_of_V2.2\DTG_Group_Viewer_GUI.m:153–158。
  - 行为：在绘图循环中切换 `plot` 或 `scatter` 绘制样本曲线与平均曲线，参见 e:\gz_tobacco\收发\界面和算法\Copy_of_V2.2\Copy_of_V2.2\DTG_Group_Viewer_GUI.m:793–799, 815–818。

## 操作按钮
- `Plot Selected`
  - 入口与行为：采集选项 → 调用 `plotGroupDataOntoAxes(ax, legendAxes, ds, plotSettings)` → 状态更新 e:\gz_tobacco\收发\界面和算法\Copy_of_V2.2\Copy_of_V2.2\DTG_Group_Viewer_GUI.m:839–868。
  - 绘图包含三子图（Q/Z/H）与右侧图例，核心绘图函数位置：e:\gz_tobacco\收发\界面和算法\Copy_of_V2.2\Copy_of_V2.2\DTG_Group_Viewer_GUI.m:674–837。

- `Plot All Stages`
  - 行为：自动将当前 Group 的所有有效样本加入选择列表后复用 `onPlotSelected` 绘制 e:\gz_tobacco\收发\界面和算法\Copy_of_V2.2\Copy_of_V2.2\DTG_Group_Viewer_GUI.m:870–886。

- `Clear Plots`
  - 行为：清空三个子图与图例坐标轴，并重置标题/坐标轴标签、状态文本 e:\gz_tobacco\收发\界面和算法\Copy_of_V2.2\Copy_of_V2.2\DTG_Group_Viewer_GUI.m:888–896。

- `Save Figure`
  - 行为：将中心绘图区域与图例面板导出为位图或结合为图像，并支持多步合并图的保存（PNG/JPG/PDF/FIG）；导出逻辑见 e:\gz_tobacco\收发\界面和算法\Copy_of_V2.2\Copy_of_V2.2\DTG_Group_Viewer_GUI.m:1064–1119, 1024–1031。

- `Export Selected Samples Data`
  - 行为：对当前选择样本提取标准字段（`truncated/cleaned_truncated/normalized/smoothed_loess/dtg_loess_from_loess`），统一长度补 NaN 后构造表并保存（CSV/MAT/Excel）；实现参见 e:\gz_tobacco\收发\界面和算法\Copy_of_V2.2\Copy_of_V2.2\DTG_Group_Viewer_GUI.m:1275–1289, 1309–1332。

- `Open Parameter Settings`
  - 行为：打开参数设置对话框（已汉化），通过 `uifigure + uigridlayout` 生成参数表单，含 `smoothMethod/diffMethod/loessSpan/sgWindow/sgPoly` 等；创建与布局位置 e:\gz_tobacco\收发\界面和算法\Copy_of_V2.2\Copy_of_V2.2\DTG_Group_Viewer_GUI.m:264–271, 322–335, 338–351, 354–364。

## 最终结果
- 绘图：三阶段子图与图例按当前选项生成；若启用平均曲线则叠加统一长度的平均序列；若启用插值则消除 NaN 缺口以便连续呈现。
- 数据：导出所选样本的各步骤关键数据列，用于后续分析与交付；字段与列名在报告中固定并与 GUI 中一致。
- 参数：通过参数对话框修改处理参数后，后续 `generateDTGSteps` 与绘图将按新参数重算（如 LOESS 窗口、SG 参数、导数方法），处理链条位置参见 e:\gz_tobacco\收发\界面和算法\Copy_of_V2.2\Copy_of_V2.2\generateDTGSteps.m:101–117, 121–165。

## 程序实现映射
- 选项采集与传递：`onPlotSelected` 组装 `plotSettings`（含 `showMean/interpNaN/showRaw/showBad/smoothDisplay/selectedStep/plotType`）并传入核心绘图函数 e:\gz_tobacco\收发\界面和算法\Copy_of_V2.2\Copy_of_V2.2\DTG_Group_Viewer_GUI.m:852–866。
- 核心绘图逻辑：`plotGroupDataOntoAxes` 中按 `Display Step`→`getStepData` 提取数据；按 `Plot Type` 绘制；按 `interpolate NaN` 插值；按 `Show mean curve` 叠加平均；并维护右侧图例 e:\gz_tobacco\收发\界面和算法\Copy_of_V2.2\Copy_of_V2.2\DTG_Group_Viewer_GUI.m:674–837。
- 步骤数据提取：`getStepData` 中依据 `selectedStep` 与 `smoothDisplay/app.smoothMethod/app.diffMethod` 返回序列与标签 e:\gz_tobacco\收发\界面和算法\Copy_of_V2.2\Copy_of_V2.2\DTG_Group_Viewer_GUI.m:1526–1597。
- 组数据载入：`loadGroup/readDTGsForStage` 封装 `steps`（含各步骤结果），并统计坏点数量，用于选择列表标注 e:\gz_tobacco\收发\界面和算法\Copy_of_V2.2\Copy_of_V2.2\DTG_Group_Viewer_GUI.m:500–563, 565–606, 608–669。
- 保存与导出：`onSave`/批量保存、`onExport`/批量导出所有组，详见前述代码引用。

## 备注
- `Show Raw overlay` 与 `Show Bad Points` 的具体叠加绘制位于核心绘图循环内，受 `plotSettings.showRaw/showBad` 控制；该区域在现有文件中以注释标记为“保持不变”（参见 e:\gz_tobacco\收发\界面和算法\Copy_of_V2.2\Copy_of_V2.2\DTG_Group_Viewer_GUI.m:804），其数据来源为 `steps.rawSeg/cleanedRaw` 与坏点掩码的对比。
- 所有绘图均基于 `generateDTGSteps` 的标准处理流程产出；LOESS 为 MATLAB 内置 `smoothdata(...,'loess')`，SG 为 `sg_smooth_tga`（基于 `sgolayfilt/sgolay`）。
