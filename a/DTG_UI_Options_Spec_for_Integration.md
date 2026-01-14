# DTG 界面选项与操作规格（用于跨项目复用）

## 概要
- 目标：在另一个项目中复用本项目的绘图选项与按钮行为，保持界面与算法一致。
- 核心文件：`DTG_Group_Viewer_GUI.m`（界面与绘图）、`generateDTGSteps.m`（处理链）、`sg_smooth_tga.m`（SG 平滑/导数）。
- 三轴布局：中间区域固定为 Q/Z/H 三个子图，见 `tiledlayout(centerPanel,3,1)` e:\gz_tobacco\收发\界面和算法\Copy_of_V2.2\Copy_of_V2.2\DTG_Group_Viewer_GUI.m:88–90。

## 选项规格
- `Show mean curve`
  - 行为：对当前 Display Step 的数据在每个阶段轴上叠加平均曲线。
  - 计算：收集多样本数据→齐长→`nanmean` 求均值 e:\...\DTG_Group_Viewer_GUI.m:701–735。
  - 绘制：在 `ax_current=targetAxes(stageIdx)` 上绘制 e:\...\DTG_Group_Viewer_GUI.m:807–819。
  - 图例：平均曲线图例仅在 `legendAxes` 添加一次（`stageIdx==1`） e:\...\DTG_Group_Viewer_GUI.m:820–836。

- `Show Raw (pre-repair) overlay`
  - 行为：在所选步骤曲线基础上叠加原始数据（`rawSeg` 或回退 `cleanedRaw`）。
  - 影响区域：绘图循环依据 `plotSettings.showRaw` 控制叠加 e:\...\DTG_Group_Viewer_GUI.m:737–806。
  - 数据来源：`steps.rawSeg/cleanedRaw` e:\...\DTG_Group_Viewer_GUI.m:582–590。

- `interpolate NaN for plotting`
  - 行为：绘制前对序列线性插值以消除 NaN 缺口。
  - 应用：样本曲线与平均曲线均执行 `fillmissing(...,'linear')` e:\...\DTG_Group_Viewer_GUI.m:772–774, 809–811。

- `Show Bad Points (removed)`
  - 行为：列表中显示坏点计数 `(bad:N)`，并在绘图时可叠加坏点标记。
  - 计数：对比 `rawSeg` 与 `cleanedRaw` 的 NaN 掩码 e:\...\DTG_Group_Viewer_GUI.m:620–626, 648–655。

- `Display Step`
  - 选项与映射：通过 `getStepData(stepData, selectedStep, smoothDisplay, app)` 提取对应字段。
    - Raw (pre-repair)：`rawSeg`，空则回退 `cleanedRaw` e:\...\DTG_Group_Viewer_GUI.m:1531–1533。
    - Cleaned (outliers removed)：若提供，取清洗后的原始数据（在本实现中主要作为 Raw 的回退来源）。
    - Truncated：`truncated` e:\...\DTG_Group_Viewer_GUI.m:1534。
    - Normalized：`normalized` e:\...\DTG_Group_Viewer_GUI.m:1535–1536。
    - Smoothed：受 `smoothDisplay` 影响，显示 `smoothed_sg/smoothed_loess/smoothed`，带标签后缀 e:\...\DTG_Group_Viewer_GUI.m:1538–1561。
    - DTG (derivative)：先确定基线平滑（`app.smoothMethod ∈ {SG, LOESS}`），再按 `app.diffMethod ∈ {'SG derivative','First difference'}` 求导或差分；失败回退差分 e:\...\DTG_Group_Viewer_GUI.m:1562–1593。
    - First Derivative (Diff)：等价于 `Display Step = DTG (derivative)` 与 `diffMethod='First difference'`。

- `Plot Type`
  - 选项：Line/Scatter。
  - 行为：在样本曲线与平均曲线的绘制函数中切换 `plot/scatter` e:\...\DTG_Group_Viewer_GUI.m:793–799, 815–818。

