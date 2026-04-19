%% ========================================================================
% 脚本名称：
%   多样品色谱峰分段 COW 批量对齐脚本（单参考样品）
%
% 功能概述：
%   本脚本用于以“一个参考样品”为基准，
%   对文件夹中多个色谱样品进行批量对齐处理。
%
%   对齐方法基于：
%       峰检测 + 峰簇分段 + Correlation Optimized Warping (COW)
%
%   核心对齐算法由函数：
%       my_cow_align_peakseg_V5_auto
%   实现，支持：
%       1) 多区间独立峰检测
%       2) 不同区间设置不同峰显著性（MinPeakProminence）
%       3) 峰簇级别的不等距分段
%       4) 分段内 COW 动态规划时间扭曲
%
% --------------------------- 主要处理流程 ---------------------------
%   1. 指定数据主目录与子目录（top_folder / sub_folder）
%   2. 自动扫描文件夹内所有样品文件
%   3. 解析文件名，生成待对齐样品编号列表（target_list）
%   4. 加载用户指定的参考样品（ref_input）
%   5. 设置峰检测与 COW 对齐参数
%   6. 逐个样品循环：
%        - 加载目标样品
%        - 调用峰分段 COW 对齐函数
%        - 保存对齐后的色谱数据
%        - 保存完整对齐参数与分段信息
%        - 自动绘制并保存三类对比图
%   7. 统计并输出整体运行时间
%
% --------------------------- 输入数据要求 ---------------------------
%   每个样品为一个 .mat 文件，文件中需包含变量：
%       segT : table
%           segT.x   - 时间轴（Retention time）
%           segT.ybc - 基线校正后的色谱强度
%
%   文件命名格式示例：
%       Sepu_0000_17_baseline_seg1.mat
%
% --------------------------- 输出结果说明 ---------------------------
%   输出主目录：
%       Different_aligned_results_<top_folder>
%
%   子目录（以参考样品为基准）：
%       Sepu_<ref_input>_jizhun
%
%   对每一个目标样品，生成以下文件：
%       1) Sepu_<target>_aligned.mat
%          - 对齐后的色谱数据表（x, aligned）
%
%       2) Sepu_<target>_alignment_info.mat
%          - 对齐参数 opts
%          - 分段边界 bounds
%          - 峰簇与分段信息 seg_info
%          - 原始与对齐后信号（便于复现绘图）
%
%       3) 三类对比图（.fig）：
%          - 对齐前参考 vs 目标
%          - 对齐后参考 vs 对齐结果
%          - 上下组合对比图
%
% --------------------------- 使用说明 ---------------------------
%   用户通常只需修改以下内容即可运行：
%       ref_input       : 参考样品编号
%       top_folder      : 数据主目录
%       sub_folder      : 子目录（Segment）
%       opts.ranges     : 分段区间（样点索引）
%       opts.rangeProm  : 各区间峰显著性阈值
%
%   本脚本适用于：
%       - 多样品对齐结果批量生成
%       - 参数固定条件下的系统性对齐
%       - 对齐结果归档与后续统计分析
%
% ========================================================================

close all; clc;
startTime = datetime('now');
fprintf('开始时间: %s\n', datestr(startTime, 'yyyy-mm-dd HH:MM:SS.FFF'));
tic;

%% ======== 文件夹设置 ========
top_folder = '0000_5';   % 参考样品编号（必须保留）
ref_input  = top_folder;   % 后续命名仍然使用样品编号

% 自动拼接输入数据文件夹路径
input_folder = ['BaseCorrect_' top_folder '_cut'];

% 获取所有样品文件
file_list = dir(fullfile(input_folder, 'Sepu_*_baseline_seg1.mat'));

%% ======== 自动解析样品编号 ========
target_list = cell(1, length(file_list));
for k = 1:length(file_list)
    fname = file_list(k).name;
    fname = erase(fname, 'Sepu_');
    fname = erase(fname, '_baseline_seg1.mat');
    target_list{k} = fname;
