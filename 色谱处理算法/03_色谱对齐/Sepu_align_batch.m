%% ========================================================================
% 多样品色谱峰分段 COW 批量对齐脚本（每个子文件夹一个参考样品）
% 规则：
%   - 子文件夹名 = 参考样品编号
%   - 参考文件   = Sepu_<子文件夹名>_baseline_seg1.mat
%   - 仅保留 combined 对比图（Before + After）
% ========================================================================

%% ========================================================================
% 脚本名称：
%   多样品色谱峰分段 COW 批量对齐脚本（每个子文件夹一个参考样品）
%
% 功能概述：
%   本脚本用于对“按编号拆分后的样品数据”执行 **多样品色谱峰分段 COW 批量对齐**。
%
% 【关键前提】
%   ★ 根目录中的每个子文件夹本身就是一个“参考样品” ★
%   ★ 所有参考样品编号均以 “0000_” 开头，例如：
%         0000_1、0000_2、0000_3 …
%
% 因此：
%   - 每个子文件夹对应一个独立的对齐任务
%   - 子文件夹名 = 参考样品编号
%   - 参考文件固定为：
%         Sepu_<参考编号>_baseline_seg1.mat
%   - 同一文件夹下的其他样品全部作为目标样品，与该参考样品对齐
%
% 【输入目录结构】
% root_folder/
%   ├── 0000_1/
%   │       ├── Sepu_0000_1_baseline_seg1.mat   ← 参考样品
%   │       ├── Sepu_0001_3_baseline_seg1.mat
%   │       ├── Sepu_0002_5_baseline_seg1.mat
%   │       └── ...
%   ├── 0000_2/
%   ├── 0000_3/
%   └── ...
%
% 【对齐流程】
%   1）遍历根目录下所有参考样品文件夹（全部以 0000_ 开头）
%   2）读取其中所有 baseline_seg1 色谱文件
%   3）加载参考样品色谱（x_ref, chrom1）
%   4）对同文件夹下的所有其他样品执行 COW 分段对齐
%   5）输出内容包括：
%         - 对齐后的色谱 aligned_table（x + aligned）
%         - 对齐信息 alignment_info（bounds、warping、参数等）
%         - combined 对比图（Before + After）
%
% 【输出目录结构】
% Different_aligned_results_<参考编号>/
%   └── Sepu_<参考编号>_jizhun/
%           ├── Sepu_XXXX_aligned.mat
%           ├── Sepu_XXXX_alignment_info.mat
%           ├── Sepu_XXXX_combined_comparison.fig
%           └── ...
%
% 【注意事项】
%   - 每个子文件夹都是参考样品，不存在“非参考样品文件夹”
%   - 参考样品自身不参与对齐
%   - COW 参数 opts 对所有样品保持一致，确保批处理一致性
%   - 输出文件夹自动创建，不会覆盖其他参考样品的结果
%
% ========================================================================

close all; clc;
startTime = datetime('now');
fprintf('开始时间: %s\n', datestr(startTime,'yyyy-mm-dd HH:MM:SS.FFF'));
tic;

%% ================= 顶层路径 =================
root_folder = 'BaseCorrect_cut';
dir_info = dir(root_folder);
is_sub = [dir_info.isdir] & ~ismember({dir_info.name},{'.','..'});
sub_folders = dir_info(is_sub);

fprintf('共检测到 %d 个样品子文件夹\n\n', numel(sub_folders));

%% ================= COW 参数（全局一致） =================
opts.minProminence = 0.05;
opts.maxClusterGap = 5;
opts.t             = 35;
opts.smoothSpan    = 5;
opts.plotDebug     = false;

opts.ranges = [
    1     600;
    601   1115;
    1116  2020;
    2021  3925;
    3926  5950;
    5951  6815;
    6816  8305;
    8306  8651;
    8652  9875;
    9875 11630;
];

opts.rangeProm = [
    1; 0.1; 0.05; 0.05; 0.05; 0.05; 0.07; 0.1; 0.05; 0.05;
];

%% ================= 遍历每个子文件夹 =================
for i = 1:numel(sub_folders)

    top_folder_id   = sub_folders(i).name;     % 参考样品编号
    input_folder    = fullfile(root_folder, top_folder_id);

    fprintf('========== 处理样品文件夹：%s ==========\n', top_folder_id);

    %% ---------- 获取所有样品 ----------
    file_list = dir(fullfile(input_folder,'Sepu_*_baseline_seg1.mat'));
    if isempty(file_list)
        warning('文件夹中无有效数据，跳过 %s\n', top_folder_id);
        continue;
    end

    %% ---------- 参考样品 ----------
    ref_input = top_folder_id;
    ref_file  = fullfile(input_folder, ...
        sprintf('Sepu_%s_baseline_seg1.mat', ref_input));

    if ~exist(ref_file,'file')
        warning('找不到参考文件 %s，跳过该文件夹\n', ref_input);
        continue;
    end

    ref_data  = load(ref_file);
    ref_table = ref_data.segT;
    x_ref  = ref_table.x;
    chrom1 = ref_table.ybc;

    fprintf('参考样品 %s 加载完成\n', ref_input);

    %% ---------- 目标样品列表 ----------
    target_list = cell(1,numel(file_list));
    for k = 1:numel(file_list)
        fname = erase(file_list(k).name,{'Sepu_','_baseline_seg1.mat'});
        target_list{k} = fname;
    end

    %% ---------- 输出文件夹 ----------
    output_main_folder = ['Different_aligned_results_' ref_input];
    output_subfolder   = sprintf('Sepu_%s_jizhun', ref_input);
    output_folder      = fullfile(output_main_folder, output_subfolder);

    if ~exist(output_folder,'dir')
        mkdir(output_folder);
    end

    %% ---------- 保存参考样品自身 ----------
