function [bestSampleDTG, S, processedData, bestFilePath] = processReplicateGroup(fileGroup, varargin)
    p = inputParser;
    addRequired(p, 'fileGroup', @(x) iscell(x) && all(cellfun(@ischar, x)) && ~isempty(x));
    addParameter(p, 'NormalizeRange', [0, 100]);
    % 移除了 WindowSizes 和 PolyOrders，因为平滑方式已改变
    addParameter(p, 'Metrics', struct('NRMSE',1));
    addParameter(p, 'MinLength', 60);
    addParameter(p, 'DefaultSG', [13,2]); % 仅用于微分
    addParameter(p, 'JumpThreshold', []);
    addParameter(p, 'MaxLength', 341);
    addParameter(p, 'InvalidFractionThreshold', 0.15);
    addParameter(p, 'CompareWindow', 'all');
    addParameter(p, 'OutlierWindow', 11);
    addParameter(p, 'OutlierNSigma', 2);
    addParameter(p, 'JumpDiffThreshold', 20);
    addParameter(p, 'GlobalDeviationNSigma', 4);
    % 新增：LOESS 平滑参数
    addParameter(p, 'LoessSpan', 0.2); % 与 GUI 默认值一致
    parse(p, fileGroup, varargin{:});
    cfg = p.Results;

    nFiles = numel(cfg.fileGroup); 
    TARGET_LENGTH = cfg.MaxLength;
    rawData = cell(nFiles,1); 
    truncated = cell(nFiles,1); 
    normData = cell(nFiles,1);
    smoothedData = cell(nFiles,1); 
    dtgData = cell(nFiles,1); 
    % sgParams 现在仅用于记录微分参数
    sgParams = repmat(struct('window', cfg.DefaultSG(1), 'poly', cfg.DefaultSG(2)), nFiles, 1);
    fixedStartPoint = 60;

    % 构造 roiRange (保持不变)
    if ischar(cfg.CompareWindow) && strcmpi(cfg.CompareWindow,'all')
        roiRangeForRead = [];
    else
        sRel = round(cfg.CompareWindow(1));
        if numel(cfg.CompareWindow) >= 2, eRel = round(cfg.CompareWindow(2)); else eRel = sRel; end
        sRel = max(1, min(sRel, TARGET_LENGTH)); 
        eRel = max(1, min(eRel, TARGET_LENGTH));
        if sRel <= eRel, roiRangeForRead = [fixedStartPoint + (sRel-1), fixedStartPoint + (eRel-1)];
        else, roiRangeForRead = [fixedStartPoint + (eRel-1), fixedStartPoint + (sRel-1)]; end
    end

    for iFile = 1:nFiles
        currentFile = cfg.fileGroup{iFile};
        try
            % 数据读取与修复逻辑 (保持不变)
            try
                [segFull, ~] = getRawSegmentNoRepair(currentFile, TARGET_LENGTH);
                cleanedSeg = getColumnC(segFull, cfg.InvalidFractionThreshold, [], ...
                    cfg.OutlierWindow, cfg.OutlierNSigma, cfg.JumpDiffThreshold, ...
                    cfg.GlobalDeviationNSigma, TARGET_LENGTH);
            catch
                cleanedSeg = getColumnC(currentFile, cfg.InvalidFractionThreshold, roiRangeForRead, ...
                    cfg.OutlierWindow, cfg.OutlierNSigma, cfg.JumpDiffThreshold, ...
                    cfg.GlobalDeviationNSigma);
            end
            
            rawData{iFile} = cleanedSeg; 
            data = cleanedSeg;
            if numel(data) < 10, error('数据点不足: 仅%d个点', numel(data)); end
        catch loadErr
            error('文件 %s 载入失败或被剔除: %s', currentFile, loadErr.message);
        end

        % 固定裁剪 (保持不变)
        startIdx = fixedStartPoint;
        if startIdx > numel(data), error('文件 %s 数据长度不足: %d < 起始点 %d', currentFile, numel(data), startIdx); end
        endIdx = min(startIdx + cfg.MaxLength - 1, numel(data));
        dataCut = data(startIdx:endIdx);
        if numel(dataCut) < cfg.MinLength, error('文件 %s 截断后长度不足: %d < 最小要求 %d', currentFile, numel(dataCut), cfg.MinLength); end
        truncated{iFile} = dataCut;
    end

    % 归一化 (保持不变)
    for iFile = 1:nFiles
        normData{iFile} = absMaxNormalize(truncated{iFile}, cfg.NormalizeRange(1), cfg.NormalizeRange(2)); 
    end

    % ===== 核心修改：使用 LOESS 平滑，然后用 SG 进行微分 =====
    for iFile = 1:nFiles
        currentNormData = normData{iFile};
        
        % 1. LOESS 平滑
        try
            sp = cfg.LoessSpan;
            if sp > 0 && sp < 1
                % 如果是小数，则视为百分比
                win = max(3, round(sp * numel(currentNormData)));
            else
                % 如果是整数，则直接用作窗口点数
                win = max(3, round(sp));
            end
            if mod(win, 2) == 0, win = win + 1; end % 确保奇数
            
            smoothed = smoothdata(currentNormData, 'loess', win);
        catch smoothErr
            warning('LOESS平滑处理文件 %s 失败: %s. 将使用原始归一化数据进行微分。', cfg.fileGroup{iFile}, smoothErr.message);
            smoothed = currentNormData; % 回退
        end
        smoothedData{iFile} = smoothed;
        
        % 2. 基于平滑后的数据，使用 SG 滤波器求导
        try
            % 直接使用 sg_smooth_tga 函数在已平滑的数据上计算导数
            % 注意：sg_smooth_tga 内部会再次平滑，这里我们只取其导数部分
            [~, dtg] = sg_smooth_tga(smoothed, cfg.DefaultSG(1), cfg.DefaultSG(2));
        catch derivErr
             warning('SG微分处理文件 %s 失败: %s. 将使用一阶差分作为替代。', cfg.fileGroup{iFile}, derivErr.message);
             dtg_diff = diff(smoothed);
             dtg = [dtg_diff; dtg_diff(end)]; % 补齐长度
        end
        dtgData{iFile} = dtg;
    end

    % 多指标评估 (保持不变)
    metricNames = fieldnames(cfg.Metrics); 
    rawWeights = cell2mat(struct2cell(cfg.Metrics));
    if any(rawWeights) && ~all(rawWeights == 0)
        weights = rawWeights / sum(rawWeights); 
    else 
        weights = ones(size(rawWeights)) / numel(rawWeights); 
    end

    if nFiles == 1
        S = struct('consensusIndex', 1, 'selectedIndex', 1, 'metricType', 'SingleSample', 'metricNames', {metricNames}, 'weights', weights, 'rawScores', 1, 'compositeScore', 1);
        bestSampleDTG = dtgData{1}; 
        bestFilePath = cfg.fileGroup{1};
    else
        S = evaluateDTGReplicates(metricNames, weights, dtgData{:});
        bestSampleDTG = dtgData{S.consensusIndex}; 
        S.selectedIndex = S.consensusIndex; 
        bestFilePath = cfg.fileGroup{S.consensusIndex};
    end

    processedData = struct('rawData', {rawData}, 'truncated', {truncated}, 'normData', {normData}, 'smoothedData', {smoothedData}, 'dtgData', {dtgData}, 'sgParams', sgParams, 'bestSampleDTG', bestSampleDTG);
    S.normalizeRange = cfg.NormalizeRange; 
    S.minLength = cfg.MinLength; 
    S.maxLength = cfg.MaxLength;
end