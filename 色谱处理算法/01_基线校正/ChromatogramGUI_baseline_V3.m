% =============================================================
% 多样品基线校正色谱曲线显示 GUI（标定样品自动高亮）
%
% 功能：
% 1. 扫描指定文件夹的 *_baseline.mat 文件，提取样品编号
% 2. 支持通过列表框选择样品，或手动输入编号绘制色谱曲线
% 3. 支持标定样品（0000_开头）加粗高亮显示
% 4. 支持绘图截取时间范围，仅显示指定时间段的数据
% 5. 提供“清除绘图”和“清除选择”按钮，方便操作
%
% 使用说明：
% - 在 GUI 中可多选列表框样品，或手动输入编号（空格、换行、逗号、分号分隔）
% - 点击“绘制色谱曲线”显示选中样品的基线校正曲线
% - 点击“清除绘图”仅清空图像
% - 点击“清除选择”同时清空列表框和手动输入框
% - 可通过修改 timeWindow 变量控制显示时间段，例如 [0,50]
% =============================================================

function ChromatogramGUI_baseline_V3

%% ================= 用户可配置区 =================
% dataFolder = '588个样品基线校正后数据';  % 基线校正数据文件夹
% dataFolder = '配方烟粉与1067_基线校正后数据';
dataFolder = 'BaseCorrect_0000_100';
xCol = 'x';   % TABLE 中作为横轴的列名
yCol = 'ybc'; % TABLE 中作为纵轴（基线校正后的色谱信号）

% ===== 可选：截取显示时间范围（单位与 x 列相同，比如分钟） =====
timeWindow = [3, 50];   % 仅显示 0~50 min 的数据，可修改为空 [] 显示全部

%% ================= 扫描数据文件 =================
files = dir(fullfile(dataFolder,'*_baseline.mat'));
if isempty(files)
    error('未在文件夹中找到 *_baseline.mat 文件');
end

fileNames = {files.name};

% ===== 去掉文件名前缀 Sepu_ 和后缀 _baseline.mat =====
sampleIDs = erase(fileNames, {'Sepu_','_baseline.mat'});  
% 这样 sampleIDs 就是 0000_18, 1001_1 ... 与用户输入一致

%% ================= 创建 GUI =================
fig = uifigure('Name','多样品基线校正色谱曲线显示','Position',[200 100 1600 600]);

% 列表框
lb = uilistbox(fig,...
    'Items',sampleIDs,...
    'Multiselect','on',...
    'Position',[20 80 120 300],...
    'Value',{});

uilabel(fig,'Text','选择样品（可多选）','Position',[20 560 200 25]);

% 手动输入样品编号
uilabel(fig,'Text','手动输入编号（空格或换行分隔）','Position',[20 420 250 22]);
editIDs = uieditfield(fig,'text','Position',[20 395 250 25]);

% 绘图坐标轴
ax = uiaxes(fig,'Position',[220 80 1200 480]);
grid(ax,'on'); box(ax,'on');
ax.FontSize   = 20;
ax.FontWeight = 'bold';
xlabel(ax,'Time','FontWeight','bold','FontSize',20);
ylabel(ax,'Intensity','FontWeight','bold','FontSize',20);
title(ax,'Baseline Corrected Chromatograms','FontWeight','bold','FontSize',20);



% 清除选择按钮
uibutton(fig,...
    'Text','清除选择',...
    'Position',[20 20 120 40],...
    'ButtonPushedFcn',@(btn,event) clearSelection(lb, editIDs));

% 绘制按钮
uibutton(fig,...
    'Text','绘制色谱',...
    'Position',[150 20 120 40],...
    'ButtonPushedFcn',@(btn,event) ...
        plotBaselineChrom(lb,editIDs,ax,dataFolder,fileNames,sampleIDs,xCol,yCol,timeWindow));

