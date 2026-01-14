function [bestParams, metrics] = compareSGParams(D_norm, windowSizes, polyOrders)
% compareSGParams   比较多组 Savitzky–Golay 参数组合并推荐最佳
%
%   [bestParams, metrics] = compareSGParams(D_norm, windowSizes, polyOrders)
%
%   输入：
%     D_norm       - 归一化后的向量数据 (Nx1)
%     windowSizes  - 奇数向量，待测试的窗口长度列表，如 [7,9,11,13,15]
%     polyOrders   - 正整数向量，待测试的多项式阶数列表，如 [2,3,4]
%
%   输出：
%     bestParams   - 结构体，包含最佳的 window_size 和 poly_order
%     metrics      - 结构体数组，每元素包含以下字段：
%         .window    - 窗口大小
%         .poly      - 多项式阶数
%         .RSS       - 残差平方和 sum((D_norm - D_smoothed).^2)
%         .rough_s   - 平滑后曲线的二阶差分平方和 sum(diff(D_smoothed,2).^2)
%         .rough_d   - DTG 曲线的二阶差分平方和 sum(diff(dtg,2).^2)
%         .combined  - 归一化 RSS + 归一化 rough_s + 归一化 rough_d
%
%   方法：
%     1) 对每组参数做 SG 平滑 & 导数  
%     2) 计算 RSS、平滑粗糙度和 DTG 粗糙度  
%     3) 分别归一化三个指标到 [0,1] 并求和得到 combined  
%     4) combined 最小者为推荐

    D_norm = D_norm(:);
    Nw = numel(windowSizes);
    Np = numel(polyOrders);
    idx = 0;
    
    % 预分配
    metrics = struct('window',{},'poly',{},'RSS',{},'rough_s',{},'rough_d',{},'combined',{});
    
    % 1) 计算所有参数组合的指标
    for wi = 1:Nw
        w = windowSizes(wi);
        for pi = 1:Np
            p = polyOrders(pi);
            idx = idx + 1;
            % 检查合法性
            if mod(w,2)==0 || p>=w || w>=numel(D_norm)
                % 跳过非法组合
                continue;
            end
            % 平滑与求导
            D_s = sgolayfilt(D_norm, p, w);
            % 构造导数系数并卷积
            [~, G] = sgolay(p, w);
            dtg = conv(D_s, G(:,2), 'same');
            
            % 计算指标
            RSS = sum((D_norm - D_s).^2);
            rough_s = sum(diff(D_s,2).^2);
            rough_d = sum(diff(dtg,2).^2);
            
            metrics(idx) = struct( ...
                'window',   w, ...
                'poly',     p, ...
                'RSS',      RSS, ...
                'rough_s',  rough_s, ...
                'rough_d',  rough_d, ...
                'combined', NaN );
        end
    end
    
    % 去除空条目
    metrics = metrics(~cellfun(@isempty,{metrics.window}));
    M = numel(metrics);
    
    % 2) 归一化三个指标并计算 combined
    RSSs     = [metrics.RSS]';
    rough_s_ = [metrics.rough_s]';
    rough_d_ = [metrics.rough_d]';
    nRSS     = (RSSs - min(RSSs))        / (max(RSSs)    - min(RSSs));
    ns        = (rough_s_ - min(rough_s_)) / (max(rough_s_) - min(rough_s_));
    nd        = (rough_d_ - min(rough_d_)) / (max(rough_d_) - min(rough_d_));
    
    for k = 1:M
        metrics(k).combined = nRSS(k) + ns(k) + nd(k);
    end
    
    % 3) 找出 combined 最小者
    [~, ibest] = min([metrics.combined]);
    bestParams = struct('window', metrics(ibest).window, ...
                        'poly',   metrics(ibest).poly);
    
    % % 4) 可视化
    % % 按 windowSizes 分组绘制 combined 曲线，每 poly 一条
    % figure('Name','SG 参数优化','NumberTitle','off','Position',[100 100 800 500]);
    % hold on;
    % legends = {};
    % for pi = 1:Np
    %     sel = [metrics.poly] == polyOrders(pi);
    %     ws = [metrics(sel).window];
    %     cb = [metrics(sel).combined];
    %     plot(ws, cb, '-o','LineWidth',1.2);
    %     legends{end+1} = sprintf('poly=%d', polyOrders(pi));
    % end
    % plot(bestParams.window, metrics(ibest).combined, 'rs','MarkerSize',8,'DisplayName','最佳');
    % hold off;
    % xlabel('window\_size');
    % ylabel('combined score');
    % title('Savitzky–Golay 参数组合综合评分');
    % legend([legends,'最佳'],'Location','best');
    % grid on;
end