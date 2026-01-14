function chineseRankTable = translateSortedRankTable(rankTable)
    % 将组间差异排序表的列名翻译为中文
    
    % 创建中文映射字典
    columnMap = containers.Map();
    
    % 固定列名翻译
    columnMap('groupIndex') = '组索引';
    columnMap('prefix') = '样品前缀';
    columnMap('compositeScore') = '综合评分';
    columnMap('rank') = '排名';
    columnMap('isGlobalBest') = '是否全局最优';
    
    % 指标列名翻译 (原始值和评分)
    columnMap('NRMSE_raw') = '标准化均方根误差(原始)';
    columnMap('NRMSE_score') = '标准化均方根误差(评分)';
    columnMap('Pearson_raw') = '皮尔逊相关系数(原始)';
    columnMap('Pearson_score') = '皮尔逊相关系数(评分)';
    columnMap('Euclidean_raw') = '欧氏距离(原始)';
    columnMap('Euclidean_score') = '欧氏距离(评分)';
    
    % 遍历原始列名并翻译
    origHeaders = rankTable.Properties.VariableNames;
    chineseHeaders = cell(size(origHeaders));
    
    for i = 1:numel(origHeaders)
        header = origHeaders{i};
        if isKey(columnMap, header)
            chineseHeaders{i} = columnMap(header);
        else
            % 自动处理动态生成的指标列（如自定义指标）
            if endsWith(header, '_raw')
                baseName = extractBefore(header, '_raw');
                chineseHeaders{i} = [baseName '(原始值)'];
            elseif endsWith(header, '_score')
                baseName = extractBefore(header, '_score');
                chineseHeaders{i} = [baseName '(评分)'];
            else
                chineseHeaders{i} = header;
            end
        end
    end
    
    % 创建翻译后的表格
    chineseRankTable = rankTable;
    chineseRankTable.Properties.VariableNames = chineseHeaders;
    
    % 添加描述信息
    chineseRankTable.Properties.Description = '大热重最佳样品差异排序表';
end