%% ========================================================================
% 基于“标定样归一化结果”的分段面积差异度计算
% 说明：
% - 仅使用 segment_areas_norm_finetuned
% - new / Calib_meanSPC / Calib_new 参与计算但不输出
% ========================================================================
%% ========================================================================
%
% 【功能概述】
% 本脚本用于对 finetuned 归一化后的分段面积（segment_areas_norm_finetuned）
% 进行两两样品差异度计算，并结合“标定样品映射表”与“标定样差异度”
% 对 meanSPC 指标进行修正，最终输出综合差异度结果。
%
% 【主要流程】
% 1. 扫描归一化结果文件（SampleData_*_norm.mat）
% 2. 读取每个样品的 segment_areas_norm_finetuned
% 3. 对所有样品进行两两差异度计算（cosine / meanSPC / new）
%    - new / Calib_meanSPC / Calib_new 参与计算但不输出
% 4. 读取“标定样品编号列表”，构建样品 → 标定样 的映射
% 5. 将每对样品映射到对应的标定样组合
% 6. 读取“标定样差异度表”，为每对样品找到对应的 Calib_cosine
% 7. 基于 Calib_cosine 计算修正因子 f，并修正 meanSPC
% 8. 输出最终差异度表（MAT + XLSX）
%
% 【输入】
% - norm_folder：归一化 finetuned 结果文件夹
% - calib_xlsx：标定样品编号映射表
% - calib_diff_xlsx：标定样差异度（如 Segment_Area_Diff_seg31_calib.xlsx）
% - selected_metrics：参与计算的差异度指标
% - gain = 15; 修正系数，预留接口
%
% 【输出】
% - Segment_Area_Diff_finetuned.mat
% - Segment_Area_Diff_finetuned.xlsx
%
% 【说明】
% - 差异度计算仅使用 segment_areas_norm_finetuned
% - new 指标参与计算但不写入最终输出表
% - meanSPC 会根据 Calib_cosine 进行修正（meanSPC_corrected）
% - 样品编号会统一进行 normalize 处理，确保匹配一致性
%
% ========================================================================

clear; clc;

%% ================= 用户设置 =================
selected_metrics = {'cosine','meanSPC','new'};   % new 仅参与计算

% 归一化数据文件夹
norm_folder = 'All_599_finetuned_results_norm_represent';

% 标定样品及后跟样品映射表
calib_xlsx = '标定样品编号列表-全599个样品.xlsx';

%% ================= 扫描样品文件 =================
files = dir(fullfile(norm_folder,'SampleData_*_norm.mat'));
num_samples = numel(files);

fprintf('检测到 %d 个样品用于差异度计算（finetuned）\n', num_samples);
if num_samples < 2
    error('样品数量不足，无法计算差异度');
end

%% ================= 读取所有样品数据 =================
sample_ids  = cell(num_samples,1);
area_matrix = [];

for i = 1:num_samples
    fpath = fullfile(files(i).folder, files(i).name);
    tmp   = load(fpath);
    fn    = fieldnames(tmp);
    SD    = tmp.(fn{1});
    clear tmp

    % 样品编号
    sample_ids{i} = normalize_id(SD.sample_id);

    % 仅使用 finetuned 归一化面积
    area_vec = SD.segment_areas_norm_finetuned(:)';
    area_matrix(i,:) = area_vec; end

num_segs = size(area_matrix,2);
fprintf('每个样品包含 %d 个分段\n', num_segs);

%% ================= 两两差异度计算 =================
PairID_list  = {};
RefName_list = {};
CmpName_list = {};

metric_values = struct();
for k = 1:numel(selected_metrics)
    metric_values.(selected_metrics{k}) = [];
end

for i = 1:num_samples-1
    for j = i+1:num_samples

        RefName = sample_ids{i};
        CmpName = sample_ids{j};
        PairID  = [RefName '_vs_' CmpName];

        PairID_list{end+1,1}  = PairID;
        RefName_list{end+1,1} = RefName;
        CmpName_list{end+1,1} = CmpName;

        x = area_matrix(i,:);
        y = area_matrix(j,:);

        for k = 1:numel(selected_metrics)
            metric = selected_metrics{k};
            switch metric
                case 'cosine'
                    val = 1 - cosineSimilarity(x,y);
                case 'meanSPC'
                    val = meanSPC(x,y);
                case 'new'
                    val = new(x,y);
            end
            metric_values.(metric)(end+1,1) = val;
        end
    end
