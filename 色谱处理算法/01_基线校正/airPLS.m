function [Yc, Z, dssn] = airPLS(Y, lambda, order, wep, p, itermax)
% 基于自适应迭代加权惩罚最小二乘的基线校正
% 输入参数:
%   Y - 色谱数据矩阵 (m×n, m为样本数, n为变量数)
%   lambda - 平滑参数，值越大基线越平滑 (默认: 1e5)
%   order - 差分阶数 (默认: 2)
%   wep - 权重例外比例，用于保护起始和结束区域 (默认: 0)
%   p - 不对称参数，控制基线拟合的对称性 (默认: 0.05)
%   itermax - 最大迭代次数 (默认: 50)
%
% 输出参数:
%   Yc - 校正后的光谱或色谱数据 (m×n)
%   Z - 拟合的基线向量 (m×n)
%   dssn - 最终的负偏差总和
%
% 示例:
%   Yc = airPLS(Y);
%   [Yc, Z] = airPLS(Y, 1e5, 2, 0.1, 0.5, 20);
%
% 参考文献:
%   [1] Eilers, P. H. C. (2003). A perfect smoother. Analytical Chemistry.
%   [2] Eilers, P. H. C. (2005). Baseline correction with asymmetric least squares smoothing.
%   [3] Gan, F., et al. (2006). Baseline correction by improved iterative polynomial fitting.

    % 参数验证和默认值设置
    if nargin < 1
        error('airPLS:NotEnoughInputs', '必须提供输入数据Y');
    end
    
    % 设置默认参数
    if nargin < 6 || isempty(itermax)
        itermax = 50;
    end
    if nargin < 5 || isempty(p)
        p = 0.05;
    end
    if nargin < 4 || isempty(wep)
        wep = 0;
    end
    if nargin < 3 || isempty(order)
        order = 2;
    end
    if nargin < 2 || isempty(lambda)
        lambda = 1e5;
    end
    
    % 验证输入参数的有效性
    validateInputParameters(Y, lambda, order, wep, p, itermax);
    
    % 转换单精度为双精度
    if isa(Y, 'single')
        Y = double(Y);
    end
    
    % 获取数据维度
    [numSamples, numVariables] = size(Y);
    
    % 初始化输出矩阵
    Z = zeros(numSamples, numVariables);
    Yc = zeros(numSamples, numVariables);
    
    % 预计算常数矩阵
    [D, DD, weightIndices] = precomputeMatrices(numVariables, order, lambda, wep);
    
    % 对每个样本进行处理
    for sampleIdx = 1:numSamples
        currentSpectrum = Y(sampleIdx, :);
        [baseline, finalDssn] = fitBaseline(currentSpectrum, D, DD, weightIndices, p, itermax);
        
        Z(sampleIdx, :) = baseline;
        dssn(sampleIdx) = finalDssn; %#ok<AGROW>
    end
    
    % 计算校正后的数据
    Yc = Y - Z;
end

%% 验证输入参数
function validateInputParameters(Y, lambda, order, wep, p, itermax)
    % 验证Y
    if ~isnumeric(Y) || isempty(Y) || any(~isfinite(Y(:)))
        error('airPLS:InvalidY', '输入Y必须是有限数值矩阵');
    end
    
    % 验证lambda
    if ~isnumeric(lambda) || lambda <= 0
        error('airPLS:InvalidLambda', 'lambda必须是正数');
    end
    
    % 验证order
    if ~isnumeric(order) || order < 1 || mod(order, 1) ~= 0
        error('airPLS:InvalidOrder', 'order必须是正整数');
    end
    
    % 验证wep
    if ~isnumeric(wep) || wep < 0 || wep > 0.5
        error('airPLS:InvalidWep', 'wep必须在0到0.5之间');
    end
    
    % 验证p
    if ~isnumeric(p) || p < 0 || p > 1
        error('airPLS:InvalidP', 'p必须在0到1之间');
    end
    
    % 验证itermax
    if ~isnumeric(itermax) || itermax < 1 || mod(itermax, 1) ~= 0
        error('airPLS:InvalidIterMax', 'itermax必须是正整数');
    end
end

%% 预计算常数矩阵
function [D, DD, weightIndices] = precomputeMatrices(numVariables, order, lambda, wep)
    % 创建差分矩阵
    D = diff(speye(numVariables), order);
    
    % 计算惩罚矩阵
    DD = lambda * (D' * D);
    
    % 计算权重例外索引
    startIdx = 1:ceil(numVariables * wep);
    endIdx = floor(numVariables - numVariables * wep + 1):numVariables;
    weightIndices = [startIdx, endIdx];
end

%% 拟合基线
function [baseline, finalDssn] = fitBaseline(spectrum, D, DD, weightIndices, p, maxIterations)
    numVariables = length(spectrum);
    
    % 初始化权重
    weights = ones(numVariables, 1);
    baseline = zeros(1, numVariables);
    finalDssn = 0;
    
    % 迭代拟合
    for iter = 1:maxIterations
        % 创建权重矩阵
        W = spdiags(weights, 0, numVariables, numVariables);
        
        % 解线性系统
        try
            C = chol(W + DD);
            temp = C \ (C' \ (weights .* spectrum'));
            baseline = temp';
        catch
            % 如果Cholesky分解失败，使用更稳定的方法
            warning('airPLS:CholeskyFailed', 'Cholesky分解失败，使用反斜杠运算符');
            baseline = (W + DD) \ (weights .* spectrum');
            baseline = baseline';
        end
        
        % 计算残差
        residuals = spectrum - baseline;
        
        % 计算负偏差总和
        negativeResiduals = residuals(residuals < 0);
        dssn = abs(sum(negativeResiduals));
        finalDssn = dssn;
        
        % 收敛检查
        if dssn < 0.001 * sum(abs(spectrum))
            break;
        end
        
        % 更新权重
        weights = updateWeights(weights, residuals, weightIndices, p, iter, dssn);
    end
end

%% 更新权重
function newWeights = updateWeights(currentWeights, residuals, weightIndices, p, iteration, dssn)
    newWeights = currentWeights;
    
    % 正残差的权重设为0
    newWeights(residuals >= 0) = 0;
    
    % 保护区域的权重设为p
    newWeights(weightIndices) = p;
    
    % 负残差的权重按指数更新
    negativeMask = (residuals < 0);
    newWeights(negativeMask) = exp(iteration * abs(residuals(negativeMask)) / dssn);
end
