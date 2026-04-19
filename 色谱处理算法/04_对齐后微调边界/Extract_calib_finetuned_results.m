%% ========================================================================
% 提取所有标定样品微调后的数据到单独文件夹
% ========================================================================
%% ========================================================================
% 提取所有标定样品微调后的数据到单独文件夹
%
% 【功能概述】
% 本脚本用于从总的微调结果文件夹（All_599_finetuned_results）中，
% 自动筛选出所有“标定样品”的微调结果文件（SampleData_0000_*.mat），
% 并将它们统一复制到一个新的文件夹（All_599_finetuned_results_calib）。
%
% 标定样品的识别方式：
%   - 文件名格式为：SampleData_0000_*.mat
%   - 其中 0000_xxx 为标定样品编号
%
% 【主要流程】
% 1. 扫描 input_root 下的所有基准样品子文件夹
% 2. 在每个子文件夹中查找所有 SampleData_0000_*.mat 文件
% 3. 将这些文件复制到 output_root
% 4. 自动避免重复复制（通过文件名去重）
%
% 【输入】
% - input_root：包含所有基准样品子文件夹的总目录
% - output_root：用于存放所有标定样品微调结果的目标目录
%
% 【输出】
% - output_root/SampleData_0000_xxx.mat
%   （所有标定样品的微调结果文件）
%
% 【特点】
% - 自动识别标定样品文件，无需手动指定
% - 自动创建输出目录
% - 自动避免重复复制
% - 适用于任意数量的基准样品子文件夹
%
% 【扩展性】
% - 可修改文件名模式以适配不同标定样编号规则
% - 可扩展为复制更多类型的文件（如 PNG、CSV）
% - 可加入日志系统记录复制详情
%
% 作者：笑莲
% ========================================================================


clear; clc;

%% ================= 用户设置 =================
input_root  = 'All_599_finetuned_results';  % 总输出文件夹
output_root = 'All_599_finetuned_results_calib';       % 新建文件夹存放所有标定样品
if ~exist(output_root,'dir')
    mkdir(output_root);
end

%% ================= 扫描所有基准样品子文件夹 =================
ref_folders = dir(input_root);
is_sub = [ref_folders.isdir] & ~ismember({ref_folders.name},{'.','..'});
ref_folders = ref_folders(is_sub);

fprintf('共检测到 %d 个基准样品子文件夹\n', numel(ref_folders));

copied_files = {};

%% ================= 遍历子文件夹 =================
for i_ref = 1:numel(ref_folders)
    sub_name = ref_folders(i_ref).name;
    sub_folder = fullfile(input_root, sub_name);
    
    %% ========== 找出标定样品 ========== 
    files = dir(fullfile(sub_folder, 'SampleData_0000_*.mat'));
    
    for i_file = 1:numel(files)
        src_file = fullfile(files(i_file).folder, files(i_file).name);
        dest_file = fullfile(output_root, files(i_file).name);
        
        % 避免重复复制
        if ~ismember(files(i_file).name, copied_files)
            copyfile(src_file, dest_file);
            copied_files{end+1} = files(i_file).name;
            fprintf('复制 %s → %s\n', files(i_file).name, output_root);
        end
    end
end

fprintf('\n✔ 所有标定样品已提取完成，总数 %d 个\n', numel(copied_files));
