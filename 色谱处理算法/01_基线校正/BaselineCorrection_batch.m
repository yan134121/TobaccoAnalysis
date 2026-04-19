% =========================================================================
% baselineCorrectionBatch.m
% 功能：批量对平行样品色谱数据进行基线校正
% 
% 主要功能：
% 1. 自动扫描parallel_samples文件夹加载所有MAT文件
% 2. 提取每个文件中的表格数据（包含x和y列）
% 3. 使用airPLS算法进行基线校正
% 4. 保存校正后的数据和基线信息
% 5. 生成包含原始数据、基线和校正后数据的表格
%
% 输入：
%   - parallel_samples 文件夹中的.mat文件
%   - 基线校正参数
%
% 输出：
%   - baseline_results_XXX 文件夹中的.mat文件
%   - 每个文件包含一个table，变量名为 'x', 'y', 'yb', 'ybc'
%   - 文件命名格式：原文件名_baseline.mat
%   - 加载到工作区的变量名统一为: data_baseline
%
% 使用示例：
%   调整 lambda, order, wep, p, itermax 参数来优化基线校正效果
% =========================================================================
clear; clc;
close all;

%% ==========用户输入区域==================
% 用户输入批次号
batchNumber = '0000_5'; % 用户输入批次样品的原始提取数据

% 基线校正参数
lambda = 1E5;     % 平滑参数,默认为10的5次方，默认
order = 2;        % 差分阶数，默认
wep = 0;          % 权重端点，默认
p = 0.05;         % 不对称参数，默认
itermax = 50;     % 最大迭代次数，默认

%% ==========创建结果目录==================
% 创建包含lambda参数的结果目录
result_dir = sprintf('BaseCorrect_%s', batchNumber);

if ~exist(result_dir, 'dir')
    mkdir(result_dir);
    fprintf('创建结果目录: %s\n', result_dir);
end

%% ==========获取文件列表==================
source_dir = sprintf('%s', batchNumber);
if ~exist(source_dir, 'dir')
    error('源文件夹不存在: %s', source_dir);
end

% 获取所有.mat文件
mat_files = dir(fullfile(source_dir, '*.mat'));
if isempty(mat_files)
    error('在文件夹 %s 中未找到MAT文件', source_dir);
end

fprintf('找到 %d 个MAT文件\n', length(mat_files));

%% ==========批量处理循环==================
for file_idx = 1:length(mat_files)
    file_name = mat_files(file_idx).name;
    file_path = fullfile(source_dir, file_name);
    
    fprintf('正在处理文件 %d/%d: %s...\n', file_idx, length(mat_files), file_name);
    
    %% ==========数据加载==================
    % 加载原始色谱数据文件
    try
        data = load(file_path);
    catch ME
        warning('无法加载文件 %s: %s，跳过处理', file_name, ME.message);
        continue;
    end
    
    % 获取变量名
    var_names = fieldnames(data);
    if isempty(var_names)
        warning('文件 %s 中没有变量，跳过处理', file_name);
        continue;
    end
    
    % 获取主要变量（应该是表格）
    main_var_name = var_names{1};
    data_table = data.(main_var_name);
    
    % 验证数据格式
    if ~istable(data_table)
        warning('文件 %s 中的数据不是表格格式，跳过处理', file_name);
        continue;
    end
    
    % 检查是否包含x和y列
    if ~all(ismember({'x', 'y'}, data_table.Properties.VariableNames))
        warning('文件 %s 的表格不包含x和y列，跳过处理', file_name);
        continue;
    end
    
    % 提取数据
    t = data_table.x;
    y = data_table.y;
    
    % 确保y是行向量（airPLS函数要求）
    y = y(:)';
    
    %% ==========基线校正处理==================
    try
        % 调用基线校正函数
        [ybc, yb, dssn] = airPLS(y, lambda, order, wep, p, itermax);
        
        % 检查结果有效性
        if any(isnan(yb)) || any(isnan(ybc))
            warning('基线校正返回了NaN值，跳过处理');
            continue;
        end
        
        % 确保基线和校正后数据是列向量
        yb = yb(:);
        ybc = ybc(:);
        
    catch ME
        warning('基线校正失败: %s，跳过处理', M.message);
        continue;
    end
    
    %% ==========创建结果表格==================
    % 创建结果表格
    result_table = table(t, y', yb, ybc, ...
        'VariableNames', {'x', 'y', 'yb', 'ybc'});
    
    % 从文件名中提取基础名称（去掉扩展名）
    [~, base_name, ~] = fileparts(file_name);
    
    % 统一使用 data_baseline 作为变量名
    data_baseline = result_table;
    
    %% ==========直接保存结果==================
    save_name = fullfile(result_dir, [base_name '_baseline.mat']); 
    save(save_name, 'data_baseline');

    %% ==========结果显示==================
    fprintf('成功处理文件 %s\n', file_name);
    fprintf('  结果保存至: %s\n', save_name);
    fprintf('  变量名: data_baseline\n\n');

end

fprintf('===== 批量处理完成 =====\n');
fprintf('批次号: %s\n', batchNumber);
fprintf('所有结果已保存至目录: %s\n', result_dir);
fprintf('处理文件总数: %d\n', length(mat_files));
fprintf('所有结果文件加载到工作区的变量名统一为: data_baseline\n');