function [sortedTable, metricsOut] = rankGroupBests_compareWindow(groupBests, groupInfo, globalBestIndex, metricsConfig, PlotNumber, compareWindow)
% 支持选择 compareWindow ('all' 或 [start,end]) 的排序版本
    if nargin < 4 || isempty(metricsConfig)
        metricsConfig = struct('NRMSE',1);
    end
    if nargin < 6
        compareWindow = [];
    end
    G = numel(groupBests);
    if G < 1, error('排序需要至少1个组'); end
    if globalBestIndex < 1 || globalBestIndex > G, error('全局最佳组索引 %d 超出范围 [1,%d]', globalBestIndex, G); end

    metricNames = fieldnames(metricsConfig);
    weights = cell2mat(struct2cell(metricsConfig));
    weights = weights / sum(weights);
    M = numel(metricNames);
    topN = min(PlotNumber, G);

    L = min(cellfun(@numel, groupBests));
    if isempty(compareWindow)
        compareIdx = 1:L;
    else
        compareIdx = getComparisonIndices(L, compareWindow);
    end

    DTGref = groupBests{globalBestIndex}(1:L);
    DTGs = cell(1,G);
    for k=1:G, DTGs{k} = groupBests{k}(1:L); end

    rawMetrics = zeros(G,M);
    for i=1:G
        segCur = DTGs{i}(compareIdx);
        segRef = DTGref(compareIdx);
        for m=1:M
            switch metricNames{m}
                case 'NRMSE'
                    diff = segCur - segRef;
                    value = sqrt(mean(diff.^2)) / (max(segRef)-min(segRef)+eps);
                case 'RMSE'
                    value = sqrt(mean((segCur-segRef).^2));
                case 'Pearson'
                    value = corr(segCur, segRef);
                case 'Euclidean'
                    value = norm(segCur - segRef);
                otherwise
                    value = NaN;
            end
            rawMetrics(i,m) = value;
        end
    end

    % NaN 处理
    for m=1:M
        colVals = rawMetrics(:,m);
        nanMask = isnan(colVals);
        if ~any(nanMask), continue; end
        validVals = colVals(~nanMask);
        if isempty(validVals)
            if strcmp(metricNames{m}, 'Pearson'), colVals(:) = -1; else colVals(:) = 10; end
        else
            switch metricNames{m}
                case {'NRMSE','RMSE','Euclidean'}
                    colVals(nanMask) = max(validVals) * 10;
                otherwise
                    colVals(nanMask) = min(validVals) - 1;
            end
        end
        rawMetrics(:,m) = colVals;
    end

    normMetrics = zeros(G,M);
    compositeScores = zeros(G,1);
    for m=1:M
        colData = rawMetrics(:,m);
        if ismember(metricNames{m}, {'NRMSE','RMSE','Euclidean'})
            normCol = normalizeVector(colData, 'inverse');
        else
            normCol = normalizeVector(colData, 'direct');
        end
        normMetrics(:,m) = normCol;
        compositeScores = compositeScores + normCol * weights(m);
    end

    if topN > 0
        [~,order] = sort(compositeScores, 'descend');
        topIndices = order(1:topN);
        % plotGroupRankingComparison(DTGs(topIndices), {groupInfo(topIndices).prefix}, compositeScores(topIndices), normMetrics(topIndices,:), metricNames, DTGref, compareIdx);
    end

    groupIdxList = (1:G)'; prefixesList = {groupInfo.prefix}'; compositeList = compositeScores;
    sortedTable = table(groupIdxList, prefixesList, compositeList, 'VariableNames', {'groupIndex','prefix','compositeScore'});
    for mIdx=1:numel(metricNames)
        baseName = matlab.lang.makeValidName(metricNames{mIdx});
        sortedTable.([baseName '_raw']) = rawMetrics(:,mIdx);
        sortedTable.([baseName '_score']) = normMetrics(:,mIdx);
    end
    [~, sortOrder] = sort(compositeScores, 'descend');
    sortedTable = sortedTable(sortOrder,:);
    sortedTable.rank = (1:G)';
    isGlobalBest = (1:G)' == globalBestIndex;
    sortedTable.isGlobalBest = isGlobalBest(sortOrder);

    % 元数据
    sortedTable.Properties.UserData.title = {'基于全局最佳组的多指标排序结果'};
    allCols = sortedTable.Properties.VariableNames; colDescs = cell(size(allCols));
    for colIdx=1:numel(allCols)
        colName = allCols{colIdx};
        if strcmp(colName,'groupIndex'), desc='组编号';
        elseif strcmp(colName,'prefix'), desc='样品前缀标识';
        elseif strcmp(colName,'compositeScore'), desc='加权综合评分';
        elseif strcmp(colName,'rank'), desc='综合排名 (1为最佳)';
        elseif strcmp(colName,'isGlobalBest'), desc='是否全局最佳参考组';
        elseif endsWith(colName,'_raw'), base=extractBefore(colName,'_raw'); desc=[base '指标原始值'];
        elseif endsWith(colName,'_score'), base=extractBefore(colName,'_score'); desc=[base '归一化评分'];
        else desc=colName; end
        colDescs{colIdx}=desc;
    end
    sortedTable.Properties.UserData.columnDescriptions = colDescs;

    metricsOut = struct('DTGs',{DTGs}, 'rawMetrics', rawMetrics, 'normalizedMetrics', normMetrics, 'compositeScores', compositeScores, 'sortIdx', sortOrder, 'metricNames',{metricNames}, 'weights', weights);
end