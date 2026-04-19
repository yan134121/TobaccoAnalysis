%% ========================================================================
% 批量提取对齐后的 MAT 文件
% 规则：
%   - 源文件夹 = 各个 "Different_aligned_results_*" 子文件夹
%   - 文件名匹配 = 'Sepu_*_aligned.mat'
%   - 目标文件夹 = 单一文件夹
% ========================================================================
%% ========================================================================
% 功能解析（Function Overview）
%
% 本脚本用于批量提取所有“不同基准样品对齐结果”中的色谱对齐文件，
% 将所有出现的 `Sepu_*_aligned.mat` 文件统一收集到一个目标文件夹中。
%
% 【处理背景】
% 在批量色谱对齐流程中，每个样品会被分别以不同的基准样品作为参考，
% 因此会生成多个文件夹：
%       Different_aligned_results_001
%       Different_aligned_results_002
%       ...
% 每个文件夹内部又包含多个子文件夹，每个子文件夹中存放若干：
%       Sepu_XXXX_aligned.mat
% 这些文件即为色谱对齐后的结果（通常包含 x 与 aligned 信号）。
%
% 【主要功能】
%   1）遍历 source_root 下所有 "Different_aligned_results_*" 文件夹
%   2）进入每个参考样品文件夹内部的子文件夹
%   3）查找所有符合命名规则的对齐文件：
%          Sepu_*_aligned.mat
%   4）将所有找到的对齐文件复制到统一的目标文件夹 target_folder
%
% 【输出结构】
% target_folder/
%   ├── Sepu_0001_aligned.mat
%   ├── Sepu_0002_aligned.mat
%   ├── Sepu_0003_aligned.mat
%   └── ...
%
% 【注意事项】
%   - 本脚本仅做“文件收集”，不修改 MAT 文件内容
%   - 若不同参考样品下出现同名文件，后复制的会覆盖先前的
%   - 适用于后续统一分析、统计或进一步筛选对齐结果
%
% ========================================================================

clc; close all;

%% ================= 用户输入 =================
source_root = '599个样品对齐结果';   % 所有不同参考样品结果的顶层文件夹
target_folder = 'All_aligned_MAT';    % 存放提取文件的目标文件夹

if ~exist(target_folder,'dir')
    mkdir(target_folder);
end

%% ================= 遍历所有参考样品文件夹 =================
dir_info = dir(source_root);
is_sub = [dir_info.isdir] & ~ismember({dir_info.name},{'.','..'});
sub_folders = dir_info(is_sub);

fprintf('共检测到 %d 个参考样品结果文件夹\n\n', numel(sub_folders));

file_count = 0;

for i = 1:numel(sub_folders)
    main_folder = fullfile(source_root, sub_folders(i).name);
    
    % 再遍历该参考样品下的子文件夹（通常是 Sepu_XXX_jizhun）
    sub_dir_info = dir(main_folder);
    is_sub2 = [sub_dir_info.isdir] & ~ismember({sub_dir_info.name},{'.','..'});
    sub_subfolders = sub_dir_info(is_sub2);
    
    for j = 1:numel(sub_subfolders)
        src_folder = fullfile(main_folder, sub_subfolders(j).name);
        mat_files = dir(fullfile(src_folder, 'Sepu_*_aligned.mat'));
        
        for k = 1:numel(mat_files)
            src_file = fullfile(src_folder, mat_files(k).name);
            dest_file = fullfile(target_folder, mat_files(k).name);
            
            copyfile(src_file, dest_file);
            file_count = file_count + 1;
        end
    end
end

fprintf('共提取 %d 个对齐 MAT 文件到文件夹: %s\n', file_count, target_folder);
