%% ==================== 统一数据处理主程序（大热重 + 小热重 合并版） ====================
% 功能：统一处理三类数据：
%   A. 卷烟工序（工序前/中/后 + 工序间分析）
%   B. 原料（组间差异排序、两两差异矩阵）
%   C. 小热重（简化版：无分段评估与权重优化）
%
% 运行时会询问数据类型并进入相应分支。
clc; close all; clearvars -except; fprintf('统一大/小热重数据处理程序启动...\n\n');
tic;

%% ========== 数据类型选择 ==========
fprintf('===== 请选择数据类型 =====\n');
% fprintf(' 1. 新批次大热重工序数据                                     （流程 A - 工序分析）\n');
fprintf(' 1. 旧批次大热重工序数据                                       （流程 A - 工序分析）\n');
fprintf(' 2. 原料大热重数据采集                                            （流程 B - 原料分析）\n');
fprintf(' 3. 小热重数据（单文件 Excel, 去除分段评估与权重优化）          （流程 C - 小热重）\n');
dataType = input('请选择数据类型 (1-3, 默认 1): ');
if isempty(dataType), dataType = 1; end

%% ========== 通用可调参数（可根据需要修改） ==========
% 非数值项允许的最大比例（例如 0.15 表示最多允许 15% 非数值单元格）
INVALID_TOKEN_FRACTION = 0.2;
% 可视化参数
plotNumber = 5;            % 默认显示前N组
nBestSamplesToPlot = 5;
% 离群点检测
OUTLIER_WINDOW = 21; OUTLIER_NSIGMA = 4;
JUMP_DIFF_THRESHOLD = 0.1;
GLOBAL_DEVIATION_NSIGMA = 2;

% 预设权重（通用）
PRESET_WEIGHTS = struct(...
    'balanced', struct('name','平衡模式','config',struct('NRMSE',1,'Pearson',0,'Euclidean',0)), ...
    'precision', struct('name','精确模式','config',struct('NRMSE',0,'Pearson',1,'Euclidean',0)), ...
    'robustness', struct('name','鲁棒模式','config',struct('NRMSE',0,'Pearson',0,'Euclidean',1)), ...
    'correlation', struct('name','全部加权','config',struct('NRMSE',1,'Pearson',1,'Euclidean',1)));

%% ========== 针对数据类型的初始设置 ==========
switch dataType
    case 1
        % rawDataFolder = '新批次大热重工序数据/'; % 流程 A
        rawDataFolder = '旧批次大热重工序数据/'; % 流程 A
        isType1 = true; isTypeSmall = false;
        fprintf('已选择 流程 A（工序分析），数据目录: %s\n\n', rawDataFolder);
    case 2
        rawDataFolder = '原料大热重数据采集'; % 流程 B
        isType1 = false; isTypeSmall = false;
        fprintf('已选择 流程 B（原料分析），数据目录: %s\n\n', rawDataFolder);
    case 3
        % 小热重现在默认放在当前工作目录下的 "小热重数据" 文件夹中
        rawDataFolder = '小热重数据';
        isType1 = false; isTypeSmall = true;
        fprintf('已选择 流程 C（小热重），数据目录（默认）: %s\n\n', rawDataFolder);
    otherwise
        warning('无效选择，默认使用流程 A');
        rawDataFolder = '20251021-1906-202509-(77-102-113-120-127-146-162-170-178）/';
        isType1 = true; isTypeSmall = false;
end

%% ========== 差异比较区间选择（仅大热重分支有意义） ==========
COMPARE_WINDOW = 'all';
if ~isTypeSmall
    fprintf('===== 差异比较区间选择 =====\n');
    fprintf('可选区间:\n 1. 20-95\n 2. 95-280\n 3. 0-280\n 4. 全部\n 5. 自定义\n');
    sel = input('请选择比较区间 (1-5, 默认 4): ');
    if isempty(sel), sel = 4; end
    switch sel
        case 1, COMPARE_WINDOW = [20,95];
        case 2, COMPARE_WINDOW = [95,280];
        case 3, COMPARE_WINDOW = [0,280];
        case 4, COMPARE_WINDOW = 'all';
        case 5
            s = input('请输入起始索引 (整数, 从1开始): ');
            e = input('请输入结束索引 (整数): ');
            COMPARE_WINDOW = [s, e];
        otherwise
            warning('无效选择，使用全部区间');
            COMPARE_WINDOW = 'all';
    end
end

