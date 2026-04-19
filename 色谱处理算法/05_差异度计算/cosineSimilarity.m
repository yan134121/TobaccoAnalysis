function similarity = cosineSimilarity(signal1, signal2)
% COSINESIMILARITY 计算两个信号的余弦相似度
% 输入：signal1, signal2 - 两个信号向量（需同长度）
% 输出：similarity - 余弦相似度值 [-1,1]

    % 校验输入
    if length(signal1) ~= length(signal2)
        error('两个信号长度必须相同');
    end
    
    % 转换为行向量
    A = signal1(:)';
    B = signal2(:)';
    
    % 计算点积和模长
    dot_product = sum(A .* B);
    norm_A = sqrt(sum(A.^2));
    norm_B = sqrt(sum(B.^2));
    
    % 避免除零
    if norm_A == 0 || norm_B == 0
        similarity = 0;
        return;
    end
    
    % 计算余弦相似度并归一化到 [0,1]
    cos_sim = dot_product / (norm_A * norm_B);
    similarity = cos_sim;
end