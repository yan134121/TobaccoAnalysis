# DTG 参数规格与实现映射（用于跨项目复用）

## 概要
- 目标：为另一个项目实现相同的 DTG 处理参数与行为，提供参数清单、取值范围、校验规则、算法影响与代码映射。
- 主要来源：`DTG_Group_Viewer_GUI.m` 的参数对话框与应用逻辑，处理核心在 `generateDTGSteps.m`、`getColumnC.m`、`sg_smooth_tga.m`。

## 参数总览
- 数值型参数默认值定义位置：e:\gz_tobacco\收发\界面和算法\Copy_of_V2.2\Copy_of_V2.2\DTG_Group_Viewer_GUI.m:219–242
- 采集与校验逻辑：e:\gz_tobacco\收发\界面和算法\Copy_of_V2.2\Copy_of_V2.2\DTG_Group_Viewer_GUI.m:365–405
- 应用与强制奇数化：e:\gz_tobacco\收发\界面和算法\Copy_of_V2.2\Copy_of_V2.2\DTG_Group_Viewer_GUI.m:429–433

## 参数规格（逐项）
- `maxInvalidFraction`（INVALID TOKEN FRACTION）
  - 默认：`0.2` e:\...\DTG_Group_Viewer_GUI.m:219
  - 范围：`[0,1]`，越大越宽松 e:\...\DTG_Group_Viewer_GUI.m:393
  - 影响：读取/清洗阶段，超过比例报错或回退 e:\...\getColumnC.m:69–75

- `outlierWindow`（OUTLIER_WINDOW，奇数）
  - 默认：`21` e:\...\DTG_Group_Viewer_GUI.m:221
  - 范围：`>=3` 且奇数；应用前强制奇数化 e:\...\DTG_Group_Viewer_GUI.m:429–431
  - 影响：窗口 Hampel 与中值滤波等鲁棒检测 e:\...\getColumnC.m:30–33, 91–101

- `outlierNSigma`（OUTLIER_NSIGMA）
  - 默认：`4` e:\...\DTG_Group_Viewer_GUI.m:222
  - 范围：正数（建议 2–6，根据噪声）
  - 影响：窗口内离群阈值倍数 e:\...\getColumnC.m:97–101

- `jumpDiffThreshold`（JUMP DIFF THRESHOLD）
  - 默认：`0.1` e:\...\DTG_Group_Viewer_GUI.m:223
  - 范围：正数（按归一化尺度）
  - 影响：一阶差分跳变检测 e:\...\getColumnC.m:104

- `diffThresholdPerc`（Mark diff threshold (%)）
  - 默认：`2` e:\...\DTG_Group_Viewer_GUI.m:224
  - 范围：`>=0` 百分比（用于标注/统计）
  - 影响：坏点提示与阈值标注（界面逻辑）

- `badPointTopN`（Bad points to show）
  - 默认：`0`（全部） e:\...\DTG_Group_Viewer_GUI.m:226
  - 范围：整数 `>=0`
  - 影响：列表中坏点标签 `(bad:N)` 显示 e:\...\DTG_Group_Viewer_GUI.m:620–626, 648–655

- `sgWindow`（SG Filter Window，奇数）
  - 默认：`11` e:\...\DTG_Group_Viewer_GUI.m:227
  - 范围：`>=3` 且奇数 e:\...\DTG_Group_Viewer_GUI.m:395–396
  - 影响：SG 平滑窗口长度 e:\...\sg_smooth_tga.m:47

- `sgPoly`（SG Filter Polynomial Order）
  - 默认：`2` e:\...\DTG_Group_Viewer_GUI.m:228
  - 范围：`>=1` 且 `< sgWindow` e:\...\DTG_Group_Viewer_GUI.m:396–397
  - 影响：SG 平滑阶次与导数系数 e:\...\sg_smooth_tga.m:47, 51–55

- `fitType`（Fit Type）
  - 默认：`'linear'` e:\...\DTG_Group_Viewer_GUI.m:231
  - 选项：`{'linear','quad'}` e:\...\DTG_Group_Viewer_GUI.m:397
  - 影响：异常块拟合修复的阶次 e:\...\getColumnC.m:165–171

- `gamma`（Gamma 0..1）
  - 默认：`0.8` e:\...\DTG_Group_Viewer_GUI.m:232
  - 范围：`[0,1]` e:\...\DTG_Group_Viewer_GUI.m:398
  - 影响：修复混合权重/平滑过渡强度（局部策略）

- `anchorWin`（Anchor window）
  - 默认：`30` e:\...\DTG_Group_Viewer_GUI.m:233
  - 范围：`>=1` 整数（建议 8–40）
  - 影响：异常块拟合的锚点扩展区间 e:\...\getColumnC.m:144–147

- `monoStart` / `monoEnd`（Monotone range）
  - 默认：`20` / `120` e:\...\DTG_Group_Viewer_GUI.m:234–235
  - 范围：`monoStart >= 1` 且 `monoEnd >= monoStart` e:\...\DTG_Group_Viewer_GUI.m:401
  - 影响：单调性检测/修复区间、PAVA 与平台消除 e:\...\getColumnC.m:114–118, 189–239

- `edgeBlend`（Edge Blend 0..1）
  - 默认：`0.6` e:\...\DTG_Group_Viewer_GUI.m:236
  - 范围：`[0,1]` e:\...\DTG_Group_Viewer_GUI.m:399
  - 影响：修复段与原始段接缝的混合强度

- `epsScale`（EPS Scale）
  - 默认：`1e-9` e:\...\DTG_Group_Viewer_GUI.m:237
  - 范围：`> 0` e:\...\DTG_Group_Viewer_GUI.m:400
  - 影响：严格递减斜坡的最小步长与容差尺度 e:\...\getColumnC.m:203–206