%% ========== 权重管理（交互式） ==========
fprintf('\n===== 权重配置选择 =====\n');
presetNames = fieldnames(PRESET_WEIGHTS);
for i = 1:numel(presetNames)
    p = PRESET_WEIGHTS.(presetNames{i});
    fprintf('%d. %s: NRMSE=%.1f, Pearson=%.1f, Euclidean=%.1f\n', i, p.name, p.config.NRMSE, p.config.Pearson, p.config.Euclidean);
end
fprintf('%d. 自定义\n', numel(presetNames)+1);
choice = input(sprintf('请选择权重配置 (1-%d, 默认 1): ', numel(presetNames)+1));
if isempty(choice), choice = 1; end

if choice > 0 && choice <= numel(presetNames)
    GLOBAL_METRICS_CONFIG = PRESET_WEIGHTS.(presetNames{choice}).config;
    fprintf('已选择预设权重: %s\n', PRESET_WEIGHTS.(presetNames{choice}).name);
elseif choice == numel(presetNames)+1
    fprintf('===== 自定义权重配置 =====\n');
    nrmse = input('请输入NRMSE权重: '); pearson = input('请输入Pearson权重: '); euclidean = input('请输入Euclidean权重: ');
    total = nrmse + pearson + euclidean;
    if total <= 0
        warning('总权重为0，使用默认平衡权重');
        GLOBAL_METRICS_CONFIG = PRESET_WEIGHTS.balanced.config;
    else
        GLOBAL_METRICS_CONFIG = struct('NRMSE', nrmse/total, 'Pearson', pearson/total, 'Euclidean', euclidean/total);
    end
    disp(GLOBAL_METRICS_CONFIG);
else
    warning('无效选择，使用默认平衡权重');
    GLOBAL_METRICS_CONFIG = PRESET_WEIGHTS.balanced.config;
end

