function summary = evaluateDTGReplicates(metricNames, weights, varargin)
% EVALUATEDTGREPLICATES 多指标评估多条 DTG 曲线的一致性
%
% 输入:
%   metricNames - 元胞数组，指定要使用的指标，如 {'NRMSE','Pearson','Euclidean'}
%   weights     - 权重向量，与 metricNames 等长 (将自动归一化)
%   varargin    - 多条 DTG 曲线向量 (可能长度不一)
%
% 输出:
%   summary - 结构体，包含以下字段：
%     .metricNames, .weights               输入复用
%     .rawMetrics                          M×nMetrics 原始平均指标值
%     .normalizedMetrics                   归一化分数
%     .compositeScores                     综合加权得分
%     .consensusIndex, .consensusData      最优样本索引及其 DTG
%     .ranking, .sortedScores              按得分降序的索引及得分
%     .detailReport                        MATLAB 表格，包含各指标原始值、分数、综合分、排名

    % --- 1. 输入验证 ---
    validateattributes(metricNames, {'cell'}, {'vector'},   mfilename, 'metricNames', 1);
    validateattributes(weights,     {'numeric'},{'vector','numel',numel(metricNames)}, ...
                       mfilename, 'weights', 2);
    nMetrics = numel(metricNames);
    M = numel(varargin);
    if M < 1
        error('至少需要一条 DTG 曲线输入。');
    end
    
    % 检查指标名称合法性
    validMetrics = {'NRMSE','RMSE','Pearson','Euclidean'};
    for k = 1:nMetrics
        if ~ismember(metricNames{k}, validMetrics)
            error('不支持的指标 "%s"，有效指标有：%s', ...
                  metricNames{k}, strjoin(validMetrics,', '));
        end
    end
    
    % 归一化权重之和为 1
    weights = weights(:);
    weights = weights / sum(weights);
    
    % --- 2. 统一长度截断 ---
    lengths = cellfun(@numel, varargin);
    minLen = min(lengths);
    dtg = cellfun(@(x)x(1:minLen), varargin, 'UniformOutput', false);
    
    % 单样本特殊处理
    if M == 1
        summary = struct( ...
            'metricNames', {metricNames}, ...
            'weights', weights, ...
            'rawMetrics',      0, ...
            'normalizedMetrics', 1, ...
            'compositeScores', 1, ...
            'consensusIndex', 1, ...
            'consensusData',   dtg{1}, ...
            'ranking',         1, ...
            'sortedScores',    1, ...
            'detailReport',    table((1),"VariableNames",{'OnlySample'}) ...
        );
        return;
    end
    
    % --- 3. 成对指标计算与平均 ---
    rawMetrics = zeros(M, nMetrics);
    for m = 1:nMetrics
        metric = metricNames{m};
        % 对称矩阵预分配
        pairValues = zeros(M);
        for i = 1:M-1
            xi = dtg{i};
            for j = i+1:M
                xj = dtg{j};
                switch metric
                    case 'NRMSE'
                        pairValues(i,j) = calculateNRMSE(xi, xj);
                    case 'RMSE'
                        pairValues(i,j) = sqrt(mean((xi - xj).^2));
                    case 'Pearson'
                        R = corr(xi, xj);
                        pairValues(i,j) = R;
                    case 'Euclidean'
                        pairValues(i,j) = norm(xi - xj);
                end
                pairValues(j,i) = pairValues(i,j);
            end
        end
        % 自己与自己的值置为 NaN，以免影响平均
        pairValues(1:M+1:end) = NaN;
        rawMetrics(:,m) = nanmean(pairValues, 2);
    end
    
    % --- 4. 归一化 & 加权综合得分 ---
    normalizedMetrics = zeros(M, nMetrics);
    for m = 1:nMetrics
        metric = metricNames{m};
        vec = rawMetrics(:,m);
        switch metric
            case {'NRMSE','RMSE','Euclidean'}
                % 越小越好 → 反向归一
                normalizedMetrics(:,m) = normalizeVector(vec, 'inverse');
            case 'Pearson'
                % 越大越好 → 直接归一
                normalizedMetrics(:,m) = normalizeVector(vec, 'direct');
        end
    end
    compositeScores = normalizedMetrics * weights;
    
    % --- 5. 最优样本 & 排名 ---
    [~, bestIdx] = max(compositeScores);
    [sortedScores, sortIdx] = sort(compositeScores, 'descend');
    
    % --- 6. 生成详细报告表格 ---
    % 构造合法字段名
    fieldNames = cellfun(@(s) matlab.lang.makeValidName(s), metricNames, 'UniformOutput', false);
    report = table((1:M)', ...
                   'VariableNames', {'SampleID'});
    for m = 1:nMetrics
        rawCol = rawMetrics(:,m);
        scCol  = normalizedMetrics(:,m);
        report.(fieldNames{m})      = rawCol;
        report.([fieldNames{m} '_Score']) = scCol;
    end
    report.CompositeScore = compositeScores;
    report.Rank           = zeros(M,1);
    report.Rank(sortIdx)  = (1:M)';
    
    % --- 7. 汇总输出 ---
    summary.metricNames       = metricNames;
    summary.weights           = weights;
    summary.rawMetrics        = rawMetrics;
    summary.normalizedMetrics = normalizedMetrics;
    summary.compositeScores   = compositeScores;
    summary.consensusIndex    = bestIdx;
    summary.consensusData     = dtg{bestIdx};
    summary.ranking           = sortIdx;
    summary.sortedScores      = sortedScores;
    summary.detailReport      = report;
end

%% 辅助：向量归一化
function y = normalizeVector(x, direction)
    % direction: 'direct' 或 'inverse'
    xmin = min(x);  xmax = max(x);
    if xmax > xmin
        y = (x - xmin) / (xmax - xmin);
    else
        y = ones(size(x));
    end
    if strcmp(direction,'inverse')
        y = 1 - y;
    end
end
