# 色谱处理算法项目框架文档

## 1. 项目目标
对烟草色谱原始数据执行“读取 → 基线校正 → 裁剪 → 峰检测/对齐 → 边界微调 → 分段面积差异度计算 → 结果导出”的批处理流水线。

## 2. 技术栈
- **语言**：MATLAB
- **数据输入**：CSV、MAT、XLSX
- **表格/文件处理**：`readmatrix`、`readtable`、`writetable`、`save`
- **数值计算**：矩阵运算、插值、平滑、相关系数、动态规划
- **算法依赖**：
  - `airPLS`：自适应迭代加权惩罚最小二乘基线校正
  - `my_findpeaks`：峰检测
  - `my_linear_resample`：线性重采样
  - `my_corr`：相关性度量
  - `my_cow_align_peakseg_V5_auto`：峰簇分段 COW 对齐
- **外部数据**：样品目录、参考边界 Excel、标定样映射 Excel、差异度 Excel

## 3. 目录树（核心）
```text
色谱处理算法/
├─ 00_原始数据/
│  └─ ReadData_folder.m
├─ 01_基线校正/
│  ├─ airPLS.m
│  ├─ BaselineCorrection_batch.m
│  └─ ChromatogramGUI_baseline_V3.m
├─ 02_裁剪数据/
│  └─ BaseCorrect_data_cut.m
├─ 03_色谱对齐/
│  ├─ Sepu_align_single.m
│  ├─ Sepu_align_batch.m
│  ├─ my_cow_align_peakseg_V5_auto.m
│  ├─ my_findpeaks.m
│  ├─ my_linear_resample.m
│  ├─ my_corr.m
│  ├─ All_aligned_by_ref_code.m
│  ├─ Extract_align_MAT_multi.m
│  ├─ summarized_data_flat.m
│  └─ 说明.txt
├─ 04_对齐后微调边界/
│  ├─ change_time_to_idx_V3_multi.m
│  ├─ Extract_calib_finetuned_results.m
│  ├─ finetuned_bounds_results.m
│  └─ summarized_data_flat.m
├─ 05_差异度计算/
│  ├─ calculate_all_difference.m
│  ├─ cosineSimilarity.m
│  ├─ meanSPC.m
│  ├─ all_norm.m
│  ├─ new.m
│  └─ select_representative_samples.m
├─ 所有基准样品分段边界线-599个样品.xlsx
└─ 标定样品编号列表-全599个样品.xlsx
```

## 4. 结构解析（感知 / 决策 / 执行）

### 4.1 感知层：数据获取与标准化
负责把“外部文件/文件夹”转换成统一 MATLAB 数据结构。
- `ReadData_folder.m`
  - 扫描样品子文件夹
  - 读取 CSV
  - 自动识别样品命名
  - 处理 NaN 并统一长度
  - 输出标准 `.mat`
- `BaseCorrect_data_cut.m`
  - 从基线校正结果中截取统一区间
  - 为后续对齐提供等长输入
- `change_time_to_idx_V3_multi.m`
  - 读取 Excel 分段边界时间
  - 映射到对齐后数据索引
  - 生成标准 `Segments`

### 4.2 决策层：参数、规则与策略选择
负责决定“用什么参数、怎么分段、怎么对齐、怎么评价”。
- `airPLS.m` 的参数：`lambda / order / wep / p / itermax`
- `Sepu_align_single.m` 中的 `opts`
  - `ranges`：大分段范围
  - `rangeProm`：各段峰显著性阈值
  - `t`：允许偏移量
  - `smoothSpan`：平滑窗口
- `my_cow_align_peakseg_V5_auto.m`
  - 自动峰检测
  - 峰簇聚类
  - 以谷底确定分段边界
  - 用 DP 选择最佳对齐路径
- `calculate_all_difference.m`
  - 选择差异度指标：`cosine / meanSPC / new`
  - 利用标定样映射修正结果

### 4.3 执行层：算法计算与结果产出
负责完成实际数值计算、对齐、重采样和指标输出。
- `airPLS.m`：基线拟合与校正
- `my_cow_align_peakseg_V5_auto.m`：分段 COW 对齐核心
- `Sepu_align_single.m` / `Sepu_align_batch.m`：批量执行与结果保存
- `calculate_all_difference.m`：两两差异度矩阵与修正指标生成
- `select_representative_samples.m`：代表样筛选

## 5. 核心逻辑流（输入 → 输出）
1. **原始采集**：样品 CSV/MAT 被读入，统一成 `table(x, y)`。
2. **基线校正**：`airPLS` 或批处理脚本生成 `ybc` 等校正结果。
3. **裁剪统一区间**：截取稳定分析区段，减少无效边缘影响。
4. **对齐**：
   - 参考样品作为模板
   - 先检测峰，再按峰簇分段
   - 段内用 COW + 动态规划对齐目标样品
