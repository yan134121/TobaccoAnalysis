function [pks, locs, widths, proms] = my_findpeaks(y, varargin)
%MY_FINDPEAKS MATLAB-like peak detection
%该函数用于替换MATLAB自带的findpeaks函数

% 用法：
%   [pks, locs] = my_findpeaks(y)
%   [pks, locs] = my_findpeaks(y,'MinPeakHeight',h)
%   [pks, locs] = my_findpeaks(y,'MinPeakProminence',p)
%   [pks, locs] = my_findpeaks(y,'MinPeakDistance',d)
%   [pks, locs, widths, proms] = my_findpeaks(...)

%% 参数解析
p = inputParser;
addParameter(p,'MinPeakHeight',-inf);
addParameter(p,'MinPeakProminence',0);
addParameter(p,'MinPeakDistance',0);
parse(p,varargin{:});

minH = p.Results.MinPeakHeight;
minP = p.Results.MinPeakProminence;
minD = p.Results.MinPeakDistance;

y = y(:);
N = length(y);

%% 1. 找局部极大值（平顶峰取中心）
locs = [];
for k = 2:N-1
    if y(k) > y(k-1) && y(k) > y(k+1)
        locs(end+1) = k;
    elseif y(k) > y(k-1) && y(k) == y(k+1)
        % 平顶峰，找到平顶区间
        j = k;
        while j < N && y(j) == y(j+1)
            j = j + 1;
        end
        locs(end+1) = round((k+j)/2); % 取中点
        k = j; % 跳过平顶
    end
end
pks = y(locs);

%% 2. MinPeakHeight
idx = pks >= minH;
locs = locs(idx);
pks  = pks(idx);

%% 3. Prominence 精确计算
nPk = length(locs);
proms  = zeros(nPk,1);
widths = zeros(nPk,1);

for i = 1:nPk
    k = locs(i);
    
    % ---- 向左搜索直到遇到更高峰或开头 ----
    L = k-1;
    while L >= 1 && y(L) <= y(k)
        L = L - 1;
    end
    left_min = min(y(L+1:k));
    
    % ---- 向右搜索直到遇到更高峰或结尾 ----
    R = k+1;
    while R <= N && y(R) <= y(k)
        R = R + 1;
    end
    right_min = min(y(k:R-1));
    
    base = max(left_min, right_min);
    proms(i) = y(k) - base;
    
    % ---- 半高宽估计（线性插值） ----
    halfH = base + proms(i)/2;
    
    % 左侧交叉点
    Lx = k;
    while Lx > 1 && y(Lx) > halfH
        Lx = Lx - 1;
    end
    % 线性插值
    if Lx > 0
        L_interp = Lx + (halfH - y(Lx)) / (y(Lx+1)-y(Lx));
    else
        L_interp = 1;
    end
    
    % 右侧交叉点
    Rx = k;
    while Rx < N && y(Rx) > halfH
        Rx = Rx + 1;
    end
    if Rx <= N
        R_interp = Rx - (y(Rx) - halfH) / (y(Rx) - y(Rx-1));
    else
        R_interp = N;
    end
    
    widths(i) = R_interp - L_interp;
end

%% 4. MinPeakProminence
idx = proms >= minP;
locs   = locs(idx);
pks    = pks(idx);
proms  = proms(idx);
widths = widths(idx);

%% 5. MinPeakDistance
if minD > 0 && ~isempty(locs)
    [~, order] = sort(pks,'descend');
    locs   = locs(order);
    pks    = pks(order);
    proms  = proms(order);
    widths = widths(order);
    
    keep = true(size(locs));
    for i = 1:length(locs)
        if ~keep(i), continue; end
        dist = abs(locs - locs(i));
        idx = dist <= minD;
        idx(i) = false;
        keep(idx) = false;
    end
    
    locs   = locs(keep);
    pks    = pks(keep);
    proms  = proms(keep);
    widths = widths(keep);
    
    % 按位置排序
    [locs, idx] = sort(locs);
    pks    = pks(idx);
    proms  = proms(idx);
    widths = widths(idx);
end

end






% function [pks, locs, widths, proms] = my_findpeaks(y, varargin)
% %MY_FINDPEAKS 简化版
% %
% % 用法：
% %   [pks, locs] = my_findpeaks(y)
% %   [pks, locs] = my_findpeaks(y,'MinPeakHeight',h)
% %   [pks, locs] = my_findpeaks(y,'MinPeakProminence',p)
% %   [pks, locs] = my_findpeaks(y,'MinPeakDistance',d)
% %   [pks, locs, widths, proms] = my_findpeaks(...)
% %
% 
% %% ================= 参数解析 =================
% p = inputParser;
% addParameter(p,'MinPeakHeight',-inf);
% addParameter(p,'MinPeakProminence',0);
% addParameter(p,'MinPeakDistance',0);
% parse(p,varargin{:});
% 
% minH = p.Results.MinPeakHeight;
% minP = p.Results.MinPeakProminence;
% minD = p.Results.MinPeakDistance;
% 
% y = y(:);
% N = length(y);
% 
% %% ================= 1. 找局部极大值 =================
% dy1 = y(2:end-1) > y(1:end-2);
% dy2 = y(2:end-1) >= y(3:end);
% 
% locs = find(dy1 & dy2) + 1;
% pks  = y(locs);
% 
% %% ================= 2. MinPeakHeight =================
% idx = pks >= minH;
% locs = locs(idx);
% pks  = pks(idx);
% 
% %% ================= 3. Prominence 计算 =================
% nPk = length(locs);
% proms  = zeros(nPk,1);
% widths = zeros(nPk,1);
% 
% for i = 1:nPk
%     k = locs(i);
% 
%     % ---- 向左找谷底 ----
%     L = k;
%     while L > 1 && y(L) >= y(L-1)
%         L = L - 1;
%     end
%     left_min = min(y(L:k));
% 
%     % ---- 向右找谷底 ----
%     R = k;
%     while R < N && y(R) >= y(R+1)
%         R = R + 1;
%     end
%     right_min = min(y(k:R));
% 
%     base = max(left_min, right_min);
%     proms(i) = y(k) - base;
% 
%     % ---- 简单宽度估计（base 水平） ----
%     widths(i) = R - L;
% end
% 
% %% ================= 4. MinPeakProminence =================
% idx = proms >= minP;
% locs   = locs(idx);
% pks    = pks(idx);
% proms  = proms(idx);
% widths = widths(idx);
% 
% %% ================= 5. MinPeakDistance =================
% if minD > 0 && ~isempty(locs)
%     [pks, order] = sort(pks,'descend');
%     locs   = locs(order);
%     proms  = proms(order);
%     widths = widths(order);
% 
%     keep = true(size(locs));
% 
%     for i = 1:length(locs)
%         if ~keep(i), continue; end
%         dist = abs(locs - locs(i));
%         idx = dist <= minD;
%         idx(i) = false;
%         keep(idx) = false;
%     end
% 
%     locs   = locs(keep);
%     pks    = pks(keep);
%     proms  = proms(keep);
%     widths = widths(keep);
% 
%     % 按位置排序（符合色谱习惯）
%     [locs, idx] = sort(locs);
%     pks    = pks(idx);
%     proms  = proms(idx);
%     widths = widths(idx);
% end
% 
% end
