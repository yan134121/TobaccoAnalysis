function nrmse = calculateNRMSE(y_sample, y_ref)
    % 计算归一化均方根误差 (NRMSE)
    % 输入:
    %   y_sample - 待比较的样本数据 (向量)
    %   y_ref - 参考数据 (向量)
    % 输出:
    %   nrmse - NRMSE值 (%)
    %
    % 计算公式:
    %   NRMSE = [sqrt(1/n * Σ(y_i^ref - y_i^sample)^2)] / (y_max_ref - y_min_ref) * 100%
    %
    % 注意：两个向量需要相同长度
    
    % 确保向量长度相同
    if length(y_sample) ~= length(y_ref)
        error('输入向量长度不一致');
    end
    
    % 计算参考曲线的范围
    y_range = max(y_ref) - min(y_ref);
    
    % 计算均方根误差
    mse = mean((y_ref - y_sample).^2);
    rmse = sqrt(mse);
    
    % 处理分母为零的情况
    if y_range == 0
        warning('参考曲线范围为零，无法计算NRMSE');
        nrmse = NaN;
        return;
    end
    
    % 计算归一化RMSE
    nrmse = (rmse / y_range) * 100;
end