5. **边界微调**：把 Excel 中的时间边界映射到对齐后索引，生成标准分段结构。
6. **归一化与差异度计算**：基于分段面积向量计算 `cosine / meanSPC / new`。
7. **标定样修正**：通过标定样映射表和标定样差异度表，对 `meanSPC` 做修正。
8. **输出**：生成 `.mat`、`.xlsx` 和 `.fig`，供可视化和后续统计分析。

## 6. 关键接口（类 / 函数 / 脚本）

### 6.1 原始数据与基线校正
- `ReadData_folder.m`
  - 批量读取样品目录，清洗 NaN，统一长度并保存 MAT
- `airPLS(Y, lambda, order, wep, p, itermax)`
  - 对单个或批量色谱矩阵做基线校正
  - 返回：`Yc`（校正后）、`Z`（基线）、`dssn`

### 6.2 裁剪与对齐
- `BaseCorrect_data_cut.m`
  - 从基线校正结果中裁剪固定区段
- `Sepu_align_single.m`
  - 单参考样品批量对齐入口
  - 读取 `segT`，调用对齐函数并保存结果图与元信息
- `my_cow_align_peakseg_V5_auto(chrom1, chrom2, opts)`
  - 核心对齐函数
  - 先峰检测与聚类，再动态规划求每段边界
  - 输出：`aligned_chrom2`、`bounds`、`seg_info`
- `my_findpeaks` / `my_linear_resample` / `my_corr`
  - 峰检测、重采样、相似度计算的基础工具函数

### 6.3 边界微调
- `change_time_to_idx_V3_multi.m`
  - 读取 Excel 边界时间，映射到索引
  - 生成 `Segments` 结构体数组

### 6.4 差异度计算
- `calculate_all_difference.m`
  - 读取 `SampleData_*_norm.mat`
  - 对样品两两计算差异度
  - 输出 `Segment_Area_Diff_finetuned.mat/.xlsx`
- `cosineSimilarity(x, y)`
  - 余弦相似度基础函数
- `meanSPC(x, y)`
  - 以分段面积为输入的差异指标
- `new(x, y)`
  - 新增实验性指标，参与计算但不一定输出
- `select_representative_samples.m`
  - 基于差异度结果筛代表样

## 7. 开发约定（从代码风格归纳）

### 7.1 命名习惯
- **脚本/函数**：`PascalCase` 或 `camelCase` 混用，但整体偏“业务可读性优先”。
- **变量**：多用语义化英文名，如 `chrom1`、`chrom2`、`seg_time`、`aligned_table`。
- **文件前缀**：
  - `Sepu_`：样品编号主命名规则
  - `SampleData_`：差异度/归一化中间结果
- **目录名**：按处理阶段编号，表示流程顺序：`00_` → `05_`。

### 7.2 数据结构约定
- 原始数据常用 `table(x, y)`。
- 基线校正后常见字段：`ybc`。
- 对齐结果常见字段：`aligned_table.x / aligned_table.aligned`。
- 分段结果使用结构体：`Segments(i).seg_id / time_start / time_end / idx_start / idx_end`。
- 差异度结果汇总为 `table`，并同时保存 MAT 与 XLSX。

### 7.3 参数约定
- 对齐参数统一封装为 `opts`。
- 阈值常使用“相对阈值 + 自动缩放”：
  - `rangeProm <= 1` 时按段最大值比例缩放
  - `rangeProm > 1` 时视为绝对阈值
- `t` 常表示“允许偏移量/扭曲窗口”。

### 7.4 错误处理方式
- 输入非法时直接 `error(...)` 中断，保证结果可信。
- 非关键问题以 `warning(...)` 提示并跳过。
- 常见容错策略：
  - 空文件/命名不符 → 跳过
  - NaN → 线性插值
  - 峰检测失败 → 回退到等长分段
  - Cholesky 失败 → 降级到反斜杠求解
- 批处理脚本通常采用“尽量继续执行”的策略，避免单样品失败影响整批。

### 7.5 结果保存约定
- 中间结果多保存为 `.mat`。
- 统计结果另存 `.xlsx`。
- 可视化结果保存 `.fig`，便于后续在 MATLAB 中二次编辑。

## 8. 流程分层总结
- **感知**：读取 CSV / MAT / Excel，统一成标准表结构。
- **决策**：根据峰、边界、阈值、标定样映射选择处理策略。
- **执行**：完成基线校正、裁剪、对齐、重采样、差异度计算与导出。

## 9. 备注
该项目本质上是一个“烟草色谱批处理分析管线”，核心难点集中在三处：
1. **基线校正稳定性**
2. **峰簇分段与动态规划对齐**
3. **分段面积差异度的标准化与标定修正**
