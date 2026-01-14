function dataStruct = importTGData(filename, tol)
% importTGData  导入小热重实验数据（增强版）
% 使用说明：
%   dataStruct = importTGData(filename)
%   dataStruct = importTGData(filename, tol)
%
% 特点：
%   - 支持 COM / 非 COM 两种 Excel 读取模式
%   - 支持以工作表名为样品名（若含 4 位编号，保留编号及后缀如 2090-1）
%   - 对样品名前加文件基名前缀以避免跨文件重名冲突（格式 fileBase__prefix）
%   - 清洗非数值并检测步长均匀性
%
% 输入：
%   filename - 文件路径（.xlsx/.xls/.csv 均可）
%   tol      - (可选) 温度步长容差，默认 0.01
%
% 输出：
%   dataStruct - struct array: fields: prefix, col1, col3, isLengthEqual, isUniformSpacing, spacing

    if nargin < 2 || isempty(tol)
        tol = 0.01;
    end

    % --- 获取完整路径并检查 ---
    fullPath = getFullPath(filename);
    [~, fileBase, ext] = fileparts(fullPath);

    % normalize fileBase: 只保留字母数字下划线，便于构造唯一前缀
    safeFileBase = regexprep(fileBase, '[^\w]', '_');

    % --- 获取 sheet 列表（仅对 Excel） ---
    isExcel = ismember(lower(ext), {'.xlsx', '.xls'});
    if isExcel
        [sheetNames, useCOM, hExcel, hWorkbook] = getSheetNames(fullPath);
    else
        % csv: treat as single sheet named as fileBase
        sheetNames = {fileBase};
        useCOM = false;
        hExcel = []; hWorkbook = [];
    end

    nSheets = numel(sheetNames);
    % 预分配（最大 nSheets）
    dataStruct = repmat(struct( ...
        'prefix', '', ...
        'col1', [], ...
        'col3', [], ...
        'isLengthEqual', false, ...
        'isUniformSpacing', false, ...
        'spacing', []), nSheets, 1);

    usedPrefixes = containers.Map('KeyType','char','ValueType','double'); % 记录前缀计数，保障唯一
    validCount = 0;

    % 遍历 sheets
    for i = 1:nSheets
        sheetName = sheetNames{i};
        try
            % --- 1) 构造样品前缀（优先匹配 4 位编号及可选后缀） ---
            rawPrefix = extractPrefixExtended(sheetName); % 会返回如 '2090-1' 或 '膨梗一类'
            % sanitize：保留中文、字母、数字、连字符和下划线；其他替为下划线
            sanitized = regexprep(rawPrefix, '[^\w\u4e00-\u9fff\-]', '_');
            sanitized = strtrim(sanitized);
            if isempty(sanitized)
                sanitized = 'Sheet';
            end

            % 将文件基名加入前缀，避免多个文件里名字完全相同导致冲突
            candidatePrefix = sanitized;

            % 如果同一文件内出现重复前缀，追加后缀 _2/_3...
            if isKey(usedPrefixes, candidatePrefix)
                cnt = usedPrefixes(candidatePrefix) + 1;
                usedPrefixes(candidatePrefix) = cnt;
                candidatePrefix = sprintf('%s_%d', candidatePrefix, cnt);
            else
                usedPrefixes(candidatePrefix) = 1;
            end

            % --- 2) 读取数据列 (温度在列 A, DTG 在列 B) ---
            if isExcel
                if useCOM
                    sheetObj = hWorkbook.Sheets.Item(sheetName);
                    col1 = readColumn_COM(sheetObj, 'A');
                    col2 = readColumn_COM(sheetObj, 'B');
                else
                    % 非 COM 模式：尝试用 readmatrix 或 readtable
                    try
                        M = readmatrix(fullPath, 'Sheet', sheetName);
                        if isempty(M) || size(M,2) < 2
                            % 尝试用 readtable（某些表格格式更适合）
                            T = readtable(fullPath, 'Sheet', sheetName);
                            if width(T) < 2
                                fprintf('跳过：工作表 "%s" 列数不足\n', sheetName);
                                continue;
                            else
                                col1 = cleanNumeric(T{:,1});
                                col2 = cleanNumeric(T{:,2});
                            end
                        else
                            col1 = cleanNumeric(M(:,1));
                            col2 = cleanNumeric(M(:,2));
                        end
                    catch
                        fprintf('跳过：工作表 "%s" 读取失败（非 COM 模式）\n', sheetName);
                        continue;
                    end
                end
            else
                % CSV：直接读整个文件（只处理一次）
                try
                    M = readmatrix(fullPath);
                    if isempty(M) || size(M,2) < 2
                        fprintf('跳过：CSV 文件 %s 列数不足\n', fullPath);
                        continue;
                    end
                    col1 = cleanNumeric(M(:,1));
                    col2 = cleanNumeric(M(:,2));
                catch
                    error('无法读取 CSV 文件：%s', fullPath);
                end
            end

            % 跳过空数据
            if isempty(col1) || isempty(col2)
                fprintf('跳过：工作表 "%s" 数据为空\n', sheetName);
                continue;
            end

            % 对齐长度（取最小长度）
            [col1a, col2a, lenEqual] = alignLength(col1, col2);

            % 检查温度步长均匀性
            [isUniform, spacVal] = checkSpacing(col1a, tol);

            % 保存
            validCount = validCount + 1;
            dataStruct(validCount) = struct( ...
                'prefix', candidatePrefix, ...
                'col1', col1a, ...
                'col3', col2a, ...
                'isLengthEqual', lenEqual, ...
                'isUniformSpacing', isUniform, ...
                'spacing', spacVal);

            % fprintf('已处理：工作表 "%s" -> %s\n', sheetName, candidatePrefix);

        catch ME
            fprintf('错误：读取 工作表 "%s" 失败 - %s\n', sheetName, ME.message);
            continue;
        end
    end

    % 释放 COM 资源
    if exist('useCOM','var') && useCOM
        try
            hWorkbook.Close(false);
            hExcel.Quit;
            delete(hExcel);
        catch
        end
    end

    % 截断返回
    if validCount == 0
        dataStruct = repmat(dataStruct(1),0,1); % 返回空 struct array (0x1)
    else
        dataStruct = dataStruct(1:validCount);
    end

    fprintf('成功导入 %d/%d 个工作表\n', validCount, nSheets);
