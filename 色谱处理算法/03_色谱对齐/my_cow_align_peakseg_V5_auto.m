function [aligned_chrom2, bounds, seg_info] = my_cow_align_peakseg_V5_auto(chrom1, chrom2, opts)
% MY_COW_ALIGN_PEAKSEG
% 按 chrom1 的峰簇做不等间距分段，然后把 chrom2 按段动态规划对齐并重采样。

%% ======================步骤0： 参数与初始化 ======================
if nargin < 3, opts = struct(); end

% 设置默认参数
if ~isfield(opts,'minProminence'), opts.minProminence = 0.05; end %%用户设置的默认显著性参数
if ~isfield(opts,'maxClusterGap'), opts.maxClusterGap = 5; end
if ~isfield(opts,'t'), opts.t = 50; end
if ~isfield(opts,'smoothSpan'), opts.smoothSpan = 5; end
if ~isfield(opts,'plotDebug'), opts.plotDebug = false; end

if ~isfield(opts,'baseline'), opts.baseline = 0; end   % 基线阈值，默认 0（改成负值则更严格）


chrom1 = chrom1(:); 
chrom2 = chrom2(:);
n1 = length(chrom1); 
n2 = length(chrom2);

%% ====================== 步骤1：平滑参考信号 ======================
if opts.smoothSpan > 1
    ref_smooth = movmean(chrom1, opts.smoothSpan);
else
    ref_smooth = chrom1;
end

%% ====================== 步骤2：多区间独立峰检测 =====================
% --------------------------------------------------------------
% 若用户未提供 ranges，则默认均分 3 段
% 用户也可以手动指定任意数量区间，例如：
% opts.ranges = [1 500; 501 1500; 1501 2500; 2501 n1];
% --------------------------------------------------------------
if ~isfield(opts,'ranges')
    n = n1;
    opts.ranges = round([
        1           n/3;
        n/3+1       2*n/3;
        2*n/3+1     n
    ]);
end

numRanges = size(opts.ranges,1);

% --------------------------------------------------------------
% 若用户未提供每段 prominence，则全部默认为 opts.minProminence
% 用户也可以提供独立区间参数，如：
% opts.rangeProm = [0.1; 0.05; 0.02; 0.03];
% --------------------------------------------------------------
if ~isfield(opts,'rangeProm')
    opts.rangeProm = opts.minProminence * ones(numRanges,1);
end

% 安全检查：若 rangeProm 数组长度不足，自动以opts.minProminence补齐
if length(opts.rangeProm) < numRanges
    opts.rangeProm(end+1:numRanges) = opts.minProminence;
end

% --------------------------------------------------------------
% 开始多区间峰检测
% --------------------------------------------------------------
all_peakLocs = [];

for ri = 1:numRanges
    r_start = opts.ranges(ri,1);
    r_end   = opts.ranges(ri,2);

    % ------ 边界自动修正 ------
    r_start = max(1, r_start);
    r_end   = min(n1, r_end);
    if r_start >= r_end
        fprintf('区间 %d 无效，跳过。\n', ri);
        continue;
    end

    % ------ 局部信号 ------
    seg = ref_smooth(r_start:r_end);

    % ------ 本区间 prominence ------
    prom_i = opts.rangeProm(ri);

    % 自动按本段强度缩放（相对阈值）
    if prom_i <= 1
        base_prom = prom_i * max(seg);
        if base_prom < eps     % 避免 max 为 0
            base_prom = prom_i;
        end
    else
        % 用户给绝对阈值
        base_prom = prom_i;
    end

    % ------ my_findpeaks ------
    [pks_i, locs_i] = my_findpeaks(seg,'MinPeakProminence',base_prom);

    % ------ 调试输出 ------
    fprintf('区间 %d [%d-%d]: 找到 %d 个峰 (prom = %.4f)\n', ...
        ri, r_start, r_end, length(pks_i), base_prom);

    % ------ 局部 → 全局索引 ------
    locs_global = locs_i + r_start - 1;

    % ------ 累积 ------
    all_peakLocs = [all_peakLocs; locs_global(:)];
end

% ------ 最终排序 ------
all_peakLocs = sort(all_peakLocs);

fprintf('多区间检测共找到 %d 个峰。\n', length(all_peakLocs));


%% ================= 步骤 3：全局峰聚类并进行分段（按 maxClusterGap） =================

if isempty(all_peakLocs)
    % ------ 无峰 → 均匀分段 ------
    warning('所有区间检测无峰，使用等长分段。');

    s = ceil(n1/20);
    seg_starts = 1:s:n1;
    seg_ends   = [seg_starts(2:end)-1, n1];
    seg_clusters = {};

