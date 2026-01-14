function chineseTable = summarizeTableWithChineseHeaders(originalTable)
    % 创建中文列名映射字典
    columnMap = containers.Map();
    
    % 固定列名翻译
    columnMap('GroupIndex') = '组索引';
    columnMap('Prefix') = '样品前缀';
    columnMap('Score') = '综合评分';
    columnMap('Rank') = '排名';
    
    % 指标列名翻译
    columnMap('NRMSE_Raw') = '标准化均方根误差(原始值)';
    columnMap('NRMSE_Norm') = '标准化均方根误差(归一化值)';
    columnMap('RMSE_Raw') = '均方根误差(原始值)';
    columnMap('RMSE_Norm') = '均方根误差(归一化值)';
    columnMap('Euclidean_Raw') = '欧氏距离(原始值)';
    columnMap('Euclidean_Norm') = '欧氏距离(归一化值)';
    columnMap('Pearson_Raw') = '皮尔逊相关系数(原始值)';
    columnMap('Pearson_Norm') = '皮尔逊相关系数(归一化值)';
    columnMap('StartTemp') = '分段起始温度(K)';
    columnMap('EndTemp') = '分段结束温度(K)';
    columnMap('MaxWeightLoss') = '最大失重(%)';
    columnMap('TempAtMaxWeightLoss') = '最大失重温度(K)';
    columnMap('MaxDTG') = '最大失重速率(%/K)';
    columnMap('TempAtMaxDTG') = '最大失重速率温度(K)';
    
    % 获取原始列名并检查长度
    origHeaders = originalTable.Properties.VariableNames;
    if length(origHeaders) ~= length(unique(origHeaders))
        error('列名存在重复，无法正确翻译');
    end
    
    % 准备中文表头
    chineseHeaders = cell(1, length(origHeaders));
    
    % 遍历翻译每个列名
    for i = 1:length(origHeaders)
        if isKey(columnMap, origHeaders{i})
            chineseHeaders{i} = columnMap(origHeaders{i});
        else
            % 处理未知列名 - 使用自动命名规则
            % fprintf('警告: 未知列名 "%s"，将保留原名称\n', origHeaders{i});
            chineseHeaders{i} = origHeaders{i};
        end
    end
    
    % 创建中文列名的表格副本
    chineseTable = originalTable;
    chineseTable.Properties.VariableNames = chineseHeaders;
    
    % 添加表格描述
    chineseTable.Properties.Description = '小热重全局样品综合排序评分表';
end