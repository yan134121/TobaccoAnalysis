# PeakSeg-COW（MATLAB移植）验证说明

本文用于验证新集成的 `PeakSeg-COW` 对齐算法（C++/Qt）与 MATLAB 原型（`my_cow_align_peakseg_V5_auto.m`）一致性，并记录参数映射与性能边界。

## 1. 代码位置

- MATLAB 原型：
  - `15_2_PeakAlignment_optimize_changePeakIdentify_send/15_2_PeakAlignment_optimize_changePeakIdentify_send/my_cow_align_peakseg_V5_auto.m`
  - `.../my_findpeaks.m`
  - `.../my_corr.m`
  - `.../my_linear_resample.m`
- C++ 集成：
  - `src/services/algorithm/PeakSegCOWAlignment.h`
  - `src/services/algorithm/PeakSegCOWAlignment.cpp`
  - 注册与调用分流：
    - `src/services/DataProcessingService.cpp`
  - UI 开关：
    - `src/core/common.h`（`ProcessingParameters::peakSegCowEnabled`）
    - `src/gui/dialogs/ChromatographParameterSettingsDialog.*`

## 2. 软件侧如何启用

在“色谱参数设置”里：

- 勾选：**启用峰对齐**
- 再勾选：**使用 PeakSeg-COW（MATLAB 版）**
- 设置：**参考样本ID**（必须是当前处理组内存在的 sampleId）

## 3. 参数映射（GUI -> MATLAB opts）

当前版本按“复用现有 GUI 参数 + MATLAB 默认值”的原则映射：

- `opts.minProminence` ← `ProcessingParameters::peakMinProminence`
- `opts.t` ← `ProcessingParameters::cowMaxWarp`
- `opts.ranges` ← 由 `ProcessingParameters::cowSegmentCount` 作为“均分区间数”生成（内部 cap 到 10）
- `opts.smoothSpan` ← 默认 5（MATLAB 默认）
- `opts.maxClusterGap` ← 默认 5（MATLAB 默认）
- `opts.plotDebug` ← false

说明：
- MATLAB 原型支持 `opts.ranges` 与 `opts.rangeProm` 更细粒度控制；当前 C++ 未暴露这些 UI 参数。

## 4. 与 MATLAB 对照的推荐验证流程

### 4.1 选择同一对参考/目标样本

从数据库中选两个样本（同一批/同组更好）：

- 参考样本：ref
- 目标样本：tgt

确保两边使用同样的输入曲线阶段：

- 软件侧对齐输入优先取 `BaselineCorrection` 阶段，否则取 `RawData`
- MATLAB 原型通常用基线校正后的 `segT.ybc`

### 4.2 MATLAB 离线跑出对齐结果

用目录内脚本（或按其逻辑）得到：

- `aligned_chrom2`（长度 = `chrom1`）
- 以及可选的 `alignment_info.mat`（bounds/seg_info/opts 等）

对齐核心函数输出：

- `aligned_chrom2`
- `bounds`

### 4.3 软件侧导出/抓取对齐后的曲线

软件侧对齐结果保存为 `StageName::PeakAlignment`：

- `StageData.stageName == PeakAlignment`
- `StageData.curve->data()` 为 `(refX, alignedY)` 点集

推荐临时对照方式：

- 在 UI 上对齐后，导出对齐曲线为 CSV（如当前软件已有导出功能）
- 或在调试时断点查看 `alignedPoints`

### 4.4 对比指标（建议）

因为浮点与边界行为可能导致轻微差异，建议使用数值指标而不是逐点全等：

- **NRMSE**：\(\sqrt{\frac{\sum (y_{cpp}-y_{mat})^2}{\sum (y_{mat}-\bar y_{mat})^2}}\)
- **最大绝对误差**：\(\max |y_{cpp}-y_{mat}|\)
- **皮尔逊相关**：corr(\(y_{cpp}\), \(y_{mat}\))

并检查宏观行为：

- 峰位置是否被“拉齐”（主要峰的 RT/索引偏移明显减小）
- 是否出现明显分段断裂（aligned 里出现大块 0 或突变）

## 5. 性能与边界说明

`PeakSeg-COW` 的 DP 复杂度与 `m`（分段数）、`n2`（目标长度）、以及 `t`（最大偏移）相关。

当前实现已做的护栏：

- `range_count = min(cowSegmentCount, 10)`（避免区间过多导致峰检测/分段数量过大）

建议使用时的经验值：

- `cowMaxWarp(t)`：从 20~80 试起，过大会显著增加 DP 搜索空间
- 若样本点数很大（上万点），建议先裁剪或下采样再对齐

## 6. 已知差异点（后续可进一步贴近 MATLAB）

- `movmean`：当前 C++ 用“缩窗均值”近似 MATLAB 边界行为；通常差异很小。
- MATLAB 原型支持 `opts.rangeProm` 分区间阈值；C++ 暂未实现分区间 prominence。
- C++ 当前主要用于“对齐输出 aligned 曲线”；若后续需要 `bounds/seg_info` 用于可视化复现，可把这些信息加入 `ProcessingResult` 或另建数据通道保存。

