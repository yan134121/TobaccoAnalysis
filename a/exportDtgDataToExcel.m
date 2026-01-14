function exportDtgDataToExcel(groupBests, groupInfo, outputFilename)
% EXPORTDTGDATATOEXCEL 将各组的最佳DTG数据导出到单个Excel文件的不同工作表中
%
% 功能:
%   为每个样品创建一个工作表，表中包含两列：序号和该样品的DTG数据。
%
% 输入:
%   groupBests      - Cell数组, 每个cell包含一个样品的DTG数据（列向量或行向量）。
%   groupInfo       - 结构体数组, 包含样品信息，特别是 .prefix 字段用于命名工作表。
%   outputFilename  - 字符串, 输出的Excel文件名。

    fprintf('正在导出用于差异度计算的DTG数据...\n');

    try
        % --- 文件预处理 ---
        % 如果目标Excel文件已存在，则删除它，以确保每次运行都生成一个全新的文件。
        if isfile(outputFilename)
            delete(outputFilename);
            fprintf('已删除旧的DTG数据文件: %s\n', outputFilename);
        end

        % --- 遍历并写入数据 ---
        % 遍历所有传入的有效分组样品
        for i = 1:numel(groupBests)
            % 1. 获取并处理工作表名称
            % 使用样品的 'prefix'作为工作表的基础名。
            % 调用 matlab.lang.makeValidName 来自动处理不符合Excel命名规范的字符（如 \ / ? * [ ]）。
            sheetName = matlab.lang.makeValidName(groupInfo(i).prefix);
            
            % Excel工作表名通常有31个字符的长度限制，超出部分进行截断。
            if strlength(sheetName) > 31
                sheetName = sheetName(1:31);
            end
            
            % 2. 获取该样品的DTG数据
            dtgDataVector = groupBests{i};
            
            % 3. 创建包含序号和DTG数据的表格
            % 为保证数据格式统一，确保DTG数据是列向量。
            if isrow(dtgDataVector)
                dtgDataVector = dtgDataVector';
            end
            
            % 创建序号列，其长度与DTG数据点数相同。
            serialNumbers = (1:numel(dtgDataVector))';
            
            % 使用序号和DTG数据创建table，并指定中文列名。
            dataTable = table(serialNumbers, dtgDataVector, 'VariableNames', {'序号', 'DTG数据'});
            
            % 4. 将表格写入Excel文件的指定工作表
            % writetable 函数会自动创建Excel文件（如果尚不存在），并根据'Sheet'参数添加新的工作表。
            writetable(dataTable, outputFilename, 'Sheet', sheetName);
        end
        
        fprintf('[SUCCESS] 已成功将 %d 个样品的DTG数据导出到: %s\n', numel(groupBests), outputFilename);

    catch ME
        % --- 异常处理 ---
        % 如果在写入过程中发生任何错误，捕获异常并在命令行窗口报告，避免程序中断。
        warning('导出DTG数据到Excel时发生错误: %s', ME.message);
        % 提供更详细的错误报告，以帮助定位问题。
        disp(getReport(ME, 'extended', 'hyperlinks', 'on'));
    end
end