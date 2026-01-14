function steps = generateDTGSteps(filePath, params)
% generateDTGSteps - 从单个文件路径处理DTG数据，返回所有处理步骤的结果
%
% 输入:
%   filePath - 字符串, 单个数据文件的完整路径
%   params   - 结构体, 包含了所有处理所需的参数，其字段应与GUI中的'app'结构体完全一致
%
% 输出:
%   steps    - 结构体, 包含了从原始数据到最终DTG的每一步计算结果

    % 初始化返回的结构体
    steps = struct('rawSeg',[],'cleanedRaw',[],'truncated',[],'cleaned_truncated',[],'normalized',[],'smoothed_sg',[],'smoothed_loess',[],'smoothed',[],'dtg',[],'dtg_sg_from_sg',[],'dtg_loess_from_loess',[],'diff_from_sg',[],'diff_from_loess',[],'sgParams',[]);
    
    fp = filePath; % 使用内部变量名 fp
    START_IDX = 60; % 截取数据的起始点

    try
        % 步骤 1: 读取原始数据段
        try
            % 这是一个假设的函数，您需要替换为您实际使用的读取原始数据的函数
            % getRawSegmentNoRepair 应该只读取数据，不做任何修复
            [rawSeg, ~] = getRawSegmentNoRepair(fp, params.TARGET_LENGTH);
        catch
            rawSeg = [];
        end
        steps.rawSeg = rawSeg;

        % 步骤 2: 清洗和修复原始数据 (调用 getColumnC)
        try
            cleaned = [];
            % 尝试使用所有参数调用 getColumnC
            try
                cleaned = getColumnC(fp, params.maxInvalidFraction, [], params.outlierWindow, params.outlierNSigma, params.jumpDiffThreshold, params.GlobalDeviationNSigma, params.TARGET_LENGTH, params.fitType, params.gamma, params.anchorWin, params.monoStart, params.monoEnd, params.edgeBlend, params.epsScale, params.slopeThreshold);
            catch
                % 如果失败，尝试使用较少参数的旧版本
                try
                    cleaned = getColumnC(fp, params.maxInvalidFraction, [], params.outlierWindow, params.outlierNSigma, params.jumpDiffThreshold);
                catch
                    cleaned = [];
                end
            end
            
            if ~isempty(cleaned)
                steps.cleanedRaw = cleaned;
            else
                steps.cleanedRaw = rawSeg; % 如果修复失败，则使用原始数据
            end
        catch
            steps.cleanedRaw = rawSeg; % 如果整个块失败，也使用原始数据
        end

        % 确定用于后续处理的基准数据
        if ~isempty(steps.cleanedRaw)
            rawCol = steps.cleanedRaw;
        else
            rawCol = steps.rawSeg;
        end
        
        if isempty(rawCol) || numel(rawCol) < START_IDX
            % 如果没有有效数据或数据太短，则提前返回
            warning('文件 %s 数据无效或过短，跳过处理。', fp);
            return;
        end

        % 步骤 3: 截取 (Truncate)
        endIdx = min(START_IDX + params.TARGET_LENGTH - 1, numel(rawCol));
        seg = rawCol(START_IDX:endIdx);
        if numel(seg) < params.TARGET_LENGTH
            seg = [seg; repmat(seg(end), params.TARGET_LENGTH - numel(seg), 1)];
        end
        steps.truncated = seg;

        % 同时截取已清洗的数据（用于导出）
        try
            if ~isempty(steps.cleanedRaw)
                cleaned_full = steps.cleanedRaw(:);
                if numel(cleaned_full) >= START_IDX
                    cend = min(numel(cleaned_full), endIdx);
                    cleaned_seg = cleaned_full(START_IDX:cend);
                    if numel(cleaned_seg) < params.TARGET_LENGTH
                        cleaned_seg = [cleaned_seg; repmat(cleaned_seg(end), params.TARGET_LENGTH - numel(cleaned_seg), 1)];
                    end
                    steps.cleaned_truncated = cleaned_seg;
                else
                    steps.cleaned_truncated = nan(params.TARGET_LENGTH,1);
                end
            end
        catch
             steps.cleaned_truncated = nan(params.TARGET_LENGTH,1);
        end

        % 步骤 4: 归一化 (Normalize)
        try
            normData = absMaxNormalize(seg, 0, 100);
        catch
            mx = max(abs(seg));
            if mx < eps, normData = zeros(size(seg)); else normData = 100 * seg / mx; end
        end
        steps.normalized = normData;

        % 步骤 5: SG 平滑
        try
            sgParams.window = params.sgWindow; sgParams.poly = params.sgPoly;
            [smoothed_sg, dtg_sg] = sg_smooth_tga(normData, sgParams.window, sgParams.poly);
            steps.smoothed_sg = smoothed_sg;
            steps.dtg = dtg_sg; % 遗留的dtg，基于SG平滑的归一化数据
            steps.sgParams = sgParams;
        catch ME_sg
            warning('SG平滑失败 for %s: %s', fp, ME_sg.message);
        end

        % 步骤 6: LOESS 平滑
        try
            sp = params.loessSpan;
            win = max(3, round(sp * numel(normData)));
            steps.smoothed_loess = smoothdata(normData,'loess',win);
        catch ME_loess
            warning('LOESS平滑失败 for %s: %s', fp, ME_loess.message);
        end
        
        % 步骤 7: 计算基于各种平滑的导数
        % 基于 smoothed_sg 计算
        if ~isempty(steps.smoothed_sg)
            try
                [~, d_sg_from_sg] = sg_smooth_tga(steps.smoothed_sg, params.sgWindow, params.sgPoly);
                steps.dtg_sg_from_sg = d_sg_from_sg;
            catch
                steps.dtg_sg_from_sg = nan(size(steps.smoothed_sg));
            end
            tmp = diff(steps.smoothed_sg); steps.diff_from_sg = [tmp; NaN];
        end

        % 基于 smoothed_loess 计算
        if ~isempty(steps.smoothed_loess)
            try
                [~, d_loess_from_loess] = sg_smooth_tga(steps.smoothed_loess, params.sgWindow, params.sgPoly);
                steps.dtg_loess_from_loess = d_loess_from_loess;
            catch
                steps.dtg_loess_from_loess = nan(size(steps.smoothed_loess));
            end
            tmp2 = diff(steps.smoothed_loess); steps.diff_from_loess = [tmp2; NaN];
        end
        % 统一选择最终用于分析/展示的 DTG（避免 GUI 与 分析逻辑不一致）
        try
            if isfield(params,'smoothMethod') && isfield(params,'diffMethod')
                if strcmpi(params.smoothMethod,'LOESS')
                    if strcmpi(params.diffMethod,'SG derivative')
                        steps.dtg_final = steps.dtg_loess_from_loess;
                    else
                        steps.dtg_final = steps.diff_from_loess;
                    end
                else % 'SG'
                    if strcmpi(params.diffMethod,'SG derivative')
                        steps.dtg_final = steps.dtg_sg_from_sg;
                    else
                        steps.dtg_final = steps.diff_from_sg;
                    end
                end
            else
                % 兼容旧行为：优先使用已有的 steps.dtg（SG-based）
                steps.dtg_final = steps.dtg;
            end
        catch
            steps.dtg_final = steps.dtg; % 保底
        end

    catch ME
        warning('处理文件 %s 时发生严重错误: %s', fp, ME.message);
        % 返回空的steps结构体
    end
end