- `slopeThreshold`（Slope Threshold）
  - 默认：`1e-9` e:\...\DTG_Group_Viewer_GUI.m:238
  - 范围：`> 0`
  - 影响：微小斜率与平台期判定阈值（修复稳健性）

- `loessSpan`（LOESS Span）
  - 默认：`0.2` e:\...\DTG_Group_Viewer_GUI.m:239
  - 范围：`> 0`；`(0,1)` 视为比例窗口，`>=1` 视为绝对点数 e:\...\DTG_Group_Viewer_GUI.m:1487–1489, 1506–1507；e:\...\generateDTGSteps.m:114–117
  - 影响：LOESS 平滑窗口长度（内置 `smoothdata(...,'loess')`）

- `smoothMethod`（Derivative Base 的平滑基线）
  - 默认：`'LOESS'` e:\...\DTG_Group_Viewer_GUI.m:240
  - 选项：`{'SG','LOESS'}` e:\...\DTG_Group_Viewer_GUI.m:403
  - 影响：DTG 基线选择（SG 或 LOESS） e:\...\generateDTGSteps.m:146–158

- `diffMethod`（Derivative Base 的微分方法）
  - 默认：`'SG derivative'` e:\...\DTG_Group_Viewer_GUI.m:241
  - 选项：`{'SG derivative','First difference'}` e:\...\DTG_Group_Viewer_GUI.m:404
  - 影响：在选定基线上，SG 导数或一阶差分 e:\...\generateDTGSteps.m:146–158；e:\...\DTG_Group_Viewer_GUI.m:1512–1518, 1583–1593
  - 注：不存在独立的“LOESS derivative”算法；“LOESS 基线 + SG 导数”为组合策略。

- `smoothDisplay`（Default Smooth Display）
  - 默认：`'LOESS (only)'` e:\...\DTG_Group_Viewer_GUI.m:242
  - 选项：`{'SG (only)','LOESS (only)','Both','None'}` e:\...\DTG_Group_Viewer_GUI.m:405
  - 影响：当 Display Step=Smoothed，决定显示哪种平滑曲线 e:\...\DTG_Group_Viewer_GUI.m:1538–1561

## 按钮行为
- `Apply`：采集→校验→写回 `app`→规范化参数（如奇数窗）→必要时重载当前组 e:\...\DTG_Group_Viewer_GUI.m:365–443
- `Reset to Default`：控件恢复默认值（与初始 `app` 一致） e:\...\DTG_Group_Viewer_GUI.m:450–478
- `Close`：关闭参数对话框 e:\...\DTG_Group_Viewer_GUI.m:361
- `Status: Ready/Applied`：状态标签更新 e:\...\DTG_Group_Viewer_GUI.m:362–365, 434–442

## 算法影响映射
- 清洗修复：`getColumnC` 使用 `outlierWindow/outlierNSigma/jumpDiffThreshold/globalDeviationNSigma/fitType/gamma/anchorWin/monoStart/monoEnd/edgeBlend/epsScale/slopeThreshold` e:\...\getColumnC.m:14–29, 80–118, 122–176, 189–239
- 单文件处理：`generateDTGSteps` 使用 `loessSpan/sgWindow/sgPoly/smoothMethod/diffMethod` 决定平滑与导数 e:\...\generateDTGSteps.m:101–117, 121–165
- SG 平滑与导数：`sg_smooth_tga` 使用 `sgWindow/sgPoly` e:\...\sg_smooth_tga.m:47–55

## 跨项目实现建议
- 校验模板（伪代码）
  - `0 <= maxInvalidFraction <= 1`
  - `outlierWindow >= 3 && outlierWindow % 2 == 1`
  - `sgWindow >= 3 && sgWindow % 2 == 1`
  - `1 <= sgPoly < sgWindow`
  - `fitType in {'linear','quad'}`；`gamma in [0,1]`
  - `monoStart >= 1 && monoEnd >= monoStart`
  - `edgeBlend in [0,1]`; `epsScale > 0`; `slopeThreshold > 0`
  - `loessSpan > 0`; `smoothMethod in {'SG','LOESS'}`
  - `diffMethod in {'SG derivative','First difference'}`
  - `smoothDisplay in {'SG (only)','LOESS (only)','Both','None'}`

- 接口与参数结构
  - 统一参数结构 `params`（或 `app`）在读取、处理、绘图、导出各环节传递；参考调用：
    - 读取/载入：e:\...\DTG_Group_Viewer_GUI.m:565–606
    - 单文件处理：e:\...\generateDTGSteps.m:1–13, 101–165
    - 绘图数据选择：e:\...\DTG_Group_Viewer_GUI.m:1526–1597

- 行为一致性
  - LOESS 为内置平滑；“LOESS + SG 导数”是组合方案而非独立“LOESS 导数”。
  - outlierWindow 等对清洗影响显著；建议与数据噪声水平联动设置。

## 参考清单（文件:行）
- `DTG_Group_Viewer_GUI.m`：默认参数 219–242；校验与应用 365–405, 429–433；绘图/数据选择 839–868, 1526–1597
- `generateDTGSteps.m`：平滑与导数 101–117, 121–165
- `getColumnC.m`：清洗与单调修复 14–29, 80–118, 122–176, 189–239
- `sg_smooth_tga.m`：SG 平滑与导数 47–55

## 备注
- 将上述规则与接口在新项目中保持一致，可获得同等的处理与展示效果；必要时根据新数据特性调整默认值与阈值范围。
