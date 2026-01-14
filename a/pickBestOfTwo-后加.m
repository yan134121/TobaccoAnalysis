function [bestIndex, scores, details] = pickBestOfTwo(file1, file2, params)
% pickBestOfTwo 在只有两个重复样本时，自动评选出最优样本
%
% 用法示例：
%   [bestIdx, scores, details] = pickBestOfTwo('样本A.csv','样本B.csv');
%
% 输入参数：
%   file1, file2  —— 两个待比较的数据文件路径（与 generateDTGSteps 同源格式）
%   params        —— 可选参数结构体，若缺省将使用合理默认值
%
% 输出参数：
%   bestIndex     —— 最优样本索引（1 或 2）
%   scores        —— 配对指标结构体：nrmse、pearson、euclid、quality1/2 等
%   details       —— 每个样本的质量细节：LOESS 残差 RMS、平滑稳定性等
%
% 评选原则（两样本场景的可解释方案）：
%   1) 两条最终 DTG 曲线对齐后，计算配对一致性指标：NRMSE、Pearson、Euclid。
%   2) 各自质量度量：
%      - 噪声：用 LOESS 平滑残差的 RMS 表征（越小越好）。
%      - 稳定性：改变平滑跨度±20% 后平滑曲线的变动量（越小越好）。
%   3) 组合质量分数：quality = 0.7*噪声 + 0.3*稳定性，取质量更优者。
%   4) 极端平局情况下，用主峰一致性（局部相关性）作为最后决策。
%
% 说明：
%   - 本函数依赖项目内已有函数：generateDTGSteps、calculateNRMSE、smooth 等。
%   - 若需要自定义窗口或参数，可通过 params 传入。
%
% 作者：项目助手（中文注释）

    % —— 参数默认值设置 ——
    defaults.TARGET_LENGTH = 1000;         % 统一长度的目标值
    defaults.compareWindow = [];           % 比较窗口（缺省用全范围）
    defaults.loessSpan = 0.05;             % LOESS 平滑跨度（按长度比例）
    defaults.sgWindow = 21;                % SG 窗口（若下游使用）
    defaults.sgOrder = 3;                  % SG 阶数（若下游使用）
    defaults.smoothMethod = 'loess_then_sg'; % 平滑/微分方法（与项目一致）

    if nargin < 3 || isempty(params)
        params = defaults;
    else
        params = localSetDefaults(params, defaults);
    end

    % —— 读入并生成两份样本的 DTG 步骤结果 ——
    S1 = generateDTGSteps(file1, params);
    S2 = generateDTGSteps(file2, params);

    % 选取“最终 DTG”作为比较对象，若不存在则回退到已知字段
    d1 = localPickFinalDTG(S1);
    d2 = localPickFinalDTG(S2);

    % 基本健壮性检查
    if isempty(d1) || isempty(d2)
        error('pickBestOfTwo:EmptyDTG', '某个样本未生成有效 DTG 曲线。');
    end

    % —— 对齐长度（取最短） ——
    n = min(length(d1), length(d2));
    d1 = d1(1:n);
    d2 = d2(1:n);

    % —— 配对一致性指标 ——
    scores.nrmse = calculateNRMSE(d1, d2);  % 归一化均方根误差

    if all(isfinite(d1)) && all(isfinite(d2)) && std(d1) > 0 && std(d2) > 0
        R = corrcoef(d1, d2);
        scores.pearson = R(1,2);            % 皮尔森相关
    else
        scores.pearson = NaN;               % 无法计算相关时给 NaN
    end

    scores.euclid = norm(d1 - d2);          % 欧氏距离（未归一化）

    % —— 各自质量度量：LOESS 残差 RMS 和稳定性 ——
    spanPts = max(5, round(params.loessSpan * n));
    s1_loess = smooth(d1, spanPts, 'loess');
    s2_loess = smooth(d2, spanPts, 'loess');

    details(1).residual_rms = sqrt(mean((d1 - s1_loess).^2));
    details(2).residual_rms = sqrt(mean((d2 - s2_loess).^2));

    spanPtsLow  = max(5, round(0.8 * spanPts));
    spanPtsHigh = max(5, round(1.2 * spanPts));

    s1_low  = smooth(d1, spanPtsLow,  'loess');
    s1_high = smooth(d1, spanPtsHigh, 'loess');
    s2_low  = smooth(d2, spanPtsLow,  'loess');
    s2_high = smooth(d2, spanPtsHigh, 'loess');

    % 稳定性：跨度变化引起的平滑曲线差异（越小越稳定）
    details(1).stability = norm(s1_low  - s1_high) / n;
    details(2).stability = norm(s2_low  - s2_high) / n;

    % —— 组合质量分数（越小越好） ——
    w_noise = 0.7;   % 噪声权重
    w_stab  = 0.3;   % 稳定性权重
    quality1 = w_noise * details(1).residual_rms + w_stab * details(1).stability;
    quality2 = w_noise * details(2).residual_rms + w_stab * details(2).stability;

    scores.quality1 = quality1;
    scores.quality2 = quality2;

    % —— 选择最优样本 ——
    epsTie = 1e-12;  % 平局容差
    if quality1 < quality2 - epsTie
        bestIndex = 1;
    elseif quality2 < quality1 - epsTie
        bestIndex = 2;
    else
        % 极端平局：按主峰一致性决策（以局部相关性为依据）
        bestIndex = localPickByPeakConsistency(d1, d2);
    end

