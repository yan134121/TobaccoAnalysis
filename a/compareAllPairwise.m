function [diffMatrix, pairwiseTable, pairwiseDetails] = compareAllPairwise(groupBestDTGs, groupInfo, metricsConfig, filePathMap)
% COMPAREALLPAIRWISE 计算所有样品的两两差异（每对只计算一次 i<j）
%
% 输入:
%   groupBestDTGs - 1xG cell, 每个元素为该组的最佳 DTG 向量
%   groupInfo     - 1xG struct array, 每个元素含 .prefix 字段（样品名）
%   metricsConfig - struct, 如 struct('NRMSE',1,'Pearson',1,'Euclidean',1)（会归一化为权重）
%   filePathMap   - containers.Map 可选，组前缀 -> 最佳样路径 （可为空）
%
% 输出:
%   diffMatrix    - GxG 矩阵，行=基准样 i，列=对比样 j，综合差异度（仅填充 i<j）
%   pairwiseTable - table，差异度矩阵，带行名与列名，便于导出 Excel
%   pairwiseDetails - table，每一行对应一对 (i,j)，包含原始指标、归一化指标、综合差异度

    if nargin < 4, filePathMap = []; end

    % --- 参数与前处理 ---
    G = numel(groupBestDTGs);
    if G ~= numel(groupInfo)
        error('groupBestDTGs 与 groupInfo 长度不匹配。');
    end
    % 提取所有组的名称，用于后续表格的表头和标签
    names = cell(1,G);
    for k = 1:G
        if isfield(groupInfo(k),'prefix') && ~isempty(groupInfo(k).prefix)
            names{k} = groupInfo(k).prefix;
        else
            names{k} = sprintf('Group_%d', k);
        end
    end

    % --- 权重配置 ---
    % 定义固定的指标顺序，确保权重和计算结果一一对应
    metricNames = {'NRMSE','Pearson','Euclidean'};
    rawWeights = zeros(1, numel(metricNames));
    for m = 1:numel(metricNames)
        if isfield(metricsConfig, metricNames{m})
            rawWeights(m) = metricsConfig.(metricNames{m});
        end
    end
    % 如果所有权重都为0，则默认使用等权重，避免除以0的错误
    if sum(rawWeights) == 0
        rawWeights = ones(size(rawWeights)); 
    end
    % 将原始权重归一化，使其总和为1
    weights = rawWeights(:) / sum(rawWeights);

    % --- 数据对齐 ---
    % 为保证比较的公平性，将所有DTG曲线裁剪到相同的最短长度
    L = min(cellfun(@numel, groupBestDTGs));
    DTGs = cellfun(@(v)v(1:L), groupBestDTGs, 'UniformOutput', false);

    % --- 遍历所有可能的配对 (i < j)，计算原始差异度指标 ---
    pairList = nchoosek(1:G,2); % 生成所有组合的索引对
    nPairs = size(pairList,1);
    
    % 初始化存储原始指标值的向量
    rawNRMSE = nan(nPairs,1);
    rawEucl  = nan(nPairs,1);
    rawDpear = nan(nPairs,1); % Dpear = 1 - Pearson Correlation, 将其转换为差异度

    for p = 1:nPairs
        i = pairList(p,1); 
        j = pairList(p,2);
        xi = DTGs{i}; % 基准曲线
        xj = DTGs{j}; % 对比曲线

        % 1. 计算 NRMSE (归一化均方根误差)
        try
            % 优先使用外部定义的 calculateNRMSE 函数
            if exist('calculateNRMSE','file') == 2
                valNRMSE = calculateNRMSE(xi, xj);
            else
                % 若函数不存在，则使用标准定义：RMSE / (参考信号的绝对值最大值)
                valNRMSE = sqrt(mean((xi - xj).^2)) / (max(abs(xi)) + eps);
            end
        catch
            valNRMSE = sqrt(mean((xi - xj).^2)) / (max(abs(xi)) + eps);
        end
        rawNRMSE(p) = valNRMSE;

        % 2. 计算 Euclidean (欧几里得距离)
        rawEucl(p) = norm(xi - xj);

        % 3. 计算 Pearson 相关系数，并转换为差异度 (1 - R)
        try
            R = corr(xi(:), xj(:));
            if isnan(R), R = 0; end % 如果计算结果为NaN（例如其中一个向量是常数），则认为不相关
        catch
            R = 0; % 如果 corr 函数出错，也认为不相关
        end
        rawDpear(p) = 1 - R; % R 越接近1，差异度越小；R 越小，差异度越大
    end

    % --- 指标归一化 (核心修改部分) ---
    % 定义一个极小的正数 epsilon，用于将归一化区间的下限从0调整为epsilon
    epsilon = 1e-9; 
    
    % 定义新的归一化函数 `safeNormalize`
    % 它将数据从 [min, max] 线性映射到 [epsilon, 1] 区间
    % 步骤1: (v - min(v)) ./ (max(v) - min(v) + eps) -> 将数据映射到 [0, 1]
    % 步骤2: ... * (1 - epsilon) + epsilon           -> 将 [0, 1] 映射到 [epsilon, 1]
    % `+eps` 是为了防止当所有值都相同时，分母为0
    safeNormalize = @(v) ...
        ( (v - min(v)) ./ (max(v) - min(v) + eps) ) * (1 - epsilon) + epsilon;

    % 对每个原始指标向量进行归一化
    normNRMSE = safeNormalize(rawNRMSE);
    normEucl  = safeNormalize(rawEucl);
    normDpear = safeNormalize(rawDpear);

    % --- 计算综合差异度 ---
    % 将归一化后的指标按列合并成一个矩阵
    normalizedMatrix = [normNRMSE, normDpear, normEucl];
    % 使用矩阵乘法，将归一化后的指标与对应的权重相乘并求和，得到每个配对的综合得分
    compositeScores = normalizedMatrix * weights(:);

    % --- 构造输出矩阵 (GxG)，用于快速查阅和可能的后续计算 ---
    diffMatrix = nan(G, G);
    for p = 1:nPairs
        i = pairList(p,1); 
        j = pairList(p,2);
        diffMatrix(i,j) = compositeScores(p); % 矩阵的上三角部分填充综合差异度
    end

    % --- 构造详细的配对信息表 (pairwiseDetails)，用于导出 Excel 明细 ---
    refNames = names(pairList(:,1))'; % 提取所有配对的基准样品名
    cmpNames = names(pairList(:,2))'; % 提取所有配对的对比样品名
    pairIDs = cellfun(@(r,c) sprintf('%s_vs_%s', r, c), refNames, cmpNames, 'UniformOutput', false);

    pairwiseDetails = table(pairIDs, refNames, cmpNames, rawNRMSE, rawDpear, rawEucl, ...
                            normNRMSE, normDpear, normEucl, compositeScores, ...
                            'VariableNames', {'PairID','RefName','CmpName','NRMSE_raw','Dpear_raw','Euclidean_raw',...
                                              'NRMSE_norm','Dpear_norm','Euclidean_norm','CompositeScore'});
    
    % --- 构造用于导出差异度矩阵的表格 (pairwiseTable) ---
    tblData = num2cell(diffMatrix);
    pairwiseTable = cell2table(tblData, 'VariableNames', names, 'RowNames', names);

end