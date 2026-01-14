function data = getColumnC(filePath, maxInvalidFraction, roiRange, outlierWindow, outlierNSigma, jumpDiffThreshold, globalDeviationNSigma, TARGET_LENGTH, fitType, gamma, anchorWin, monoStart, monoEnd, edgeBlend, epsScale, slopeThreshold)
% getColumnC - 读取并鲁棒修复一列数据（最终修复版）
%
% 主要功能:
%   - 读取、解析和初步清理数据。
%   - 多阶段坏点检测（全局、局部、跳变）。
%   - 针对 monoStart:monoEnd 区间内的单调性破坏点（向上凸起）进行局部拟合修复。
%   - 对 monoStart:monoEnd 区间应用 PAVA 强制非增。
%   - **【关键修复】** 在 PAVA 之后，显式查找并替换所有水平线段为严格递减的微小斜坡，彻底消除平台期。
%   - 确保仅在 monoStart:monoEnd 区间内强制单调性，其他区间保持原有趋势。
%
% 作者: 最终修复版

    % ------------------- 参数默认值 -------------------
    if nargin < 2 || isempty(maxInvalidFraction), maxInvalidFraction = 0.15; end
    if nargin < 4 || isempty(outlierWindow), outlierWindow = 21; end
    if nargin < 5 || isempty(outlierNSigma), outlierNSigma = 4; end
    if nargin < 6 || isempty(jumpDiffThreshold), jumpDiffThreshold = 2; end
    if nargin < 7 || isempty(globalDeviationNSigma), globalDeviationNSigma = 4; end
    if nargin < 8 || isempty(TARGET_LENGTH), TARGET_LENGTH = 341; end
    if nargin < 9 || isempty(fitType), fitType = 'linear'; end
    if nargin < 10 || isempty(gamma), gamma = 0.7; end
    if nargin < 11 || isempty(anchorWin), anchorWin = 8; end
    if nargin < 12 || isempty(monoStart), monoStart = 20; end
    if nargin < 13 || isempty(monoEnd), monoEnd = 120; end
    if nargin < 14 || isempty(edgeBlend), edgeBlend = 0.6; end
    if nargin < 15 || isempty(epsScale), epsScale = 1e-9; end
    if nargin < 16 || isempty(slopeThreshold), slopeThreshold = 1e-9; end

    % 确保 outlierWindow 为奇数且至少为 3
    if mod(outlierWindow,2)==0, outlierWindow = outlierWindow + 1; end
    if outlierWindow < 3, outlierWindow = 3; end

    % ------------------- 读取文件并定位列 -------------------
    try
        C = readcell(filePath);
    catch ME
        error('无法读取文件 %s: %s', filePath, ME.message);
    end

    mask = cellfun(@(x) (ischar(x) || isstring(x)) && strcmp(string(x), '天平示数/g'), C);
    [r, c] = find(mask, 1);
    if isempty(r)
        error('文件 %s 中未找到标题 "天平示数/g"', filePath);
    end

    colCells = C(r+1:end, c);
    total = numel(colCells);
    data = nan(total, 1);
    isInvalidNumeric = false(total,1);

    for k = 1:total
        v = colCells{k};
        if isnumeric(v)
            if isscalar(v) && ~isnan(v) && ~isinf(v), data(k) = double(v);
            else isInvalidNumeric(k) = true; end
        elseif ischar(v) || isstring(v)
            s = strtrim(string(v));
            if s=="" || any(strcmpi(s,{"Err","Error","NA","N/A","-","--","---","×"}))
                isInvalidNumeric(k) = true; continue;
            end
            n = str2double(regexprep(strrep(s,',','.'),'[^0-9+\-\.eE]',''));
            if isnan(n) || isinf(n), isInvalidNumeric(k) = true; else data(k) = n; end
        else
            isInvalidNumeric(k) = true;
        end
    end

    if (sum(isInvalidNumeric) / total) > maxInvalidFraction
        error('文件 %s: 非数值项过多，超过阈值。', filePath);
    end

    if sum(~isnan(data)) < 2 * outlierWindow
        warning('文件 %s: 有效数据点过少，直接插值返回。', filePath);
        data = fillmissing(data, 'pchip', 'endvalues', 'nearest');
        data = double(data(:));
        return;
    end

    tempData = fillmissing(data, 'linear', 'endvalues', 'nearest');

    % ------------------- 多阶段坏点检测 -------------------
    median_win = max(outlierWindow, round(total/10));
    if mod(median_win,2) == 0, median_win = median_win + 1; end
    robustBaseline = medfilt1(tempData, median_win, 'truncate');
    deviation = abs(tempData - robustBaseline);
    deviation_mad = 1.4826 * median(abs(deviation - median(deviation)));
    if deviation_mad < 1e-12, deviation_mad = max(1e-9, 1.4826 * std(deviation)); end
    isGlobalOutlier = deviation > (globalDeviationNSigma * deviation_mad);

    w = outlierWindow; half = floor(w/2); n = numel(tempData);
    isHampelOutlier = false(n,1);
    for i = 1:n
        lo = max(1, i-half); hi = min(n, i+half);
        winVals = tempData(lo:hi);
        med = median(winVals); madv = median(abs(winVals - med));
        sigma = 1.4826 * madv;
        if sigma < 1e-12, sigma = max(1e-9, std(winVals)); end
        if sigma > 0 && abs(tempData(i) - med) > outlierNSigma * sigma
            isHampelOutlier(i) = true;
        end
    end

    isJumpOutlier = [false; abs(diff(tempData)) > jumpDiffThreshold];

    % ------------------- 标记所有需要修复的点 -------------------
    pointsToFix = isInvalidNumeric | isGlobalOutlier | isHampelOutlier | isJumpOutlier;
    
    if monoEnd > total, monoEnd = total; end
    if monoStart < 1, monoStart = 1; end
    if monoEnd < monoStart, monoEnd = monoStart; end

    isMonoViolator = false(total,1);
    if total >= monoEnd && monoStart < monoEnd
        isMonoViolator(monoStart:monoEnd) = [false; diff(tempData(monoStart:monoEnd)) >= 0];
    end
    
    pointsToFix(monoStart:monoEnd) = pointsToFix(monoStart:monoEnd) | isMonoViolator(monoStart:monoEnd);
    
    data_corrected = data;

    % ------------------- 局部趋势修复 (主要针对 monoStart:monoEnd 区间) -------------------
    if monoStart <= monoEnd && sum(pointsToFix(monoStart:monoEnd)) > 0
        current_idx = monoStart;
        while current_idx <= monoEnd
            if pointsToFix(current_idx)
                block_start = current_idx;
                while current_idx <= monoEnd && pointsToFix(current_idx)
                    current_idx = current_idx + 1;
                end
                block_end = current_idx - 1;

                anchor1 = block_start - 1;
                while anchor1 >= monoStart && pointsToFix(anchor1)
                    anchor1 = anchor1 - 1;
                end
                
                anchor2 = block_end + 1;
                while anchor2 <= monoEnd && pointsToFix(anchor2)
                    anchor2 = anchor2 + 1;
                end
                
                if anchor1 >= monoStart && anchor2 <= monoEnd && anchor2 > anchor1
                    pre_fit_start = max(monoStart, anchor1 - anchorWin + 1);
                    post_fit_end = min(monoEnd, anchor2 + anchorWin - 1);

                    fit_indices_candidate = [(pre_fit_start:anchor1), (anchor2:post_fit_end)]';
                    fit_indices = fit_indices_candidate(~pointsToFix(fit_indices_candidate));
                    
                    min_fit_points = 2;
                    if strcmpi(fitType, 'quad'), min_fit_points = 3; end

                    if numel(fit_indices) >= min_fit_points && numel(unique(data_corrected(fit_indices))) > 1
                        x_raw = double(fit_indices(:));
                        y_raw = double(data_corrected(fit_indices));
                        
                        x_mean = mean(x_raw); x_std = std(x_raw);
                        if x_std < eps, x_std = 1; end
                        x_norm = (x_raw - x_mean) / x_std;

                        indices_to_replace = (block_start:block_end)';
                        xrep_raw = double(indices_to_replace);
                        xrep_norm = (xrep_raw - x_mean) / x_std;

                        deg = 1;
                        if strcmpi(fitType, 'quad'), deg = 2; end
                        
                        try
                            px = polyfit(x_norm, y_raw, deg);
                            replacement_values = polyval(px, xrep_norm);
                            data_corrected(indices_to_replace) = replacement_values;
                            pointsToFix(indices_to_replace) = false;
                        catch
                            % Polyfit failed, points remain marked for fixing
                        end
                    end
                end
            else
                current_idx = current_idx + 1;
            end
        end
    end
    
    data(pointsToFix) = NaN;
    data(~isnan(data_corrected) & ~pointsToFix) = data_corrected(~isnan(data_corrected) & ~pointsToFix);
    
    data = fillmissing(data, 'pchip', 'endvalues', 'nearest');

    % ------------------- PAVA 单调性校正，并强制严格递减（最终修复版） -------------------
    % 此部分逻辑仅应用于 monoStart:monoEnd 区间
    if monoStart >= 1 && monoEnd <= total && monoEnd > monoStart
        segIdx = monoStart:monoEnd;
        sub = data(segIdx);

        % 1. 使用 PAVA 获得非增序列
        sub_iso = isotonic_decreasing_fallback(sub);

        % 2. 【关键修复】将所有水平段转换为严格递减的斜坡
        y = sub_iso;
        nseg = numel(y);
        
        range_sub = max(y) - min(y);
        if range_sub < eps, range_sub = 1; end % 避免范围为0
        strict_epsilon = max(epsScale, range_sub * 1e-7); 
        strict_epsilon = min(strict_epsilon, max(1e-6, range_sub * 1e-3));

        i = 1;
        while i < nseg
            % 检查是否为水平段的起点 (使用一个小的容差来处理浮点数)
            if abs(y(i+1) - y(i)) < (strict_epsilon / 10)
                j = i + 1;
                % 寻找这个水平段的终点
                while j < nseg && abs(y(j+1) - y(j)) < (strict_epsilon / 10)
                    j = j + 1;
                end
                
                % 水平段是从 i 到 j
                start_val = y(i);
                len_block = j - i + 1;
                
                % 创建一个严格递减的线性斜坡来替换它
                ramp = (0 : len_block - 1)' * strict_epsilon;
                y(i:j) = start_val - ramp;
                
                % 从这个块的末尾继续搜索
                i = j;
            end
            i = i + 1;
        end
        
        % 3. 双重保险：最后再快速检查一遍，确保没有遗漏
        for k = 2:nseg
            if y(k) >= y(k-1)
                y(k) = y(k-1) - strict_epsilon;
            end
        end

        data(segIdx) = y;
    end

    data = double(data(:));
end


%% ----------------- 辅助函数：PAVA Isotonic Regression (for non-increasing) -----------------
function y_iso = isotonic_decreasing_fallback(y)
% 使用 Pool Adjacent Violators 的简单实现（递减）
    y = y(:);
    n = numel(y);
    if n == 0
        y_iso = y; return;
    end
    
    z = -y; % 转为非减问题
    
    blocks = num2cell(z);
    
    i = 1;
    while i < numel(blocks)
        avg_i = mean(blocks{i});
        avg_i_plus_1 = mean(blocks{i+1});

        if avg_i > avg_i_plus_1 % 违反非减条件
            % 合并块 i 和 i+1
            blocks{i} = [blocks{i}; blocks{i+1}];
            blocks(i+1) = [];

            % 合并后，需要与前一个块再比较
            if i > 1
                i = i - 1;
            end
        else
            i = i + 1;
        end
    end
    
    % 从块重构序列
    y_iso = zeros(n,1);
    current_pos = 1;
    for k = 1:numel(blocks)
        avg_val = mean(blocks{k});
        len_block = numel(blocks{k});
        y_iso(current_pos : current_pos + len_block - 1) = avg_val;
        current_pos = current_pos + len_block;
    end
    
    y_iso = -y_iso; % 转回非增
end