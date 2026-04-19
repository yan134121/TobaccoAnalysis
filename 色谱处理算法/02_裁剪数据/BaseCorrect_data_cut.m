%% ========================================================================
% 批处理色谱数据（单段裁剪）
%
% 功能：
%   - 批量读取 input_folder 中所有 Sepu_*_baseline.mat 文件
%   - 从 data_baseline 中提取 x 和 ybc 两列
%   - 按指定区间 segment_bounds 裁剪一个区段
%   - 将裁剪后的 TABLE（segT）保存到 <input_folder>_cut/
%
% 输入数据位置：
%   <input_folder>/Sepu_*_baseline.mat
%
% 输出数据位置：
%   <input_folder>_cut/<原文件名>_seg1.mat
%
% 用途：
%   为后续 COW 对齐准备统一长度的单段色谱数据
% ========================================================================

clear; clc;

%% ======== 输入与输出文件夹 ========
input_folder = 'BaseCorrect_0000_5';

% 新建一个总输出文件夹：BaseCorrect_0000_100_cut
out_folder = [input_folder '_cut'];
if ~exist(out_folder, 'dir')
    mkdir(out_folder);
end

%% ======== 单段裁剪区间 ========
segment_bounds = [1 11630];

%% ======== 获取所有 mat 文件 ========
files = dir(fullfile(input_folder, 'Sepu_*_baseline.mat'));
fprintf('找到 %d 个 mat 文件，开始处理...\n', numel(files));

%% ======== 批量处理 ========
for k = 1:numel(files)
    fname = files(k).name;
    fpath = fullfile(input_folder, fname);

    % 加载
    S = load(fpath);
    if ~isfield(S, 'data_baseline')
        warning('%s 中找不到 data_baseline，跳过', fname);
        continue;
    end

    T = S.data_baseline; % TABLE

    % ---- 仅保留 x 和 ybc 两列 ----
    T2 = T(:, [1 4]);
    T2.Properties.VariableNames = {'x', 'ybc'};

    % ---- 单段裁剪 ----
    idx1 = segment_bounds(1);
    idx2 = segment_bounds(2);

    if height(T2) < idx2
        warning('%s 行数不足，跳过', fname);
        continue;
    end

    segT = T2(idx1:idx2, :);   % TABLE 格式

    % 输出文件名：原名 + _seg1.mat
    [~, baseName] = fileparts(fname);
    outName = sprintf('%s_seg1.mat', baseName);

    % 保存
    save(fullfile(out_folder, outName), 'segT');

    fprintf('完成：%s\n', fname);
end

fprintf('\n全部处理完成！输出目录：%s\n', out_folder);