else
    % ------ 聚类 ------
    pk_idx = all_peakLocs(:);
    clusters = {};
    current_cluster = pk_idx(1);

    for k = 2:length(pk_idx)
        if pk_idx(k) - pk_idx(k-1) <= opts.maxClusterGap
            current_cluster = [current_cluster; pk_idx(k)];
        else
            clusters{end+1} = current_cluster;
            current_cluster = pk_idx(k);
        end
    end
    clusters{end+1} = current_cluster;

    % ------ 每个簇的中心点 ------
    cluster_centers = cellfun(@(c) round(median(c)), clusters);

    % ------ 通过峰簇之间谷底找边界 ------
    seg_ends = [];
    for k = 1:(length(cluster_centers)-1)
        left  = cluster_centers(k);
        right = cluster_centers(k+1);

        if right-left <= 1
            % 太近 → 直接用左点作为边界
            split = left;
        else
            % 从平滑信号中找局部极小值作为分段点
            [~, rel_min] = min(ref_smooth(left:right));
            split = left + rel_min - 1;
        end

        seg_ends(end+1) = split;
    end

    seg_starts = [1, seg_ends+1];
    seg_ends   = [seg_ends, n1];
    seg_clusters = clusters;
end

%% 参考信号分段图
%% ====================== 步骤4：可视化调试 （确认分段效果）======================
if opts.plotDebug
    figure('Name','COW Debug Plot','NumberTitle','off', ...
   'Units', 'normalized', ...
   'Position', [0.0, 0.0, 0.6, 0.4]);  % 初始位置和大小（可调）

    % 获取大分段区间信息
    big_ranges = opts.ranges;
    numBigRanges = size(big_ranges, 1);

    % 为每个大分段分配颜色
    big_range_colors = lines(numBigRanges);
    plot(chrom1, 'b-', 'LineWidth', 1); 
    title('chrom1 (reference) - 大分段区间标注'); 
    hold on;
    
    % 绘制大分段边界和背景色块
    for ri = 1:numBigRanges
        r_start = big_ranges(ri, 1);
        r_end = big_ranges(ri, 2);
    
        % 检查是否为有效区间
        if r_start >= r_end || r_start > n1 || r_end < 1
            fprintf('跳过无效大分段 %d [%d-%d]\n', ri, r_start, r_end);
            continue;
        end
    
        % 边界自动修正（确保在有效范围内）
        r_start = max(1, r_start);
        r_end = min(n1, r_end);
    
        % 绘制大分段边界线（粗线）
        plot([r_start r_start], ylim, '-', 'Color', big_range_colors(ri,:), 'LineWidth', 3);
        if ri == numBigRanges
            plot([r_end r_end], ylim, '-', 'Color', big_range_colors(ri,:), 'LineWidth', 3);
        end
    
        % 添加半透明背景色块突出显示大分段
        y_limits = ylim;
        patch([r_start, r_end, r_end, r_start], ...
              [y_limits(1), y_limits(1), y_limits(2), y_limits(2)], ...
              big_range_colors(ri,:), 'FaceAlpha', 0.1, 'EdgeColor', 'none');
    
        % 标注大分段编号和范围
        range_center = (r_start + r_end) / 2;
        text(range_center, max(chrom1)*0.95, sprintf('大分段%d\n[%d-%d]', ri, r_start, r_end), ...
             'HorizontalAlignment', 'center', 'FontSize', 9, 'FontWeight', 'bold', ...
             'Color', big_range_colors(ri,:), 'BackgroundColor', 'white', ...
             'EdgeColor', big_range_colors(ri,:)); 
            % 保留原来的小分段边界标注（虚线）
        for k = 1:length(seg_starts)
            x = seg_starts(k); 
            plot([x x], ylim, '--k');
        end
        grid on;
    end
end
%% ====================== 步骤5：计算每段长度 ======================
    m = length(seg_starts);
    seg_len = seg_ends - seg_starts + 1;
    fprintf('分段总数量为：%d\n', m);

%% ====================== 步骤6：保存分段信息 ======================
    seg_info.seg_starts = seg_starts(:);
    seg_info.seg_ends   = seg_ends(:);
    seg_info.seg_len    = seg_len(:);
    seg_info.peaks_clusters = seg_clusters;

%% ====================== 步骤7：动态规划（更新DP矩阵与path矩阵） ======================
    DP   = inf(m, n2);
    path = zeros(m, n2);
    
% -------- 7.1 第一段 DP 初始化 --------
    first_len = seg_len(1);
    jmin = max(first_len - opts.t, 1);
    jmax = min(first_len + opts.t, n2);
    ref_seg = chrom1(seg_starts(1):seg_ends(1));
    
    for j = jmin:jmax
        seg_to_warp = chrom2(1:j);
        warped_seg = my_linear_resample(seg_to_warp, length(ref_seg));
        DP(1, j) = 1 - my_corr(ref_seg, warped_seg);
        path(1, j) = 1;
    end

