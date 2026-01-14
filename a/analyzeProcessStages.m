function stageTable = analyzeProcessStages(keyMap, metricsConfig, compareWindow, maxInvalidFraction)
    if nargin < 4 || isempty(maxInvalidFraction), maxInvalidFraction = 0.15; end
    if nargin < 3 || isempty(compareWindow), compareWindow = 'all'; end
    stageOrder = {'Q','Z','H'};

    % 获取全局参考曲线（用于NRMSE计算）
    globalRefDTG = getGlobalReferenceDTG(keyMap, maxInvalidFraction);
    globalRefRange = max(globalRefDTG) - min(globalRefDTG);
    
    % 构建工序映射
    keysAll = keyMap.keys();
    procMap = containers.Map('KeyType','char','ValueType','any');
    for k = 1:numel(keysAll)
        key = keysAll{k};
        tok = regexp(key, '^(.*)-([QZHqzh])$', 'tokens');
        if ~isempty(tok)
            base = tok{1}{1};
            stage = upper(tok{1}{2});
            if ~procMap.isKey(base)
                procMap(base) = containers.Map('KeyType','char','ValueType','char');
            end
            sub = procMap(base);
            sub(stage) = key;
            procMap(base) = sub;
        end
    end

    if isempty(procMap.keys)
        stageTable = table('Size',[0,6], ...
            'VariableTypes',{'string','string','double','double','double','logical'}, ...
            'VariableNames',{'ProcessBase','Stage','NReplicates','TotalPairDiff','AvgPairDiff','IsBest'});
        return;
    end

    % 获取指标配置
    metricNames = fieldnames(metricsConfig);
    w = cell2mat(struct2cell(metricsConfig));
    w = w / sum(w);
    M = numel(metricNames);
    
    % 第一遍遍历：收集每个工序的配对原始指标
    baseKeys = procMap.keys();
    perStage = struct('ProcessBase',{}, 'Stage', {}, 'NReplicates', {}, 'Pairs', {});
    allPairs = [];
    
    for b = 1:numel(baseKeys)
        base = baseKeys{b};
        subMap = procMap(base);
        
        for si = 1:numel(stageOrder)
            stage = stageOrder{si};
            if subMap.isKey(stage)
                fullKey = subMap(stage);
                files = keyMap(fullKey);
                if isstruct(files)
                    filePaths = {files.filePath}';
                else
                    try filePaths = {files.filePath}'; catch, filePaths={}; end
                end

                % 读取DTG曲线
                dtgs = {};
                for iF = 1:numel(filePaths)
                    try
                        TARGET_LENGTH = 341;
                        d = computeDTGForFile_old(filePaths{iF}, maxInvalidFraction, TARGET_LENGTH, compareWindow);
                        if ~isempty(d) && isnumeric(d)
                            dtgs{end+1,1} = d(:);
                        end
                    catch
                        % skip
                    end
                end

                nGood = numel(dtgs);
                pairs = [];
                
                if nGood >= 2
                    % 统一长度
                    lens = cellfun(@numel, dtgs);
                    minLen = min(lens);
                    for i=1:nGood
                        dtgs{i} = dtgs{i}(1:minLen);
                    end
                    
                    % 确定比较区间
                    if ischar(compareWindow) && strcmpi(compareWindow,'all')
                        idx = 1:minLen;
                    else
                        s0 = round(compareWindow(1)); e0 = round(compareWindow(2));
                        sidx = max(1, min(minLen, s0));
                        eidx = max(1, min(minLen, e0));
                        if sidx > eidx, tmp=sidx; sidx=eidx; eidx=tmp; end
                        idx = sidx:eidx;
                    end

                    pcount = 0;
                    for i1=1:(nGood-1)
                        a = dtgs{i1}(idx);
                        for j1=(i1+1):nGood
                            pcount = pcount+1;
                            bvec = dtgs{j1}(idx);
                            
                            metricValues = [];
                            
                            % 计算每个指标
                            for m = 1:M
                                switch metricNames{m}
                                    case 'NRMSE'
                                        diff = a - bvec;
                                        mse = mean(diff.^2);
                                        rmse = sqrt(mse);
                                        
                                        % 计算两个向量各自的标准差
                                        std_a = std(a);
                                        std_b = std(bvec);
                                        
                                        % 分母是两个标准差的几何平均值（或直接相乘）
                                        % 我们使用相乘，对差异更敏感
                                        denom = std_a * std_b;
                                        
                                        % 避免除以零
                                        if denom < eps
                                            % 如果两个信号都是平的，只有在它们不相等时才有误差
                                            if rmse > eps
                                                nrmseVal = inf; % 无限大误差
                                            else
                                                nrmseVal = 0;   % 完全相等
                                            end
                                        else
                                            % 原始定义是 RMSE / (std_a * std_b), 可能会 > 1
                                            % 为了使其更像一个[0,1]区间的误差，我们可以用 RMSE / max(std_a, std_b)
                                            % 或者 RMSE / mean([std_a, std_b])
                                            % 这里我们采用更简单且能区分的 RMSE / (sqrt(std_a * std_b))
                                            nrmseVal = rmse / (sqrt(denom) + eps);
                                        end
                                        metricValues(end+1) = nrmseVal;
                                        
                                    case 'Pearson'
                                        % 稳健的Pearson计算
                                        na = a - mean(a); 
                                        nb = bvec - mean(bvec);
                                        denom = (norm(na) * norm(nb));
                                        if denom < eps
                                            pval = 1; % 完全平坦视为完全相关
                                        else
                                            pval = max(-1, min(1, (na' * nb) / denom));
                                        end
                                        pval = (pval + 1)/2; % 转换为[0,1]
                                        metricValues(end+1) = pval;
                                        
                                    case 'Euclidean'
                                        % 欧氏距离
                                        eVal = norm(a - bvec);
                                        metricValues(end+1) = eVal;
                                end
                            end
                            
                            pairs(pcount, :) = metricValues;
                        end
                    end
                end

                perStage(end+1).ProcessBase = base;
                perStage(end).Stage = stage;
                perStage(end).NReplicates = nGood;
                perStage(end).Pairs = pairs;

                if ~isempty(pairs)
                    allPairs = [allPairs; pairs];
                end
            end
        end
    end

    % 使用分位数归一化（排除异常值）
    if isempty(allPairs)
        error('没有任何有效配对数据可计算差异度。');
    end
    
    gmin = quantile(allPairs, 0.05, 1);
    gmax = quantile(allPairs, 0.95, 1);
    
    % 对每个工序计算差异度
    rows = {};
    
    for k = 1:numel(perStage)
        base = perStage(k).ProcessBase;
        stage = perStage(k).Stage;
        pairs = perStage(k).Pairs;
        nGood = perStage(k).NReplicates;
        
        if isempty(pairs) || isempty(pairs(:,1))
            totalDiff = NaN; 
            avgDiff = NaN;
        else
            % 归一化处理
            normMat = zeros(size(pairs));
            for col = 1:M
                colv = pairs(:,col);
                mn = gmin(col); mx = gmax(col);
                
                if mx == mn
                    normMat(:,col) = ones(size(colv));
                else
                    if strcmp(metricNames{col}, 'Pearson')
                        % Pearson已经是[0,1]
                        normMat(:,col) = (colv - mn) / (mx - mn);
                    else
                        % NRMSE & Euclidean（差异型指标）
                        normMat(:,col) = 1 - (colv - mn) / (mx - mn);
                    end
                end
            end
            
            % 处理NaN值（使用列中位数）
            for col=1:M
                cv = normMat(:,col);
                if any(isnan(cv))
                    valid = cv(~isnan(cv));
                    if isempty(valid), fill = 0.5; else fill = median(valid); end
                    cv(isnan(cv)) = fill;
                    normMat(:,col) = cv;
                end
            end

            % 非线性差异度转换（S型函数）
            compSim = normMat * w;
            compSim = min(max(compSim, 0), 1); % 确保在[0,1]范围内
            dissim = 1 - 1./(1+exp(-10*(compSim-0.8))); 
            dissim = min(max(dissim, 0), 1); % 确保在[0,1]范围内
            
            % 计算总差异度（考虑样本量）
            totalDiff = sum(dissim) / sqrt(nGood); % 样本量加权
            avgDiff = mean(dissim);
        end
        
        rows(end+1,:) = {base, stage, double(nGood), double(totalDiff), double(avgDiff)};
    end

    T = cell2table(rows, 'VariableNames', {'ProcessBase','Stage','NReplicates','TotalPairDiff','AvgPairDiff'});
    T.IsBest = false(height(T),1);
    bases = unique(T.ProcessBase);
    
    for i = 1:numel(bases)
        idx = strcmp(T.ProcessBase, bases{i});
        vals = T.TotalPairDiff(idx);
        if all(isnan(vals)), continue; end
        [~, mi] = min(vals);
        locs = find(idx);
        T.IsBest(locs(mi)) = true;
    end
    
    % ***** 核心修正点: 实现五级自定义排序 (包含Stage) *****
    % =================================================================
    
    % --- 步骤 1: 创建第一级排序列 (品牌/系列 - 自定义顺序) ---
    customBrandOrder = {'1906', 'HUAYUE', 'YOUXI'};
    brands = regexp(T.ProcessBase, '^([^-]+)', 'tokens', 'once');
    brands = string(cellfun(@(c) c{1}, brands, 'UniformOutput', false, 'ErrorHandler', @(s,c) ''));
    catBrands = categorical(brands, customBrandOrder, 'Protected', true);
    
    % --- 步骤 2: 创建第二级排序列 (批次号 - 标准排序) ---
    batchNumbers = regexp(T.ProcessBase, '^[^-]+-([^-]+)', 'tokens', 'once');
    batchNumbers = string(cellfun(@(c) c{1}, batchNumbers, 'UniformOutput', false, 'ErrorHandler', @(s,c) ''));
    
    % --- 步骤 3: 创建第三级排序列 (工序尾缀 - 自定义顺序) ---
    customSuffixOrder = {'QS', 'FX', 'CG', 'JZ-YS', 'JZ-YZ'};
    suffixes = regexp(T.ProcessBase, '^[^-]+-[^-]+-(.*)$', 'tokens', 'once');
    suffixes = string(cellfun(@(c) c{1}, suffixes, 'UniformOutput', false, 'ErrorHandler', @(s,c) ''));
    catSuffixes = categorical(suffixes, customSuffixOrder, 'Protected', true);

    % --- 步骤 4: 创建第四级排序列 (Stage - 自定义顺序) ---
    customStageOrder = {'Q', 'Z', 'H'};
    catStages = categorical(T.Stage, customStageOrder, 'Protected', true);
    
    % --- 步骤 5: 执行排序 ---
    T.SortKey1_Brand = catBrands;
    T.SortKey2_Batch = batchNumbers;
    T.SortKey3_Suffix = catSuffixes;
    T.SortKey4_Stage = catStages;
    
    % 依次按五级键进行排序 (品牌 -> 批次 -> 尾缀 -> 阶段)
    T = sortrows(T, {'SortKey1_Brand', 'SortKey2_Batch', 'SortKey3_Suffix', 'SortKey4_Stage'});
    
    % 移除临时的排序列
    T.SortKey1_Brand = [];
    T.SortKey2_Batch = [];
    T.SortKey3_Suffix = [];
    T.SortKey4_Stage = [];
    % =================================================================
    
    stageTable = T;
end

%% 辅助函数：获取全局参考曲线
function globalRefDTG = getGlobalReferenceDTG(keyMap, maxInvalidFraction)
    % 尝试获取一个代表性的全局参考曲线
    allKeys = keyMap.keys();
    allDTGs = [];
    
    % 收集所有有效DTG曲线
    for k = 1:min(50, numel(allKeys)) % 限制数量防止内存不足
        key = allKeys{k};
        files = keyMap(key);
        if isstruct(files)
            filePaths = {files.filePath}';
        else
            try filePaths = {files.filePath}'; catch, continue; end
        end
        
        for iF = 1:min(3, numel(filePaths)) % 每组取前3个
            try
                d = computeDTGForFile_old(filePaths{iF}, maxInvalidFraction, 341, []);
                if ~isempty(d) && isnumeric(d)
                    allDTGs = [allDTGs; d(:)'];
                end
            catch
                % skip
            end
        end
    end
    
    if isempty(allDTGs)
        globalRefDTG = linspace(0, 100, 341)'; % 默认曲线
    else
        % 使用中位数曲线作为全局参考
        globalRefDTG = median(allDTGs, 1)';
    end
end