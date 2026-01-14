function [groupStruct, keyMap] = groupCSVFiles(folder)
% groupCSVFiles - (更新版) 递归扫描文件夹并按核心前缀分组。
%
% 更新日志:
%   - 增强了前缀提取逻辑，以兼容新文件名格式（含时间戳后缀）。
%   - 支持全角和半角括号的识别与移除。
%   - 保持了原有的特殊拆分规则。
%
% 核心分组逻辑:
%   1. 从文件名中移除末尾的时间戳 (e.g., "_20250915_102407")。
%   2. 在处理后的字符串中，提取“盘”字之前的所有内容作为基础前缀。
%   3. 从前缀中移除括号内的编号 (e.g., "(2)" 或 "（2）")。
%   4. 对清理后的前缀应用特殊的拆分规则 (e.g., "CGZ" -> "CG-Z") 以生成最终的 groupKey。
%
% 返回：
%   groupStruct - 结构数组，字段: groupKey, filePaths, fullPrefixes
%   keyMap      - containers.Map (key: groupKey (char)，value: struct array with fields filePath, fullPrefix, fileName)

    if ~isfolder(folder)
        error('[文件分组] 文件夹不存在: %s', folder);
    end

    % 递归获取所有 CSV 文件（保留 folderPath）
    csvFiles = [];
    scanSubfolders(folder);

    function scanSubfolders(currentFolder)
        curFiles = dir(fullfile(currentFolder, '*.csv'));
        if ~isempty(curFiles)
            for ii = 1:numel(curFiles)
                curFiles(ii).folderPath = currentFolder;
            end
            csvFiles = [csvFiles; curFiles];
        end
        subDirs = dir(currentFolder);
        subDirs = subDirs([subDirs.isdir] & ~ismember({subDirs.name}, {'.','..'}));
        for kk = 1:numel(subDirs)
            scanSubfolders(fullfile(currentFolder, subDirs(kk).name));
        end
    end

    fprintf('正在处理 %d 个CSV文件...\n', numel(csvFiles));

    keyMap = containers.Map('KeyType', 'char', 'ValueType', 'any');

    for iFile = 1:numel(csvFiles)
        filePath = fullfile(csvFiles(iFile).folderPath, csvFiles(iFile).name);
        [~, baseName] = fileparts(csvFiles(iFile).name);

        % =================================================================
        % ***** 修改点 1: 增强的前缀提取逻辑 *****
        % =================================================================
        % 步骤 A: 首先移除文件名末尾可能存在的时间戳后缀 (e.g., _YYYYMMDD_HHMMSS)
        % 这个正则表达式匹配一个下划线后跟数字，可能重复一次（匹配_date_time），并确保它在字符串末尾
        prefix_no_ts = regexprep(baseName, '_\d+(_\d+)?$', '');

        % 步骤 B: 在去除了时间戳的字符串中，查找“盘”字
        panPos = strfind(prefix_no_ts, '盘');
        if isempty(panPos)
            % 如果找不到“盘”，则使用整个去除了时间戳的字符串
            panPrefix = prefix_no_ts;
        else
            % 如果找到，则取第一个“盘”字前的所有内容
            panPrefix = prefix_no_ts(1:panPos(1)-1);
        end
        % =================================================================

        % 2) 去掉末尾多余 '-' 并 trim
        panPrefix = regexprep(panPrefix, '-+$', '');
        panPrefix = strtrim(panPrefix);

        % =================================================================
        % ***** 修改点 2: 支持全角和半角括号 *****
        % =================================================================
        % 3) 去掉所有形如 "(数字)" 或 "（数字）" 的编号
        %    [\(（] 匹配半角或全角左括号, [\)）] 匹配半角或全角右括号
        cleanedPrefix = regexprep(panPrefix, '[\(（]\d+[\)）]', '');
        % =================================================================

        % 4) 合并连续 '-'，去首尾 '-'，trim
        cleanedPrefix = regexprep(cleanedPrefix, '-{2,}', '-');
        cleanedPrefix = regexprep(cleanedPrefix, '^-|-$', '');
        cleanedPrefix = strtrim(cleanedPrefix);

        % 5) 特殊转换规则（保持不变）
        cp_up = upper(cleanedPrefix);

        pattern1 = '^(?<pre>.+)-(?<code>[A-Z]{3})-(?<suf>[A-Z]{2})$';
        t1 = regexp(cp_up, pattern1, 'names');

        if ~isempty(t1)
            pre  = t1.pre;
            code = t1.code;
            suf  = t1.suf;
            newKey = sprintf('%s-%s-%s-%s', pre, code(1:2), suf, code(3));
            groupKey = char(regexprep(newKey, '-{2,}', '-'));
        else
            pattern2 = '^(?<pre>.+)-(?<code>[A-Z]{3})$';
            t2 = regexp(cp_up, pattern2, 'names');
            if ~isempty(t2)
                pre = t2.pre;
                code = t2.code;
                newKey = sprintf('%s-%s-%s', pre, code(1:2), code(3));
                groupKey = char(regexprep(newKey, '-{2,}', '-'));
            else
                if isempty(cleanedPrefix)
                    groupKey = char(baseName); 
                else
                    groupKey = char(cleanedPrefix);
                end
            end
        end

        % 保存文件信息
        fileInfo = struct('filePath', filePath, 'fullPrefix', char(panPrefix), 'fileName', csvFiles(iFile).name);

        if keyMap.isKey(groupKey)
            tmp = keyMap(groupKey);
            tmp = [tmp, fileInfo];
            keyMap(groupKey) = tmp;
        else
            keyMap(groupKey) = fileInfo;
        end
    end

    % 格式化输出以兼容旧接口
    gkeys = keys(keyMap);
    nGroups = numel(gkeys);
    groupStruct = struct('groupKey', cell(1,nGroups), 'filePaths', cell(1,nGroups), 'fullPrefixes', cell(1,nGroups));
    for ig = 1:nGroups
        k = gkeys{ig};
        files = keyMap(k); 
        groupStruct(ig).groupKey = k;
        groupStruct(ig).filePaths = {files.filePath};
        groupStruct(ig).fullPrefixes = {files.fullPrefix};
    end
end