end

disp(target_list);
fprintf('\n参考样品: %s\n', ref_input);

%% ======== 加载参考样品 ========
ref_file = fullfile(input_folder, sprintf('Sepu_%s_baseline_seg1.mat', ref_input));
ref_data = load(ref_file);
ref_table = ref_data.segT;
x_ref = ref_table.x;
chrom1 = ref_table.ybc;
fprintf('参考样品加载成功！\n');


%% ======== 对齐参数设置 ========
opts.minProminence = 0.05;     % 峰显著性（默认0.05）
opts.maxClusterGap = 5;        % 峰簇聚合距离（样点默认5）
opts.t = 20;                   % 每段允许的最大偏移量（样点默认50）
opts.smoothSpan = 5;           % 平滑窗口（样点默认5）
opts.plotDebug = false;        % 是否画调试图，true是画调试图，false是不画，批量处理一般不画。

opts.ranges = [
    1   600;
    601  1115;
    1116 2020;
    2021 3925;
    3926 5950;
    5951 6815;
    6816 8305;
    8306 8651;
    8652 9875;
    9875 11630;
];
% opts.rangeProm = [
%     1; 0.1; 0.05; 0.05; 0.01; 0.05; 0.07; 0.1; 0.05; 0.05;
% ]; %% 0000_93为参考进行对齐

opts.rangeProm = [
    1; 0.1; 0.05; 0.05; 0.05; 0.05; 0.07; 0.1; 0.05; 0.05;
]; %% 0000_17为参考进行对齐

% opts.rangeProm = [
%     1; 0.1; 0.05; 0.05; 0.02; 0.05; 0.07; 0.1; 0.05; 0.05;
% ]; %% 0000_60为参考进行对齐

% opts.rangeProm = [
%     1; 0.1; 0.05; 0.05; 0.05; 0.05; 0.07; 0.1; 0.05; 0.05;
% ]; %% 0000_97_align为参考进行对齐

%% ======== 输出文件夹 ========
output_main_folder = ['Different_aligned_results_' top_folder];
output_subfolder = sprintf('Sepu_%s_jizhun', ref_input);
output_folder = fullfile(output_main_folder, output_subfolder);
if ~exist(output_folder, 'dir')
    mkdir(output_folder);
    fprintf('创建文件夹: %s\n', output_folder);
end

