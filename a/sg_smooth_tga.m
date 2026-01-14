function [D_smoothed, dtg_index] = sg_smooth_tga(D_norm, window_size,poly_order)
% SG_SMOOTH_TGA 使用Savitzky-Golay滤波器对归一化热重数据进行平滑处理
%               并计算相对于索引的导数
%
% 输入参数:
%   D_norm     - 归一化后的热重数据，应为列向量
%   window_size - 平滑窗口点数，必须为正奇数
%
% 输出参数:
%   D_smoothed - 平滑处理后的热重数据
%   dtg_index  - 相对于索引的导数（DTG曲线）
%
% 函数会自动绘制三个子图：
%   1. 原始归一化数据
%   2. 平滑后的数据
%   3. 相对于索引的导数曲线

    % 输入验证
    if nargin < 2
        error('请提供归一化数据和窗口大小两个输入参数');
    end
    
    % 确保输入是列向量
    if isrow(D_norm)
        D_norm = D_norm';
    end
    
    % 检查窗口大小是否为正奇数
    if mod(window_size, 2) == 0 || window_size <= 0
        error('窗口大小必须是正奇数');
    end
    
    % 检查窗口大小是否小于数据长度
    if window_size >= length(D_norm)
        error('窗口大小必须小于数据长度');
    end
    
    % 确定多项式阶数，通常选择2或3
    % poly_order = 2;
    
    % 确保多项式阶数小于窗口大小
    if poly_order >= window_size
        error('多项式阶数必须小于窗口大小');
    end
    
    % 应用Savitzky-Golay平滑滤波
    D_smoothed = sgolayfilt(D_norm, poly_order, window_size);
    
    % ===== 计算相对于索引的导数 =====
    % 设计Savitzky-Golay滤波器系数
    [b, g] = sgolay(poly_order, window_size);
    
    % 提取一阶导数系数（第二列）
    deriv_coeff = g(:, 2);
    
    % 计算半窗口大小
    half_win = (window_size - 1) / 2;
    
    % 初始化导数向量
    n = length(D_smoothed);
    dtg_index = zeros(n, 1);
    
    % 为边界处理创建填充数据
    padded_data = [repmat(D_smoothed(1), half_win, 1); 
                  D_smoothed; 
                  repmat(D_smoothed(end), half_win, 1)];
    
    % 计算相对于索引的导数
    for i = 1:n
        % 获取当前窗口数据
        start_idx = i;
        end_idx = i + window_size - 1;
        window_data = padded_data(start_idx:end_idx);
        
        % 计算导数
        dtg_index(i) = dot(deriv_coeff, window_data);
    end
   
end