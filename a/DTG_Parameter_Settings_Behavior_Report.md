# DTG 参数设置与状态行为报告

## 概要
- 参数对话框创建与布局：`openParameterDialog()` 在右栏触发，构建表单与按钮 e:\gz_tobacco\收发\界面和算法\Copy_of_V2.2\Copy_of_V2.2\DTG_Group_Viewer_GUI.m:264–271, 322–335, 338–351, 354–364。
- 参数读取/校验/应用：`applyParams()` 完成采集、校验、赋值、状态更新与重载组 e:\gz_tobacco\收发\界面和算法\Copy_of_V2.2\Copy_of_V2.2\DTG_Group_Viewer_GUI.m:365–443。
- 重置默认：`resetParams()` 将所有控件恢复为默认值 e:\gz_tobacco\收发\界面和算法\Copy_of_V2.2\Copy_of_V2.2\DTG_Group_Viewer_GUI.m:450–478。
- 默认参数初始化：`app` 初始值定义 e:\gz_tobacco\收发\界面和算法\Copy_of_V2.2\Copy_of_V2.2\DTG_Group_Viewer_GUI.m:219–243。

## 参数明细与作用
- `INVALID TOKEN FRACTION (max fraction)` → `maxInvalidFraction`
  - 作用：读取时非数值项占比上限，超限直接错误退出 e:\DTG_Group_Viewer_GUI.m:69–71（在 `getColumnC` 内部执行）；`app` 默认 e:\DTG_Group_Viewer_GUI.m:219。
  - 流程影响：控制输入质量门槛，过多非数值则改用插值或拒绝。
- `OUTLIER_WINDOW (odd integer)` → `outlierWindow`
  - 作用：局部异常检测滑窗长度（奇数强制）；应用前奇数化 e:\DTG_Group_Viewer_GUI.m:429–431。
  - 使用：`getColumnC` Hampel/窗口检测 e:\getColumnC.m:30–33, 91–101。
- `OUTLIER_NSIGMA (Hampel threshold)` → `outlierNSigma`
  - 作用：窗口内阈值倍数，超过视为异常 e:\getColumnC.m:97–101；默认 e:\DTG_Group_Viewer_GUI.m:222。
- `JUMP DIFF THRESHOLD (diff threshold)` → `jumpDiffThreshold`
  - 作用：一阶差分跳变阈值 e:\getColumnC.m:104；默认 e:\DTG_Group_Viewer_GUI.m:223。
- `Mark diff threshold (%)` → `diffThresholdPerc`
  - 作用：绘图或统计时用于阈值百分比标注（界面数据列表中坏点统计辅助）；默认 e:\DTG_Group_Viewer_GUI.m:224。
- `Bad points to show (0 = All, >0 = top N)` → `badPointTopN`
  - 作用：选择列表中坏点提示的显示数量（0 显示全部）；默认 e:\DTG_Group_Viewer_GUI.m:226；统计逻辑 e:\DTG_Group_Viewer_GUI.m:620–626, 648–655。
- `SG Filter Window (odd integer)` → `sgWindow`
  - 作用：SG 平滑窗口长度，必须奇数且 ≥3；校验 e:\DTG_Group_Viewer_GUI.m:395–396。
  - 使用：`sg_smooth_tga(D_norm, sgWindow, sgPoly)` 生成平滑与导数 e:\sg_smooth_tga.m:47–55。
  - 在流程中：生成 DTG 或在 LOESS 之上求 SG 导数 e:\generateDTGSteps.m:104–108, 125–127, 136–139。
- `SG Filter Polynomial Order` → `sgPoly`
  - 作用：SG 多项式阶数，必须 `< sgWindow`；校验 e:\DTG_Group_Viewer_GUI.m:396–397；使用同上。
- `Fit Type (linear/quad)` → `fitType`
  - 作用：异常块局部拟合的阶次（线性或二次）；用于修复异常段 e:\getColumnC.m:151–171。
- `Gamma (mix weight 0..1)` → `gamma`
  - 作用：局部拟合混合权或调和比例（用于修复策略）；默认 e:\DTG_Group_Viewer_GUI.m:232；在 `getColumnC` 中参与修复策略权衡。
- `Anchor window (samples)` → `anchorWin`
  - 作用：异常块拟合时前后锚点扩展长度，决定拟合区间 e:\getColumnC.m:144–147。