end

% ===== 辅助函数：参数补全 =====
function params = localSetDefaults(params, defaults)
% 将 defaults 中缺失的字段补全到 params
    fns = fieldnames(defaults);
    for i = 1:numel(fns)
        k = fns{i};
        if ~isfield(params, k) || isempty(params.(k))
            params.(k) = defaults.(k);
        end
    end
end

% ===== 辅助函数：选取最终 DTG =====
function dtg = localPickFinalDTG(S)
% 优先使用 S.dtg_final，若不存在则按已知字段回退
    dtg = [];
    if isfield(S, 'dtg_final') && ~isempty(S.dtg_final)
        dtg = S.dtg_final;
        return;
    end
    % 回退到其它常见字段（具体字段以项目现状为准）
    if isempty(dtg)
        if isfield(S, 'dtg_loess') && ~isempty(S.dtg_loess)
            dtg = S.dtg_loess;
        elseif isfield(S, 'dtg_sg') && ~isempty(S.dtg_sg)
            dtg = S.dtg_sg;
        elseif isfield(S, 'dtg_raw') && ~isempty(S.dtg_raw)
            dtg = S.dtg_raw;
        end
    end
end

% ===== 辅助函数：主峰一致性平局决策 =====
function idx = localPickByPeakConsistency(d1, d2)
% 在平局时，用主峰附近的局部相关性作为最后决策依据。
% 简化策略：
%   1) 定位两条 DTG 的全局极小值（通常 DTG 为负峰）。
%   2) 以该位置为中心，取固定半窗（默认 ~5% 长度）。
%   3) 比较两条曲线在该窗内与其各自 LOESS 平滑的相关性，高者为优。
    n = length(d1);
    [~, p1] = min(d1);   % 负峰位置（全局）
    [~, p2] = min(d2);
    p = round((p1 + p2)/2);   % 取中位近似统一窗中心
    halfWin = max(5, round(0.05 * n));
    i1 = max(1, p - halfWin);
    i2 = min(n, p + halfWin);

    % 局部 LOESS 平滑并计算相关性
    spanPts = max(5, round(0.05 * (i2 - i1 + 1)));
    s1 = smooth(d1(i1:i2), spanPts, 'loess');
    s2 = smooth(d2(i1:i2), spanPts, 'loess');

    r1 = localSafeCorr(d1(i1:i2), s1);
    r2 = localSafeCorr(d2(i1:i2), s2);

    if r1 >= r2
        idx = 1;
    else
        idx = 2;
    end
end

% ===== 辅助函数：安全相关性 =====
function r = localSafeCorr(x, y)
% 计算相关系数，遇到常量或非数值时返回 0
    if all(isfinite(x)) && all(isfinite(y)) && std(x) > 0 && std(y) > 0
        C = corrcoef(x, y);
        r = C(1,2);
    else
        r = 0;
    end
end