function DTG_Viewer_GUI(S, summaryT)
% DTG_Viewer_GUI 交互式可视化小热重DTG曲线（前N排名 + 参考/对比 + 分段绘制）
% Usage:
%   DTG_Viewer_GUI(S, summaryT)
% If S or summaryT not provided, it will try to load them from base workspace.

% --------------- 输入容错与准备 ---------------
if nargin < 1 || isempty(S)
    if evalin('base','exist(''S'',''var'')')
        S = evalin('base','S');
    else
        error('请传入 S 结构数组，或在 base workspace 中存在变量 S。');
    end
end
if nargin < 2 || isempty(summaryT)
    if evalin('base','exist(''summaryT'',''var'')')
        summaryT = evalin('base','summaryT');
    else
        error('请传入 summaryT 表，或在 base workspace 中存在变量 summaryT。');
    end
end

% 基本检查
if ~isstruct(S) || isempty(S)
    error('S 必须为非空结构数组');
end
if ~istable(summaryT) || isempty(summaryT)
    error('summaryT 必须为非空表格');
end

% 构造 prefixesAll（便于匹配）
nSamples = numel(S);
prefixesAll = cell(nSamples,1);
for i=1:nSamples
    if isfield(S(i),'prefix') && ~isempty(S(i).prefix)
        prefixesAll{i} = S(i).prefix;
    else
        prefixesAll{i} = sprintf('Sample_%02d', i);
    end
end

% 若 summaryT 缺少 Prefix 列，尝试自动构造（以 S 的前缀为准）
if ~any(strcmp(summaryT.Properties.VariableNames,'Prefix'))
    try
        summaryT.Prefix = prefixesAll(:);
    catch
        error('summaryT 中缺少 Prefix 列，且无法自动构造。请提供包含 Prefix 的 summaryT。');
    end
end

% 补齐 Rank/Score/GroupIndex
if ~any(strcmp(summaryT.Properties.VariableNames,'Rank')), summaryT.Rank = (1:height(summaryT))'; end
if ~any(strcmp(summaryT.Properties.VariableNames,'Score')), summaryT.Score = zeros(height(summaryT),1); end
if ~any(strcmp(summaryT.Properties.VariableNames,'GroupIndex'))
    gi = zeros(height(summaryT),1);
    for i = 1:height(summaryT)
        idx = find(strcmp(prefixesAll, summaryT.Prefix{i}),1);
        if ~isempty(idx), gi(i) = idx; else gi(i) = i; end
    end
    summaryT.GroupIndex = gi;
end

% --------------- UI 创建 ---------------
fig = uifigure('Name','DTG 排名前样本可视化与分段比较','Position',[200 100 1280 760]);

% 网格：1 行 3 列
gl = uigridlayout(fig,[1,3]);
gl.ColumnWidth = {320, '1x', 420}; % 右侧稍宽以放下新控件
gl.RowHeight = {'1x'};

%% 左侧面板：样本选择（表格 + 列表 + 按钮）
leftPanel = uipanel(gl,'Title','样本列表与选择','FontWeight','bold');
leftPanel.Layout.Row = 1; leftPanel.Layout.Column = 1;

% 使用 8 行布局；第2行（表格）和第5行（快速列表）固定高度 200 px，
% 这样表格和列表自带滚动条而不会把整个左侧撑满，按钮将一直可见。
leftGrid = uigridlayout(leftPanel,[8,1]);
leftGrid.RowHeight = {'fit', 200, 'fit', 'fit', 200, 'fit', 'fit', 'fit'};
leftGrid.Padding = [8 8 8 8];

% summaryT 表格（Rank, Prefix, Score, GroupIndex）
showTbl = table(summaryT.Rank, summaryT.Prefix, summaryT.Score, summaryT.GroupIndex, ...
    'VariableNames',{'Rank','Prefix','Score','GroupIndex'});
tbl = uitable(leftGrid,'Data', showTbl, 'ColumnEditable', false(1,4), 'RowName', []);
tbl.Layout.Row = 2; tbl.Layout.Column = 1;
% 兼容不同 MATLAB 版本的多选属性
try, tbl.MultiSelect = 'on'; catch, try tbl.Multiselect='on'; catch, end; end
tbl.CellSelectionCallback = @onTableSelect;

% Top-N 控件（小行）
hNpanel = uigridlayout(leftGrid,[1,3]);
hNpanel.Layout.Row = 3; hNpanel.Layout.Column = 1;
lblN = uilabel(hNpanel,'Text','绘制前 N 个样本:','HorizontalAlignment','left'); lblN.Layout.Row=1; lblN.Layout.Column=1;
edtN = uieditfield(hNpanel,'numeric','Value',min(10,height(showTbl)),'Limits',[1,inf]); edtN.Layout.Row=1; edtN.Layout.Column=2;
btnPlotTop = uibutton(hNpanel,'Text','Plot Top N','ButtonPushedFcn',@(btn,event) plotTopN()); btnPlotTop.Layout.Row=1; btnPlotTop.Layout.Column=3;

% listboxRanks：用于多选并保留顺序（用户首选方式）
lblLB = uilabel(leftGrid,'Text','快速选择（列表，多选并保留选择顺序）:','HorizontalAlignment','left');
lblLB.Layout.Row = 4;
itemsRanks = arrayfun(@(r,p) sprintf('%d | %s', r, p{1}), showTbl.Rank, showTbl.Prefix, 'UniformOutput', false);
listboxRanks = uilistbox(leftGrid,'Items', itemsRanks, 'Multiselect','on'); listboxRanks.Layout.Row=5;
listboxRanks.Value = listboxRanks.Items(1:min(10,end));
listboxRanks.ValueChangedFcn = @(src,event) onListboxSelectionChanged(src,event);

% 按钮：绘制（列表顺序） —— 显眼在列表下方
btnPlotList = uibutton(leftGrid,'Text','绘制选中（列表顺序）','ButtonPushedFcn',@(btn,event) plotSelectedFromList());
btnPlotList.Layout.Row=6;

% 按钮：绘制（表格选择） —— 仍保留
btnPlotTable = uibutton(leftGrid,'Text','绘制选中（表格）','ButtonPushedFcn',@(btn,event) plotSelectedFromTable());
btnPlotTable.Layout.Row=7;

