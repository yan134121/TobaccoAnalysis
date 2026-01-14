function [seg, rawMax] = getRawSegmentNoRepair(filePath, TARGET_LENGTH)
% 读取原始列并返回截取段（不做坏点剔除/插值），并计算用于归一化的 rawMax
% seg: 列向量长度 = TARGET_LENGTH（自动以末尾值填充）
% rawMax: 用于归一化的分母，优先使用 seg 的正向最大值，若 <= eps 则用 abs max，若仍小则设为 1.

    if nargin < 2 || isempty(TARGET_LENGTH), TARGET_LENGTH = 341; end
    try
        C = readcell(filePath);
    catch ME
        error('无法读取文件 %s: %s', filePath, ME.message);
    end
    mask = cellfun(@(x) (ischar(x) || isstring(x)) && strcmp(string(x), '天平示数/g'), C);
    [r,c] = find(mask, 1);
    if isempty(r), error('文件 %s 中未找到标题 "天平示数/g"', filePath); end

    colCells = C(r+1:end, c);
    total = numel(colCells);
    data = nan(total,1);

    for k = 1:total
        v = colCells{k};
        if isnumeric(v)
            if isscalar(v) && ~isnan(v) && ~isinf(v), data(k) = double(v); end
        elseif ischar(v) || isstring(v)
            s = strtrim(string(v));
            if s=="" || any(strcmpi(s,{"Err","Error","NA","N/A","-","--","---","×"})), continue; end
            n = str2double(regexprep(strrep(s,',','.'),'[^0-9+\-\.eE]',''));
            if ~isnan(n) && ~isinf(n), data(k) = n; end
        end
    end

    START_IDX = 60;
    if numel(data) < START_IDX
        error('文件 %s 数据长度不足: %d < %d', filePath, numel(data), START_IDX);
    end
    endIdx = min(START_IDX + TARGET_LENGTH - 1, numel(data));
    seg = data(START_IDX:endIdx);
    if numel(seg) < TARGET_LENGTH
        seg = [seg; repmat(seg(end), TARGET_LENGTH - numel(seg), 1)];
    end

    % 计算 rawMax（优先正向最大值）
    posMax = max(seg);
    if posMax > eps
        rawMax = posMax;
    else
        rawMax = max(abs(seg));
        if rawMax < eps
            rawMax = 1.0; % fallback 防止除0
        end
    end

    seg = seg(:);
end
