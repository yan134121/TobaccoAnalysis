%% ========================================================================
% 脚本名称：
%   change_time_to_idx_V4_multi_noSubfolder.m
%
% 功能：
%   - Excel 第一行是样品编号（0000_1、0000_5…）
%   - 每一列对应一个基准样品
%   - 从第二行开始是分段边界时间
%   - 将分段时间映射为 aligned 数据索引
%   - 生成标准 Segments 结构体
%   - 每个样品保存一个 seg_样品编号.mat
% ========================================================================
%% ========================================================================
% 脚本名称：
%   change_time_to_idx_V4_multi_noSubfolder.m
%
% 功能概述：
%   本脚本用于将 Excel 表格中的分段边界时间映射到已对齐的色谱数据索引，
%   并生成标准化的分段结构体 Segments，最终为每个样品保存独立的结果文件。
%
% 输入：
%   1. Excel 文件（例如：所有基准样品分段边界线-599个样品.xlsx）
%      - 第一行：样品编号（如 0000_1、0000_5 …）
%      - 每一列：对应一个基准样品
%      - 从第二行开始：该样品的分段边界时间
%
%   2. Aligned 数据文件夹（例如：All_599_aligned_by_ref_flat）
%      - 文件命名格式：Sepu_样品编号_aligned.mat
%      - 每个文件包含结构 aligned_table：
%          .x        → 时间序列
%          .aligned  → 对齐后的信号数据
%
% 输出：
%   - 输出文件夹（例如：All_599_ref_segments）
%   - 每个样品生成一个 seg_样品编号.mat 文件，包含：
%       Segments → 分段结构体数组，字段包括：
%           seg_id     ：分段编号
%           time_start ：分段起始时间
%           time_end   ：分段结束时间
%           idx_start  ：分段起始索引
%           idx_end    ：分段结束索引
%       seg_time → Excel 中的分段边界时间
%       seg_idx  → 对应的索引位置
%
% 处理流程：
%   1. 读取 Excel，获取样品编号及分段时间
%   2. 逐样品加载对应的 aligned 数据
%   3. 提取并检查分段时间的合法性（严格递增、范围合理）
%   4. 将分段时间映射为 aligned 数据索引
%   5. 构造 Segments 结构体，记录每段的起止时间与索引
%   6. 保存结果到输出文件夹
% 日期：20260328
% ========================================================================

clear; clc;

%% ===================== 路径设置 =====================
seg_excel    = '所有基准样品分段边界线-599个样品.xlsx';
aligned_root = 'All_599_aligned_by_ref_flat';   % aligned 数据根目录（无子文件夹）
out_folder   = 'All_599_ref_segments';

if ~exist(out_folder, 'dir')
    mkdir(out_folder);
end

%% ===================== Step 1：读取 Excel =====================
% 直接按 table 读，第一行是列名
T = readtable(seg_excel, 'VariableNamingRule','preserve');

% 样品编号来自列名
sample_ids  = T.Properties.VariableNames;
num_samples = numel(sample_ids);

fprintf('检测到 %d 个基准样品。\n', num_samples);

%% ===================== Step 2：逐样品处理 =====================
for k = 1:num_samples

    sample_id = sample_ids{k};
    fprintf('处理样品 %s (%d / %d)...\n', sample_id, k, num_samples);

    %% -------- Step 2.1 加载 aligned 数据 --------
    aligned_file = fullfile(aligned_root, ['Sepu_' sample_id '_aligned.mat']);

    if ~exist(aligned_file, 'file')
        warning('找不到 aligned 文件：%s，已跳过', aligned_file);
        continue;
    end

    load(aligned_file); 
    % 假设 MAT 文件里有 aligned_table，包含 .x 和 .aligned
    time = aligned_table.x;
    y    = aligned_table.aligned;

    %% -------- Step 2.2 提取分段时间（从第 2 行开始） --------
    raw_col = T{:, k};   % 当前样品整列（可能是 cell / double）

    if iscell(raw_col)
        seg_time = cellfun(@double, raw_col);
    else
        seg_time = raw_col;
    end

    % 去掉 NaN（Excel 空单元格）
    seg_time = seg_time(~isnan(seg_time));
    seg_time = seg_time(:);   % 列向量

    %% -------- Step 2.3 合法性检查 --------
    if any(diff(time) <= 0)
        error('样品 %s：time 不是严格递增', sample_id);
    end

    if any(diff(seg_time) <= 0)
        error('样品 %s：seg_time 不是严格递增', sample_id);
    end

    if seg_time(1) < time(1)
        error('样品 %s：seg_time 起点早于色谱起点', sample_id);
    end

    %% -------- Step 2.4 分段时间 → 索引 --------
    numSegTime = length(seg_time);
    seg_idx = zeros(numSegTime,1);

    for i = 1:numSegTime
        % 所有段边界都取最接近的索引，包括最后一个
        [~, seg_idx(i)] = min(abs(time - seg_time(i)));
    end

    if any(diff(seg_idx) <= 0)
        error('样品 %s：seg_idx 非递增', sample_id);
    end

    %% -------- Step 2.5 构造 Segments 结构体 --------
    numSeg = numSegTime - 1;
    Segments = struct();

    for i = 1:numSeg
        if i == 1
            idx_start = seg_idx(1);
        else
            idx_start = seg_idx(i) + 1;
        end
        idx_end = seg_idx(i+1);   % 使用 Excel 的分段边界对应索引作为段末尾

        Segments(i).seg_id     = i;
        Segments(i).time_start = time(idx_start);
        Segments(i).time_end   = time(idx_end);
        Segments(i).idx_start  = idx_start;
        Segments(i).idx_end    = idx_end;
    end

    %% -------- Step 2.6 保存 --------
    save_file = fullfile(out_folder, ['seg_' sample_id '.mat']);
    save(save_file, 'Segments', 'seg_time', 'seg_idx');

end

fprintf('✅ 所有基准样品分段文件已生成完成。\n');
