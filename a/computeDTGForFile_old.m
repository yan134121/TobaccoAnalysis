function dtg = computeDTGForFile_old(filePath, maxInvalidFraction, TARGET_LENGTH, compareWindow, opts)
% computeDTGForFile - 读取文件并返回 DTG（长度 = TARGET_LENGTH）
%   dtg = computeDTGForFile(filePath, maxInvalidFraction, TARGET_LENGTH, compareWindow, opts)
% compareWindow: 'all' 或 [s,e] — 相对于截取后序列 1..TARGET_LENGTH 的区间
% opts: struct 可选字段:
%   - outlierWindow
%   - outlierNSigma
%   - jumpDiffThreshold
%
% 如果 opts 未传入或字段缺失，getColumnC 会使用其默认值。

    if nargin < 3 || isempty(TARGET_LENGTH), TARGET_LENGTH = 341; end
    if nargin < 2 || isempty(maxInvalidFraction), maxInvalidFraction = 0.15; end
    if nargin < 4 || isempty(compareWindow)
        compareIdx = 1:TARGET_LENGTH;
    else
        % 确保 compareWindow 合法
        if ischar(compareWindow) && strcmpi(compareWindow,'all')
            compareIdx = 1:TARGET_LENGTH;
        elseif numel(compareWindow) ~= 2 || any(isnan(compareWindow))
            compareIdx = 1:TARGET_LENGTH;  % 回退到全区间
        else
            sidx = max(1, round(compareWindow(1)));
            eidx = min(TARGET_LENGTH, round(compareWindow(2)));
            if sidx > eidx
                compareIdx = 1:TARGET_LENGTH; % 回退到全区间
            else
                compareIdx = sidx:eidx;
            end
        end
    end

    START_IDX = 60;  % 你现有脚本中固定的截取起点

    % 计算要传给 getColumnC 的 roiRange（基于 START_IDX 与 compareWindow）
    roiRange = [];
    if ~(ischar(compareWindow) && strcmpi(compareWindow,'all'))
        % 期望 compareWindow 为 [s,e]（相对于截取后序列 1..TARGET_LENGTH）
        if ~isnumeric(compareWindow) || isempty(compareWindow)
            compareWindow = 'all';
        else
            sRel = round(compareWindow(1));
            if numel(compareWindow) >= 2
                eRel = round(compareWindow(2));
            else
                eRel = sRel;
            end
            % 保证相对区间合理
            sRel = max(1, min(sRel, TARGET_LENGTH));
            eRel = max(1, min(eRel, TARGET_LENGTH));
            if sRel <= eRel
                roiStartRaw = START_IDX + (sRel - 1);
                roiEndRaw   = START_IDX + (eRel - 1);
                roiRange = [roiStartRaw, roiEndRaw];
            else
                % 交换
                roiStartRaw = START_IDX + (eRel - 1);
                roiEndRaw   = START_IDX + (sRel - 1);
                roiRange = [roiStartRaw, roiEndRaw];
            end
        end
    end

    % 从文件获取整列（或按 roiRange 判定剔除）
    % 如果 opts 存在则将其内的 outlier/jump 参数转发给 getColumnC
    try
        if nargin >= 5 && isstruct(opts)
            w = []; ns = []; jth = [];
            if isfield(opts,'outlierWindow'), w = opts.outlierWindow; end
            if isfield(opts,'outlierNSigma'), ns = opts.outlierNSigma; end
            if isfield(opts,'jumpDiffThreshold'), jth = opts.jumpDiffThreshold; end
            % call with explicit outlier/jump if provided
            if ~isempty(w) || ~isempty(ns) || ~isempty(jth)
                % supply defaults for missing ones so arg positions match
                if isempty(w), w = []; end
                if isempty(ns), ns = []; end
                if isempty(jth), jth = []; end
                raw = getColumnC(filePath, maxInvalidFraction, roiRange, w, ns, jth);
            else
                raw = getColumnC(filePath, maxInvalidFraction, roiRange);
            end
        else
            raw = getColumnC(filePath, maxInvalidFraction, roiRange);
        end
    catch ME
        rethrow(ME);
    end

    % 后续行为与原有函数保持一致
    if numel(raw) < START_IDX
        error('文件 %s 数据长度不足: %d < %d', filePath, numel(raw), START_IDX);
    end

    endIdx = min(START_IDX + TARGET_LENGTH - 1, numel(raw));
    seg = raw(START_IDX:endIdx);
    if numel(seg) < TARGET_LENGTH
        seg = [seg; repmat(seg(end), TARGET_LENGTH - numel(seg), 1)];
    end

    % 归一化
    minVal = min(seg); maxVal = max(seg);
    if maxVal - minVal < eps
        normData = ones(TARGET_LENGTH,1) * 50;
    else
        normData = 100 * seg / maxVal;
    end

    % 在 computeDTGForFile 的 SG 平滑段替换为：
    if nargin >= 5 && isstruct(opts) && isfield(opts,'sgWindow') && isfield(opts,'sgPoly')
        % sgWin = opts.sgWindow; sgPoly = opts.sgPoly;
        sgWin = 11; sgPoly = 2;
        [~, dtg] = sg_smooth_tga(normData, sgWin, sgPoly);
    else
        try
            bestParams = compareSGParams(normData, [7,9,11,13,15,17,19,21,23,25], [2,3]);
            [~, dtg] = sg_smooth_tga(normData, bestParams.window, bestParams.poly);
        catch
            [~, dtg] = sg_smooth_tga(normData, 11, 2);
        end
    end


    % 若 dtg 异常则尝试其它 SG 参数
    if all(isnan(dtg)) || var(dtg) < eps
        cfgWins = [7,9,11,13,15,17,19,21,23,25];
        cfgPolys = [2,3];
        found = false;
        for w = cfgWins
            for p = cfgPolys
                try
                    [~, testDTG] = sg_smooth_tga(normData, w, p);
                    if ~all(isnan(testDTG)) && var(testDTG) > eps
                        dtg = testDTG; found = true; break;
                    end
                catch
                end
            end
            if found, break; end
        end
    end
end