- `Mono Start (truncated idx)` → `monoStart`
  - 作用：强制单调区间起点（在截取后的索引）；单调性检测与 PAVA 区间 e:\getColumnC.m:114–118, 189–239。
- `Mono End (truncated idx)` → `monoEnd`
  - 作用：强制单调区间终点；边界与一致性检查 e:\getColumnC.m:109–118。
- `Edge Blend (0..1)` → `edgeBlend`
  - 作用：强制单调修复与边缘过渡的混合比例；默认 e:\DTG_Group_Viewer_GUI.m:236；用于修复后平滑过渡。
- `EPS Scale (e.g. 1e-9)` → `epsScale`
  - 作用：严格递减斜坡的最小步长下限与容差尺度 e:\getColumnC.m:203–206。
- `Slope Threshold (e.g. 1e-9)` → `slopeThreshold`
  - 作用：单调性判定与斜率阈值的最小容差，用于消除平台期与微小上凸；默认 e:\DTG_Group_Viewer_GUI.m:238。
- `LOESS Span (fraction 0..1 or integer window)` → `loessSpan`
  - 作用：LOESS 平滑窗口，比例或绝对点数；计算并调用平滑 e:\generateDTGSteps.m:114–117；GUI 绘制 e:\DTG_Group_Viewer_GUI.m:1487–1489, 1506–1507。
- `Derivative Base (SG derivative / LOESS derivative / First difference)`
  - 参数映射：
    - 平滑基线选择：`smoothMethod ∈ {SG, LOESS}` e:\DTG_Group_Viewer_GUI.m:240。
    - 微分方法选择：`diffMethod ∈ {'SG derivative','First difference'}` e:\DTG_Group_Viewer_GUI.m:241。
  - 行为：
    - 若 `smoothMethod='LOESS'`，基线为 LOESS；`diffMethod='SG derivative'` 则基于 SG 系数在 LOESS 基线求导；`First difference` 则取一阶差分 e:\generateDTGSteps.m:146–151。
    - 若 `smoothMethod='SG'`，基线为 SG；同理选择 `dtg_sg_from_sg` 或 `diff_from_sg` e:\generateDTGSteps.m:152–158。
- `Default Smooth Display` → `smoothDisplay`
  - 作用：当 `Display Step=Smoothed` 时选择显示 SG/LOESS/Both/None；实现与回退 e:\DTG_Group_Viewer_GUI.m:1538–1561。

## 对话框按钮与状态
- `Apply`
  - 处理：采集控件值→构造 `newParams`→数值/范围/枚举校验→写回 `app`→奇数化 `outlierWindow`→更新 UI 值与状态→重载当前组以应用新参数 e:\DTG_Group_Viewer_GUI.m:365–443。
  - 生效范围：影响后续 `generateDTGSteps` 计算及 `getStepData` 的数据选择与导数策略。
- `Reset to Default`
  - 处理：将所有控件写回默认值（与 `app` 初始值一致） e:\DTG_Group_Viewer_GUI.m:450–478。
- `Close`
  - 处理：关闭参数对话框窗口 e:\DTG_Group_Viewer_GUI.m:361。
- `Status: Ready`
  - 状态标签：`statusParamLabel` 初始为“就绪”，应用后更新为“参数应用成功”；右栏 `statusLabel` 同步提示并在重载组后更新 e:\DTG_Group_Viewer_GUI.m:362–365, 434–442。

## 与核心处理的关联
- 单文件处理：`generateDTGSteps(filePath, params)` 直接使用 `app` 参数进行截取、归一化、平滑与导数选择 e:\generateDTGSteps.m:65–100, 101–117, 121–165。
- 批量绘制：`onPlotSelected` 将当前 `app` 参数打包到 `plotSettings`，影响 `getStepData` 的平滑/导数路径与标签 e:\DTG_Group_Viewer_GUI.m:852–866, 1526–1597。
- 组数据加载：`loadGroup/readDTGsForStage` 每样本调用 `generateDTGSteps(fp, app)`，确保 GUI 参数贯穿数据链 e:\DTG_Group_Viewer_GUI.m:575–581, 592–606。

## 备注
- 所有参数校验失败将抛出错误并阻止应用，保证后续计算稳定性（奇数窗口、阶数关系、取值范围与枚举合法性）。
- LOESS 使用 MATLAB 内置 `smoothdata(...,'loess')`；SG 导数通过 `sg_smooth_tga`（`sgolayfilt/sgolay`）。
