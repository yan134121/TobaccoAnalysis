%% ========================================================================
% 基于标定样的分段面积归一化（orig + finetuned）
% 数据源：All_finetuned_results_merge
% ========================================================================
%% ========================================================================
% 基于标定样的分段面积归一化（orig + finetuned）
%
% 【功能概述】
% 本脚本用于根据“标定样品 → 后跟样品”的映射关系，对所有样品的
% 原始分段面积（segments_orig）和 finetuned 分段面积（segments_finetuned）
% 进行归一化处理。归一化方式为：
%
%   每个后跟样的每个分段面积 ＝ 该样品的原面积 ×（1 / 标定样对应分段面积）
%
% 即：所有样品均使用“标定样的分段面积”作为缩放基准。
%
% 归一化后会生成：
%   - segments_orig(i).area_norm
%   - segments_finetuned(i).area_norm
%   - segment_areas_norm_orig（向量）
%   - segment_areas_norm_finetuned（向量）
%
% 【主要流程】
% 1. 读取标定样 → 后跟样映射表（Excel）
% 2. 对每个标定样：
%       a. 读取标定样数据
%       b. 计算 orig 与 finetuned 的分段面积缩放因子（1 / area）
%       c. 对标定样自身进行归一化（结果均为 1）
%       d. 保存归一化后的标定样
% 3. 对每个标定样的所有后跟样：
%       a. 读取样品数据
%       b. 使用“标定样的缩放因子”进行归一化
%       c. 特殊处理：第 31 段 area_norm 强制设为 1（orig + finetuned）
%       d. 保存归一化后的样品
%
% 【输入】
% - in_folder：包含所有样品（orig + finetuned）的文件夹
% - out_folder：归一化结果输出文件夹
% - map_file：标定样 → 后跟样 映射表（Excel）
%
% 【输出】
% - SampleData_XXX_norm.mat：包含归一化后的 orig + finetuned 分段面积
%
% 【特殊逻辑】
% - 所有归一化均基于“标定样的分段面积”
% - 标定样自身归一化后所有分段面积均为 1
% - 后跟样使用标定样的缩放因子进行归一化
% - 第 31 段 area_norm 强制设为 1（临时策略，可修改,需要预留接口）
%

%
% 作者：笑莲
% ========================================================================

clear; clc;

%% ================= 路径设置 =================
in_folder  = 'All_599_finetuned_results_flat';
out_folder = 'All_599_finetuned_results_norm';
map_file   = '标定样品编号列表-全599个样品.xlsx';

if ~exist(out_folder,'dir')
    mkdir(out_folder);
end

%% ================= 读取标定样-后跟样映射 =================
map_table = readtable(map_file, ...
    'ReadVariableNames', true, ...
    'PreserveVariableNames', true);

calib_ids = map_table.Properties.VariableNames;
fprintf('检测到 %d 个标定样品组\n', numel(calib_ids));

%% ================= 遍历每个标定样 =================
for c = 1:numel(calib_ids)

    calib_id = calib_ids{c};
    fprintf('\n====================================\n');
    fprintf('处理标定样：%s\n', calib_id);

    %% ---------- 读取标定样 ----------
    calib_file = fullfile(in_folder, ...
        sprintf('SampleData_%s_finetuned.mat', calib_id));

    if ~exist(calib_file,'file')
        warning('❌ 标定样文件不存在，跳过：%s', calib_id);
        continue;
    end

    load(calib_file,'SampleData');
    S_ref = SampleData;

    %% ---------- 一致性检查 ----------
    n_orig = numel(S_ref.segments_orig);
    n_fine = numel(S_ref.segments_finetuned);

    if n_orig ~= n_fine
        warning('❌ 标定样 %s：orig 与 finetuned 段数不一致，跳过', calib_id);
        continue;
    end

    num_segs = n_orig;

    %% ---------- 提取标定样段面积 ----------
    area_ref_orig = zeros(num_segs,1);
    area_ref_fine = zeros(num_segs,1);

    for i = 1:num_segs
        area_ref_orig(i) = S_ref.segments_orig(i).area;
        area_ref_fine(i) = S_ref.segments_finetuned(i).area;
    end

    %% ---------- 计算缩放因子 ----------
    scale_orig = 1 ./ area_ref_orig;
    scale_fine = 1 ./ area_ref_fine;

    %% ---------- 标定样自身归一化 ----------
    for i = 1:num_segs
        S_ref.segments_orig(i).area_norm = ...
            area_ref_orig(i) * scale_orig(i);          % = 1

        S_ref.segments_finetuned(i).area_norm = ...
            area_ref_fine(i) * scale_fine(i);          % = 1
    end

    S_ref.segment_areas_norm_orig      = area_ref_orig .* scale_orig;
    S_ref.segment_areas_norm_finetuned = area_ref_fine .* scale_fine;

    %% ---------- 保存标定样 ----------
    save(fullfile(out_folder, ...
        sprintf('SampleData_%s_norm.mat', calib_id)), ...
        'S_ref','-v7.3');

    fprintf('✔ 标定样归一化完成\n');

    %% ---------- 读取后跟样品列表 ----------
    follow_ids = map_table.(calib_id);
    follow_ids = follow_ids(~cellfun(@isempty, follow_ids));

    %% ---------- 遍历后跟样 ----------
    for k = 1:numel(follow_ids)

        sid = follow_ids{k};
        fprintf('  → 后跟样品：%s\n', sid);

        in_file = fullfile(in_folder, ...
            sprintf('SampleData_%s_finetuned.mat', sid));

        if ~exist(in_file,'file')
            warning('    文件不存在，跳过：%s', sid);
            continue;
        end

        load(in_file,'SampleData');
        S = SampleData;

        %% ---------- 一致性检查 ----------
        if numel(S.segments_orig) ~= num_segs || ...
           numel(S.segments_finetuned) ~= num_segs
            warning('    段数不一致，跳过：%s', sid);
            continue;
        end

        %% ---------- 提取段面积 ----------
        area_orig = zeros(num_segs,1);
        area_fine = zeros(num_segs,1);

        for i = 1:num_segs
            area_orig(i) = S.segments_orig(i).area;
            area_fine(i) = S.segments_finetuned(i).area;
        end

        %% ---------- 使用“标定样缩放因子”归一化 ----------
        for i = 1:num_segs
            S.segments_orig(i).area_norm = ...
                area_orig(i) * scale_orig(i);

            S.segments_finetuned(i).area_norm = ...
                area_fine(i) * scale_fine(i);
        end

        S.segment_areas_norm_orig      = area_orig .* scale_orig;
        S.segment_areas_norm_finetuned = area_fine .* scale_fine;

        %% ---------- 特殊处理：第31段强制设为1（临时） ----------
force_seg = 31;

if num_segs >= force_seg  %只要样品的分段数 至少 包含第 31 段，就允许修改第 31 段
    % orig
    S.segments_orig(force_seg).area_norm = 1;
    S.segment_areas_norm_orig(force_seg) = 1;

    % finetuned
    S.segments_finetuned(force_seg).area_norm = 1;
    S.segment_areas_norm_finetuned(force_seg) = 1;
end


        %% ---------- 保存 ----------
        save(fullfile(out_folder, ...
            sprintf('SampleData_%s_norm.mat', sid)), ...
            'S','-v7.3');

        fprintf('    ✔ 完成归一化\n');
    end
end

fprintf('\n🎉 所有样品（基于标定样的 orig + finetuned）归一化完成！\n');