% -------- 7.2 主 DP 循环 --------
fprintf('COW DP Progress:   0%%\n');
    for i = 2:m
        % ===== 进度显示 =====
        fprintf('\b\b\b\b%3d%%', round(100*i/m));
        ref_start = seg_starts(i);
        ref_end   = seg_ends(i);
        ref_seg = chrom1(ref_start:ref_end);
        
        curr_len = length(ref_seg);
    
        theoretical_sum = sum(seg_len(1:i));
        j_lo = max(1, theoretical_sum - i*opts.t);
        j_hi = min(n2, theoretical_sum + i*opts.t);
    
        for j = j_lo:j_hi
            min_k = max(1, j - curr_len - opts.t);
            max_k = min(n2, j - curr_len + opts.t);
    
            best_cost = inf;
            best_k = 0;
    
            for k = min_k:max_k
                if k >= j, continue; end
                if DP(i-1, k) < inf
                    seg_to_warp = chrom2(k+1:min(j,n2));
                    warped_seg = my_linear_resample(seg_to_warp, curr_len);
                    cost = 1 - my_corr(ref_seg, warped_seg);
    
                    total_cost = DP(i-1, k) + cost;
                    if total_cost < best_cost
                        best_cost = total_cost;
                        best_k = k;
                    end
                end
            end
    
            if isfinite(best_cost)
                DP(i, j) = best_cost;
                path(i, j) = best_k;
            end
        end
    end

%% ====================== 步骤8：DP 回溯（确定每段结束位置） ======================
    bounds = zeros(m,1);
    [~, end_pos] = min(DP(m,:));
    bounds(m) = end_pos;

    for i = m-1:-1:1
        bounds(i) = path(i+1, bounds(i+1));
    end

%% ====================== 步骤9：根据 bounds 重采样生成输出 ======================
    aligned_chrom2 = zeros(n1,1);
    prev_end = 0;

    for i = 1:m
        start_idx = prev_end + 1;
        end_idx = bounds(i);

        ref_start = seg_starts(i);
        ref_end   = seg_ends(i);
        target_len = ref_end - ref_start + 1;

        if end_idx >= start_idx && start_idx <= n2
            orig_seg = chrom2(start_idx:min(end_idx,n2));

            if i == m && length(orig_seg) == target_len
                aligned_chrom2(ref_start:ref_end) = orig_seg;
            else
                aligned_chrom2(ref_start:ref_end) = my_linear_resample(orig_seg, target_len);
            end
        else
            aligned_chrom2(ref_start:ref_end) = 0;
        end

        prev_end = end_idx;
    end

%% ====================== 步骤10：可视化调试 ======================
if opts.plotDebug
        figure('Name','COW Debug Plot','NumberTitle','off', ...
       'Units', 'normalized', ...
       'Position', [0.0, 0.0, 0.6, 0.4]);  % 初始位置和大小（可调）

        % 获取大分段区间信息
        big_ranges = opts.ranges;
        numBigRanges = size(big_ranges, 1);

        % 为每个大分段分配颜色
        big_range_colors = lines(numBigRanges);

        % 子图1: chrom1 (reference) 带大分段标注
        subplot(3,1,1); 
        plot(chrom1, 'b-', 'LineWidth', 1); 
        title('chrom1 (reference) - 大分段区间标注'); 
        hold on;

% 绘制大分段边界和背景色块
for ri = 1:numBigRanges
    r_start = big_ranges(ri, 1);
    r_end = big_ranges(ri, 2);

    % 检查是否为有效区间
    if r_start >= r_end || r_start > n1 || r_end < 1
        fprintf('跳过无效大分段 %d [%d-%d]\n', ri, r_start, r_end);
        continue;
    end

    % 边界自动修正（确保在有效范围内）
    r_start = max(1, r_start);
    r_end = min(n1, r_end);

    % 绘制大分段边界线（粗线）
    plot([r_start r_start], ylim, '-', 'Color', big_range_colors(ri,:), 'LineWidth', 3);
    if ri == numBigRanges
        plot([r_end r_end], ylim, '-', 'Color', big_range_colors(ri,:), 'LineWidth', 3);
    end

    % 添加半透明背景色块突出显示大分段
    y_limits = ylim;
    patch([r_start, r_end, r_end, r_start], ...
          [y_limits(1), y_limits(1), y_limits(2), y_limits(2)], ...
          big_range_colors(ri,:), 'FaceAlpha', 0.1, 'EdgeColor', 'none');

    % 标注大分段编号和范围
    range_center = (r_start + r_end) / 2;
    text(range_center, max(chrom1)*0.95, sprintf('大分段%d\n[%d-%d]', ri, r_start, r_end), ...
         'HorizontalAlignment', 'center', 'FontSize', 9, 'FontWeight', 'bold', ...
         'Color', big_range_colors(ri,:), 'BackgroundColor', 'white', ...
         'EdgeColor', big_range_colors(ri,:));
end

% 保留原来的小分段边界标注（虚线）
for k = 1:length(seg_starts)
    x = seg_starts(k); 
    plot([x x], ylim, '--k');
end
grid on;

        subplot(3,1,2); 
        plot(chrom2); 
        title('chrom2 (original)'); hold on;
        plot(bounds, chrom2(bounds), 'ro');

        subplot(3,1,3); 
        plot(aligned_chrom2); 
        title('aligned chrom2');
    end
    
end