aligned_table = table(x_ref, chrom1, 'VariableNames', {'x','aligned'});
save(fullfile(output_folder, sprintf('Sepu_%s_aligned.mat', ref_input)), 'aligned_table');

alignment_info.reference_sample = ref_input;
alignment_info.target_sample    = ref_input;
alignment_info.bounds           = [];
alignment_info.seg_info         = [];
alignment_info.opts             = opts;
alignment_info.processing_time  = 0;
alignment_info.chrom1           = chrom1;
alignment_info.chrom2           = chrom1;
alignment_info.x_ref            = x_ref;
alignment_info.warped_chrom2    = chrom1;

save(fullfile(output_folder, sprintf('Sepu_%s_alignment_info.mat', ref_input)), 'alignment_info');

fig = figure('Visible','on','Units','normalized','Position',[0.05 0.1 0.85 0.6]);
subplot(2,1,1);
plot(x_ref, chrom1, 'k');
title(['Reference Sample (unaligned): ' ref_input],'Interpreter','none');
grid on;

subplot(2,1,2);
plot(x_ref, chrom1, 'b');
title(['Reference Sample (aligned = original): ' ref_input],'Interpreter','none');
grid on;

saveas(fig, fullfile(output_folder, sprintf('Sepu_%s_combined_comparison.fig', ref_input)));
close(fig);

fprintf('参考样品 %s 已保存自身结果\n', ref_input);

    
    
    
    %% ================= 批量对齐 =================
    for k = 1:numel(target_list)

        target_input = target_list{k};

        % 参考样品自己不对齐
        if strcmp(target_input, ref_input)
            continue;
        end

        t_sample = tic;

        target_file = fullfile(input_folder, ...
            sprintf('Sepu_%s_baseline_seg1.mat', target_input));
        target_data  = load(target_file);
        chrom2 = target_data.segT.ybc;

        %% ---- COW 对齐 ----
        [aligned_chrom2, bounds, seg_info] = ...
            my_cow_align_peakseg_V5_auto(chrom1, chrom2, opts);

        %% ---- 保存对齐结果 ----
        aligned_table = table(x_ref, aligned_chrom2, ...
            'VariableNames',{'x','aligned'});
        save(fullfile(output_folder, ...
            sprintf('Sepu_%s_aligned.mat', target_input)), ...
            'aligned_table');

        %% ---- 保存对齐信息 ----
        alignment_info.reference_sample = ref_input;
        alignment_info.target_sample    = target_input;
        alignment_info.bounds           = bounds;
        alignment_info.seg_info         = seg_info;
        alignment_info.opts             = opts;
        alignment_info.processing_time  = toc(t_sample);
        alignment_info.chrom1           = chrom1;
        alignment_info.chrom2           = chrom2;
        alignment_info.x_ref            = x_ref;
        alignment_info.warped_chrom2    = aligned_chrom2;

        save(fullfile(output_folder, ...
            sprintf('Sepu_%s_alignment_info.mat', target_input)), ...
            'alignment_info');

        %% ---- combined 对比图 ----
        fig = figure('Visible','on','Units','normalized',...
            'Position',[0.05 0.1 0.85 0.6]);

        subplot(2,1,1);
        plot(x_ref,chrom1,'k',x_ref,chrom2,'r');
        title(['Before Alignment: ' ref_input ' vs ' target_input],'Interpreter','none');
        grid on;

        subplot(2,1,2);
        plot(x_ref,chrom1,'k',x_ref,aligned_chrom2,'b');
        title(['After Alignment: ' ref_input ' vs ' target_input],'Interpreter','none');
        grid on;

        saveas(fig, fullfile(output_folder, ...
            sprintf('Sepu_%s_combined_comparison.fig', target_input)));
        close(fig);

        fprintf('  [%2d/%2d] %s 对齐完成 | %.2f s\n', ...
            k, numel(target_list), target_input, toc(t_sample));
    end

    fprintf('>>>> 文件夹 %s 全部完成\n\n', top_folder_id);
end

elapsedTime = toc;
fprintf('=== 所有样品处理完成，总耗时 %.2f s ===\n', elapsedTime);
