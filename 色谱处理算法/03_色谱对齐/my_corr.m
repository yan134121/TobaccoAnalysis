function r = my_corr(x, y)
    % 中心化数据
    x_centered = x - mean(x);
    y_centered = y - mean(y);
    
    % 计算点积
    numerator = dot(x_centered, y_centered);
    denom_x = norm(x_centered);
    denom_y = norm(y_centered);
    
    % 处理分母为零的情况
    if denom_x < eps || denom_y < eps
        if all(x == 0) && all(y == 0)
            r = NaN;  % 两个都是零向量
        elseif isequal(x, y)
            r = 1.0;  % 完全相同（且不是零向量）
        else
            r = NaN;  % 零向量与其他向量的相关性无定义
        end
    else
        r = numerator / (denom_x * denom_y);
    end
end