end

%% ================= 构建样品 -> 标定样映射 =================
calib_tbl = readtable(calib_xlsx,'ReadVariableNames',false);
calib_raw = normalize_cell(calib_tbl{:,:});
calib_headers = calib_raw(1,:);

sample_to_calib = containers.Map('KeyType','char','ValueType','char');

for c = 1:size(calib_raw,2)
    calib_id = calib_headers{c};
    if isempty(calib_id), continue; end
    calib_id = normalize_id(calib_id);
    sample_to_calib(calib_id) = calib_id;

    followers = calib_raw(2:end,c);
    for r = 1:numel(followers)
        f = followers{r};
        if isempty(f), continue; end
        f = normalize_id(f);
        sample_to_calib(f) = calib_id;
    end
end

%% ================= 构建输出表 =================
diff_table = table( ...
    PairID_list, RefName_list, CmpName_list, ...
    metric_values.cosine, metric_values.meanSPC, ...
    'VariableNames',{'PairID','RefSample','CmpSample','cosine','meanSPC'});

%% ================= 添加标定样映射 =================
RefCalib = cell(height(diff_table),1);
CmpCalib = cell(height(diff_table),1);
missing_keys = {};

for i = 1:height(diff_table)
    r = normalize_id(diff_table.RefSample{i});
    c = normalize_id(diff_table.CmpSample{i});

    if isKey(sample_to_calib,r)
        RefCalib{i} = sample_to_calib(r);
    else
        RefCalib{i} = '未知';
        missing_keys{end+1} = r;
    end

    if isKey(sample_to_calib,c)
        CmpCalib{i} = sample_to_calib(c);
    else
        CmpCalib{i} = '未知';
        missing_keys{end+1} = c;
    end
end

diff_table.RefCalibSample = RefCalib;
diff_table.CmpCalibSample = CmpCalib;

if ~isempty(missing_keys)
    fprintf('⚠ 以下样品未在标定表中出现：\n');
    disp(unique(missing_keys(:)));
end

%% ================= 读取标定样品差异度 =================
calib_diff_xlsx = 'Segment_Area_Diff_seg31_calib.xlsx';
calib_diff_tbl  = readtable(calib_diff_xlsx);

Calib_cosine = nan(height(diff_table),1);

for i = 1:height(diff_table)
    r = diff_table.RefCalibSample{i};
    c = diff_table.CmpCalibSample{i};

    if strcmp(r,c)
        Calib_cosine(i) = 0;
    else
        idx = (strcmp(calib_diff_tbl.RefSample,r) & strcmp(calib_diff_tbl.CmpSample,c)) | ...
              (strcmp(calib_diff_tbl.RefSample,c) & strcmp(calib_diff_tbl.CmpSample,r));
        if any(idx)
            Calib_cosine(i) = calib_diff_tbl.cosine(idx);
        end
    end
end

diff_table.Calib_cosine = Calib_cosine;

%% ================= meanSPC 修正 =================
gain = 15;
diff_table.f = 1 - gain .* diff_table.Calib_cosine;
diff_table.meanSPC_corrected = diff_table.meanSPC .* diff_table.f;

%% ================= 保存结果 =================
out_mat  = fullfile(norm_folder,'Segment_Area_Diff_finetuned.mat');
out_xlsx = fullfile(norm_folder,'Segment_Area_Diff_finetuned.xlsx');

save(out_mat,'diff_table');
writetable(diff_table,out_xlsx);

fprintf('\n🎉 差异度计算完成（基于标定样归一化 finetuned 面积）\n');
fprintf('MAT : %s\n',out_mat);
fprintf('XLSX: %s\n',out_xlsx);

%% ==================== 辅助函数 ====================
function s = normalize_id(s)
    if isstring(s), s = char(s); end
    if isnumeric(s), s = num2str(s); end
    if iscell(s), s = s{1}; end
    if ~ischar(s), s = ''; return; end
    s = strtrim(s);
end

function C = normalize_cell(C)
    for ii = 1:numel(C)
        v = C{ii};
        if isempty(v)
            C{ii} = '';
        elseif isstring(v)
            C{ii} = strtrim(char(v));
        elseif isnumeric(v)
            C{ii} = strtrim(num2str(v));
        elseif ischar(v)
            C{ii} = strtrim(v);
        else
            try
                C{ii} = strtrim(char(v));
            catch
                C{ii} = '';
            end
        end
    end
end
