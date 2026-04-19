%% ========================================================================
% 脚本名称：
%   select_representative_samples.m
%
% 功能解析：
%   本脚本用于在“最终差异度计算”之前，对存在平行样的样品进行
%   代表样品筛选，并生成一个仅包含代表样品的新数据文件夹。
%
%   筛选的核心目的是：
%   - 避免同一样品的多个平行样同时参与差异度计算；
%   - 减少重复样本对整体差异度统计结果的干扰；
%   - 保证每个样品在后续分析中仅由一个“稳定、具有代表性”的
%     色谱样品参与计算。
%
% 处理规则说明：
%   1）标定样品：
%      - 样品编号以“0000_”开头；
%      - 认为不存在平行样；
%      - 不进行筛选，直接作为代表样品保留。
%
%   2）非标定样品：
%      - 仅依据样品编号“最后一个下划线后的尾缀”区分平行样；
%      - 尾缀数字仅用于区分不同平行样，不代表平行样数量或顺序。
%
%      平行样分组方式：
%      - 去除最后一个“_尾缀”后的部分，作为平行样组 ID；
%      - 组内样品视为同一原始样品的平行样。
%
%      代表样品选择规则：
%      a）平行样数量 ≥ 3：
%         - 计算每个平行样与其余平行样之间的 meanSPC；
%         - 选取 meanSPC 最小的样品作为代表；
%         - 该样品被认为最接近整体、稳定性最好。
%
%      b）平行样数量 = 2：
%         - 不进行差异度计算；
%         - 直接选取“尾缀最大的样品”作为代表；
%         - 用于避免在样本不足时引入不稳定判断。
%
%      c）平行样数量 = 1：
%         - 直接作为代表样品使用。
%
% 输出结果：
%   - 新建一个数据文件夹，仅包含代表样品对应的 MAT 文件；
%   - 文件名及文件内容保持与原始数据一致；
%   - 该文件夹作为后续差异度计算的唯一数据来源。
%
% 说明：
%   - 本脚本不会修改或删除原始数据文件；
%   - 差异度计算仅使用 segment_areas_norm_finetuned；
%   - 默认已存在 meanSPC(x, y) 函数。
% ========================================================================

clear; clc;

%% ================= 路径设置 =================
src_folder = 'All_599_finetuned_results_norm';
dst_folder = 'All_599_finetuned_results_norm_represent';

if ~exist(dst_folder,'dir')
    mkdir(dst_folder);
end

files = dir(fullfile(src_folder,'SampleData_*_norm.mat'));
fprintf('检测到 %d 个原始样品文件\n', numel(files));

%% ================= 读取样品信息 =================
% 将所有样品的关键信息（编号、分组、是否标定样、面积数据）统一存入结构体

Samples = struct([]);

for i = 1:numel(files)
    fpath = fullfile(files(i).folder, files(i).name);
    tmp   = load(fpath);
    fn    = fieldnames(tmp);
    SD    = tmp.(fn{1});
    clear tmp

    sid = normalize_id(SD.sample_id);

    % 是否为标定样（以 0000_ 开头）
    is_calib = startsWith(sid,'0000_');

    % 拆分平行样组 ID 与尾缀编号
    [group_id, suffix] = split_group_suffix(sid);

    Samples(i).file      = files(i);
    Samples(i).sample_id = sid;
    Samples(i).group_id  = group_id;
    Samples(i).suffix    = suffix;
    Samples(i).is_calib  = is_calib;

    % 仅使用 finetuned 归一化后的分段面积
    Samples(i).area = SD.segment_areas_norm_finetuned(:)';
end

%% ================= 按平行样组进行代表样品筛选 =================
all_groups   = unique({Samples.group_id});
selected_idx = [];

for g = 1:numel(all_groups)
    gid = all_groups{g};

    % 当前平行样组的所有样品索引
    idx = find(strcmp({Samples.group_id}, gid));
    grp = Samples(idx);

    % ---------- 情况 1：标定样品 ----------
    % 标定样品不认为存在平行样，直接保留
    if grp(1).is_calib
        selected_idx = [selected_idx, idx];
        continue;
    end

    n = numel(grp);

    % ---------- 情况 2：仅 1 个样品 ----------
    if n == 1
        selected_idx = [selected_idx, idx];
        continue;
    end

    % ---------- 情况 3：2 个平行样 ----------
    % 直接选择尾缀编号较大的样品作为代表
    if n == 2
        [~, k] = max([grp.suffix]);
        selected_idx = [selected_idx, idx(k)];
        continue;
    end

    % ---------- 情况 4：≥ 3 个平行样 ----------
    % 通过平行样内部 meanSPC 判断代表样品
    meanSPC_vals = zeros(n,1);

    for i = 1:n
        spc_vals = [];
        for j = 1:n
            if i == j, continue; end
            spc_vals(end+1) = meanSPC(grp(i).area, grp(j).area);
        end
        meanSPC_vals(i) = mean(spc_vals);
    end

    % 选取 meanSPC 最小的样品
    [~, k] = min(meanSPC_vals);
    selected_idx = [selected_idx, idx(k)];
end

%% ================= 复制代表样品数据文件 =================
selected_idx = unique(selected_idx);

for i = selected_idx
    src = fullfile(Samples(i).file.folder, Samples(i).file.name);
    dst = fullfile(dst_folder, Samples(i).file.name);
    copyfile(src, dst);
end

fprintf('✅ 平行样品代表筛选完成\n');
fprintf('代表样品数量：%d\n', numel(selected_idx));
fprintf('输出文件夹：%s\n', dst_folder);

%% ==================== 辅助函数 ====================
function [gid, suf] = split_group_suffix(s)
    % 功能：将样品编号拆分为“平行样组 ID”和“尾缀编号”
    % 示例：
    %   1013_1_5   -> group_id = 1013_1, suffix = 5
    %   pgyl_2     -> group_id = pgyl,   suffix = 2

    parts = split(s,'_');
    if numel(parts) == 1
        gid = s;
        suf = NaN;
        return;
    end

    suf = str2double(parts{end});
    if isnan(suf)
        % 尾缀不是数字，整体作为 group_id
        gid = s;
    else
        gid = strjoin(parts(1:end-1),'_');
    end
end

function s = normalize_id(s)
    if isstring(s),  s = char(s); end
    if isnumeric(s), s = num2str(s); end
    if iscell(s),    s = s{1}; end
    s = strtrim(s);
end