% 一个帮助提示
lblHelp = uilabel(leftGrid,'Text','提示: 在列表中多选后点击 "绘制选中（列表顺序）"。', 'FontAngle','italic');
lblHelp.Layout.Row=8;

%% 中间：绘图区（单图或 2x2 子图）与下方控件
centerPanel = uipanel(gl,'Title','绘图窗口','FontWeight','bold');
centerPanel.Layout.Row = 1; centerPanel.Layout.Column = 2;
centerGrid = uigridlayout(centerPanel,[2,1]);
centerGrid.RowHeight = {'1x','fit'};

% 单一主轴（初始）
ax = uiaxes(centerGrid);
ax.Layout.Row=1; ax.Layout.Column=1;
ax.Title.String = 'DTG 曲线';
ax.XLabel.String='温度 (K)';
ax.YLabel.String='DTG (%/K)';
ax.Box='on';
ax.NextPlot='add';

% 底部控件
bottomPanel = uigridlayout(centerGrid,[1,8]);
bottomPanel.Layout.Row=2; bottomPanel.Layout.Column=1; bottomPanel.Padding=[6 6 6 6];
lblSmooth = uilabel(bottomPanel,'Text','平滑窗口 (odd):','HorizontalAlignment','right'); lblSmooth.Layout.Row=1; lblSmooth.Layout.Column=1;
sldSmooth = uislider(bottomPanel,'Limits',[1,101],'Value',11,'MajorTicks',[],'MinorTicks',[]); sldSmooth.Layout.Row=1; sldSmooth.Layout.Column=2;
sldSmooth.ValueChangedFcn = @(s,e) updateSmoothLabel();
lblSmoothVal = uilabel(bottomPanel,'Text','11','HorizontalAlignment','left'); lblSmoothVal.Layout.Row=1; lblSmoothVal.Layout.Column=3;
chkMarkers = uicheckbox(bottomPanel,'Text','显示标记','Value',false); chkMarkers.Layout.Row=1; chkMarkers.Layout.Column=4;
lblLineW = uilabel(bottomPanel,'Text','线宽:','HorizontalAlignment','right'); lblLineW.Layout.Row=1; lblLineW.Layout.Column=5;
edtLineW = uieditfield(bottomPanel,'numeric','Value',1.8,'Limits',[0.1,10]); edtLineW.Layout.Row=1; edtLineW.Layout.Column=6;
btnExport = uibutton(bottomPanel,'Text','导出图像','ButtonPushedFcn',@(btn,event) exportFigure()); btnExport.Layout.Row=1; btnExport.Layout.Column=8;

%% 右侧：图例与参数（包含更明显的 legendList） 及 新增导出比较功能（已调整布局，cmpList 高度固定 150）
rightPanel = uipanel(gl,'Title','图例与参数','FontWeight','bold');
rightPanel.Layout.Row = 1; rightPanel.Layout.Column = 3;

% 我们用 16 行布局，并明确指定第6行（cmpList）高度为 150px，其他行用 'fit'/'1x' 组合
rightGrid = uigridlayout(rightPanel,[16,1]);
rightGrid.RowHeight = {'fit', '1x', 'fit', 'fit', 'fit', 150, 'fit', 'fit', 'fit', 'fit', 'fit', 'fit', 'fit', 'fit', 'fit', 'fit'};
rightGrid.Padding=[8 8 8 8];

% 更显眼的图例模块：使用 uilistbox （便于快速查看和选择）
lblLegend = uilabel(rightGrid,'Text','图例（点击可选中查看）:','HorizontalAlignment','left'); lblLegend.Layout.Row=1;
legendList = uilistbox(rightGrid,'Items',{'[无]'}, 'Multiselect','off', 'Tag','legendList');
legendList.Layout.Row=2;

lblRef = uilabel(rightGrid,'Text','选择参考样 (Reference):','HorizontalAlignment','left'); lblRef.Layout.Row=3;
ddRef = uidropdown(rightGrid,'Items', summaryT.Prefix, 'Value', summaryT.Prefix{1}); ddRef.Layout.Row=4;

% === 对比样多选列表（高度固定 150px，超过会出现滚动条） ===
lblCmp = uilabel(rightGrid,'Text','选择对比样 (可多选，用于批量导出):','HorizontalAlignment','left'); lblCmp.Layout.Row=5;
cmpList = uilistbox(rightGrid,'Items', summaryT.Prefix, 'Multiselect','on'); cmpList.Layout.Row=6;
% 说明：cmpList 的行高已经设置为 150（像素），当项过多时会自动显示滚动条

% 保留原先的单个下拉用于快速绘制单个对比（不影响批量导出）
lblCmpSingle = uilabel(rightGrid,'Text','单个对比 (快速绘图):','HorizontalAlignment','left'); lblCmpSingle.Layout.Row=7;
ddCmp = uidropdown(rightGrid,'Items', [{'(none)'}; summaryT.Prefix(:)], 'Value','(none)'); ddCmp.Layout.Row=8;

% 绘制参考 & 单个对比（原有功能）
btnCompare = uibutton(rightGrid,'Text','绘制参考 & 对比','ButtonPushedFcn',@(btn,event) plotCompare()); btnCompare.Layout.Row=9;

% === 导出对比图片（选中 / 全部） — 放在 cmpList 下面，保证可见 ===
btnExportSelCmp = uibutton(rightGrid,'Text','导出参考样对比图片(选中)','ButtonPushedFcn',@(btn,event) exportComparisons());
btnExportSelCmp.Layout.Row = 10;
btnExportAllCmp = uibutton(rightGrid,'Text','导出参考样对比图片(全部)','ButtonPushedFcn',@(btn,event) exportAllComparisons());
btnExportAllCmp.Layout.Row = 11;

lblSeg = uilabel(rightGrid,'Text','分段可视化:','HorizontalAlignment','left'); lblSeg.Layout.Row=12;
segItems = {'FullRange','Segment1','Segment2','Segment3','Segment4','4Segments'}; % 包含 4Segments
ddSegment = uidropdown(rightGrid,'Items', segItems, 'Value','FullRange'); ddSegment.Layout.Row=13;
btnPlotSeg = uibutton(rightGrid,'Text','Plot Segment Top N','ButtonPushedFcn',@(btn,event) plotSegmentTopN()); btnPlotSeg.Layout.Row=14;

