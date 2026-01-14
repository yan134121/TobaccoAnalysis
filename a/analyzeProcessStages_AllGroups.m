function [stageTableALL, detailStruct, sampleTableALL, exportSheets] = analyzeProcessStages_AllGroups(keyMap, metricsConfig, compareWindow, maxInvalidFraction)

    if nargin < 4 || isempty(maxInvalidFraction), maxInvalidFraction = 0.15; end
    if nargin < 3 || isempty(compareWindow), compareWindow = 'all'; end

    TARGET_LENGTH = 341;
    
    % ==================== 步骤 1: 定义固定的分析参数 ====================
    % 创建一个结构体，包含所有 generateDTGSteps 需要的参数。
    % 这些参数是用于分析的“黄金标准”，不随GUI改变。
    % !!! 您可以根据需要调整这些值，但要确保它们与您期望的分析标准一致 !!!
    analysisParams = struct();
    analysisParams.TARGET_LENGTH = TARGET_LENGTH;
    analysisParams.maxInvalidFraction = maxInvalidFraction;
    % --- 以下参数应与GUI的默认值或您希望的分析标准对齐 ---
    analysisParams.outlierWindow = 21;
    analysisParams.outlierNSigma = 4;
    analysisParams.jumpDiffThreshold = 0.1;
    analysisParams.GlobalDeviationNSigma = 4;
    analysisParams.fitType = 'linear';
    analysisParams.gamma = 0.8;
    analysisParams.anchorWin = 30;
    analysisParams.monoStart = 20;
    analysisParams.monoEnd = 120;
    analysisParams.edgeBlend = 0.6;
    analysisParams.epsScale = 1e-9;
    analysisParams.slopeThreshold = 1e-9;
    analysisParams.sgWindow = 11;       % **重要分析参数**
    analysisParams.sgPoly = 2;          % **重要分析参数**
    analysisParams.loessSpan = 0.2;     % **重要分析参数**
    analysisParams.smoothMethod = 'LOESS'; % **重要: 定义分析时统一使用的平滑方法**
    analysisParams.diffMethod = 'SG derivative'; % **重要: 定义分析时统一使用的微分方法**
    % ====================================================================


    keysAll = keyMap.keys();
    procMap = containers.Map('KeyType','char','ValueType','any');
    for k = 1:numel(keysAll)
        key = keysAll{k};
        tok = regexp(key,'^(.*)-([QZHqzh])$','tokens');
        if ~isempty(tok)
            base = tok{1}{1};
            if ~procMap.isKey(base), procMap(base) = struct('files', {{}} ); end
            filesStruct = keyMap(key);
            try, fps = {filesStruct.filePath}'; catch, fps = {}; end
            s = procMap(base);
            s.files = [s.files; fps(:)];
            procMap(base) = s;
        end
    end
    baseKeys = procMap.keys;
    rows = {};
    detailStruct = struct();
    sampleRows = cell(0, 9);

    % ===== 新增：用于收集每个样本 compareIdx 数据的结构 =====
    exportSheets = {};    % cell array of structs: {struct('sheet',sheetName,'T',table,...), ...}
    usedSheetNames = {};  % 帮助确保 sheet 名称唯一

    % 主循环：独立处理每个工序
    for i = 1:numel(baseKeys)
        base = baseKeys{i};
        entry = procMap(base);
        filePaths = unique(entry.files, 'stable');
        dtgs = {}; shortNames = {}; successFiles = {};
        for j = 1:numel(filePaths)
            try
                % ==================== 步骤 2: 调用统一的核心函数 ====================
                steps = generateDTGSteps(filePaths{j}, analysisParams);

                % 根据 analysisParams 中定义的标准，从steps中提取最终用于分析的DTG曲线
                d = [];
                % 选择最终 DTG
                if isfield(steps,'dtg_final') && ~isempty(steps.dtg_final)
                    d = steps.dtg_final;
                else
                    % 退回到原始选择逻辑
                    if strcmpi(analysisParams.smoothMethod, 'LOESS')
                        if strcmpi(analysisParams.diffMethod, 'SG derivative')
                            d = steps.dtg_loess_from_loess;
                        else
                            d = steps.diff_from_loess;
                        end
                    else
                        if strcmpi(analysisParams.diffMethod, 'SG derivative')
                            d = steps.dtg_sg_from_sg;
                        else
                            d = steps.diff_from_sg;
                        end
                    end
                end
                % =================================================================

                if ~isempty(d) && isnumeric(d) && all(isfinite(d))
                    dtgs{end+1,1} = d(:);
                    [~, nm, ~] = fileparts(filePaths{j});
                    shortNames{end+1,1} = nm;
                    successFiles{end+1,1} = filePaths{j};
                end
            catch ME
                 warning('分析过程中处理文件 %s 失败: %s', filePaths{j}, ME.message);
            end
        end
        nGood = numel(dtgs);
        totalToMean = NaN; avgToMean = NaN; groupRanks = zeros(nGood, 1);
        sampNR = []; sampP = []; sampE = []; compS = [];

        if nGood >= 1
                        % 统一使用固定长度 TARGET_LENGTH 构造矩阵，避免按组最短样本截断导致的不一致
            % TARGET_LENGTH 在函数顶部已定义
            Tlen = TARGET_LENGTH;
            compareIdx = getCompareIdx_local(compareWindow, Tlen); % 以 TARGET_LENGTH 为基准计算 compareIdx

            % 构造 M：每列长度固定为 Tlen，短的用末值填充（与 generateDTGSteps 的行为一致）
            M = nan(Tlen, nGood);
            for ii = 1:nGood
                tmp = dtgs{ii}(:);
                L = numel(tmp);
                if L >= Tlen
                    M(:,ii) = tmp(1:Tlen);
                elseif L > 0
                    M(1:L,ii) = tmp;
                    M(L+1:end,ii) = tmp(end); % 填充末值，保持一致性
                else
                    M(:,ii) = NaN;
                end
            end

            meanCurve = nanmean(M,2);

            % 使用更稳健的尺度（percentile range）作为归一化分母，防止极值或平坦段放大 NR
            pr = prctile(meanCurve(compareIdx), [2, 98]);
            localRefRange = max(pr(2) - pr(1), 1e-6); % 保底，避免除以 0
            % 记录用于诊断（可注释）
            % fprintf('Base %s: localRefRange=%g (pct2=%g,pct98=%g)\n', base, localRefRange, pr(1), pr(2));

            % ---------------- 新增：收集每个样本在 compareIdx 区间的横纵坐标并保存到 exportSheets ----------------
            for ii = 1:nGood
                % 确保该列无 NaN/填充（和分析中对 a 的处理一致）
                a_full = M(:,ii);
                if any(isnan(a_full)), a_full = fillmissing(a_full,'linear'); end
            
                idx_vals = compareIdx(:);
                sample_vals = a_full(compareIdx(:));
                mean_vals = meanCurve(compareIdx(:));
                diff_vals = sample_vals - mean_vals;
            
                % 构造 sheetName，基于 ProcessBase 与样本简称
                rawSheetName = sprintf('%s-%s', base, shortNames{ii});
                % 替换非法字符并截断到 31 字符
                sheetName = regexprep(rawSheetName, '[:\\/*?[\]]', '_'); % 替换非法字符
                sheetName = strtrim(sheetName);
                maxLen = 31;
                if numel(sheetName) > maxLen
                    sheetName = sheetName(1:maxLen);
                end
                % 确保唯一：如果已存在则追加序号
                baseSheetName = sheetName;
                kSuffix = 1;
                while any(strcmp(usedSheetNames, sheetName))
                    kSuffix = kSuffix + 1;
                    suffix = sprintf('_%d', kSuffix);
                    allowedLen = maxLen - numel(suffix);
                    sheetName = [baseSheetName(1:min(numel(baseSheetName), allowedLen)), suffix];
                end
                usedSheetNames{end+1} = sheetName;
            
                % 创建 table 并加入 exportSheets (列名已修改为中文)
                Tsheet = table(idx_vals, sample_vals, mean_vals, diff_vals, ...
                    'VariableNames', {'索引', '样本值', '均值', '差异值'});
                exportSheets{end+1} = struct('sheet', sheetName, 'T', Tsheet, 'FilePath', successFiles{ii});
            end

            for ii = 1:nGood
                a = M(:,ii); b = meanCurve;
                if any(isnan(a)), a = fillmissing(a,'linear'); end
                if any(isnan(b)), b = fillmissing(b,'linear'); end
                if all(isnan(a)) || all(isnan(b))
                    sampNR(ii)=NaN; sampP(ii)=NaN; sampE(ii)=NaN; continue;
                end
                % 仅在 compareIdx 区间计算指标，保持与 GUI 分析窗口一致
                diff_in_window = a(compareIdx) - b(compareIdx);
                mse = mean(diff_in_window.^2);
                sampNR(ii) = sqrt(mse) / localRefRange; % 使用稳健尺度

                na = a(compareIdx)-mean(a(compareIdx)); nb = b(compareIdx)-mean(b(compareIdx)); denom=norm(na)*norm(nb);
                if denom<eps, pval=1; else, pval=max(-1,min(1,(na'*nb)/denom)); end
                sampP(ii) = (pval+1)/2;

                sampE(ii) = norm(diff_in_window);
            end


            % ==================== 步骤 3: 简化差异度计算 ====================
            w = cell2mat(struct2cell(metricsConfig));
            if isempty(w) || all(w==0), w = [1; 0; 0]; end
            w = w / sum(w);
            
            sampNR_filled = fillNaNWithMedian(sampNR);
            sampP_filled = fillNaNWithMedian(sampP);
            sampE_filled = fillNaNWithMedian(sampE);

            % 直接用原始指标加权计算最终差异度得分 (compS)
            compS = sampNR_filled*w(1) + (1 - sampP_filled)*w(2) + sampE_filled*w(3);
            
            avgToMean = mean(compS);
            totalToMean = sum(compS) / sqrt(nGood);

            [~, sortIdx] = sort(compS, 'ascend'); 
            if nGood > 0
                sortedScores = compS(sortIdx);
                groupRanks(sortIdx(1)) = 1;
                for r = 2:nGood
                    if abs(sortedScores(r) - sortedScores(r-1)) < 1e-9
                        groupRanks(sortIdx(r)) = groupRanks(sortIdx(r-1));
                    else
                        groupRanks(sortIdx(r)) = r;
                    end
                end
            end
            % =============================================================
        end
        rows(end+1,:) = {base, "ALL", double(nGood), totalToMean, avgToMean, {successFiles}, {shortNames}};
        if isempty(compS), compS = nan(1, nGood); end
        for jj=1:nGood
            sampleRows(end+1,:)= {base, "ALL", successFiles{jj}, shortNames{jj}, sampNR(jj), sampP(jj), sampE(jj), compS(jj), groupRanks(jj)};
        end
    end
    
    % --- 表格构建与排序部分 ---
    T = cell2table(rows, 'VariableNames', {'ProcessBase','Stage','NReplicates','TotalToMean','AvgToMean','SampleFiles','SampleShortNames'});
    
    T.SampleFilesStr = cellfun(@(c) strjoin(c, '; '), T.SampleFiles, 'UniformOutput', false);
    T.SampleShortNamesStr = cellfun(@(c) strjoin(c, '; '), T.SampleShortNames, 'UniformOutput', false);

    if height(T) > 0
        tempT = sortrows(T, {'AvgToMean','TotalToMean'}, {'ascend','ascend'});
        tempT.Rank = (1:height(tempT))';
        T = join(T, tempT(:,{'ProcessBase', 'Rank'}));
    else
        T.Rank = [];
    end

    customBrandOrder = {'1906', 'HUAYUE', 'YOUXI'};
    customSuffixOrder = {'QS', 'FX', 'CG', 'JZ-YS', 'JZ-YZ'};
    
    brands = string(regexp(T.ProcessBase, '^([^-]+)', 'tokens', 'once'));
    batchNumbers = string(regexp(T.ProcessBase, '^[^-]+-([^-]+)', 'tokens', 'once'));
    suffixes = string(regexp(T.ProcessBase, '^[^-]+-[^-]+-(.*)$', 'tokens', 'once'));
    
    T.SortKey1_Brand = categorical(brands, customBrandOrder, 'Protected', true);
    T.SortKey2_Batch = batchNumbers;
    T.SortKey3_Suffix = categorical(suffixes, customSuffixOrder, 'Protected', true);
    
    T = sortrows(T, {'SortKey1_Brand', 'SortKey2_Batch', 'SortKey3_Suffix'});
    
    T.SortKey1_Brand = []; T.SortKey2_Batch = []; T.SortKey3_Suffix = [];
    
    stageTableALL = T;
    
    sampleTableALL = cell2table(sampleRows, 'VariableNames', ...
        {'工艺基础', '阶段', '样本文件', '样本简称', '归一化均方根误差', '皮尔逊相关系数', '欧几里得距离', '组合得分', '组内排名'});
        
    if height(sampleTableALL) > 0
        s_brands = string(regexp(sampleTableALL{:,'工艺基础'}, '^([^-]+)', 'tokens', 'once'));
        s_batchNumbers = string(regexp(sampleTableALL{:,'工艺基础'}, '^[^-]+-([^-]+)', 'tokens', 'once'));
        s_suffixes = string(regexp(sampleTableALL{:,'工艺基础'}, '^[^-]+-[^-]+-(.*)$', 'tokens', 'once'));
        
        sampleTableALL.SortKey1_Brand = categorical(s_brands, customBrandOrder, 'Protected', true);
        sampleTableALL.SortKey2_Batch = s_batchNumbers;
        sampleTableALL.SortKey3_Suffix = categorical(s_suffixes, customSuffixOrder, 'Protected', true);

        sampleTableALL = sortrows(sampleTableALL, ...
            {'SortKey1_Brand', 'SortKey2_Batch', 'SortKey3_Suffix', '组内排名'});

        sampleTableALL.SortKey1_Brand = [];
        sampleTableALL.SortKey2_Batch = [];
        sampleTableALL.SortKey3_Suffix = [];
    end
end


%% ---------------- 辅助函数 ----------------
function idx = getCompareIdx_local(compareWindow, minLen)
    if ischar(compareWindow) && strcmpi(compareWindow,'all')
        idx = 1:minLen;
    else
        try
            if numel(compareWindow) >= 2
                s0 = round(compareWindow(1)); e0 = round(compareWindow(2));
            else
                s0 = round(compareWindow(1)); e0 = s0;
            end
        catch
            s0 = 1; e0 = minLen;
        end
        sidx = max(1, min(minLen, s0));
        eidx = max(1, min(minLen, e0));
        if sidx > eidx, tmp = sidx; sidx = eidx; eidx = tmp; end
        idx = sidx:eidx;
        if isempty(idx), idx = 1:minLen; end
    end
end



%% 归一化辅助函数
function normVals = normalizeMetric(vals, gmin, gmax, direction)
    if isempty(vals)
        normVals = [];
        return;
    end
    
    if gmax == gmin
        normVals = ones(size(vals));
        return;
    end
    
    % 归一化处理
    normVals = (vals - gmin) / (gmax - gmin + eps);
    
    % 根据指标类型调整
    if strcmp(direction, 'inverse')
        normVals = 1 - normVals; % 差异型指标：值越大表示差异越大
    end
    
    % 确保值在[0,1]范围内
    normVals = min(max(normVals, 0), 1);
end

%% 填充NaN值辅助函数
function vals = fillNaNWithMedian(vals)
    if isempty(vals)
        return;
    end
    
    nanMask = isnan(vals);
    if any(nanMask)
        validVals = vals(~nanMask);
        if isempty(validVals)
            fillValue = 0.5;
        else
            fillValue = median(validVals);
        end
        vals(nanMask) = fillValue;
    end
end