%% ========================================================================
% 将不同基准样品对齐结果整理到一个新文件夹
% 子文件夹命名为基准样品编号
% 每个子文件夹包含：
%   1. 基准样品自身对齐文件（从 All_base_aligned 提取）
%   2. 以该基准样品为参考的所有 aligned 文件（重复文件只保留一个）
% ========================================================================
%% ========================================================================
% 功能解析（Function Overview）
%
% 本脚本用于将“不同基准样品对齐结果”按基准样品重新整理成结构化目录。
% 在批量色谱对齐流程中，每个样品会被分别以不同的基准样品作为参考，
% 因此会生成多个文件夹：
%       Different_aligned_results_001
%       Different_aligned_results_002
%       ...
% 每个文件夹内部又包含多个子文件夹，每个子文件夹中存放若干：
%       Sepu_XXXX_aligned.mat
% 这些文件即为色谱对齐后的结果（通常包含 x 与 aligned 信号）。
%
% 【脚本目标】
% 将所有对齐结果按“基准样品编号”重新归类，生成结构如下：
%
%   All_aligned_by_ref/
%       ├── 001/
%       │     ├── Sepu_001_aligned.mat          ← 基准样品自身对齐文件
%       │     ├── Sepu_002_aligned.mat
%       │     ├── Sepu_003_aligned.mat
%       │     └── ...
%       ├── 002/
%       └── ...
%
% 【主要功能】
%   1）遍历 source_root 下所有 "Different_aligned_results_*" 文件夹
%   2）提取基准样品编号（如 001、002、003）
%   3）为每个基准样品创建独立子文件夹
%   4）从 All_base_aligned 中复制该基准样品自身的对齐文件
%   5）收集所有以该基准样品为参考的 aligned 文件
%   6）对重复文件名进行去重（同名文件只保留一个）
%
% 【输入目录结构示例】
% 588个样品对齐结果/
%   ├── Different_aligned_results_001/
%   │       ├── Sepu_002/
%   │       │       ├── Sepu_002_aligned.mat
%   │       ├── Sepu_003/
%   │       │       ├── Sepu_003_aligned.mat
%   │       └── ...
%   ├── Different_aligned_results_002/
%   └── ...
%
% 【输出目录结构示例】
% All_aligned_by_ref/
%   ├── 001/
%   │       ├── Sepu_001_aligned.mat
%   │       ├── Sepu_002_aligned.mat
%   │       ├── Sepu_003_aligned.mat
%   │       └── ...
%   ├── 002/
%   └── ...
%
% 【注意事项】
%   - 若基准样品自身对齐文件不存在，将给出 warning。
%   - 若不同子文件夹中出现同名 aligned 文件，仅保留第一个。
%   - 输出结构便于后续按基准样品进行对齐质量评估或进一步分析。
%
% ========================================================================


clc; close all;

%% ================= 用户输入 =================
source_root = '599个样品对齐结果';   % 原批量对齐结果文件夹
base_aligned_folder = 'All_599_base_aligned'; % 基准样品自身对齐数据
target_root = 'All_599_aligned_by_ref';      % 整理后的总文件夹

if ~exist(target_root,'dir')
    mkdir(target_root);
end

%% ================= 遍历每个基准样品文件夹 =================
dir_info = dir(source_root);
is_sub = [dir_info.isdir] & ~ismember({dir_info.name},{'.','..'});
sub_folders = dir_info(is_sub);

fprintf('共检测到 %d 个参考样品结果文件夹\n\n', numel(sub_folders));

for i = 1:numel(sub_folders)
    % 基准样品编号
    ref_name = erase(sub_folders(i).name,'Different_aligned_results_');
    fprintf('处理基准样品: %s\n', ref_name);
    
    % 新子文件夹
    target_subfolder = fullfile(target_root, ref_name);
    if ~exist(target_subfolder,'dir')
        mkdir(target_subfolder);
    end
    
    %% ---------- 1. 拷贝基准样品自身 aligned 文件 ----------
    base_file = fullfile(base_aligned_folder, sprintf('Sepu_%s_aligned.mat', ref_name));
    if exist(base_file,'file')
        dest_file = fullfile(target_subfolder, sprintf('Sepu_%s_aligned.mat', ref_name));
        if ~exist(dest_file,'file')  % 不存在才拷贝
            copyfile(base_file, dest_file);
        end
        fprintf('  基准样品自身文件已拷贝\n');
    else
        warning('  基准样品自身文件不存在: %s\n', base_file);
    end
    
    %% ---------- 2. 拷贝该基准样品下的所有 aligned 文件 ----------
    main_folder = fullfile(source_root, sub_folders(i).name);
    sub_dir_info = dir(main_folder);
    is_sub2 = [sub_dir_info.isdir] & ~ismember({sub_dir_info.name},{'.','..'});
    sub_subfolders = sub_dir_info(is_sub2);
    
    file_count = 0;
    copied_files = containers.Map();  % 记录已拷贝的文件名
    
    for j = 1:numel(sub_subfolders)
        src_folder = fullfile(main_folder, sub_subfolders(j).name);
        mat_files = dir(fullfile(src_folder, 'Sepu_*_aligned.mat'));
        
        for k = 1:numel(mat_files)
            src_file = fullfile(src_folder, mat_files(k).name);
            dest_file = fullfile(target_subfolder, mat_files(k).name);
            
            % 如果已经拷贝过同名文件，跳过
            if isKey(copied_files, mat_files(k).name)
                continue;
            end
            
            copyfile(src_file, dest_file);
            copied_files(mat_files(k).name) = true;
            file_count = file_count + 1;
        end
    end
    fprintf('  拷贝 %d 个 aligned 文件完成（重复文件已去重）\n\n', file_count);
end

fprintf('=== 所有基准样品整理完成 ===\n');