lblMeta = uilabel(rightGrid,'Text','样本信息:','HorizontalAlignment','left'); lblMeta.Layout.Row=15;
txtMeta = uitextarea(rightGrid,'Editable','off','Value','等待选择...'); txtMeta.Layout.Row=16;

% 右侧底部放一个说明提示（作为 panel 内置注释）
lblNote = uilabel(rightPanel,'Text','提示: 批量导出会将参考样与每个对比样分别保存为单张图片（每张图仅两条曲线）。','FontAngle','italic',...
    'Position',[8,6,380,24]);


% 内部状态
state = struct();
state.S = S;
state.summaryT = summaryT;
state.prefixesAll = prefixesAll;
state.colors = lines(128);
state.selectedRows = [];
state.ax = ax;
state.centerGrid = centerGrid;
state.multiGrid = [];
state.axArray = gobjects(1,4);
state.mode = 'single';

% 初始绘制
plotTopN();

% --------------- 回调与绘图子函数 ---------------

    function updateSmoothLabel()
        val = round(sldSmooth.Value);
        if mod(val,2)==0, val = val+1; end
        sldSmooth.Value = val;
        lblSmoothVal.Text = num2str(val);
    end

    function onListboxSelectionChanged(~,~)
        sel = listboxRanks.Value;
        if isempty(sel), return; end
        tok = split(sel{1}, '|');
        if numel(tok) >= 2, p = strtrim(tok{2}); else p = strtrim(tok{1}); end
        updateMeta(p);
    end

    % 确保处于单一 axes 模式
    function ensureSingleAxes()
        if strcmp(state.mode,'single') && ~isempty(state.ax) && isvalid(state.ax), return; end
        % 删除 multi axes（若存在）
        try
            if ~isempty(state.axArray)
                for k=1:numel(state.axArray)
                    if isvalid(state.axArray(k)), delete(state.axArray(k)); end
                end
            end
            if ~isempty(state.multiGrid) && isvalid(state.multiGrid), delete(state.multiGrid); end
        catch, end
        state.multiGrid = [];
        state.axArray = gobjects(1,4);
        state.ax = uiaxes(state.centerGrid); state.ax.Layout.Row=1; state.ax.Layout.Column=1;
        state.ax.Title.String = 'DTG 曲线'; state.ax.XLabel.String='温度 (K)'; state.ax.YLabel.String='DTG (%/K)';
        state.ax.Box='on'; state.ax.NextPlot='add';
        state.mode = 'single';
    end

    % 创建 2x2 多子图
    function ensureMultiAxes()
        if strcmp(state.mode,'multi') && all(arrayfun(@(h) isvalid(h), state.axArray)), return; end
        try
            if ~isempty(state.ax) && isvalid(state.ax), delete(state.ax); end
        catch, end
        state.multiGrid = uigridlayout(centerGrid,[2,2]); state.multiGrid.Layout.Row=1; state.multiGrid.Layout.Column=1;
        state.multiGrid.RowHeight = {'1x','1x'}; state.multiGrid.ColumnWidth = {'1x','1x'};
        state.multiGrid.Padding = [4 4 4 4];
        for ii = 1:4
            ax_i = uiaxes(state.multiGrid); ax_i.Layout.Row = ceil(ii/2); ax_i.Layout.Column = mod(ii-1,2)+1;
            ax_i.Box='on'; ax_i.NextPlot='add'; ax_i.GridLineStyle='-'; ax_i.Title.Interpreter='none';
            state.axArray(ii) = ax_i;
        end
        state.mode = 'multi';
    end

    function plotTopN()
        ensureSingleAxes();
        N = round(edtN.Value); if isempty(N)||N<1, N=5; end; N = min(N, height(state.summaryT));
        topPrefixes = state.summaryT.Prefix(1:N);
        cla(state.ax); hold(state.ax,'on'); legends = cell(N,1);
        for k=1:N
            p = topPrefixes{k};
            idx = find(strcmp(state.prefixesAll, p),1);
            if isempty(idx)
                idx = state.summaryT.GroupIndex(k);
                if isempty(idx)||idx<1||idx>numel(state.S), continue; end
            end
            temp = safeGetField(state.S(idx),'col1'); dtg = safeGetField(state.S(idx),'col3');
            plotLen = min(length(temp), length(dtg)); if plotLen<2, continue; end
            x = temp(1:plotLen); y = dtg(1:plotLen);
            w=round(sldSmooth.Value);
            if w>1 && numel(y)>=w
                try, yplot=sgolayfilt(y,2,w); catch, yplot=movmean(y,w); end;
            else yplot=y; end
            lw = edtLineW.Value; marker='none'; if chkMarkers.Value, marker='o'; end
            c = state.colors(mod(k-1,size(state.colors,1))+1,:);
            plot(state.ax, x, yplot, 'LineWidth', lw, 'Marker', marker, 'MarkerIndices', 1:floor(plotLen/10):plotLen, 'Color', c);
            % 修改：在样品名前添加排名
            rank = state.summaryT.Rank(k);
            legends{k} = sprintf('[%d] %s (score=%.4f)', rank, p, state.summaryT.Score(k));
        end
        hold(state.ax,'off');
        if ~isempty(legends)
            try legend(state.ax, legends, 'Interpreter','none','Location','bestoutside'); end
            legendList.Items = legends;
        else
            legendList.Items = {'[无]'};
        end
        grid(state.ax,'on');
        title(state.ax, sprintf('排名前 %d 的样本DTG曲线', N), 'Interpreter','none');
        if ~isempty(topPrefixes), updateMeta(topPrefixes{1}); end
    end

    function onTableSelect(~, event)
        if isempty(event) || isempty(event.Indices), state.selectedRows = []; return; end
        rows = unique(event.Indices(:,1));
        state.selectedRows = rows;
        try
            prefixSel = showTbl.Prefix{rows(1)};
            ddRef.Items = state.summaryT.Prefix;
            ddRef.Value = prefixSel;
            updateMeta(prefixSel);
        catch, end
    end

    function plotSelectedFromList()
        selItems = listboxRanks.Value;
        if isempty(selItems)
            uialert(fig,'请在左侧列表中选择要绘制的样本（支持多选）。','未选择样本'); return;
        end

        prefixesToPlot = cell(size(selItems));
        for i = 1:numel(selItems)
            parts = split(selItems{i}, '|');
            if numel(parts) >= 2
                prefixesToPlot{i} = strtrim(parts{2});
            else
                prefixesToPlot{i} = strtrim(selItems{i});
            end
        end

        ensureSingleAxes(); cla(state.ax); hold(state.ax,'on'); legends = cell(numel(prefixesToPlot),1);
        for k=1:numel(prefixesToPlot)
            p = prefixesToPlot{k};
            idx = find(strcmp(state.prefixesAll,p),1);
            if isempty(idx), continue; end
            temp = safeGetField(state.S(idx),'col1'); dtg = safeGetField(state.S(idx),'col3');
            plotLen = min(length(temp), length(dtg)); if plotLen<2, continue; end
            x = temp(1:plotLen); y = dtg(1:plotLen);
            w=round(sldSmooth.Value);
            if w>1 && numel(y)>=w
                try, yplot=sgolayfilt(y,2,w); catch, yplot=movmean(y,w); end;
            else yplot=y; end
            lw = edtLineW.Value; marker='none'; if chkMarkers.Value, marker='o'; end
            c = state.colors(mod(k-1,size(state.colors,1))+1,:);
            plot(state.ax,x,yplot,'LineWidth',lw,'Marker',marker,'MarkerIndices',1:floor(plotLen/15):plotLen,'Color',c);
            ridx = find(strcmp(state.summaryT.Prefix, p),1);
            if ~isempty(ridx)
                % 修改：在样品名前添加排名
                rank = state.summaryT.Rank(ridx);
                legends{k} = sprintf('[%d] %s (score=%.4f)', rank, p, state.summaryT.Score(ridx));
            else
                legends{k} = p;
            end
        end
        hold(state.ax,'off');
        if ~isempty(legends)
            try legend(state.ax, legends,'Interpreter','none','Location','bestoutside'); end
            legendList.Items = legends;
        else
            legendList.Items = {'[无]'};
        end
        grid(state.ax,'on');
        if ~isempty(prefixesToPlot), updateMeta(prefixesToPlot{1}); end
    end

    function plotSelectedFromTable()
        if isempty(state.selectedRows)
            uialert(fig,'请先在左侧表格中选择行（支持多选）。','未选择样本'); return;
        end
        prefixesToPlot = cell(1,numel(state.selectedRows));
        for ii=1:numel(state.selectedRows), prefixesToPlot{ii} = showTbl.Prefix{state.selectedRows(ii)}; end
        ensureSingleAxes(); cla(state.ax); hold(state.ax,'on'); legends = cell(numel(prefixesToPlot),1);
        for k=1:numel(prefixesToPlot)
            p = prefixesToPlot{k}; idx = find(strcmp(state.prefixesAll,p),1); if isempty(idx), continue; end
            temp = safeGetField(state.S(idx),'col1'); dtg = safeGetField(state.S(idx),'col3');
            plotLen = min(length(temp), length(dtg)); if plotLen<2, continue; end
            x = temp(1:plotLen); y = dtg(1:plotLen);
            w=round(sldSmooth.Value); if w>1 && numel(y)>=w, try, yplot=sgolayfilt(y,2,w); catch, yplot=movmean(y,w); end; else yplot=y; end
            lw=edtLineW.Value; marker='none'; if chkMarkers.Value, marker='o'; end
            c = state.colors(mod(k-1,size(state.colors,1))+1,:);
            plot(state.ax,x,yplot,'LineWidth',lw,'Marker',marker,'MarkerIndices',1:floor(plotLen/15):plotLen,'Color',c);
            ridx = find(strcmp(state.summaryT.Prefix, p),1);
            if ~isempty(ridx)
                % 修改：在样品名前添加排名
                rank = state.summaryT.Rank(ridx);
                legends{k} = sprintf('[%d] %s (score=%.4f)', rank, p, state.summaryT.Score(ridx));
            else
                legends{k} = p;
            end
        end
        hold(state.ax,'off');
        if ~isempty(legends), try legend(state.ax, legends,'Interpreter','none','Location','bestoutside'); end; legendList.Items = legends; else legendList.Items = {'[无]'}; end
        grid(state.ax,'on'); updateMeta(prefixesToPlot{1});
    end

    function plotCompare()
        refPrefix = ddRef.Value; cmpPrefix = ddCmp.Value;
        ensureSingleAxes(); cla(state.ax); hold(state.ax,'on'); legends = {};
        % reference
        refIdx = find(strcmp(state.prefixesAll, refPrefix),1);
        if ~isempty(refIdx)
            temp = safeGetField(state.S(refIdx),'col1'); dtg = safeGetField(state.S(refIdx),'col3');
            plotLen = min(length(temp), length(dtg));
            if plotLen>=2
                x = temp(1:plotLen); y = dtg(1:plotLen);
                w=round(sldSmooth.Value); if w>1 && numel(y)>=w, try, yplot=sgolayfilt(y,2,w); catch, yplot=movmean(y,w); end; else yplot=y; end
                plot(state.ax,x,yplot,'k-','LineWidth',3);
                rr = find(strcmp(state.summaryT.Prefix, refPrefix),1);
                if ~isempty(rr)
                    % 修改：在参考样品名前添加排名
                    rank = state.summaryT.Rank(rr);
                    legends{end+1} = sprintf('[REF] [%d] %s (score=%.4f)', rank, refPrefix, state.summaryT.Score(rr));
                else
                    legends{end+1} = sprintf('[REF] %s', refPrefix);
                end
            end
        else, uialert(fig,sprintf('未在 S 中找到参考样 %s', refPrefix),'参考样未找到'); end
        % compare
        if ~strcmp(cmpPrefix,'(none)')
            cmpIdx = find(strcmp(state.prefixesAll, cmpPrefix),1);
            if ~isempty(cmpIdx)
                temp = safeGetField(state.S(cmpIdx),'col1'); dtg = safeGetField(state.S(cmpIdx),'col3');
                plotLen = min(length(temp), length(dtg));
                if plotLen>=2
                    x = temp(1:plotLen); y = dtg(1:plotLen);
                    w=round(sldSmooth.Value); if w>1 && numel(y)>=w, try, yplot=sgolayfilt(y,2,w); catch, yplot=movmean(y,w); end; else yplot=y; end
                    plot(state.ax,x,yplot,'r--','LineWidth',2.2);
                    rr = find(strcmp(state.summaryT.Prefix, cmpPrefix),1);
                    if ~isempty(rr)
                        % 修改：在对比样品名前添加排名
                        rank = state.summaryT.Rank(rr);
                        legends{end+1} = sprintf('[CMP] [%d] %s (score=%.4f)', rank, cmpPrefix, state.summaryT.Score(rr));
                    else
                        legends{end+1} = sprintf('[CMP] %s', cmpPrefix);
                    end
                end
            else, uialert(fig,sprintf('未在 S 中找到对比样 %s', cmpPrefix),'对比样未找到'); end
        end
        hold(state.ax,'off');
        if ~isempty(legends), try legend(state.ax, legends,'Interpreter','none','Location','bestoutside'); end; legendList.Items = legends; else legendList.Items = {'[无]'}; end
        grid(state.ax,'on'); updateMeta(refPrefix);
    end

    function plotSegmentTopN()
        segChoice = ddSegment.Value; N = round(edtN.Value); if isempty(N)||N<1, N=5; end
        if strcmpi(segChoice,'FullRange'), plotTopN(); return; end

        % 确认 allSegments 存在
        if ~evalin('base','exist(''allSegments'',''var'')')
            uialert(fig,'在 base workspace 中未找到变量 allSegments，无法进行分段绘制。请先运行主脚本并确保 allSegments 存在。','缺少变量'); return;
        end
        allSegments = evalin('base','allSegments');

        % 如果选择 4Segments，切换 multi axes 并分别绘制段1-4（每子图 Top N）
        if strcmpi(segChoice,'4Segments')
            ensureMultiAxes();
            % 尝试获取每段 segSummary（优先 segmentResults）
            segSummaryTables = cell(1,4);
            if evalin('base','exist(''segmentResults'',''var'')')
                try
                    segmentResults = evalin('base','segmentResults');
                    for sN=1:4
                        if length(segmentResults)>=sN && isfield(segmentResults(sN),'SummaryTable') && ~isempty(segmentResults(sN).SummaryTable)
                            segSummaryTables{sN} = segmentResults(sN).SummaryTable;
                        else
                            segSummaryTables{sN} = [];
                        end
                    end
                catch
                    segSummaryTables = {[],[],[],[]};
                end
            else
                % try other possible variables
                if evalin('base','exist(''segmentSummaryTables'',''var'')')
                    sst = evalin('base','segmentSummaryTables');
                    for sN=1:4, if length(sst)>=sN, segSummaryTables{sN}=sst{sN}; else segSummaryTables{sN}=[]; end; end
                elseif evalin('base','exist(''allSegmentSummary'',''var'')')
                    ass = evalin('base','allSegmentSummary');
                    for sN=1:4
                        if any(strcmp(ass.Properties.VariableNames,'Segment'))
                            segSummaryTables{sN} = ass(strcmp(ass.Segment,sprintf('段%d',sN)) | strcmp(ass.Segment,['段' num2str(sN)]), :);
                        else segSummaryTables{sN} = []; end
                    end
                else
                    segSummaryTables = {[],[],[],[]};
                end
            end

            % 绘制每个子图并在每个子图上创建图例（如果有）
            combinedLegend = {};
            for segNum = 1:4
                ax_i = state.axArray(segNum);
                cla(ax_i); hold(ax_i,'on'); legends_local = {};
                segSummary = segSummaryTables{segNum};
                if ~isempty(segSummary)
                    topN = min(N, height(segSummary));
                    topPrefixes = segSummary.Prefix(1:topN);
                else
                    % fallback: collect prefixes from allSegments that have data
                    pfx = {};
                    for ii=1:length(allSegments)
                        if isfield(allSegments(ii), sprintf('Segment%d', segNum))
                            segD = allSegments(ii).(sprintf('Segment%d', segNum));
                            if isfield(segD,'DTG') && ~isempty(segD.DTG), pfx{end+1} = allSegments(ii).Prefix; end
                        end
                    end
                    topPrefixes = unique(pfx,'stable');
                    topN = min(N, numel(topPrefixes));
                end

                for k = 1:min(topN, numel(topPrefixes))
                    p = topPrefixes{k};
                    idxAS = find(strcmp({allSegments.Prefix}, p),1);
                    if isempty(idxAS)
                        idxS = find(strcmp(state.prefixesAll,p),1);
                        if isempty(idxS), continue; else temp = safeGetField(state.S(idxS),'col1'); dtg = safeGetField(state.S(idxS),'col3'); end
                        scoreVal = getScoreForPrefix(p, segNum, segSummary);
                    else
                        segD = allSegments(idxAS).(sprintf('Segment%d', segNum));
                        temp = safeGetField(segD,'Temperature'); dtg = safeGetField(segD,'DTG');
                        scoreVal = getScoreForPrefix(p, segNum, segSummary);
                    end
                    if isempty(temp) || isempty(dtg) || numel(temp) < 2, continue; end
                    w = round(sldSmooth.Value);
                    if w>1 && numel(dtg)>=w
                        try, yplot = sgolayfilt(dtg,2,w); catch, yplot = movmean(dtg,w); end
                    else
                        yplot = dtg;
                    end
                    c = state.colors(mod(k-1,size(state.colors,1))+1,:);
                    plot(ax_i, temp, yplot, 'LineWidth', 1.6, 'Color', c);
                    if ~isnan(scoreVal)
                        % 修改：在样品名前添加排名
                        legends_local{end+1} = sprintf('[%d] %s (segScore=%.4f)', k, p, scoreVal);
                    else
                        ridx = find(strcmp(state.summaryT.Prefix, p),1);
                        if ~isempty(ridx)
                            % 修改：在样品名前添加排名
                            legends_local{end+1} = sprintf('[%d] %s (score=%.4f)', k, p, state.summaryT.Score(ridx));
                        else
                            legends_local{end+1} = sprintf('[%d] %s', k, p);
                        end
                    end
                end

                hold(ax_i,'off');
                title(ax_i, sprintf('Segment %d (Top %d)', segNum, min(N,numel(topPrefixes))),'Interpreter','none');
                grid(ax_i,'on');
                % 在子图上创建图例（如果存在）
                if ~isempty(legends_local)
                    try
                        lg = legend(ax_i, legends_local, 'Interpreter','none', 'Location','best');
                        % 使图例字号稍小以避免遮挡
                        lg.FontSize = max(8, lg.FontSize-1);
                    catch
                    end
                end
                % 收集一些代表性的图例项以在右侧显示（这里选择第一个子图或合并部分）
                if segNum == 1
                    combinedLegend = legends_local;
                else
                    % 合并但不重复（保持顺序）
                    for ii = 1:numel(legends_local)
                        if ~any(strcmp(combinedLegend, legends_local{ii}))
                            combinedLegend{end+1} = legends_local{ii}; %#ok<AGROW>
                        end
                    end
                end
            end

            if isempty(combinedLegend)
                legendList.Items = {'[无]'};
            else
                legendList.Items = combinedLegend;
            end
            return;
        end

        % 非 4Segments：单轴绘制某一段 Top N（保留 score 显示）
        segNum = sscanf(segChoice,'Segment%d');
        if isempty(segNum) || segNum<1 || segNum>4, uialert(fig,'无效分段编号。','错误'); return; end

        % 尝试找到 segSummary
        segSummary = [];
        if evalin('base','exist(''segmentResults'',''var'')')
            sr = evalin('base','segmentResults');
            if length(sr) >= segNum && isfield(sr(segNum),'SummaryTable') && ~isempty(sr(segNum).SummaryTable)
                segSummary = sr(segNum).SummaryTable;
            end
        end
        if isempty(segSummary)
            if evalin('base','exist(''segmentSummaryTables'',''var'')')
                sst = evalin('base','segmentSummaryTables'); if length(sst)>=segNum, segSummary = sst{segNum}; end
            elseif evalin('base','exist(''allSegmentSummary'',''var'')')
                ass = evalin('base','allSegmentSummary'); if any(strcmp(ass.Properties.VariableNames,'Segment'))
                    segSummary = ass(strcmp(ass.Segment,sprintf('段%d',segNum)) | strcmp(ass.Segment,['段' num2str(segNum)]), :);
                end
            end
        end
        if isempty(segSummary)
            pfx = {};
            for ii=1:length(allSegments)
                segD = allSegments(ii).(sprintf('Segment%d', segNum));
                if isfield(segD,'DTG') && ~isempty(segD.DTG), pfx{end+1} = allSegments(ii).Prefix; end
            end
            if isempty(pfx), uialert(fig,sprintf('分段 %d 没有有效样本可绘。',segNum),'无数据'); return; end
            segSummary = table(pfx(:),'VariableNames',{'Prefix'}); segSummary.Score = zeros(height(segSummary),1);
        end

        topN = min(N, height(segSummary)); topPrefixes = segSummary.Prefix(1:topN);
        ensureSingleAxes(); cla(state.ax); hold(state.ax,'on'); legends = cell(topN,1);
        for k=1:topN
            p = topPrefixes{k};
            idxAS = find(strcmp({allSegments.Prefix}, p),1);
            if isempty(idxAS)
                idxS = find(strcmp(state.prefixesAll,p),1);
                if isempty(idxS), continue; else temp = safeGetField(state.S(idxS),'col1'); dtg = safeGetField(state.S(idxS),'col3'); end
            else
                segD = allSegments(idxAS).(sprintf('Segment%d', segNum));
                temp = safeGetField(segD,'Temperature'); dtg = safeGetField(segD,'DTG');
            end
            if isempty(temp) || isempty(dtg) || numel(temp)<2, continue; end
            w = round(sldSmooth.Value);
            if w>1 && numel(dtg)>=w, try, yplot=sgolayfilt(dtg,2,w); catch, yplot=movmean(dtg,w); end; else yplot=dtg; end
            c = state.colors(mod(k-1,size(state.colors,1))+1,:);
            plot(state.ax, temp, yplot, 'LineWidth', 1.8, 'Color', c);
            scoreVal = getScoreForPrefix(p, segNum, segSummary);
            if ~isnan(scoreVal)
                % 修改：在样品名前添加排名
                legends{k} = sprintf('[%d] %s (segScore=%.4f)', k, p, scoreVal);
            else
                ridx = find(strcmp(state.summaryT.Prefix, p),1);
                if ~isempty(ridx)
                    % 修改：在样品名前添加排名
                    legends{k} = sprintf('[%d] %s (score=%.4f)', k, p, state.summaryT.Score(ridx));
                else
                    legends{k} = sprintf('[%d] %s', k, p);
                end
            end
        end
        hold(state.ax,'off');
        if ~isempty(legends), try legend(state.ax, legends,'Interpreter','none','Location','bestoutside'); end; legendList.Items = legends; else legendList.Items = {'[无]'}; end
        title(state.ax, sprintf('分段 %d (Top %d)', segNum, topN), 'Interpreter','none'); grid(state.ax,'on');
        if ~isempty(topPrefixes), updateMeta(topPrefixes{1}); end
    end

    % 获取 prefix 对应分段/全局分数（优先 segSummary 的 Score 字段，否则退回到 summaryT）
    function scoreVal = getScoreForPrefix(prefix, segNum, segSummaryTable)
        scoreVal = NaN;
        try
            if nargin >= 3 && ~isempty(segSummaryTable) && any(strcmp(segSummaryTable.Properties.VariableNames,'Score'))
                idx = find(strcmp(segSummaryTable.Prefix, prefix),1);
                if ~isempty(idx), scoreVal = segSummaryTable.Score(idx); return; end
            end
            % fallback to global summaryT score
            idxG = find(strcmp(state.summaryT.Prefix, prefix),1);
            if ~isempty(idxG) && isfield(state.summaryT,'Score'), scoreVal = state.summaryT.Score(idxG); end
        catch
            scoreVal = NaN;
        end
    end

    function exportFigure()
        [file,path] = uiputfile({'*.png';'*.jpg';'*.pdf';'*.tif'},'保存图像为');
        if isequal(file,0), return; end
        out = fullfile(path,file);
        try
            if strcmp(state.mode,'single')
                if exist('exportgraphics','file'), exportgraphics(state.ax, out, 'ContentType','image');
                else f=figure('Visible','off'); copyobj(allchild(state.ax), gca); saveas(f,out); close(f); end
            else
                tmpf = figure('Visible','off','Position',[100 100 1200 800]);
                for ii=1:4
                    ax_tmp = subplot(2,2,ii);
                    try copyobj(allchild(state.axArray(ii)), ax_tmp); title(ax_tmp, state.axArray(ii).Title.String); xlabel(ax_tmp, state.axArray(ii).XLabel.String); ylabel(ax_tmp, state.axArray(ii).YLabel.String); catch; end
                end
                saveas(tmpf, out); close(tmpf);
            end
            uialert(fig,sprintf('已保存到 %s', out),'保存成功');
        catch ME
            uialert(fig,sprintf('导出失败: %s', ME.message),'错误','Icon','error');
        end
    end

    function updateMeta(prefix)
        idx = find(strcmp(state.prefixesAll, prefix),1);
        lines = {};
        if isempty(idx)
            lines{end+1} = sprintf('Prefix: %s (未在 S 中找到)', prefix);
            txtMeta.Value = lines; return;
        end
        s = state.S(idx);
        lines{end+1} = sprintf('Prefix: %s', prefix);
        if isfield(s,'isUniformSpacing'), lines{end+1} = sprintf('步长均匀: %s', bool2str(s.isUniformSpacing)); end
        if isfield(s,'spacing') && ~isempty(s.spacing), lines{end+1} = sprintf('温度步长: %.4g', s.spacing); end
        if isfield(s,'isLengthEqual'), lines{end+1} = sprintf('长度对齐: %s', bool2str(s.isLengthEqual)); end
        if isfield(s,'col1'), lines{end+1} = sprintf('数据点: %d', numel(s.col1)); end
        ridx = find(strcmp(state.summaryT.Prefix, prefix),1);
        if ~isempty(ridx)
            if isfield(state.summaryT,'Rank'), lines{end+1} = sprintf('Global Rank: %d', state.summaryT.Rank(ridx)); end
            if isfield(state.summaryT,'Score'), lines{end+1} = sprintf('Score: %.6f', state.summaryT.Score(ridx)); end
        end
        txtMeta.Value = lines;
    end

