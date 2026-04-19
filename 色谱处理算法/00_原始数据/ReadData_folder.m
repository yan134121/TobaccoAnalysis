% =========================================================================
% 📊 批量样品 CSV 数据预处理与标准化保存脚本
% =========================================================================
% 🧭 功能说明：
% 本脚本用于批量读取指定父文件夹下的多个样品子文件夹中的 CSV 数据，
% 自动识别样品类型（标定样或普通样），统一截断为最短长度，处理缺失值，
% 并保存为命名规范的 `.mat` 文件，便于后续分析与调用。

% 📂 输入结构：
% - parent：父文件夹路径（如 '第六批1'），包含多个子文件夹，每个子文件夹对应一个样品
% - 每个子文件夹内应包含一个 CSV 文件，格式为两列（x, y）

% 🧪 样品命名规则：
% - 标定样：文件夹名包含“标定样”，编号提取为 Sepu_0_编号
% - 普通样：文件夹名包含“数字-数字”，编号提取为 Sepu_编号（将 - 替换为 _）

% ⚙️ 处理流程：
% 1️⃣ 第一次遍历所有样品，确定最短数据长度（用于统一截断）
% 2️⃣ 第二次遍历，读取 CSV 数据，处理 NaN 缺失值（线性插值），截断为统一长度
% 3️⃣ 构造 Table 并保存为命名规范的 `.mat` 文件（变量名与文件名一致）

% 💾 输出结果：
% - 所有样品数据保存至 outputFolder（如 'parallel_samples_batch6_1'）
% - 每个样品保存为 Sepu_编号.mat，内容为 Table(x, y) + Excel 文件

% ⚠️ 注意事项：
% - 若样品数据存在 NaN，将自动进行线性插值并提示
% - 若子文件夹中无 CSV 文件或命名不符合规则，将自动跳过
% - 使用 eval 动态变量名保存，确保变量名与文件名一致

% ✨ 作者：笑莲
% 📅 日期：2025年
% =========================================================================
clear; clc;

%% ⏱️ 开始计时
tic;

%% 设置路径
parent = '0000_100_文件夹数据';
outputFolder = '0000_100';

if ~exist(outputFolder, 'dir')
    mkdir(outputFolder);
end

%% 获取所有子文件夹
subFolders = dir(parent);
subFolders = subFolders([subFolders.isdir] & ~startsWith({subFolders.name}, '.'));

%% 初始化统一长度与计数器
minLength = inf;
totalSamples = 0; % 📦 有效样品总数（含 CSV 文件）
sampleCount = 0;  % ✅ 成功处理样品数

%% 第一次遍历：确定最短数据长度
for i = 1:length(subFolders)
    subPath = fullfile(parent, subFolders(i).name);
    csvFiles = dir(fullfile(subPath, '*.csv'));
    if isempty(csvFiles), continue; end
    totalSamples = totalSamples + 1;
    data = readmatrix(fullfile(subPath, csvFiles(1).name));
    if size(data, 1) < minLength
        minLength = size(data, 1);
    end
end

%% 第二次遍历：提取数据并保存为 Table
for i = 1:length(subFolders)
    subName = subFolders(i).name;
    subPath = fullfile(parent, subName);
    
    % 判断样品类型
    isCalibration = contains(subName, '标定样');
    skipSample = false;
    varName = '';

    if isCalibration
        token = regexp(subName, '标定样(\d+)', 'tokens');
        if isempty(token)
            skipSample = true;
        else
            id = token{1}{1};
            varName = sprintf('Sepu_0000_%s', id);
        end

    elseif contains(subName, '配方')
    token = regexp(subName, '配方(\d+)-(\d+)', 'tokens');
    if isempty(token)
        skipSample = true;
    else
        nums = token{1};
        varName = sprintf('Sepu_pf_%s_%s', nums{1}, nums{2});
    end

    elseif contains(subName, '膨梗一类')
        token = regexp(subName, '膨梗一类(\d+)', 'tokens');
        if isempty(token)
            skipSample = true;
        else
            id = token{1}{1};
            varName = sprintf('Sepu_pgyl_%s', id);
        end
    elseif contains(subName, '薄片粉')
        token = regexp(subName, '薄片粉(\d+)', 'tokens');
        if isempty(token)
            skipSample = true;
        else
            id = token{1}{1};
            varName = sprintf('Sepu_bpf_%s', id);
        end
    else
        token = regexp(subName, '(\d+)-(\d+)(?:-(\d+))?', 'tokens');
        if isempty(token)
            skipSample = true;
        else
            nums = token{1};
            fullNum = nums{1};
            if length(fullNum) > 4
                nums = [{fullNum(1:4)}, {fullNum(5:end)}, nums(2:end)];
            end
            varName = ['Sepu_', strjoin(nums, '_')];
        end
    end

    if skipSample
        fprintf('⏭️ 样品 %s 命名不符合规则，已跳过。\n', subName);
        continue;
    end

    fileName = fullfile(outputFolder, [varName, '.mat']);
    
    % 查找 CSV 文件
    csvFiles = dir(fullfile(subPath, '*.csv'));
    if isempty(csvFiles), continue; end
    data = readmatrix(fullfile(subPath, csvFiles(1).name));
    
    % 检查并插值 NaN
    for col = 1:2
        if any(isnan(data(:,col)))
            xvec = 1:size(data,1);
            nanIdx = isnan(data(:,col));
            data(nanIdx,col) = interp1(xvec(~nanIdx), data(~nanIdx,col), xvec(nanIdx), 'linear', 'extrap');
            colName = ["x", "y"];
            fprintf('⚠️ 样品 %s 的列 %s 存在 NaN，已进行线性插值处理。\n', subName, colName(col));
        end
    end

    % 截断并构造 Table
    data = data(1:minLength, 1:2);
    T = array2table(data, 'VariableNames', {'x', 'y'});
    
    % 使用动态变量名保存
    eval([varName ' = T;']);
    save(fileName, varName);

    % 成功计数
    sampleCount = sampleCount + 1;

    fprintf('✅ 样品 %s 已处理完成并保存为 %s，数据长度为 %d。\n', ...
            subName, [varName, '.mat'], height(T));
end

%% ⏱️ 结束提示
elapsedTime = toc;
fprintf('✅ 共发现 %d 个有效样品文件夹，成功处理并保存 %d 个样品。总耗时：%.2f 秒。\n', ...
        totalSamples, sampleCount, elapsedTime);