% 清除绘图按钮，放在“绘制色谱”右边
uibutton(fig,...
    'Text','清除绘图',...
    'Position',[280 20 120 40],...   % x 坐标改为 280
    'ButtonPushedFcn',@(btn,event) cla(ax));

end   % ===== 主函数结束 =====

%% ================= 绘图回调函数 =================
function plotBaselineChrom(lb,editIDs,ax,dataFolder,fileNames,sampleIDs,xCol,yCol,timeWindow)

% ===== 获取样品编号 =====
selectedIDs = lb.Value;  % 列表选择
manualStr = strtrim(editIDs.Value); % 文本框输入

if ~isempty(manualStr)
    % 支持空格、换行、逗号、分号分隔
    manualIDs = strtrim(strsplit(manualStr, {',',';',' ',newline}));
    manualIDs = manualIDs(~cellfun('isempty', manualIDs));  % 删除空字符串
    selectedIDs = unique([selectedIDs, manualIDs]);          % 合并选择
end

if isempty(selectedIDs)
    uialert(ax.Parent,'请至少选择或输入一个样品编号','提示');
    return;
end

cla(ax); hold(ax,'on');

% ===== 绘制色谱曲线 =====
for i = 1:numel(selectedIDs)
    idx = strcmp(sampleIDs, selectedIDs{i});
    if ~any(idx)
        warning('样品 %s 不存在，已跳过', selectedIDs{i});
        continue;
    end

    S = load(fullfile(dataFolder, fileNames{idx}));
    fn = fieldnames(S);
    T = S.(fn{1});

    % ===== 标定样品判断（0000_ 开头）=====
    isCalibration = startsWith(selectedIDs{i}, '0000_');

    % ===== 时间截取 =====
    t = T.(xCol);
    ydata = T.(yCol);
    if exist('timeWindow','var') && ~isempty(timeWindow)
        idxTime = t >= timeWindow(1) & t <= timeWindow(2);
        t = t(idxTime);
        ydata = ydata(idxTime);
    end

    % ===== 绘图 =====
    if isCalibration
        plot(ax, t, ydata, 'LineWidth', 1.5, 'DisplayName', selectedIDs{i});
    else
        plot(ax, t, ydata, 'LineWidth', 1, 'DisplayName', selectedIDs{i});
    end
end

% ===== 设置坐标轴显示范围 =====
if exist('timeWindow','var') && ~isempty(timeWindow)
    xlim(ax, timeWindow);   % x 轴显示范围固定为 timeWindow
end

% 坐标轴字体设置（加粗 + 放大 + Times New Roman）
ax.FontSize   = 14;
ax.FontWeight = 'bold';
ax.FontName   = 'Times New Roman';

xlabel(ax,'Time','FontWeight','bold','FontSize',20,'FontName','Times New Roman');
ylabel(ax,'Intensity','FontWeight','bold','FontSize',20,'FontName','Times New Roman');
title(ax,'Baseline Corrected Chromatograms','FontWeight','bold','FontSize',20,'FontName','Times New Roman');

% ===== 设置坐标轴显示范围 =====
if exist('timeWindow','var') && ~isempty(timeWindow)
    xlim(ax, timeWindow);   % x 轴显示范围固定为 timeWindow
end

% 图例设置
lgd = legend(ax,'show','Interpreter','none','Location','best');
lgd.FontSize   = 13;
lgd.FontWeight = 'bold';
lgd.FontName   = 'Times New Roman';

% 图例设置
lgd = legend(ax,'show','Interpreter','none','Location','best');
lgd.FontSize   = 13;
lgd.FontWeight = 'bold';
lgd.FontName   = 'Times New Roman';

hold(ax,'off');

end   


%% ================= 清除选择回调函数 =================
function clearSelection(lb, editIDs)
    % 清空列表框选择
    lb.Value = {};
    % 清空手动输入框
    editIDs.Value = '';
end    
% ===== 回调函数结束 =====