% ----------------- 新增：批量导出参考 vs 对比 图相关函数 -----------------
    function exportComparisons()
        % 导出参考样与 cmpList 中选中的每个样的对比图（逐一文件）
        refPrefix = ddRef.Value;
        cmpSel = cmpList.Value;
        if isempty(cmpSel)
            uialert(fig,'请先在“选择对比样 (可多选)”中选择至少一个对比样。','未选择对比样'); return;
        end
        % 选择输出文件夹
        outDir = uigetdir(pwd, sprintf('选择保存参考样 %s 的对比图片的文件夹', refPrefix));
        if isequal(outDir,0), return; end

        saved = 0; failed = 0; failList = {};
        % 使用稳健函数获取排名字符串
        refRankStr = getRankStr(refPrefix);

        for ii = 1:numel(cmpSel)
            cmpPrefix = cmpSel{ii};
            if strcmp(cmpPrefix, refPrefix)
                % 跳过与自身比较
                continue;
            end

            % 对比样排名查找（稳健）
            cmpRankStr = getRankStr(cmpPrefix);

            % 文件名加上排名前缀
            fname = sprintf('%s_REF_%s__%s_CMP_%s.png', ...
                refRankStr, sanitizeFilename(refPrefix), cmpRankStr, sanitizeFilename(cmpPrefix));
            outPath = fullfile(outDir, fname);
            try
                makeComparisonPlot(refPrefix, cmpPrefix, outPath);
                saved = saved + 1;
            catch ME
                failed = failed + 1;
                failList{end+1} = sprintf('%s: %s', cmpPrefix, ME.message); %#ok<AGROW>
            end
        end

        msg = sprintf('完成：成功保存 %d 张，失败 %d 张。\n保存路径：%s', saved, failed, outDir);
        if failed>0
            msg = sprintf('%s\n失败项示例：\n%s', msg, strjoin(failList(1:min(10,numel(failList))),char(10)));
        end
        uialert(fig,msg,'导出完成');
    end

    function exportAllComparisons()
        % 导出参考样与所有其它样的对比图
        refPrefix = ddRef.Value;
        allPrefixes = state.summaryT.Prefix(:);
        % 选择输出文件夹
        outDir = uigetdir(pwd, sprintf('选择保存参考样 %s 的对比图片的文件夹', refPrefix));
        if isequal(outDir,0), return; end

        % 参考样排名查找（稳健）
        refRankStr = getRankStr(refPrefix);

        saved = 0; failed = 0; failList = {};
        for ii = 1:numel(allPrefixes)
            cmpPrefix = allPrefixes{ii};
            if strcmp(cmpPrefix, refPrefix), continue; end

            % 对比样排名查找（稳健）
            cmpRankStr = getRankStr(cmpPrefix);

            fname = sprintf('%s_REF_%s__%s_CMP_%s.png', ...
                refRankStr, sanitizeFilename(refPrefix), cmpRankStr, sanitizeFilename(cmpPrefix));
            outPath = fullfile(outDir, fname);
            try
                makeComparisonPlot(refPrefix, cmpPrefix, outPath);
                saved = saved + 1;
            catch ME
                failed = failed + 1;
                failList{end+1} = sprintf('%s: %s', cmpPrefix, ME.message); %#ok<AGROW>
            end
        end

        msg = sprintf('完成：成功保存 %d 张，失败 %d 张。\n保存路径：%s', saved, failed, outDir);
        if failed>0
            msg = sprintf('%s\n失败项示例：\n%s', msg, strjoin(failList(1:min(10,numel(failList))),char(10)));
        end
        uialert(fig,msg,'导出完成');
    end

    function rstr = getRankStr(pref)
        % 返回格式 'Rxxx'（3位数补零），或 'R_NA'（找不到或无效）
        try
            p = string(pref);
            p = strtrim(p);
            % 将 summaryT 中的 Prefix 一律转成 string 并 trim
            pfxs = string(state.summaryT.Prefix);
            pfxs = strtrim(pfxs);

            % 1) 精确匹配（区分大小写）
            idx = find(pfxs == p, 1);
            % 2) 精确不区分大小写
            if isempty(idx)
                idx = find(strcmpi(cellstr(pfxs), char(p)), 1);
            end
            % 3) startsWith 兜底（小心重名，取第一个）
            if isempty(idx)
                idx = find(startsWith(lower(pfxs), lower(p)), 1);
            end

            if isempty(idx)
                rstr = 'R_NA';
                return;
            end

            if ismember('Rank', state.summaryT.Properties.VariableNames)
                rVal = state.summaryT.Rank(idx);
                % 处理不同类型的 Rank（numeric / string / categorical）
                if isempty(rVal) || (isnumeric(rVal) && (isnan(rVal) || ~isfinite(rVal)))
                    rstr = 'R_NA';
                else
                    if isnumeric(rVal)
                        rnum = round(rVal);
                        rstr = sprintf('R%03d', rnum);
                    else
                        % 试着把字符串转换为数字
                        rnum = str2double(string(rVal));
                        if ~isnan(rnum)
                            rstr = sprintf('R%03d', round(rnum));
                        else
                            % 非数字时，直接把原值做简单清理放入文件名（慎用）
                            tmp = char(string(rVal));
                            tmp = regexprep(tmp,'\s+','_');
                            tmp = regexprep(tmp,'[^0-9A-Za-z_\-]','_');
                            rstr = ['R' tmp];
                        end
                    end
                end
            else
                rstr = 'R_NA';
            end
        catch
            rstr = 'R_NA';
        end
    end

    function makeComparisonPlot(refPrefix, cmpPrefix, outPath)
        % 在隐藏 figure 中绘制参考样 vs 对比样（仅两条曲线），并保存到 outPath
        refIdx = find(strcmp(state.prefixesAll, refPrefix),1);
        if isempty(refIdx), error('参考样 %s 未在 S 中找到', refPrefix); end
        cmpIdx = find(strcmp(state.prefixesAll, cmpPrefix),1);
        if isempty(cmpIdx), error('对比样 %s 未在 S 中找到', cmpPrefix); end

        refTemp = safeGetField(state.S(refIdx),'col1'); refDtg = safeGetField(state.S(refIdx),'col3');
        cmpTemp = safeGetField(state.S(cmpIdx),'col1'); cmpDtg = safeGetField(state.S(cmpIdx),'col3');
        if isempty(refTemp) || isempty(refDtg) || numel(refTemp)<2, error('参考样 %s 数据不足', refPrefix); end
        if isempty(cmpTemp) || isempty(cmpDtg) || numel(cmpTemp)<2, error('对比样 %s 数据不足', cmpPrefix); end

        % 对齐长度
        Lref = min(numel(refTemp), numel(refDtg));
        Lcmp = min(numel(cmpTemp), numel(cmpDtg));
        L = min(Lref, Lcmp); % 为了直接比较，使用最小公共长度（若你想用各自全长，可改）
        xRef = refTemp(1:L); yRef = refDtg(1:L);
        xCmp = cmpTemp(1:L); yCmp = cmpDtg(1:L);

        % 平滑
        w = round(sldSmooth.Value);
        if w>1
            try
                if numel(yRef) >= w, yRefP = sgolayfilt(yRef,2,w); else yRefP = yRef; end
            catch
                if numel(yRef) >= w, yRefP = movmean(yRef,w); else yRefP = yRef; end
            end
            try
                if numel(yCmp) >= w, yCmpP = sgolayfilt(yCmp,2,w); else yCmpP = yCmp; end
            catch
                if numel(yCmp) >= w, yCmpP = movmean(yCmp,w); else yCmpP = yCmp; end
            end
        else
            yRefP = yRef; yCmpP = yCmp;
        end

        % 创建不可见 figure 绘图
        f = figure('Visible','off','Position',[100 100 900 600]);
        axTemp = axes(f);
        hold(axTemp,'on');
        % 绘制参考样（黑色粗线）
        plot(axTemp, xRef, yRefP, 'k-', 'LineWidth', 2.8);
        % 绘制对比样（颜色取 state.colors 随机一色）
        % 尝试使用在 summaryT 中的位置作为 color index，fallback 到循环索引
        ridx = find(strcmp(state.summaryT.Prefix, cmpPrefix),1);
        if ~isempty(ridx)
            colorIdx = mod(ridx-1, size(state.colors,1))+1;
        else
            colorIdx = 1;
        end
        plot(axTemp, xCmp, yCmpP, 'LineWidth', 1.8, 'Color', state.colors(colorIdx,:));
        % 标注
        rr = find(strcmp(state.summaryT.Prefix, refPrefix),1);
        rc = find(strcmp(state.summaryT.Prefix, cmpPrefix),1);
        legendItems = {};
        if ~isempty(rr), legendItems{end+1} = sprintf('[REF] [%d] %s (score=%.4f)', state.summaryT.Rank(rr), refPrefix, state.summaryT.Score(rr)); else legendItems{end+1} = sprintf('[REF] %s', refPrefix); end
        if ~isempty(rc), legendItems{end+1} = sprintf('[CMP] [%d] %s (score=%.4f)', state.summaryT.Rank(rc), cmpPrefix, state.summaryT.Score(rc)); else legendItems{end+1} = sprintf('[CMP] %s', cmpPrefix); end
        legend(axTemp, legendItems, 'Interpreter','none', 'Location','best');
        title(axTemp, sprintf('REF: %s  vs  CMP: %s', refPrefix, cmpPrefix), 'Interpreter','none');
        xlabel(axTemp, '温度 (K)'); ylabel(axTemp, 'DTG (%/K)');
        grid(axTemp,'on'); box(axTemp,'on');
        hold(axTemp,'off');

        % 保存（使用 exportgraphics 优先）
        try
            if exist('exportgraphics','file')
                exportgraphics(axTemp, outPath, 'ContentType','image');
            else
                saveas(f, outPath);
            end
            close(f);
        catch ME
            try close(f); catch; end
            rethrow(ME);
        end
    end

    function s = sanitizeFilename(name)
        % 简单替换空格与文件名非法字符
        s = char(name);
        s = strrep(s, ' ', '_');
        invalid = ['<>:"/\\|?*'];
        for k = 1:length(invalid)
            s = strrep(s, invalid(k), '_');
        end
    end
% ----------------- 辅助小函数 -----------------
    function out = safeGetField(s, fld)
        if isempty(s) || (~isstruct(s) && ~istable(s)), out = []; return; end
        if isstruct(s)
            if isfield(s,fld), out = s.(fld); else out = []; end
        elseif istable(s)
            if any(strcmp(s.Properties.VariableNames,fld)), out = s.(fld); else out = []; end
        else
            out = [];
        end
    end

    function str = bool2str(tf)
        if isempty(tf), str = 'N/A'; return; end
        if tf, str = 'Yes'; else str = 'No'; end
    end

end