%% ==================== 主流程：大热重（A/B）或小热重（C） ====================
if isTypeSmall
    % ================= 小热重 分支（流程 C） =================
    fprintf('\n===== 流程 C：小热重数据处理（去除分段评估/权重优化） =====\n');
    try
        % --- 新逻辑：在指定文件夹中查找多个表格文件（.xlsx .xls .csv）并逐个导入 ---
        folderPath = fullfile(pwd, rawDataFolder); % rawDataFolder 已在 switch 中设为 '小热重数据'
        if ~isfolder(folderPath)
            error('小热重数据文件夹不存在: %s', folderPath);
        end

        % 支持的扩展名
        exts = {'*.xlsx','*.xls','*.csv'};
        fileList = [];
        for k = 1:numel(exts)
            D = dir(fullfile(folderPath, exts{k}));
            if ~isempty(D)
                fileList = [fileList; D]; %#ok<AGROW>
            end
        end

        if isempty(fileList)
            error('在文件夹 %s 中未找到任何 Excel/CSV 文件。', folderPath);
        end

        % 逐文件调用 importTGData，并把结果合并到 S_all（保持 importTGData 的内部数据提取逻辑不变）
        S_all = [];
        totalFiles = numel(fileList);
        fprintf('发现 %d 个文件，开始逐个导入...\n', totalFiles);
        for f = 1:totalFiles
            fname = fileList(f).name;
            fpath = fullfile(folderPath, fname);
            try
                S_tmp = importTGData(fpath); % 你已有的函数：应返回 struct 数组（prefix, col1, col3, ...）
                if isempty(S_tmp)
                    warning('文件 %s 导入后为空，跳过。', fname);
                    continue;
                end
                % 有时 importTGData 对 prefix 处理可能缺失，确保 prefix 存在并包含文件/表识
                for ii = 1:numel(S_tmp)
                    if ~isfield(S_tmp(ii),'prefix') || isempty(S_tmp(ii).prefix)
                        % 以文件名去除扩展名作为前缀标识，若S_tmp含多个 sheet，可附加序号
                        [~, nm, ~] = fileparts(fname);
                        if numel(S_tmp) > 1
                            S_tmp(ii).prefix = sprintf('%s_sheet%d', nm, ii);
                        else
                            S_tmp(ii).prefix = nm;
                        end
                    end
                end

                % 合并
                if isempty(S_all)
                    S_all = S_tmp;
                else
                    S_all = [S_all; S_tmp]; %#ok<AGROW>
                end

                fprintf('已导入 (%d/%d): %s （包含 %d 个样本）\n', f, totalFiles, fname, numel(S_tmp));
            catch ME
                warning('导入文件 %s 失败：%s，已跳过。', fname, ME.message);
            end
        end

        if isempty(S_all)
            error('所有文件均导入失败或未包含有效样本。');
        end

        S = S_all; % 与原小热重逻辑兼容，后续全部使用 S
        fprintf('成功导入 %d 组小热重数据（共来自 %d 个文件）\n', numel(S), totalFiles);

    catch ME
        error('小热重数据导入失败: %s', ME.message);
    end

    % 使用已有权重 GLOBAL_METRICS_CONFIG 或局部变量
    metricsConfig = GLOBAL_METRICS_CONFIG;

    % 两两差异度（小热重的专用比较函数）
    fprintf('正在计算小热重两两差异度...\n');
    try
        [diffMatrix, pairwiseTable, pairwiseDetails_small] = compareAllPairwiseSmall(S, metricsConfig);
    catch ME
        error('compareAllPairwiseSmall 调用失败: %s', ME.message);
    end

    % 保存两个表
    try
        writetable(pairwiseTable, '小热重_两两差异度矩阵.xlsx', 'WriteRowNames', true);
        writetable(pairwiseDetails_small, '小热重_两两差异度明细.xlsx');
        fprintf('已保存小热重两两差异结果（Excel）。\n');
    catch
        warning('写入小热重 Excel 失败。');
    end

    % 计算 summary 排序（与之前小热重脚本相同逻辑）
    G = size(diffMatrix,1);
    meanDiff = nan(G,1);
    for i = 1:G, meanDiff(i) = nanmean(diffMatrix(i,:)); end
    if all(isnan(meanDiff)) || nanmax(meanDiff)-nanmin(meanDiff) < eps
        simScore = 0.5 * ones(G,1);
    else
        normMeanDiff = (meanDiff - nanmin(meanDiff)) ./ (nanmax(meanDiff) - nanmin(meanDiff) + eps);
        simScore = 1 - normMeanDiff;
    end
    prefixes = pairwiseTable.Properties.RowNames;
    summaryT = table((1:G)', prefixes, simScore, 'VariableNames', {'GroupIndex','Prefix','Score'});
    [~, sidx] = sort(summaryT.Score, 'descend');
    summaryT = summaryT(sidx, :);
    summaryT.Rank = (1:height(summaryT))';

    % 导出全局排序
    try
        writetable(summarizeTableWithChineseHeaders(summaryT), '小热重全局排序.xlsx');
        fprintf('小热重全局排序已保存。\n');
    catch
        writetable(summaryT, '小热重全局排序.csv');
        fprintf('保存为 CSV：小热重全局排序.csv\n');
    end

    % % 可视化（前 N）
    % FigsnNum = 10;
    % try
    %     % 计算 minLen 用于绘图（与之前脚本一致）
    %     allLens = arrayfun(@(x) min(numel(x.col1), numel(x.col3)), S);
    %     minLen = min(allLens);
    %     if isempty(minLen) || minLen < 2, error('minLen 无效'); end
    % 
    %     topK = min(FigsnNum, height(summaryT));
    %     fig = figure('Name','小热重 DTG 比较','Position',[100,100,1000,600]); ax = axes(fig); hold(ax,'on');
    %     colours = lines(topK);
    %     legends = cell(topK,1);
    %     for i = 1:topK
    %         idx = summaryT.GroupIndex(i);
    %         len = min([minLen, numel(S(idx).col1), numel(S(idx).col3)]);
    %         if len < 3, continue; end
    %         plot(ax, S(idx).col1(1:len), S(idx).col3(1:len), 'LineWidth',1.6);
    %         legends{i} = sprintf('[%d] %s (%.4f)', i, summaryT.Prefix{i}, summaryT.Score(i));
    %     end
    %     grid(ax,'on'); xlabel(ax,'Temperature'); ylabel(ax,'DTG'); title(ax,'小热重排名前样本 DTG 曲线');
    %     legend(ax, legends(~cellfun(@isempty,legends)),'Location','bestoutside');
    %     hold(ax,'off');
    % catch
    %     warning('小热重可视化失败或数据不足。');
    % end

    % 可选：导出前后数据
    exportPrePost = input('是否导出每个样本处理前后数据到同一Excel? (y/n, 默认 n): ','s');
    if isempty(exportPrePost), exportPrePost = 'n'; end
    if strcmpi(exportPrePost,'y')
        try
            processedP = computeProcessedDTG(S, 'sgolay', 11);
            savePrePostExcel(S, processedP, summaryT, 0, '小热重_前后数据导出.xlsx');
            fprintf('已导出小热重前后数据。\n');
        catch ME
            warning('导出小热重前后数据失败');
        end
    end

else
    % ================= 大热重 分支（流程 A 或 B） =================
    fprintf('\n===== 大热重数据准备（分组） =====\n');
    try
        [~, keyMap] = groupCSVFiles(rawDataFolder);
        fprintf('[SUCCESS] 已成功加载 %d 个分组数据\n', keyMap.Count);
    catch ME
        error('groupCSVFiles 调用失败: %s\n路径: %s', ME.message, rawDataFolder);
    end

    % 分组处理
    prefixes = keyMap.keys(); nGroups = numel(prefixes);
    groupBests = cell(1, nGroups); isValidGroup = false(1, nGroups);
    groupInfo = repmat(struct('prefix','', 'fileCount',0, 'sgWindow',0, 'sgPoly',0), 1, nGroups);
    groupBestFilePaths = containers.Map(); groupProcessedData = cell(1, nGroups); validGroupCount = 0;
    fprintf('开始处理 %d 个分组...\n', nGroups);

    for groupIdx = 1:nGroups
        currentPrefix = prefixes{groupIdx};
        fileGroup = keyMap(currentPrefix);
        filePaths = {fileGroup.filePath}';
        if isempty(currentPrefix) || isempty(filePaths)
            fprintf('忽略无效组: 索引%d [%s]\n', groupIdx, currentPrefix);
            continue;
        end
        try
            [bestDTG, S, processed, bestFilePath] = processReplicateGroup(filePaths, ...
                'Metrics', GLOBAL_METRICS_CONFIG, ...
                'InvalidFractionThreshold', INVALID_TOKEN_FRACTION, ...
                'CompareWindow', COMPARE_WINDOW, ...
                'OutlierWindow', OUTLIER_WINDOW, ...
                'OutlierNSigma', OUTLIER_NSIGMA, ...
                'JumpDiffThreshold', JUMP_DIFF_THRESHOLD, ...
                'GlobalDeviationNSigma', GLOBAL_DEVIATION_NSIGMA);

            if isfield(processed,'sgParams') && ~isempty(processed.sgParams)
                if isfield(S,'selectedIndex') && ~isempty(S.selectedIndex)
                    winParam = processed.sgParams(S.selectedIndex).window;
                    polyParam = processed.sgParams(S.selectedIndex).poly;
                else
                    winParam = processed.sgParams(1).window;
                    polyParam = processed.sgParams(1).poly;
                end
            else
                winParam = 11; polyParam = 2;
            end

            validGroupCount = validGroupCount + 1;
            groupInfo(validGroupCount) = struct('prefix', currentPrefix, 'fileCount', numel(filePaths), 'sgWindow', winParam, 'sgPoly', polyParam);
            groupBests{validGroupCount} = bestDTG;
            isValidGroup(groupIdx) = true;
            groupBestFilePaths(currentPrefix) = bestFilePath;
            groupProcessedData{validGroupCount} = processed;
        catch ME
            fprintf('组 [%s] 处理失败：%s\n', currentPrefix, ME.message);
        end
    end

    if validGroupCount == 0
        error('处理终止：无有效分组 (共 %d 组)', nGroups);
    end
    groupBests = groupBests(1:validGroupCount);
    groupInfo = groupInfo(1:validGroupCount);
    fprintf('有效分组: %d/%d\n', validGroupCount, nGroups);

    if dataType == 2
        % ================ 流程 B：原料分析 ================
        fprintf('\n===== 流程 B：组间差异矩阵与排序（原料） =====\n');
        try
            [diffMatrix, pairwiseTable, pairwiseDetails_big] = compareAllPairwise(groupBests, groupInfo, GLOBAL_METRICS_CONFIG, groupBestFilePaths);
            writetable(pairwiseTable, '大热重原料两两差异度矩阵.xlsx', 'WriteRowNames', true);
            writetable(pairwiseDetails_big, '大热重原料两两差异度明细.xlsx');
            fprintf('大热重原料两两差异结果已保存。\n');
        catch ME
            warning('compareAllPairwise 调用失败');
        end

        % ===== 可选导出：用于计算的 DTG 数据（流程 B） =====
        exportDTG_choice = input('是否导出用于计算的 DTG 数据到 Excel? (y/n, 默认 n): ','s');
        if isempty(exportDTG_choice), exportDTG_choice = 'n'; end
        
        if strcmpi(exportDTG_choice,'y')
            dtgDataExportFile = '大热重差异度计算的DTG数据.xlsx';
            try
                fprintf('正在导出 DTG 数据到：%s ...\n', dtgDataExportFile);
                exportDtgDataToExcel(groupBests, groupInfo, dtgDataExportFile);
                fprintf('DTG 数据导出成功：%s\n', dtgDataExportFile);
            catch ME
                warning('导出 DTG 数据失败: （请检查 exportDtgDataToExcel 函数是否存在并接受相应参数）');
            end
        else
            fprintf('跳过 DTG 数据导出。\n');
        end


        % 组间整体排序（rankGroupBests_compareWindow）
        try
            [sortedRankTable, metricValues] = rankGroupBests_compareWindow(groupBests, groupInfo, groupIdx, GLOBAL_METRICS_CONFIG, plotNumber, COMPARE_WINDOW);
            try
                chineseRankTable = translateSortedRankTable(sortedRankTable);
                writetable(chineseRankTable, '大热重原料最佳样品差异排序表.xlsx');
            catch
                writetable(sortedRankTable, '大热重原料最佳样品差异排序表.xlsx');
            end
            fprintf('组间排序已保存。\n');
        catch ME
            warning('组间排序失败');
        end

    else
        % ================ 流程 A：工序分析 ================
        fprintf('\n===== 流程 A：工序步骤差异度分析 =====\n');
        try
            stageTable = analyzeProcessStages(keyMap, GLOBAL_METRICS_CONFIG, COMPARE_WINDOW, INVALID_TOKEN_FRACTION);
            filenameQZH = '大热重工序步骤差异度分析表.xlsx';
            if isfile(filenameQZH), delete(filenameQZH); end
            writetable(stageTable, filenameQZH);
            fprintf('已保存工序步骤差异度分析表。\n');
        catch ME
            warning('analyzeProcessStages 调用失败');
        end

        fprintf('\n===== 流程 A：工序间 差异度分析 =====\n');
        try
            [stageTableALL, detailAll, sampleTableALL, exportSheets] = analyzeProcessStages_AllGroups(keyMap, GLOBAL_METRICS_CONFIG, COMPARE_WINDOW, INVALID_TOKEN_FRACTION);
            filenameALL = '大热重工序间差异度排序表.xlsx';
            if isfile(filenameALL), delete(filenameALL); end
            % 选择导出列并替换为中文列名（按需可修改）
            Texport = stageTableALL(:, {'ProcessBase','Stage','NReplicates','TotalToMean','AvgToMean','SampleFilesStr','SampleShortNamesStr','Rank'});
            chineseColumnNames_Group = {'工艺基础','阶段','有效样本数','总差异度','平均差异度','样本文件列表','样本简称列表','全局排名'};
            Texport.Properties.VariableNames = chineseColumnNames_Group;
            writetable(Texport, filenameALL);
            fprintf('已保存工序间差异排序表: %s\n', filenameALL);

            filenameSample = '大热重工序样本差异度详细表.xlsx';
            if isfile(filenameSample), delete(filenameSample); end
            writetable(sampleTableALL, filenameSample);
            fprintf('已保存样本差异度详细表: %s\n', filenameSample);

            % ===== 可选导出：每样本 compareIdx 数据（流程 A） =====
            exportSheets_choice = input('是否导出每样本 compareIdx 数据为多 sheet Excel? (y/n, 默认 n): ','s');
            if isempty(exportSheets_choice), exportSheets_choice = 'n'; end
            
            if strcmpi(exportSheets_choice,'y')
                try
                    filenameSamplesCompare = '大热重详细DTG数据.xlsx';
                    if isfile(filenameSamplesCompare), delete(filenameSamplesCompare); end
                    fprintf('正在导出每样本 compareIdx 数据到：%s ...\n', filenameSamplesCompare);
                    for s = 1:numel(exportSheets)
                        % exportSheets{s}.T 为 table，exportSheets{s}.sheet 为 sheet 名
                        writetable(exportSheets{s}.T, filenameSamplesCompare, 'Sheet', exportSheets{s}.sheet);
                    end
                    fprintf('已保存每样本 compareIdx 数据（多 sheet）：%s\n', filenameSamplesCompare);
                catch ME
                    warning('导出每样本 compareIdx 数据失败: （请检查 exportSheets 格式和 writetable 权限）');
                end
            else
                fprintf('跳过每样本 compareIdx 多 sheet 导出。\n');
            end

            
        catch ME
            warning('analyzeProcessStages_AllGroups 调用失败');
        end
    end
end

%% ===== 程序结束与计时 =====
elapsedTimeSeconds = toc;
fprintf('\n====== 程序执行完成 ======\n');
fprintf('程序运行时间：%.2f 分钟（%.1f 秒）\n', elapsedTimeSeconds/60, elapsedTimeSeconds);

%% ==================== 辅助函数（脚本末尾） ====================
% 这些函数为通用实用函数：平滑处理、导出前后数据对齐、工作表名清理、指标描述、表头中文化（最小化实现）
% 若你已在工程里有类似实现，可删除或替换这些函数。

function P = computeProcessedDTG(S, method, window)
    if nargin < 3, window = 15; end
    P = repmat(struct('prefix','', 'col1', [], 'col3', []), numel(S), 1);
    for ii = 1:numel(S)
        P(ii).prefix = S(ii).prefix;
        t = S(ii).col1(:);
        y = S(ii).col3(:);
        n = numel(y);
        P(ii).col1 = t;
        if n < 3
            P(ii).col3 = y; continue;
        end
        switch lower(method)
            case 'sgolay'
                w = min(window, n - (mod(n,2)==0));
                if mod(w,2)==0, w = w-1; end
                if w < 3
                    P(ii).col3 = movmean(y, 3);
                else
                    try
                        P(ii).col3 = sgolayfilt(y, 2, w);
                    catch
                        P(ii).col3 = movmean(y, 3);
                    end
                end
            otherwise
                P(ii).col3 = movmean(y, 3);
        end
    end
end

function savePrePostExcel(S, P, rankingTable, topN, outFile)
    if nargin < 5 || isempty(outFile), outFile = 'PrePostExport.xlsx'; end
    if topN <= 0
        selPrefixes = rankingTable.Prefix;
    else
        selPrefixes = rankingTable.Prefix(1:min(topN, height(rankingTable)));
    end
    if isfile(outFile)
        try delete(outFile); 
        catch 
        end
    end
    usedNames = containers.Map();
    for i = 1:length(selPrefixes)
        p = selPrefixes{i};
        idxS = find(strcmp({S.prefix}, p),1);
        idxP = find(strcmp({P.prefix}, p),1);
        if isempty(idxS)
            warning('未找到样本 %s 的原始数据，跳过。', p); continue;
        end
        temp = S(idxS).col1(:);
        before = S(idxS).col3(:);
        after = [];
        if ~isempty(idxP), after = P(idxP).col3(:); end
        [t,b,a] = alignLengthThree(temp,before,after);
        tbl = table(t,b,a,'VariableNames',{'Temperature','BeforeDTG','AfterDTG'});
        sheetName = sanitizeSheetName(sprintf('%s', p));
        original = sheetName; cnt = 1;
        while isKey(usedNames, sheetName)
            cnt = cnt + 1;
            sheetName = sprintf('%s_%d', original, cnt);
            if length(sheetName) > 31, sheetName = sheetName(1:31); end
        end
        usedNames(sheetName) = true;
        writetable(tbl, outFile, 'Sheet', sheetName);
    end
    dirTbl = table((1:length(selPrefixes))', selPrefixes(:), 'VariableNames', {'Order','Prefix'});
    try writetable(dirTbl, outFile, 'Sheet', '导出目录'); catch 
    end
end

function [t,b,a] = alignLengthThree(t0,b0,a0)
    if isempty(t0), t = []; b = []; a = []; return; end
    t = t0(:); b = b0(:);
    if isempty(a0), a = nan(size(b)); 
    else a = a0(:); 
    end
    maxL = max([numel(t), numel(b), numel(a)]);
    if numel(t) < maxL, t(end+1:maxL) = nan; end
    if numel(b) < maxL, b(end+1:maxL) = nan; end
    if numel(a) < maxL, a(end+1:maxL) = nan; end
end

function s = sanitizeSheetName(name)
    illegal = '[\\/:*?"<>|]';
    s = regexprep(name, illegal, '_');
    s = strtrim(s);
    if isempty(s), s = 'Sheet'; end
    if length(s) > 31, s = s(1:31); end
end
