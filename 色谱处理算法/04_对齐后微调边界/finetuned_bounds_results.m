%% ========================================================================
% 批量微调所有基准样品文件夹内的 aligned 样品分段边界
% 初始边界信息来源于 All_base_segments/seg_<ref_name>.mat
% ========================================================================
%% ========================================================================
% 批量微调所有基准样品文件夹内的 aligned 样品分段边界
%
% 【功能概述】
% 本脚本用于对所有基准样品（reference samples）对应的 aligned 样品，
% 批量执行“分段起点微调（finetuning）”操作。微调依据为：
%
%   在每个段的允许范围（默认 ±5 点）内搜索最小值点作为新的段起点，
%   并确保不越界、不跨段。
%
% 微调完成后，会重新计算每段面积，并生成包含：
%   - 原始分段信息（segments_orig）
%  - 微调后分段信息（segments_finetuned）
%   - 微调位移记录（results）
%   - aligned 原始数据表
% 的 SampleData 结构体，并按样品编号保存为 MAT 文件。
%
% 【主要流程】
% 1. 遍历 input_root 下的所有基准样品子文件夹
% 2. 对每个基准样：
%       a. 读取基准样的初始分段（seg_<ref>.mat）
%       b. 获取该基准样文件夹内所有 aligned 样品
% 3. 对每个 aligned 样品：
%       a. 加载 aligned_table（chrom + time）
%       b. 复制基准样的初始段起点作为初始模板
%       c. 对每个分段执行微调：
%           - 根据硬约束（不跨段）与用户范围（±5）确定搜索区间
%           - 在区间内寻找最小值点作为新的段起点
%           - 记录微调位移
%       d. 根据微调后的边界重新计算每段面积
%       e. 构建 SampleData 结构体并保存
%
% 【输入】
% - input_root：包含所有基准样品子文件夹，每个子文件夹内存放 aligned 样品
% - base_seg_folder：存放基准样初始分段文件（seg_<ref>.mat）
% - output_root：微调结果输出文件夹
% - adjust_range_default：每段允许的微调范围（默认 [-5, +5]）
%
% 【输出】
% - output_root/<ref_name>/SampleData_<sample_id>_finetuned.mat
%   包含：
%       SampleData.sample_id
%       SampleData.aligned_table
%       SampleData.segments_orig
%       SampleData.segments_finetuned
%       SampleData.results（微调记录）
%       SampleData.timestamp
%
% 【微调逻辑说明】
% - 每段起点仅在允许范围内移动，不跨越前后段边界
% - 搜索区间 = max(硬约束, 用户范围)
% - 微调依据：区间内信号最小值点
% - 微调后重新计算面积（trapz）
%
% 【扩展性】
% - 可替换微调策略（如最大斜率点、局部极值点等）
% - 可为不同段设置不同微调范围
%
% 作者：笑莲
% ========================================================================

clc; clear; close all;

%% ================= 用户输入 =================
input_root       = 'All_599_aligned_by_ref';    % 原整理好的 aligned 文件夹
base_seg_folder  = 'All_599_ref_segments';     % 基准样品初始分段文件夹
output_root      = 'All_599_finetuned_results';% 保存微调后的结果
adjust_range_default = [-5, 5];             % 所有段 ±5 微调，预留接口

if ~exist(output_root,'dir')
    mkdir(output_root);
end

%% ================= 遍历基准样品子文件夹 =================
ref_folders = dir(input_root);
is_sub = [ref_folders.isdir] & ~ismember({ref_folders.name},{'.','..'});
ref_folders = ref_folders(is_sub);

fprintf('共检测到 %d 个基准样品子文件夹\n\n', numel(ref_folders));

