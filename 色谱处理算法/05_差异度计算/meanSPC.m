function meanSPC = meanSPC(x, y)
% MEANSPC 计算两个信号的平均相对差异（Mean SPC）
%
%   meanSPC = MEANSPC(x, y) 计算向量 x 和 y 之间的平均 SPC（Signal Percent Change）。
%   SPC 是一种基于信号幅值的相对差异度度量。
%
% 输入：
%   x, y : 两个长度相同的信号向量
%
% 输出：
%   meanSPC : 两个信号的平均 SPC 值（越小表示越相似）
%   输出的结果是差异度值
%
% 示例：
%   x = [1 2 3];
%   y = [1.1 1.9 3.05];
%   mSPC = meanSPC(x, y);

    %% ======== 输入校验 ========
    if length(x) ~= length(y)
        error('两个信号长度必须相同');
    end

    %% ======== 保证输入为行向量 ========
    A = x(:)';  % 将 x 转换为行向量
    B = y(:)';  % 将 y 转换为行向量

    %% ======== 计算每个点的相对差异 SPC ========
    % q = |A - B| ：每个对应元素的绝对差
    q = abs(A - B);

    % m = A + B ：每个对应元素的幅值和
    m = A + B;

    % 避免除以零
    m(m == 0) = eps;

    % 每个点的 SPC = |A-B| / (A+B)
    SPC = q ./ m;

    %% ======== 计算平均 SPC ========
    n = length(A);               % 信号长度
    meanSPC = sum(SPC) / n;      % 平均 SPC

end
