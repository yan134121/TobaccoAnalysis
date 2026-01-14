function D_norm = absMaxNormalize(D, a, b)
    % 改进：使用 最大值(max(D)) 作为首选缩放因子（满足最大值归一化要求）
    if ~isvector(D)
        error('输入 D 必须是一维向量');
    end
    if a >= b
        error('参数 a 必须小于 b');
    end

    sz = size(D);
    Dv = D(:);

    % 首选用正向最大值（保持“最大值归一化”）
    posMax = max(Dv);
    if posMax > eps
        Dv_norm = (Dv / posMax) * (b - a) + a;
    else
        % 当没有正值时退回到绝对最大值（防止除0），并保证输出合理
        absMax = max(abs(Dv));
        if absMax < eps
            warning('向量 D 全部接近零，返回全 a 向量');
            Dv_norm = a * ones(size(Dv));
        else
            Dv_norm = (Dv / absMax) * (b - a) + a;
        end
    end

    D_norm = reshape(Dv_norm, sz);
end
