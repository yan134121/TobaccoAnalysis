% function resampled = my_linear_resample(data, new_len)
%     % 自定义线性插值函数（替换interp1的'linear'方法）
%     % 输入：
%     %   data - 原始数据向量
%     %   new_len - 目标长度
%     % 输出：
%     %   resampled - 重采样后的数据
% 
%     orig_len = length(data);
%     if orig_len == new_len
%         resampled = data;
%         return;
%     end
% 
%     % 生成原始坐标和新坐标
%     orig_x = linspace(1, orig_len, orig_len);
%     new_x = linspace(1, orig_len, new_len);
% 
%     % 手动实现线性插值
%     resampled = zeros(1, new_len);
% 
%     for i = 1:new_len
%         % 处理边界情况
%         if new_x(i) <= 1
%             resampled(i) = data(1);
%         elseif new_x(i) >= orig_len
%             resampled(i) = data(end);
%         else
%             % 找到最近的左侧点
%             left_idx = floor(new_x(i));
% 
%             % 确保索引在有效范围内
%             if left_idx < 1
%                 left_idx = 1;
%             elseif left_idx >= orig_len
%                 left_idx = orig_len - 1;
%             end
% 
%             % 计算插值权重
%             alpha = new_x(i) - left_idx;
% 
%             % 线性插值公式
%             resampled(i) = (1 - alpha) * data(left_idx) + alpha * data(left_idx + 1);
%         end
%     end
% end

function resampled = my_linear_resample(data, new_len)
    data = data(:);  % 确保是列向量
    orig_len = length(data);
    
    if orig_len == new_len
        resampled = data;
        return;
    end
    
    orig_x = linspace(1, orig_len, orig_len);
    new_x = linspace(1, orig_len, new_len);
    resampled = zeros(new_len, 1);
    
    for i = 1:new_len
        if new_x(i) <= 1
            resampled(i) = data(1);
        elseif new_x(i) >= orig_len
            resampled(i) = data(end);
        else
            left_idx = floor(new_x(i));
            left_idx = max(1, min(left_idx, orig_len - 1));
            alpha = new_x(i) - left_idx;
            resampled(i) = (1 - alpha) * data(left_idx) + alpha * data(left_idx + 1);
        end
    end
end