for i_ref = 1:numel(ref_folders)
    ref_name = ref_folders(i_ref).name;
    fprintf('==== 处理基准样品: %s (%d/%d) ====\n', ref_name, i_ref, numel(ref_folders));
    
    input_subfolder  = fullfile(input_root, ref_name);
    output_subfolder = fullfile(output_root, ref_name);
    if ~exist(output_subfolder,'dir')
        mkdir(output_subfolder);
    end
    
    %% ========== 加载该基准样品初始分段 ==========
    seg_file = fullfile(base_seg_folder, sprintf('seg_%s.mat', ref_name));
    if ~exist(seg_file,'file')
        warning('基准样品分段文件不存在: %s, 跳过该基准样品', seg_file);
        continue;
    end
    seg_data = load(seg_file);  % 包含 Segments 结构体数组
    Segments = seg_data.Segments;
    num_segs = numel(Segments);
    seg_starts_orig_template = [Segments.idx_start]';
    
    %% ========== 获取该子文件夹内所有 aligned 文件 ==========
    files = dir(fullfile(input_subfolder, 'Sepu_*_aligned.mat'));
    fprintf('检测到 %d 个样品文件\n', numel(files));
    
    for f = 1:numel(files)
        sample_file = fullfile(files(f).folder, files(f).name);
        [~, name, ~] = fileparts(files(f).name);
        
        % 样品编号
        sample_id = erase(name, 'Sepu_');
        sample_id = erase(sample_id, '_aligned');
        
        fprintf('\n------------------------------\n');
        fprintf('处理样品 %s (%d/%d)\n', sample_id, f, numel(files));
        
        %% ========== 加载样品数据 ==========
        data = load(sample_file);
        tbl  = data.aligned_table;
        chrom = tbl.aligned(:);
        time  = tbl.x(:);
        
        %% ========== 初始化边界和微调记录 ==========
        seg_starts = seg_starts_orig_template;
        results = struct('seg_id', {}, 'old_start', {}, 'new_start', {}, 'displacement', {}, 'adjust_range', {});
        
        %% ========== 计算微调前段面积 ==========
        segments_orig = Segments;  % 复制结构体
        segment_areas_orig = zeros(num_segs,1);
        for s = 1:num_segs
            start_idx = seg_starts(s);
            if s < num_segs
                end_idx = seg_starts(s+1) - 1;
            else
                end_idx = length(chrom);
            end
            segment_areas_orig(s) = trapz(time(start_idx:end_idx), chrom(start_idx:end_idx));
            segments_orig(s).area = segment_areas_orig(s);
        end
        
        %% ========== 批量微调（所有段 ±5） ==========
        for s = 1:num_segs
            old_start = seg_starts(s);
            
            % ---- 硬约束（不越段） ----
            hard_L = 1;
            hard_R = length(chrom);
            if s > 1
                hard_L = seg_starts(s-1) + 1;
            end
            if s < num_segs
                hard_R = seg_starts(s+1) - 1;
            end
            
            % ---- 用户范围 ----
            user_L = old_start + adjust_range_default(1);
            user_R = old_start + adjust_range_default(2);
            
            % ---- 最终搜索区间 ----
            L = max([hard_L, user_L, 1]);
            R = min([hard_R, user_R, length(chrom)]);
            if L >= R
                warning('Seg %d 搜索区间非法，跳过', s);
                continue;
            end
            
            idx = L:R;
            sig = chrom(idx);
            
            % ---- 最小值 ----
            [~, k] = min(sig);
            new_start = idx(k);
            displacement = new_start - old_start;
            
            % ---- 更新起点 ----
            seg_starts(s) = new_start;
            
            % ---- 记录结果 ----
            results(end+1).seg_id     = s;
            results(end).old_start    = old_start;
            results(end).new_start    = new_start;
            results(end).displacement = displacement;
            results(end).adjust_range = adjust_range_default;
        end
        
        %% ========== 保存微调后的边界 ==========
        segments_finetuned = Segments;
        for s = 1:num_segs
            segments_finetuned(s).idx_start = seg_starts(s);
        end
        
        %% ========== 计算微调后段面积 ==========
        segment_areas_finetuned = zeros(num_segs,1);
        for s = 1:num_segs
            start_idx = seg_starts(s);
            if s < num_segs
                end_idx = seg_starts(s+1) - 1;
            else
                end_idx = length(chrom);
            end
            segment_areas_finetuned(s) = trapz(time(start_idx:end_idx), chrom(start_idx:end_idx));
            segments_finetuned(s).area = segment_areas_finetuned(s);
        end
        
        %% ========== 构建 SampleData 结构体 ==========
        SampleData.sample_id          = sample_id;
        SampleData.aligned_table      = tbl;                     
        SampleData.segments_orig      = segments_orig;               
        SampleData.segments_finetuned = segments_finetuned;     
        SampleData.results            = results;                
        SampleData.timestamp          = datetime('now');
        
        %% ========== 保存 MAT 文件 ==========
        save_name = fullfile(output_subfolder, sprintf('SampleData_%s_finetuned.mat', sample_id));
        save(save_name, 'SampleData', '-v7.3');
        
        fprintf('✔ 样品 %s 微调完成并保存结构体文件\n', sample_id);
    end
end

fprintf('\n🎉 所有基准样品子文件夹微调完成！\n');