%% ======== 批量处理 ========
numTargets = length(target_list);
for k = 1:numTargets
    t_sample = tic;   % ★ 单个样品计时开始
    target_input = target_list{k};

    %% 加载目标样品
    target_file = fullfile(input_folder, sprintf('Sepu_%s_baseline_seg1.mat', target_input));
    target_data = load(target_file);
    target_table = target_data.segT;
    chrom2 = target_table.ybc;

    %% 调用对齐函数
    [aligned_chrom2, bounds, seg_info] = my_cow_align_peakseg_V5_auto(chrom1, chrom2, opts);

    %% ======== 保存对齐后的数据 ========
    aligned_table = table(x_ref, aligned_chrom2, 'VariableNames', {'x', 'aligned'});
    output_filename = sprintf('Sepu_%s_aligned.mat', target_input);
    output_filepath = fullfile(output_folder, output_filename);
    save(output_filepath, 'aligned_table');

    %% ======== 保存对齐参数信息 ========
    info_filename = sprintf('Sepu_%s_alignment_info.mat', target_input);
    info_filepath = fullfile(output_folder, info_filename);
    alignment_info = struct();
    alignment_info.reference_sample = ref_input;
    alignment_info.target_sample = target_input;
    alignment_info.bounds = bounds;
    alignment_info.seg_info = seg_info;
    alignment_info.opts = opts;
    alignment_info.processing_time = toc;
    alignment_info.chrom1 = chrom1;
    alignment_info.chrom2 = chrom2;
    alignment_info.x_ref = x_ref;
    alignment_info.warped_chrom2 = aligned_chrom2;
    save(info_filepath, 'alignment_info');

    %% ======== 绘图并保存三幅图 ========
    % 对齐前对比图
    fig_before = figure('Name', 'Before Alignment Comparison', ...
                       'NumberTitle', 'off', 'Units', 'normalized', ...
                       'Position', [0.0, 0.0, 0.8, 0.4], 'Visible','on');
    plot(x_ref, chrom1, 'k', 'LineWidth', 1.2); hold on;
    plot(x_ref, chrom2, 'r', 'LineWidth', 1.1);
    xlabel('Retention time'); ylabel('Intensity');
    grid on; set(gca, 'GridLineStyle', '--', 'GridAlpha', 0.2);
    title(sprintf('Before Alignment (Ref = %s, Target = %s)', ref_input, target_input), ...
        'Interpreter', 'none');
    legend('Reference', 'Target (unaligned)');
    saveas(fig_before, fullfile(output_folder, sprintf('Sepu_%s_before_alignment.fig', target_input)));
    close(fig_before);

    % 对齐后对比图
    fig_after = figure('Name', 'After Alignment Comparison', ...
                       'NumberTitle', 'off', 'Units', 'normalized', ...
                       'Position', [0.0, 0.4, 0.8, 0.4], 'Visible','on');
    plot(x_ref, chrom1, 'k', 'LineWidth', 1.2); hold on;
    plot(x_ref, aligned_chrom2, 'b', 'LineWidth', 1.1);
    xlabel('Retention time'); ylabel('Intensity');
    grid on; set(gca, 'GridLineStyle', '--', 'GridAlpha', 0.2);
    title(sprintf('After COW Alignment (Ref = %s, Target = %s)', ref_input, target_input), ...
        'Interpreter', 'none');
    legend('Reference', 'Aligned target');
    saveas(fig_after, fullfile(output_folder, sprintf('Sepu_%s_after_alignment.fig', target_input)));
    close(fig_after);

    % 上下组合对比图
    fig_combined = figure('Name', 'COW Peak-Segmented Alignment', ...
                          'NumberTitle', 'off', 'Units', 'normalized', ...
                          'Position', [0.0, 0.0, 0.8, 0.3], 'Visible','on');
    % 对齐前
    subplot(2,1,1);
    plot(x_ref, chrom1, 'k', 'LineWidth',1.2); hold on;
    plot(x_ref, chrom2, 'r', 'LineWidth',1.1);
    xlabel('Retention time'); ylabel('Intensity'); grid on;
    set(gca, 'GridLineStyle', '--', 'GridAlpha', 0.2);
    title(sprintf('Before alignment (Ref = %s, Target = %s)', ref_input, target_input), 'Interpreter','none');
    legend('Reference','Target (unaligned)');
    % 对齐后
    subplot(2,1,2);
    plot(x_ref, chrom1, 'k', 'LineWidth',1.2); hold on;
    plot(x_ref, aligned_chrom2, 'b', 'LineWidth',1.1);
    xlabel('Retention time'); ylabel('Intensity'); grid on;
    set(gca, 'GridLineStyle', '--', 'GridAlpha', 0.2);
    title(sprintf('After COW alignment (Ref = %s, Target = %s)', ref_input, target_input), 'Interpreter','none');
    legend('Reference','Aligned target');
    saveas(fig_combined, fullfile(output_folder, sprintf('Sepu_%s_combined_comparison.fig', target_input)));
    close(fig_combined);
    sampleTime = toc(t_sample);
    fprintf('\r>>> 进度: %3d / %3d | 当前样品: %-10s | 耗时: %.2f s\n', ...
            k, numTargets, target_input, sampleTime);
end

elapsedTime = toc;
endTime = datetime('now');
fprintf('\n批量处理完成，总耗时 %.4f 秒\n', elapsedTime);
fprintf('结束时间: %s\n', datestr(endTime, 'yyyy-mm-dd HH:MM:SS.FFF'));