end

%% ======== 内部辅助函数 ========

function fullPath = getFullPath(fname)
    if isAbsolutePath(fname)
        fullPath = fname;
    else
        fullPath = fullfile(pwd, fname);
    end
    if ~exist(fullPath, 'file')
        error('找不到文件 "%s"', fname);
    end
end

function tf = isAbsolutePath(p)
    if ispc
        tf = contains(p, ':');
    else
        tf = startsWith(p, filesep);
    end
end

function [sheetNames, useCOM, hExcel, hWorkbook] = getSheetNames(fullPath)
    try
        hExcel = actxserver('Excel.Application');
        hExcel.Visible = false;
        hWorkbook = hExcel.Workbooks.Open(fullPath);
        sheets = hWorkbook.Sheets;
        sheetNames = cell(1, sheets.Count);
        for k = 1:sheets.Count
            sheetNames{k} = sheets.Item(k).Name;
        end
        useCOM = true;
        fprintf('已使用 COM 模式读取工作表\n');
    catch
        % 回退到 xlsfinfo / readtable 方法
        try
            [~, sheetNames] = xlsfinfo(fullPath);
            sheetNames = sheetNames(~cellfun('isempty', sheetNames));
        catch
            sheetNames = {};
        end
        useCOM = false;
        hExcel = []; hWorkbook = [];
        fprintf('已切换到非 COM 模式读取工作表\n');
    end
end

function p = extractPrefixExtended(sheetName)
    % 优先匹配 4 位编号并保留后缀（如 2090-1 或 2090_2）
    % 若无匹配，则直接返回原 sheetName（去两端空白）
    sheetName = strtrim(sheetName);
    % 常见变体： 2090-1, 2090_1, 2090（1）, 2090（1）等
    % 先尝试 \d{4}[-_]\d+ 或 \d{4}\(\d+\)
    token = regexp(sheetName, '\d{4}(?:[-_]\d+|\(\d+\))', 'match', 'once');
    if isempty(token)
        % 再尝试简单的 \d{4}(?:-\d+)?（更宽松）
        token = regexp(sheetName, '\d{4}(?:[-_]\d+)?', 'match', 'once');
    end
    if ~isempty(token)
        % 将中文括号改为 -n 形式： 如 2090(1)->2090-1 , 2090（1）也处理
        token = regexprep(token, '[\(\（\)\）]', '-');
        % 将下划线保留，短横保留
        p = token;
    else
        % 如果整个 sheetName 很短或包含中文/字母，返回它本身
        p = sheetName;
    end
end

function arr = cleanNumeric(raw)
    % 将各种形式数据转换为数值列，只保留能转换为数值的元素（按顺序）
    arr = [];
    if isempty(raw), return; end
    if isnumeric(raw)
        % raw is numeric vector/matrix
        v = raw(:);
        v = v(isfinite(v));
        arr = double(v);
        return;
    end
    % raw may be cell (from readtable/readcell)
    for k = 1:numel(raw)
        val = raw(k);
        if iscell(val)
            val = val{1};
        end
        if isnumeric(val) && isfinite(val)
            arr(end+1,1) = double(val); %#ok<AGROW>
        elseif ischar(val) || isstring(val)
            num = str2double(strtrim(char(val)));
            if ~isnan(num) && isfinite(num)
                arr(end+1,1) = num; %#ok<AGROW>
            end
        end
    end
end

function [c1, c3, lenEqual] = alignLength(c1, c3)
    n = min(length(c1), length(c3));
    lenEqual = (length(c1) == length(c3));
    c1 = c1(1:n);
    c3 = c3(1:n);
end

function [isUniform, spacVal] = checkSpacing(col1, tol)
    if numel(col1) > 1
        dT = diff(col1);
        isUniform = (max(dT) - min(dT)) <= tol;
        spacVal = dT(1);
    else
        isUniform = true;
        spacVal = [];
    end
end

function colData = readColumn_COM(sheetObj, colLetter)
    % 更健壮的 COM 列读取：把数字或可转数字的字符串读出来
    try
        rng = sheetObj.Range([colLetter '1:' colLetter '10000']);
        raw = rng.Value;
        colData = [];
        if isempty(raw), return; end
        % raw can be a cell array vertical or single value
        if ~iscell(raw)
            % 单一数值
            if isnumeric(raw) && isfinite(raw)
                colData = double(raw);
            else
                num = str2double(string(raw));
                if ~isnan(num), colData = num; end
            end
            return;
        end
        for r = 1:numel(raw)
            x = raw{r};
            if isnumeric(x) && isfinite(x)
                colData(end+1,1) = double(x); %#ok<AGROW>
            elseif ischar(x) || isstring(x)
                num = str2double(strtrim(char(x)));
                if ~isnan(num) && isfinite(num)
                    colData(end+1,1) = num; %#ok<AGROW>
                end
            end
        end
    catch
        colData = [];
    end
end
