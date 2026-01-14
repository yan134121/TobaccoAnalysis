function [diffMatrix, pairwiseTable, pairwiseDetails] = compareAllPairwiseSmall(S, metricsConfig)
% COMPAREALLPAIRWISESMALL 对小热重样本集合 S 做两两差异度计算（两两各算一次，结果对称）
%
% 输入:
%   S - 结构体数组，字段至少包含 .prefix 和 .col3（DTG）.col1 可选
%   metricsConfig - 权重结构体，例如 struct('NRMSE',0.5,'Pearson',0.3,'Euclidean',0.2)
%
% 输出:
%   diffMatrix      - GxG 对称矩阵（综合差异度，越大表示越不相似）
%   pairwiseTable   - table，可直接写入 Excel（带 RowNames 为 Prefix）
%   pairwiseDetails - table，每一行对应一对样本的原始/归一化指标及综合差异度

    % --- 1. 基本校验与准备 ---
    if ~isstruct(S) || isempty(S)
        error('输入 S 必须为非空结构体数组');
    end
    G = numel(S);
    prefixes = cell(G,1);
    for i = 1:G
        if isfield(S(i),'prefix') && ~isempty(S(i).prefix)
            prefixes{i} = S(i).prefix;
        else
            prefixes{i} = sprintf('Sample_%02d', i);
        end
    end

    metricNamesAll = {'NRMSE','Pearson','Euclidean'};
    % 构造权重向量，若某项缺失则设为 0
    w = zeros(1, numel(metricNamesAll));
    for m = 1:numel(metricNamesAll)
        if isfield(metricsConfig, metricNamesAll{m})
            w(m) = metricsConfig.(metricNamesAll{m});
        end
    end
    if sum(w) == 0
        w = ones(size(w));
    end
    w = w / sum(w);  % 归一化权重，使其总和为1

    % --- 2. 逐对计算原始指标（只计算 i<j），对齐长度到 minLen_ij ---
    pairs = nchoosek(1:G,2);
    nPairs = size(pairs,1);
    rawNRMSE = nan(nPairs,1);
    rawEucl  = nan(nPairs,1);
    rawDpear = nan(nPairs,1);

    for p = 1:nPairs
        i = pairs(p,1); j = pairs(p,2);
        xi = S(i).col3(:);
        xj = S(j).col3(:);
        L = min(numel(xi), numel(xj));
        if L < 2
            % 如果数据点不足，无法计算，则将该对指标置为 NaN
            rawNRMSE(p) = NaN;
            rawEucl(p)  = NaN;
            rawDpear(p) = NaN;
            continue;
        end
        xi = xi(1:L); xj = xj(1:L);

        % NRMSE: RMSE / (range(ref) + eps)
        rmse = sqrt(mean((xi - xj).^2));
        rangeRef = max(xi) - min(xi);
        rawNRMSE(p) = rmse / (rangeRef + eps);

        % Euclidean
        rawEucl(p) = norm(xi - xj);

        % Pearson -> Dpear = 1 - Pearson
        try
            r = corr(xi(:), xj(:));
            if isnan(r), r = 0; end
        catch
            r = 0;
        end
        rawDpear(p) = 1 - r;
    end

    % --- 3. 指标归一化 (核心修改部分) ---
    % 定义一个极小的正数 epsilon，作为归一化区间的下限，以避免出现0值
    epsilon = 1e-9;

    % 定义新的归一化函数 `safeNorm`
    % 它将数据从 [min, max] 线性映射到 [epsilon, 1] 区间
    % `nanmin` 和 `nanmax` 用于在计算时忽略可能存在的 NaN 值
    safeNorm = @(v) ((v - nanmin(v)) ./ (nanmax(v) - nanmin(v) + eps)) * (1 - epsilon) + epsilon;

    % 使用新的归一化函数处理每个原始指标
    nNRMSE = safeNorm(rawNRMSE);
    nEucl  = safeNorm(rawEucl);
    nDpear = safeNorm(rawDpear);

    % --- 4. 综合差异度（按权重）, composite in [epsilon,1], NaN 保持 NaN ---
    % 注意顺序： weights 对应 [NRMSE, Pearson, Euclidean]，计算时 Pearson 的差异度用 Dpear
    normalizedMatrix = [nNRMSE, nDpear, nEucl]; % nPairs x 3
    composite = nan(nPairs,1);
    % 此循环的逻辑是为了稳健地处理某些指标可能为NaN的情况
    for p = 1:nPairs
        row = normalizedMatrix(p,:);
        if all(isnan(row))
            composite(p) = NaN;
        else
            validMask = ~isnan(row);
            if all(~validMask)
                composite(p) = NaN;
            else
                % 动态调整权重：只使用当前配对中有效的指标进行加权平均
                w_eff = w; w_eff(~validMask) = 0;
                if sum(w_eff) == 0
                    % 如果所有带权重的指标都无效，则对有效指标进行等权平均
                    w_eff(validMask) = 1;
                end
                w_eff = w_eff / sum(w_eff); % 重新归一化有效权重
                composite(p) = nansum(row .* w_eff);
            end
        end
    end

    % --- 5. 构造对称 diffMatrix（填 i<j 和 j<i） ---
    diffMatrix = nan(G, G);
    for p = 1:nPairs
        i = pairs(p,1); j = pairs(p,2);
        diffMatrix(i,j) = composite(p);
        diffMatrix(j,i) = composite(p); % 对称填入，方便后续进行均值等操作
    end
    % 对角线置0，代表样品与自身的差异为0
    for i = 1:G, diffMatrix(i,i) = 0; end

    % --- 6. pairwiseDetails 表（逐对明细） ---
    PairID = cell(nPairs,1); RefName = cell(nPairs,1); CmpName = cell(nPairs,1);
    for p = 1:nPairs
        i = pairs(p,1); j = pairs(p,2);
        PairID{p} = sprintf('%s_vs_%s', prefixes{i}, prefixes{j});
        RefName{p} = prefixes{i};
        CmpName{p} = prefixes{j};
    end
    pairwiseDetails = table(PairID, RefName, CmpName, rawNRMSE, rawDpear, rawEucl, ...
                           nNRMSE, nDpear, nEucl, composite, ...
                           'VariableNames', {'PairID','RefName','CmpName','NRMSE_raw','Dpear_raw','Euclidean_raw',...
                                             'NRMSE_norm','Dpear_norm','Euclidean_norm','CompositeScore'});

    % --- 7. pairwiseTable（写 Excel 用）构造：RowNames 为 Prefix，列为 To_Prefix ---
    tblData = num2cell(diffMatrix);
    pairwiseTable = cell2table(tblData, 'VariableNames', prefixes, 'RowNames', prefixes);

end