## 操作按钮规格
- `Plot Selected`
  - 入口：采集选项→`plotGroupDataOntoAxes(ax, legendAxes, ds, plotSettings)` e:\...\DTG_Group_Viewer_GUI.m:839–867。
  - 数据：`ds` 来自当前组 `detailStruct`，包含每样本 `steps` 字段 e:\...\DTG_Group_Viewer_GUI.m:575–606。

- `Clear Plots`
  - 行为：清空 Q/Z/H 三轴与右侧图例 e:\...\DTG_Group_Viewer_GUI.m:892–896。

- `Plot All Stages`
  - 行为：将当前组所有有效样本加入选择→调用 `onPlotSelected` e:\...\DTG_Group_Viewer_GUI.m:870–886。

- `Save Figure`
  - 行为：导出当前绘图区域与图例为图片（PNG/JPG/PDF/FIG），支持多步骤合并布局 e:\...\DTG_Group_Viewer_GUI.m:1000–1016, 1024–1031, 1064–1119。

- `Export Selected Samples Data`
  - 行为：统一长度打表并导出（CSV/MAT/Excel），字段含 `truncated/cleaned_truncated/normalized/smoothed_loess/dtg_loess_from_loess` e:\...\DTG_Group_Viewer_GUI.m:1275–1289, 1309–1332；批量导出见 e:\...\DTG_Group_Viewer_GUI.m:1180–1196, 1392–1436。

- `Open Parameter Settings`
  - 行为：打开参数对话框，采集/校验/应用后重载当前组 e:\...\DTG_Group_Viewer_GUI.m:264–271, 365–443, 450–478。

## 使用方式（跨项目实现建议）
- 轴与图例
  - 创建三轴布局：`tiledlayout(3,1)`，`ax = [nexttile; nexttile; nexttile]`。
  - 单独的图例轴：在右侧面板创建一个隐藏刻度的 `legendAxes`，仅用于承载图例。

- 数据结构
  - 组数据 `ds.detailStruct.(sampleName).steps`，包含字段：`rawSeg/cleanedRaw/truncated/normalized/smoothed_sg/smoothed_loess/smoothed/dtg_*/*diff_*`。
  - 统一长度：对跨样本曲线取最小长度并截断，以便平均与导出。

- 处理入口
  - 生成步骤：`steps = generateDTGSteps(filePath, params)`，`params` 包含前述参数（SG/LOESS 与导数方法等） e:\...\generateDTGSteps.m:101–165。
  - 绘图入口：收集 UI 选项到 `plotSettings`，包含 `selectedStep/smoothDisplay/showMean/showRaw/showBad/interpNaN/plotType/appParams`，传入核心绘图函数。

- 关键函数
  - `plotGroupDataOntoAxes(ax, legendAxes, ds, plotSettings)`：在三轴上绘制样本与平均曲线，并在 `legendAxes` 管理图例 e:\...\DTG_Group_Viewer_GUI.m:674–837。
  - `getStepData(stepData, selectedStep, smoothDisplay, app)`：根据 Display Step 与参数返回数据与标签 e:\...\DTG_Group_Viewer_GUI.m:1526–1597。

## 一致性与注意事项
- “Show mean curve” 不改变三轴布局，只在每个阶段轴上叠加平均曲线。
- “First Derivative (Diff)”并非独立步骤；通过 `DTG (derivative)` + `diffMethod='First difference'` 获得。
- LOESS 为内置平滑；如需“LOESS 导数”，实际采用“LOESS 基线 + SG 导数”组合。
- 插值仅用于绘制，不建议回写到原始数据结构，避免影响后续计算。

## 参考清单（文件:行）
- `DTG_Group_Viewer_GUI.m`：布局 88–90；绘图 674–837；入口 839–867；导出 1180–1196, 1275–1289, 1309–1332；参数对话框 264–271, 365–443
- `generateDTGSteps.m`：处理链平滑与导数 101–165
- `sg_smooth_tga.m`：SG 平滑/导数调用 47–55

## 交付建议
- 在新项目中先搭建三轴+独立图例的 UI 骨架，再移植 `plotSettings` 结构与 `plotGroupDataOntoAxes/getStepData` 的调用流程；参数模块参考 `DTG_Parameter_Spec_for_Integration.md`。
