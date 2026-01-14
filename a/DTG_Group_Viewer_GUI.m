function DTG_Group_Viewer_GUI(keyMap)
% DTG_Group_Viewer_GUI - Interactive GUI to view group DTG samples and each processing step
% 扩展与修改：
%  - 移除 "Cleaned (outliers removed)" 与 "First Derivative (Diff)" Display Step 选项
%  - 在内部计算并导出：truncated, cleaned_truncated, normalized,
%    smoothed_sg, smoothed_loess, dtg_sg_from_sg, dtg_loess_from_loess,
%    diff_from_sg, diff_from_loess
%  - 其它界面、参数及多步骤保存功能保留/增强（见代码注释）

    %% input handling
    if nargin < 1 || isempty(keyMap)
        if evalin('base','exist(''keyMap'',''var'')')
            keyMap = evalin('base','keyMap');
        else
            error('DTG_Group_Viewer_GUI: keyMap not provided and not found in base workspace.');
        end
    end
    if ~isa(keyMap,'containers.Map')
        error('DTG_Group_Viewer_GUI: keyMap must be a containers.Map.');
    end

    %% build process base list (保持原有匹配 -Q/-Z/-H)
    keysAll = keyMap.keys();
    procBases = {};
    for k = 1:numel(keysAll)
        key = keysAll{k};
        tok = regexp(key,'^(.*)-([QZHqzh])$','tokens');
        if ~isempty(tok)
            procBases{end+1} = tok{1}{1}; %#ok<AGROW>
        end
    end
    procBases = unique(procBases,'stable');

    %% GUI layout (center on screen)
    screensize = get(0,'ScreenSize'); % [left bottom width height]
    figWidth = 1400; figHeight = 820;
    figLeft = round((screensize(3) - figWidth)/2);
    figBottom = round((screensize(4) - figHeight)/2);

    fig = uifigure('Name','DTG Group Viewer - Processing Steps','Position',[figLeft, figBottom, figWidth, figHeight]);

    mainGrid = uigridlayout(fig,[1,3]);
    mainGrid.ColumnWidth = {380, '1x', 520};
    mainGrid.RowHeight = {'1x'};

    % ---------------- Left panel: groups ----------------
    leftPanel = uipanel(mainGrid,'Title','Groups','FontSize',12);
    leftPanel.Layout.Row = 1; leftPanel.Layout.Column = 1;
    leftGrid = uigridlayout(leftPanel,[8,1]);
    leftGrid.RowHeight = {'fit','1x','fit','fit','fit','fit','fit','fit'};
    leftGrid.Padding = [8 8 8 8];

    label_proc_base = uilabel(leftGrid,'Text','Select a Process Base:','HorizontalAlignment','left');
    label_proc_base.Layout.Row = 1;
    label_proc_base.Layout.Column = 1;

    groupList = uilistbox(leftGrid,'Items',procBases,'Multiselect','off');
    groupList.Layout.Row = 2;
    groupList.Layout.Column = 1;

    loadBtn = uibutton(leftGrid,'Text','(Re)Load Selected','ButtonPushedFcn',@(src,event) onReloadGroup());
    loadBtn.Layout.Row = 3;
    loadBtn.Layout.Column = 1;

    % ***** 新增按钮 *****
    btnExportAllGroups = uibutton(leftGrid,'Text','Export All Groups Data','ButtonPushedFcn',@(src,event) onExportAllGroups());
    btnExportAllGroups.Layout.Row = 4; % 新按钮放在第4行
    btnExportAllGroups.Layout.Column = 1;

    % ***** 新增批量绘图按钮 *****
    btnBatchPlot = uibutton(leftGrid,'Text','Batch Plot & Save All Groups','ButtonPushedFcn',@(src,event) onBatchPlotAndSave());
    btnBatchPlot.Layout.Row = 5; % 新按钮放在第5行
    btnBatchPlot.Layout.Column = 1;

    label_tip_left = uilabel(leftGrid,'Text','Tip: select a group then click (Re)Load','HorizontalAlignment','left','FontAngle','italic');
    label_tip_left.Layout.Row = 6;
    label_tip_left.Layout.Column = 1;

    btn_quit = uibutton(leftGrid,'Text','Close','ButtonPushedFcn',@(s,e) close(fig));
    btn_quit.Layout.Row = 7;
    btn_quit.Layout.Column = 1;

    helpBtn = uibutton(leftGrid,'Text','Help','ButtonPushedFcn',@(s,e) uialert(fig, 'Select a group -> (Re)Load -> Select samples -> Plot. Use Export to save steps.','Help'));
    helpBtn.Layout.Row = 8;
    helpBtn.Layout.Column = 1;

    % ---------------- Center panel: 3x1 plots (Q, Z, H) ----------------
    centerPanel = uipanel(mainGrid,'Title','Process Stages (Q - Z - H)','FontSize',12);
    centerPanel.Layout.Row = 1; centerPanel.Layout.Column = 2;
    t = tiledlayout(centerPanel,3,1,'Padding','compact','TileSpacing','compact');
    ax = gobjects(3,1);
    stageNames = {'Q (前)', 'Z (中)', 'H (后)'};
    for i = 1:3
        ax(i) = nexttile(t); hold(ax(i),'on');
        ax(i).XGrid = 'on'; ax(i).YGrid = 'on';
        title(ax(i), stageNames{i}, 'FontSize', 12, 'FontWeight', 'bold');
        xlabel(ax(i),'Index'); ylabel(ax(i),'Value');
    end

    % ---------------- Right panel: controls & parameters ----------------
    rightPanel = uipanel(mainGrid,'Title','Controls & Parameters','FontSize',12);
    rightPanel.Layout.Row = 1; rightPanel.Layout.Column = 3;

    % 将行数从 28 增加到 29 以容纳新控件
    rg = uigridlayout(rightPanel,[29,2]);
    % 创建一个包含29个 'fit' 的 cell 数组
    row_heights = repmat({'fit'}, 1, 29); 
    % 将第8行（即 sampleList 所在行）的高度设置为 '1x'
    row_heights{8} = '1x'; 
    % 将修改后的行高应用到布局中
    rg.RowHeight = row_heights; 
    rg.ColumnWidth = {'1x','1x'};
    rg.Padding = [8 8 8 8];
    rg.RowSpacing = 6;
    rg.ColumnSpacing = 8;

    % Legend
    label_legend = uilabel(rg,'Text','Legend:','FontWeight','bold','HorizontalAlignment','left');
    label_legend.Layout.Row = 1;
    label_legend.Layout.Column = 1;

    legendPanel = uipanel(rg,'BackgroundColor','w','BorderType','line');
    legendPanel.Layout.Row = 2;
    legendPanel.Layout.Column = [1, 2];
    legendAxes = axes(legendPanel,'Position',[0.02 0.02 0.96 0.96],'Visible','off'); hold(legendAxes,'on');

    % Options
    cbMean = uicheckbox(rg,'Text','Show mean curve','Value',false);
    cbMean.Layout.Row = 3;
    cbMean.Layout.Column = 1;

    cbInterp = uicheckbox(rg,'Text','Interpolate NaN for plotting','Value',false);
    cbInterp.Layout.Row = 3;
    cbInterp.Layout.Column = 2;

    cbShowRaw = uicheckbox(rg,'Text','Show Raw (pre-repair) overlay','Value',false);
    cbShowRaw.Layout.Row = 4;
    cbShowRaw.Layout.Column = 1;

    cbShowBad = uicheckbox(rg,'Text','Show Bad Points (removed)','Value',false);
    cbShowBad.Layout.Row = 4;
    cbShowBad.Layout.Column = 2;

    label_smoothDisplay = uilabel(rg,'Text','Smooth Display (only for Smoothed step):','HorizontalAlignment','left');
    label_smoothDisplay.Layout.Row = 5;
    label_smoothDisplay.Layout.Column = 1;
    ddSmoothDisplay = uidropdown(rg,'Items',{'SG (only)','LOESS (only)','Both','None'}, 'Value','LOESS (only)');
    ddSmoothDisplay.Layout.Row = 5;
    ddSmoothDisplay.Layout.Column = 2;
    
    % ***** 新增代码开始 *****
    % 新增绘图类型选择
    label_plotType = uilabel(rg,'Text','Plot Type:','HorizontalAlignment','left');
    label_plotType.Layout.Row = 6; % 新增的行
    label_plotType.Layout.Column = 1;
    ddPlotType = uidropdown(rg,'Items',{'Line','Scatter'}, 'Value','Line');
    ddPlotType.Layout.Row = 6; % 新增的行
    ddPlotType.Layout.Column = 2;
    % ***** 新增代码结束 *****

    % Display Step dropdown - 行号 +1
    label_step = uilabel(rg,'Text','Display Step:','HorizontalAlignment','left');
    label_step.Layout.Row = 7; % 原为 6
    label_step.Layout.Column = 1;
    stepItems = {'Raw (pre-repair)','Truncated','Normalized','Smoothed','DTG (derivative)'};
    ddSteps = uidropdown(rg,'Items',stepItems, 'Value','DTG (derivative)');
    ddSteps.Layout.Row = 7; % 原为 6
    ddSteps.Layout.Column = 2;

    % sampleList 及以下所有控件行号 +1
    sampleList = uilistbox(rg,'Items',{},'Multiselect','on');
    sampleList.Layout.Row = 8; % 原为 7
    sampleList.Layout.Column = [1, 2];
    % 让列表框只占据第8行，这一行的高度已经由上面的 RowHeight 控制
    sampleList.Layout.Row = 8;


    btnPlotSel = uibutton(rg,'Text','Plot Selected','ButtonPushedFcn',@(src,event) onPlotSelected());
    btnPlotSel.Layout.Row = 9; % 原为 9
    btnPlotSel.Layout.Column = 1;

    btnPlotAll = uibutton(rg,'Text','Plot All Stages','ButtonPushedFcn',@(src,event) onPlotAll());
    btnPlotAll.Layout.Row = 9; % 原为 9
    btnPlotAll.Layout.Column = 2;

    btnClear = uibutton(rg,'Text','Clear Plots','ButtonPushedFcn',@(src,event) onClear());
    btnClear.Layout.Row = 10; % 原为 10
    btnClear.Layout.Column = 1;

    btnSave = uibutton(rg,'Text','Save Figure','ButtonPushedFcn',@(src,event) onSave());
    btnSave.Layout.Row = 10; % 原为 10
    btnSave.Layout.Column = 2;

    btnExport = uibutton(rg,'Text','Export Selected Samples Data','ButtonPushedFcn',@(src,event) onExport());
    btnExport.Layout.Row = 11; % 原为 11
    btnExport.Layout.Column = [1, 2];

    btnOpenParams = uibutton(rg,'Text','Open Parameter Settings','ButtonPushedFcn',@(src,event) openParameterDialog());
    btnOpenParams.Layout.Row = 12; % 原为 12
    btnOpenParams.Layout.Column = [1, 2];

    divider = uilabel(rg,'Text','--------------------','HorizontalAlignment','center','FontAngle','italic');
    divider.Layout.Row = 13; % 原为 13
    divider.Layout.Column = [1, 2];

    statusLabel = uilabel(rg,'Text','Status: Idle','HorizontalAlignment','left');
    statusLabel.Layout.Row = 14; % 原为 14
    statusLabel.Layout.Column = [1, 2];

    infoLabel = uilabel(rg,'Text','Export: choose folder -> choose format (CSV/MAT/Excel)','HorizontalAlignment','left','FontAngle','italic');
    infoLabel.Layout.Row = 15; % 原为 15
    infoLabel.Layout.Column = [1, 2];

    %% app storage 初始参数 (包含新参数)
    app = struct();
    app.detailStruct = struct();
    app.currentGroup = '';
    app.compareWindow = 'all';
    app.maxInvalidFraction = 0.2;
    app.TARGET_LENGTH = 341;
    app.outlierWindow = 21;
    app.outlierNSigma = 4;
    app.jumpDiffThreshold = 0.1;
    app.diffThresholdPerc = 2;
    app.GlobalDeviationNSigma = 4;
    app.badPointTopN = 0;
    app.sgWindow = 11;
    app.sgPoly = 2;

    % 新增参数
    app.fitType = 'linear';
    app.gamma = 0.8;
    app.anchorWin = 30;
    app.monoStart = 20;
    app.monoEnd = 120;
    app.edgeBlend = 0.6;
    app.epsScale = 1e-9;
    app.slopeThreshold = 1e-9;
    app.loessSpan = 0.2;
    app.smoothMethod = 'LOESS';
    app.diffMethod = 'SG derivative';
    app.smoothDisplay = 'LOESS (only)';

    setappdata(fig,'app',app);

    % 尝试从 base workspace 覆盖参数（兼容）
    try
        if evalin('base','exist(''SG_WINDOW'',''var'')'), app.sgWindow = evalin('base','SG_WINDOW'); end
        if evalin('base','exist(''SG_POLY'',''var'')'), app.sgPoly = evalin('base','SG_POLY'); end
        if evalin('base','exist(''LOESS_SPAN'',''var'')'), app.loessSpan = evalin('base','LOESS_SPAN'); end
        if evalin('base','exist(''SMOOTH_METHOD'',''var'')'), app.smoothMethod = evalin('base','SMOOTH_METHOD'); end
        if evalin('base','exist(''DIFF_METHOD'',''var'')'), app.diffMethod = evalin('base','DIFF_METHOD'); end
        if evalin('base','exist(''SMOOTH_DISPLAY'',''var'')'), app.smoothDisplay = evalin('base','SMOOTH_DISPLAY'); end
        setappdata(fig,'app',app);
        ddSmoothDisplay.Value = app.smoothDisplay;
    catch
    end

    % 仅设置一次回调
    groupList.ValueChangedFcn = @onGroupSelect;

    %% 参数设置对话框（略去重复注释，保留功能）
    %% 参数设置对话框（已汉化）
    function openParameterDialog()
        screenSize = get(0, 'ScreenSize');
        dialogWidth = 560; dialogHeight = 760;
        left = (screenSize(3) - dialogWidth) / 2;
        bottom = (screenSize(4) - dialogHeight) / 2;
        % --- 修改点: 对话框窗口标题汉化 ---
        paramDialog = uifigure('Name', '参数设置', 'Position', [left bottom dialogWidth dialogHeight], 'Resize', 'off','WindowStyle','normal');

        % --- 修改点: 将所有参数标签(labels)改为中文 ---
        labels = {
            '无效数据最大比例 (max fraction):', ...
            '离群点检测窗口 (OUTLIER_WINDOW):', ...
            '离群点检测标准差倍数 (OUTLIER_NSIGMA):', ...
            '跳变点差异阈值 (JUMP_DIFF_THRESHOLD):', ...
            '标记差异百分比阈值 (Mark diff threshold %):', ...
            '显示坏点数量 (0=全部, >0=Top N):', ...
            'SG 滤波器窗口 (SG Filter Window):', ...
            'SG 滤波器多项式阶数 (SG Filter Polynomial Order):', ...
            '数据修复拟合类型 (Fit Type linear/quad):', ...
            '伽玛混合权重 (Gamma mix weight 0..1):', ...
            '锚定窗口大小 (Anchor window):', ...
            '单调区间起始点 (Mono Start):', ...
            '单调区间结束点 (Mono End):', ...
            '边缘混合系数 (Edge Blend 0..1):', ...
            'EPS 缩放因子 (EPS Scale):', ...
            '斜率阈值 (Slope Threshold):', ...
            'LOESS 平滑跨度 (LOESS Span):', ...
            '平滑方法 (Smoothing Method SG/LOESS):', ...
            '微分方法 (Differentiation Method):', ...
            '默认平滑曲线显示 (Default Smooth Display):' ...
            };

        currentApp = getappdata(fig,'app');

        paramValues = {
            num2str(currentApp.maxInvalidFraction), ...
            num2str(currentApp.outlierWindow), ...
            num2str(currentApp.outlierNSigma), ...
            num2str(currentApp.jumpDiffThreshold), ...
            num2str(currentApp.diffThresholdPerc), ...
            num2str(currentApp.badPointTopN), ...
            num2str(currentApp.sgWindow), ...
            num2str(currentApp.sgPoly), ...
            char(currentApp.fitType), ...
            num2str(currentApp.gamma), ...
            num2str(currentApp.anchorWin), ...
            num2str(currentApp.monoStart), ...
            num2str(currentApp.monoEnd), ...
            num2str(currentApp.edgeBlend), ...
            num2str(currentApp.epsScale), ...
            num2str(currentApp.slopeThreshold), ...
            num2str(currentApp.loessSpan), ...
            currentApp.smoothMethod, ...
            currentApp.diffMethod, ...
            currentApp.smoothDisplay
            };

        nRows = numel(labels) + 4;
        paramGrid = uigridlayout(paramDialog,[nRows,2]);
        paramGrid.RowHeight = repmat({'fit'},1,nRows);
        paramGrid.ColumnWidth = {'fit','1x'};
        paramGrid.Padding = [12 12 12 12];
        paramGrid.RowSpacing = 8;
        paramGrid.ColumnSpacing = 8;
        
        % --- 修改点: 标题和描述文本汉化 ---
        titleLabel = uilabel(paramGrid,'Text','处理参数','FontSize',16,'FontWeight','bold');
        titleLabel.Layout.Row = 1; titleLabel.Layout.Column = [1,2];

        descLabel = uilabel(paramGrid,'Text','修改参数后点击“应用”以更新:','FontSize',10);
        descLabel.Layout.Row = 2; descLabel.Layout.Column = [1,2];

        editFields = gobjects(numel(labels),1);
        for i = 1:numel(labels)
            label = uilabel(paramGrid,'Text',labels{i},'HorizontalAlignment','left');
            label.Layout.Row = i+2; label.Layout.Column = 1;
            if i == 9
                editFields(i) = uidropdown(paramGrid,'Items',{'linear','quad'},'Value',paramValues{i});
            elseif i == 18
                editFields(i) = uidropdown(paramGrid,'Items',{'SG','LOESS'},'Value',paramValues{i});
            elseif i == 19
                editFields(i) = uidropdown(paramGrid,'Items',{'SG derivative','First difference'},'Value',paramValues{i});
            elseif i == 20
                editFields(i) = uidropdown(paramGrid,'Items',{'SG (only)','LOESS (only)','Both','None'},'Value',paramValues{i});
            else
                editFields(i) = uieditfield(paramGrid,'text','Value',paramValues{i});
            end
            editFields(i).Layout.Row = i+2; editFields(i).Layout.Column = 2;
        end

        btnGrid = uigridlayout(paramGrid,[1,3]);
        btnGrid.Layout.Row = numel(labels)+3; btnGrid.Layout.Column = [1,2];
        btnGrid.ColumnWidth = {'1x','1x','1x'};

        % --- 修改点: 按钮和状态文本汉化 ---
        btnApply = uibutton(btnGrid,'Text','应用','ButtonPushedFcn',@applyParams);
        btnReset = uibutton(btnGrid,'Text','重置为默认值','ButtonPushedFcn',@resetParams);
        btnClose = uibutton(btnGrid,'Text','关闭','ButtonPushedFcn',@(s,e) close(paramDialog));
        statusParamLabel = uilabel(paramGrid,'Text','状态: 就绪','FontAngle','italic');
        statusParamLabel.Layout.Row = numel(labels)+4; statusParamLabel.Layout.Column = [1,2];

        function applyParams(~,~)
            try
                newParams.maxInvalidFraction = str2double(editFields(1).Value);
                newParams.outlierWindow = round(str2double(editFields(2).Value));
                newParams.outlierNSigma = str2double(editFields(3).Value);
                newParams.jumpDiffThreshold = str2double(editFields(4).Value);
                newParams.diffThresholdPerc = str2double(editFields(5).Value);
                newParams.badPointTopN = round(str2double(editFields(6).Value));
                newParams.sgWindow = round(str2double(editFields(7).Value));
                newParams.sgPoly = round(str2double(editFields(8).Value));
                newParams.fitType = char(editFields(9).Value);
                newParams.gamma = str2double(editFields(10).Value);
                newParams.anchorWin = round(str2double(editFields(11).Value));
                newParams.monoStart = round(str2double(editFields(12).Value));
                newParams.monoEnd = round(str2double(editFields(13).Value));
                newParams.edgeBlend = str2double(editFields(14).Value);
                newParams.epsScale = str2double(editFields(15).Value);
                newParams.slopeThreshold = str2double(editFields(16).Value);
                newParams.loessSpan = str2double(editFields(17).Value);
                newParams.smoothMethod = char(editFields(18).Value);
                newParams.diffMethod = char(editFields(19).Value);
                newParams.smoothDisplay = char(editFields(20).Value);

                % validation
                numericFields = rmfield(newParams, {'fitType','smoothMethod','diffMethod','smoothDisplay'});
                if any(isnan(struct2array(numericFields)))
                    error('所有数值参数必须为有效数值');
                end
                if newParams.maxInvalidFraction < 0 || newParams.maxInvalidFraction > 1, error('无效数据最大比例必须在 [0,1] 之间'); end
                if newParams.badPointTopN < 0, error('显示坏点数量必须为非负整数'); end
                if newParams.sgWindow < 3 || mod(newParams.sgWindow,2)==0, error('SG 窗口必须为奇数且 >=3'); end
                if newParams.sgPoly < 1 || newParams.sgPoly >= newParams.sgWindow, error('SG 多项式阶数不合法'); end
                if ~ismember(lower(newParams.fitType),{'linear','quad'}), error('拟合类型必须是 linear 或 quad'); end
                if newParams.gamma < 0 || newParams.gamma > 1, error('Gamma 必须在 0..1 之间'); end
                if newParams.edgeBlend < 0 || newParams.edgeBlend > 1, error('边缘混合系数必须在 0..1 之间'); end
                if newParams.epsScale <= 0, error('EPS 缩放因子必须 > 0'); end
                if newParams.monoStart < 1 || newParams.monoEnd < newParams.monoStart, error('单调区间起止点不合法'); end
                if newParams.loessSpan <= 0, error('LOESS 平滑跨度必须 > 0'); end
                if ~ismember(newParams.smoothMethod, {'SG','LOESS'}), error('平滑方法不合法'); end
                if ~ismember(newParams.diffMethod, {'SG derivative','First difference'}), error('微分方法不合法'); end
                if ~ismember(newParams.smoothDisplay, {'SG (only)','LOESS (only)','Both','None'}), error('平滑显示选项不合法'); end

                currentApp = getappdata(fig,'app');
                currentApp.maxInvalidFraction = newParams.maxInvalidFraction;
                currentApp.outlierWindow = newParams.outlierWindow;
                currentApp.outlierNSigma = newParams.outlierNSigma;
                currentApp.jumpDiffThreshold = newParams.jumpDiffThreshold;
                currentApp.diffThresholdPerc = newParams.diffThresholdPerc;
                currentApp.badPointTopN = newParams.badPointTopN;
                currentApp.sgWindow = newParams.sgWindow;
                currentApp.sgPoly = newParams.sgPoly;
                currentApp.fitType = newParams.fitType;
                currentApp.gamma = newParams.gamma;
                currentApp.anchorWin = newParams.anchorWin;
                currentApp.monoStart = newParams.monoStart;
                currentApp.monoEnd = newParams.monoEnd;
                currentApp.edgeBlend = newParams.edgeBlend;
                currentApp.epsScale = newParams.epsScale;
                currentApp.slopeThreshold = newParams.slopeThreshold;
                currentApp.loessSpan = newParams.loessSpan;
                currentApp.smoothMethod = newParams.smoothMethod;
                currentApp.diffMethod = newParams.diffMethod;
                currentApp.smoothDisplay = newParams.smoothDisplay;

                if mod(currentApp.outlierWindow,2)==0, currentApp.outlierWindow = currentApp.outlierWindow + 1; end
                if currentApp.outlierWindow < 3, currentApp.outlierWindow = 3; end
                setappdata(fig,'app',currentApp);
                ddSmoothDisplay.Value = currentApp.smoothDisplay;

                statusParamLabel.Text = '状态: 参数应用成功';
                statusLabel.Text = sprintf('状态: 参数已更新 (SG: 窗口=%d, 阶数=%d, LOESS 跨度=%s)', currentApp.sgWindow, currentApp.sgPoly, num2str(currentApp.loessSpan));

                if ~isempty(currentApp.currentGroup)
                    statusLabel.Text = '状态: 正在使用新参数重新加载组...';
                    drawnow;
                    loadGroup(currentApp.currentGroup, true);
                    statusLabel.Text = sprintf('状态: 组 %s 已使用新参数重新加载', currentApp.currentGroup);
                end

            catch ME
                statusParamLabel.Text = sprintf('状态: 错误 - %s', ME.message);
                uialert(paramDialog, ME.message, '参数错误');
            end
        end

        function resetParams(~,~)
            defaults = struct(...
                'maxInvalidFraction', 0.2, 'outlierWindow',21, 'outlierNSigma',4, 'jumpDiffThreshold',0.1, ...
                'diffThresholdPerc',2, 'badPointTopN',0, 'sgWindow',11, 'sgPoly',2, 'fitType','quad', ...
                'gamma',0.8, 'anchorWin',30, 'monoStart',20, 'monoEnd',120, 'edgeBlend',0.6, 'epsScale',1e-9, 'slopeThreshold',1e-9, ...
                'loessSpan',0.2, 'smoothMethod','LOESS', 'diffMethod','SG derivative', 'smoothDisplay','LOESS (only)');
            for i = 1:numel(labels)
                switch i
                    case 1, editFields(i).Value = num2str(defaults.maxInvalidFraction);
                    case 2, editFields(i).Value = num2str(defaults.outlierWindow);
                    case 3, editFields(i).Value = num2str(defaults.outlierNSigma);
                    case 4, editFields(i).Value = num2str(defaults.jumpDiffThreshold);
                    case 5, editFields(i).Value = num2str(defaults.diffThresholdPerc);
                    case 6, editFields(i).Value = num2str(defaults.badPointTopN);
                    case 7, editFields(i).Value = num2str(defaults.sgWindow);
                    case 8, editFields(i).Value = num2str(defaults.sgPoly);
                    case 9, editFields(i).Value = defaults.fitType;
                    case 10, editFields(i).Value = num2str(defaults.gamma);
                    case 11, editFields(i).Value = num2str(defaults.anchorWin);
                    case 12, editFields(i).Value = num2str(defaults.monoStart);
                    case 13, editFields(i).Value = num2str(defaults.monoEnd);
                    case 14, editFields(i).Value = num2str(defaults.edgeBlend);
                    case 15, editFields(i).Value = num2str(defaults.epsScale);
                    case 16, editFields(i).Value = num2str(defaults.slopeThreshold);
                    case 17, editFields(i).Value = num2str(defaults.loessSpan);
                    case 18, editFields(i).Value = defaults.smoothMethod;
                    case 19, editFields(i).Value = defaults.diffMethod;
                    case 20, editFields(i).Value = defaults.smoothDisplay;
                end
            end
        end
    end

    %% Callbacks and helpers
    function onReloadGroup()
        if isempty(groupList.Value)
            uialert(fig,'Please select a group first.','No Group'); return;
        end
        loadGroup(groupList.Value, true);
    end

    function onGroupSelect(src, ~)
        if isempty(src.Value), return; end
        loadGroup(src.Value, false);
    end

    function loadGroup(groupBase, forceReload)
        statusLabel.Text = sprintf('Status: Loading %s ...', groupBase); drawnow;
        if nargin < 2, forceReload = false; end

        app = getappdata(fig,'app');
        safeName = matlab.lang.makeValidName(groupBase);

        if isfield(app.detailStruct, safeName) && ~forceReload
            ds = app.detailStruct.(safeName);
        else
            filePathsQ = {}; filePathsZ = {}; filePathsH = {};
            shortNamesQ = {}; shortNamesZ = {}; shortNamesH = {};

            kAll = keyMap.keys();
            for kk = 1:numel(kAll)
                key = kAll{kk};
                tok = regexp(key,'^(.*)-([QZHqzh])$','tokens');
                if ~isempty(tok) && strcmp(tok{1}{1}, groupBase)
                    stage = upper(tok{1}{2});
                    files = keyMap(key);
                    try
                        fps = {files.filePath}';
                        for ff = 1:numel(fps)
                            [~, nm, ~] = fileparts(fps{ff});
                            switch stage
                                case 'Q'
                                    filePathsQ = [filePathsQ; fps(ff)];
                                    shortNamesQ = [shortNamesQ; {[nm ' (Q)']}];
                                case 'Z'
                                    filePathsZ = [filePathsZ; fps(ff)];
                                    shortNamesZ = [shortNamesZ; {[nm ' (Z)']}];
                                case 'H'
                                    filePathsH = [filePathsH; fps(ff)];
                                    shortNamesH = [shortNamesH; {[nm ' (H)']}];
                            end
                        end
                    catch
                    end
                end
            end

            [dtgCellsQ, rawCellsQ, validIdxQ, stepsQ] = readDTGsForStage(filePathsQ, shortNamesQ, app);
            [dtgCellsZ, rawCellsZ, validIdxZ, stepsZ] = readDTGsForStage(filePathsZ, shortNamesZ, app);
            [dtgCellsH, rawCellsH, validIdxH, stepsH] = readDTGsForStage(filePathsH, shortNamesH, app);

            ds = struct();
            ds.stages = {'Q','Z','H'};
            ds.filePaths = {filePathsQ, filePathsZ, filePathsH};
            ds.shortNames = {shortNamesQ, shortNamesZ, shortNamesH};
            ds.DTGcells = {dtgCellsQ, dtgCellsZ, dtgCellsH};
            ds.DTGcellsRaw = {rawCellsQ, rawCellsZ, rawCellsH};
            ds.StepsCells = {stepsQ, stepsZ, stepsH};
            ds.validIdx = {validIdxQ, validIdxZ, validIdxH};
            
            app.detailStruct.(safeName) = ds;
            setappdata(fig,'app',app);
        end

        updateSampleList(ds);

        app.currentGroup = groupBase;
        setappdata(fig,'app',app);

        nSamplesQ = numel(ds.filePaths{1});
        nSamplesZ = numel(ds.filePaths{2});
        nSamplesH = numel(ds.filePaths{3});
        statusLabel.Text = sprintf('Status: Loaded group %s (Q:%d, Z:%d, H:%d samples)', groupBase, nSamplesQ, nSamplesZ, nSamplesH);
    end

    %% 读取并计算每个样本各处理步骤（新增多种平滑/导数字段）
    function [dtgCells, rawCells, validIdx, stepsCells] = readDTGsForStage(filePaths, shortNames, app)
        dtgCells = {}; rawCells = {}; validIdx = []; stepsCells = {};
        if isempty(filePaths), return; end
        START_IDX = 60;

        for ii = 1:numel(filePaths)
            fp = filePaths{ii};
            steps = struct('rawSeg',[],'cleanedRaw',[],'truncated',[],'cleaned_truncated',[],'normalized',[],'smoothed_sg',[],'smoothed_loess',[],'smoothed',[],'dtg',[],'dtg_sg_from_sg',[],'dtg_loess_from_loess',[],'diff_from_sg',[],'diff_from_loess',[],'sgParams',[]);

            try
                % ==================== 核心修改点 ====================
                % 此处不再执行复杂的计算，而是直接调用统一的核心处理函数
                % GUI当前的参数 'app' 被直接传递过去
                steps = generateDTGSteps(fp, app);
                % ================================================

                % 从返回的steps结构体中提取所需数据以填充旧的cell数组
                rawToStore = [];
                if ~isempty(steps.rawSeg)
                    rawToStore = steps.rawSeg;
                elseif ~isempty(steps.cleanedRaw)
                    rawToStore = steps.cleanedRaw;
                end
                rawCells{end+1,1} = rawToStore;

                if isfield(steps,'dtg_final') && ~isempty(steps.dtg_final)
                    dtgCells{end+1,1} = steps.dtg_final(:);
                else
                    dtgCells{end+1,1} = [];
                end


                validIdx(end+1) = ii;
                stepsCells{end+1,1} = steps;

            catch ME
                warning('Error processing %s via generateDTGSteps: %s', fp, ME.message);
                dtgCells{end+1,1} = []; rawCells{end+1,1} = []; stepsCells{end+1,1} = struct();
            end
        end
    end

    function updateSampleList(ds)
        items = {};
        for stageIdx = 1:3
            stageName = ds.stages{stageIdx};
            shortNames = ds.shortNames{stageIdx};
            validIdx = ds.validIdx{stageIdx};

            if ~isempty(validIdx)
                items{end+1} = sprintf('--- %s Stage (%d samples) ---', stageName, numel(validIdx));
                for jj = 1:numel(validIdx)
                    nBad = 0;
                    try
                        steps = ds.StepsCells{stageIdx}{validIdx(jj)};
                        if isfield(steps,'rawSeg') && isfield(steps,'cleanedRaw') && ~isempty(steps.rawSeg) && ~isempty(steps.cleanedRaw)
                            compareLen = min(numel(steps.rawSeg), numel(steps.cleanedRaw));
                            rawChk = steps.rawSeg(1:compareLen);
                            cleanedChk = steps.cleanedRaw(1:compareLen);
                            nBad = sum(isnan(cleanedChk) & ~isnan(rawChk));
                        end
                    catch
                        nBad = 0;
                    end
                    if nBad > 0
                        items{end+1} = sprintf('  %s (bad:%d)', shortNames{validIdx(jj)}, nBad);
                    else
                        items{end+1} = ['  ' shortNames{validIdx(jj)}];
                    end
                end
            else
                items{end+1} = sprintf('--- %s Stage (0 samples) ---', stageName);
            end
        end
        sampleList.Items = items;

        allValidItems = {};
        for stageIdx = 1:3
            validIdx = ds.validIdx{stageIdx};
            shortNames = ds.shortNames{stageIdx};
            for jj = 1:numel(validIdx)
                nBad = 0;
                try
                    steps = ds.StepsCells{stageIdx}{validIdx(jj)};
                    if isfield(steps,'rawSeg') && isfield(steps,'cleanedRaw') && ~isempty(steps.rawSeg) && ~isempty(steps.cleanedRaw)
                        compareLen = min(numel(steps.rawSeg), numel(steps.cleanedRaw));
                        rawChk = steps.rawSeg(1:compareLen);
                        cleanedChk = steps.cleanedRaw(1:compareLen);
                        nBad = sum(isnan(cleanedChk) & ~isnan(rawChk));
                    end
                catch
                    nBad = 0;
                end
                if nBad > 0
                    allValidItems{end+1} = sprintf('  %s (bad:%d)', shortNames{validIdx(jj)}, nBad);
                else
                    allValidItems{end+1} = ['  ' shortNames{validIdx(jj)}];
                end
            end
        end
        if ~isempty(allValidItems)
            sampleList.Value = allValidItems;
        end
    end


    %% Plot selected
    %% 新增：可重用的核心绘图函数
    function plotGroupDataOntoAxes(targetAxes, legendAxesHandle, ds, plotSettings)
        % targetAxes: 一个包含3个坐标轴句柄的数组 [axQ, axZ, axH]
        % legendAxesHandle: 用于绘制图例的坐标轴句柄
        % ds: group的数据结构 (detailStruct)
        % plotSettings: 包含所有可视化选项的结构体

        % 从 plotSettings 中解包所有设置
        showMean = plotSettings.showMean;
        interpNaN = plotSettings.interpNaN;
        showRaw = plotSettings.showRaw;
        showBad = plotSettings.showBad;
        smoothDisplay = plotSettings.smoothDisplay;
        selectedStep = plotSettings.selectedStep;
        plotType = plotSettings.plotType;
        samplesToSelect = plotSettings.samplesToSelect; % 要绘制的样本列表
        app = plotSettings.appParams; % 包含所有处理参数

        cla(legendAxesHandle); hold(legendAxesHandle,'on');
        allLegendHandles = gobjects(0);
        allLegendLabels = {};

        START_IDX = 60;
        stageNames = {'Q (前)', 'Z (中)', 'H (后)'};

        % ***** 核心修正点: 动态计算平均曲线 *****
        % =================================================================
        
        % --- 步骤1: 跨所有阶段收集当前步骤的数据用于计算总平均 ---
        all_y_data_for_mean = {};
        if showMean
            % 遍历所有阶段 (Q,Z,H)
            for stageIdx_mean = 1:3
                stepsCells_mean = ds.StepsCells{stageIdx_mean};
                validIdx_mean = ds.validIdx{stageIdx_mean};
                shortNames_mean = ds.shortNames{stageIdx_mean};
                
                % 遍历该阶段的所有有效样本
                for ii_mean = 1:numel(validIdx_mean)
                    idx = validIdx_mean(ii_mean);
                    
                    % 根据用户选择，从所有样本中提取当前步骤的数据
                    y_data = getStepData(stepsCells_mean{idx}, selectedStep, smoothDisplay, app);
                    
                    if ~isempty(y_data)
                        all_y_data_for_mean{end+1} = y_data(:);
                    end
                end
            end
        end
        
        % --- 步骤2: 计算总平均曲线 ---
        meanCurve = [];
        if showMean && ~isempty(all_y_data_for_mean)
            lens = cellfun(@numel, all_y_data_for_mean);
            minLen = min(lens);
            M = nan(minLen, numel(all_y_data_for_mean));
            for ii = 1:numel(all_y_data_for_mean)
                v = all_y_data_for_mean{ii};
                M(:,ii) = v(1:minLen);
            end
            meanCurve = nanmean(M, 2);
        end
        
        % --- 步骤3: 正常绘图循环 ---
        for stageIdx = 1:3
            ax_current = targetAxes(stageIdx);
            cla(ax_current); hold(ax_current,'on'); grid(ax_current,'on');
            title(ax_current, stageNames{stageIdx}, 'FontSize', 12, 'FontWeight', 'bold');
            xlabel(ax_current,'Index'); ylabel(ax_current,'Value');

            shortNames = ds.shortNames{stageIdx};
            validIdx = ds.validIdx{stageIdx};
            stepsCells = ds.StepsCells{stageIdx};

            if isempty(validIdx), continue; end

            samplesToPlot = [];
            for ii = 1:numel(validIdx)
                sampleNameInList = ['  ' shortNames{validIdx(ii)}];
                if any(strcmp(samplesToSelect, sampleNameInList)), samplesToPlot(end+1) = validIdx(ii); end
            end
            if isempty(samplesToPlot), continue; end

            badLegendAdded = false;
            nCurvesColors = max(1, numel(samplesToPlot) + double(showMean));
            colors = lines(nCurvesColors);
            colorIndex = 1;

            for k = 1:numel(samplesToPlot)
                sampleIdx = samplesToPlot(k);
                stepData = stepsCells{sampleIdx};
                
                % --- 使用辅助函数获取数据 ---
                [y, labelSuffix] = getStepData(stepData, selectedStep, smoothDisplay, app);

                if isempty(y), continue; end
                % ... (从这里开始的绘图代码与您之前的版本几乎完全相同)
                
                xr = (1:numel(y))';
                if interpNaN && any(isnan(y)) && any(~isnan(y)), y = fillmissing(y,'linear'); end

                % ==================== 核心修改点: 简化图例名称 ====================
                originalShortName = shortNames{sampleIdx};

                % 使用正则表达式查找'-盘'或'盘'之前的部分
                token = regexp(originalShortName, '^(.*?)(\-盘|盘)', 'tokens', 'once');
                if ~isempty(token)
                    simplifiedName = token{1};
                else
                    % 如果没有找到'盘'字，则使用原始名称，以防万一
                    simplifiedName = originalShortName;
                end

                % 移除可能存在的阶段标识 '(Q)', '(Z)', '(H)'
                simplifiedName = strtrim(regexprep(simplifiedName, '\s*\([QZH]\)$', ''));
                
                mainLabel = [simplifiedName, labelSuffix]; % 使用简化后的名称
                % ================================================================

                if strcmp(plotType, 'Scatter')
                    p = scatter(ax_current, xr, y, 5, 'filled', 'DisplayName', mainLabel);
                    try p.CData = colors(colorIndex,:); catch, end
                else
                    p = plot(ax_current, xr, y, 'LineWidth', 1.5, 'DisplayName', mainLabel);
                    try set(p,'Color',colors(colorIndex,:)); catch, end
                end

                allLegendHandles(end+1) = p; allLegendLabels{end+1} = mainLabel;
                colorIndex = colorIndex + 1;
                
                % ... (showBad, showRaw 等代码保持不变, 此处省略)
            end

            % --- 步骤4: 绘制动态计算出的平均曲线 ---
            if showMean && ~isempty(meanCurve)
                v = meanCurve;
                if interpNaN && any(isnan(v)) && any(~isnan(v)), v = fillmissing(v,'linear'); end
                
                meanLabel = sprintf('Mean of ALL Stages (%s)', selectedStep);
                
                if strcmp(plotType, 'Scatter')
                    p = scatter(ax_current, (1:numel(v))', v, 10, 'd', 'DisplayName', meanLabel);
                else
                    p = plot(ax_current,(1:numel(v))', v, 'LineWidth',2.2,'LineStyle','--','DisplayName', meanLabel);
                end
                
                % 确保图例只添加一次
                if stageIdx == 1
                    allLegendHandles(end+1) = p;
                    allLegendLabels{end+1} = meanLabel;
                end
            end
        end

        % 更新图例
        if ~isempty(allLegendHandles)
            legend(legendAxesHandle,'off');
            try
                lgd = legend(legendAxesHandle, allLegendHandles, allLegendLabels, 'Orientation','vertical','Box','off','FontSize',9);
                set(lgd,'Position',[0.05 0.02 0.9 0.96]);
            catch
            end
        end
    end

    %% Plot selected (这个函数现在变得非常简洁)
    function onPlotSelected()
        app = getappdata(fig,'app');
        if isempty(app.currentGroup), uialert(fig,'Please select a group first.','No Group'); return; end
        safeName = matlab.lang.makeValidName(app.currentGroup);
        if ~isfield(app.detailStruct, safeName)
            choice = questdlg('Selected group not loaded. Load now?','Load Group','Yes','No','Yes');
            if strcmp(choice,'Yes'), loadGroup(app.currentGroup, true); else return; end
        end
        ds = app.detailStruct.(safeName);
        sel = sampleList.Value;
        if isempty(sel), uialert(fig,'Please select one or more samples.','No Selection'); return; end

        % 1. 捕获所有当前设置
        plotSettings = struct();
        plotSettings.showMean = cbMean.Value;
        plotSettings.interpNaN = cbInterp.Value;
        plotSettings.showRaw = cbShowRaw.Value;
        plotSettings.showBad = cbShowBad.Value;
        plotSettings.smoothDisplay = ddSmoothDisplay.Value;
        plotSettings.selectedStep = ddSteps.Value;
        plotSettings.plotType = ddPlotType.Value;
        plotSettings.samplesToSelect = sel; % 传递当前选择的样本
        plotSettings.appParams = app;

        % 2. 调用重构后的绘图函数，作用于主GUI的坐标轴
        plotGroupDataOntoAxes(ax, legendAxes, ds, plotSettings);

        statusLabel.Text = sprintf('Status: Plotted "%s" for selected samples in group %s', ddSteps.Value, app.currentGroup);
    end

    function onPlotAll()
        app = getappdata(fig,'app');
        if isempty(app.currentGroup), uialert(fig,'Please select a group first.','No Group'); return; end
        safeName = matlab.lang.makeValidName(app.currentGroup);
        if ~isfield(app.detailStruct, safeName), uialert(fig,'Selected group not loaded. Please (Re)Load first.','No Data'); return; end
        ds = app.detailStruct.(safeName);
        allValidItems = {};
        for stageIdx = 1:3
            validIdx = ds.validIdx{stageIdx};
            shortNames = ds.shortNames{stageIdx};
            for jj = 1:numel(validIdx)
                allValidItems{end+1} = ['  ' shortNames{validIdx(jj)}];
            end
        end
        if ~isempty(allValidItems), sampleList.Value = allValidItems; end
        onPlotSelected();
    end

    function onClear()
        for ii = 1:3
            cla(ax(ii)); grid(ax(ii),'on');
            title(ax(ii), stageNames{ii}, 'FontSize', 12, 'FontWeight', 'bold');
            xlabel(ax(ii),'Index'); ylabel(ax(ii),'Value');
        end
        cla(legendAxes); legend(legendAxes,'off');
        statusLabel.Text = 'Status: Cleared plots';
    end

    %% 新增：批量绘图并保存所有 Group 的回调函数 (增加状态恢复和终止功能)
    function onBatchPlotAndSave(~,~)
        % =================================================================
        % ***** 核心修正点 1: 保存当前状态 *****
        % =================================================================
        originalGroup = groupList.Value; % 记录批量操作前选中的Group
    
        plotSettings = struct();
        plotSettings.showMean = cbMean.Value;
        plotSettings.interpNaN = cbInterp.Value;
        plotSettings.showRaw = cbShowRaw.Value;
        plotSettings.showBad = cbShowBad.Value;
        plotSettings.smoothDisplay = ddSmoothDisplay.Value;
        plotSettings.selectedStep = ddSteps.Value;
        plotSettings.plotType = ddPlotType.Value;
        plotSettings.appParams = getappdata(fig,'app');
        
        allGroups = groupList.Items;
        if isempty(allGroups), uialert(fig, 'No groups found.', 'Error'); return; end
        
        outFolder = uigetdir(pwd, 'Select a FOLDER to save all generated plots');
        if isequal(outFolder,0), return; end
        
        formatChoice = questdlg('Choose an image format for ALL plots', 'Image Format', 'PNG', 'JPG', 'PNG');
        if isempty(formatChoice), return; end
        fileFormat = lower(formatChoice);

        originalStatus = statusLabel.Text;
        plotsSaved = 0;
        
        try
            hWait = uiprogressdlg(fig,'Title','Batch Plotting & Saving','Message','Starting...','Cancelable','on','Indeterminate','on');
            
            for i = 1:numel(allGroups)
                if hWait.CancelRequested, break; end
                
                groupBase = allGroups{i};
                
                msg = sprintf('Processing group %d/%d: %s', i, numel(allGroups), groupBase);
                statusLabel.Text = msg; hWait.Message = msg; drawnow;
                
                loadGroup(groupBase, true);
                currentApp = getappdata(fig,'app');
                safeName = matlab.lang.makeValidName(groupBase);
                if ~isfield(currentApp.detailStruct, safeName), continue; end
                ds = currentApp.detailStruct.(safeName);
                
                allValidItems = {};
                for stageIdx = 1:3
                    validIdx = ds.validIdx{stageIdx};
                    shortNames = ds.shortNames{stageIdx};
                    for jj = 1:numel(validIdx)
                        allValidItems{end+1} = ['  ' shortNames{validIdx(jj)}];
                    end
                end
                plotSettings.samplesToSelect = allValidItems;
                
                generateAndSaveGroupPlot(ds, groupBase, outFolder, fileFormat, plotSettings);
                plotsSaved = plotsSaved + 1;
            end
            
            wasCanceled = hWait.CancelRequested;
            close(hWait);
            
            if wasCanceled
                uialert(fig, sprintf('Batch plot CANCELED by user.\n\n%d plot(s) were saved.', plotsSaved), 'Operation Canceled');
            else
                uialert(fig, sprintf('Batch plot complete.\n\n%d plots saved in:\n%s', plotsSaved, outFolder), 'Success');
            end
            
        catch ME
            if exist('hWait','var') && isvalid(hWait), close(hWait); end
            uialert(fig, sprintf('An error occurred during batch processing:\n\n%s', ME.message), 'Error');
            statusLabel.Text = originalStatus;
            rethrow(ME);
        end

        % =================================================================
        % ***** 核心修正点 2: 恢复原始状态 *****
        % =================================================================
        if ~isempty(originalGroup)
            statusLabel.Text = 'Status: Restoring original selection...'; drawnow;
            loadGroup(originalGroup, true);
        else
            statusLabel.Text = 'Status: Batch operation finished.';
        end
    end

    %% 新增：在后台生成并保存单个Group图像的函数 (修正版 - 增加drawnow)
    function generateAndSaveGroupPlot(ds, groupName, outFolder, fileFormat, settings)
        % 创建一个临时的、不可见的 uifigure
        figWidth = 1200; figHeight = 800;
        hFigTemp = uifigure('Visible', 'off', 'Name', ['Plot for ' groupName], 'Position', [0 0 figWidth figHeight]);
        
        % 在临时 figure 中创建布局和坐标轴
        mainGrid = uigridlayout(hFigTemp,[1,2]);
        mainGrid.ColumnWidth = {'1x', 400}; % 分配400像素宽度给图例
        mainGrid.RowHeight = {'1x'};
        
        centerPanel = uipanel(mainGrid,'Title','Process Stages (Q-Z-H)');
        legendPanel = uipanel(mainGrid,'Title','Legend','BackgroundColor','w');
        
        t = tiledlayout(centerPanel,3,1,'Padding','compact','TileSpacing','compact');
        temp_ax = gobjects(3,1);
        for i=1:3, temp_ax(i) = nexttile(t); end
        
        % --- 核心修正点: 创建一个“伪”可见的坐标轴来承载图例 ---
        % 1. 创建一个默认可见的坐标轴
        temp_legendAxes = axes(legendPanel, 'Position', [0.02 0.02 0.96 0.96]); 
        
        % 2. 将其所有装饰元素（坐标轴线、刻度、背景色）都设为不可见或透明
        %    这样坐标轴本身不显示任何东西，但它内部的图例对象会被正确渲染
        set(temp_legendAxes, 'XTick', [], 'YTick', [], 'XColor', 'none', 'YColor', 'none', 'Color', 'none', 'Box', 'off');
        hold(temp_legendAxes, 'on'); % 保持 hold on 状态
        
        % 3. 调用核心绘图函数时，传入这个新的临时句柄
        plotGroupDataOntoAxes(temp_ax, temp_legendAxes, ds, settings);
        
        % =================================================================
        % ***** 核心修正点 *****
        % 在保存之前，强制 MATLAB 完成所有挂起的绘图操作，确保图例被渲染
        drawnow;
        % =================================================================
        
        % 定义输出文件名
        stepNameClean = matlab.lang.makeValidName(settings.selectedStep);
        fileName = fullfile(outFolder, sprintf('GroupPlot_%s_%s.%s', groupName, stepNameClean, fileFormat));
        
        % 使用与 onSave 类似的逻辑来拼接和保存图像
        try
            % 导出中心绘图区域与图例面板为位图
            tmp1 = [tempname '.png'];
            tmp2 = [tempname '.png'];
            exportgraphics(centerPanel, tmp1, 'Resolution', 200);
            exportgraphics(legendPanel, tmp2, 'Resolution', 200);

            im1 = imread(tmp1);
            im2 = imread(tmp2);
            
            % 清理临时文件（尽早删除）
            delete(tmp1); delete(tmp2);

            % 对齐高度并拼接
            h1 = size(im1,1); h2 = size(im2,1); H = max(h1,h2);
            % 使用白色像素(255)填充
            if h1 < H, pad = floor((H - h1)/2); im1 = padarray(im1, [pad,0], 255, 'pre'); im1 = padarray(im1, [H-size(im1,1),0], 255, 'post'); end
            if h2 < H, pad = floor((H - h2)/2); im2 = padarray(im2, [pad,0], 255, 'pre'); im2 = padarray(im2, [H-size(im2,1),0], 255, 'post'); end
            
            % 确保图像尺寸完全一致再拼接
            final_h = max(size(im1,1), size(im2,1));
            im1 = im1(1:final_h, :, :);
            im2 = im2(1:final_h, :, :);
            imCombined = [im1, im2];
            
            imwrite(imCombined, fileName);
            
        catch ME
            warning('Failed to save combined plot for %s: %s. Saving main plot only.', groupName, ME.message);
            exportgraphics(centerPanel, fileName); % Fallback
        end
        
        % 关闭临时figure
        close(hFigTemp);
    end

    %% Save: 支持多 step 的合并图片（每行 3 列）
    function onSave()
        app = getappdata(fig,'app');
        if isempty(app.currentGroup)
            uialert(fig,'Please select a group first.','No Group'); return;
        end
        defaultFilename = sprintf('DTG_Group_%s_%s.png', app.currentGroup, datestr(now,'yyyy-mm-dd_HHMMSS'));
        [filename, pathname] = uiputfile({'*.png','PNG Image (*.png)'; '*.jpg','JPEG Image (*.jpg)'; '*.pdf','PDF File (*.pdf)'; '*.fig','MATLAB Figure (*.fig)'}, 'Save Figure As', defaultFilename);
        if isequal(filename,0) || isequal(pathname,0), return; end
        fullname = fullfile(pathname, filename);

        % 尝试将 centerPanel 和 legendPanel 分别导出为图像，然后合并
        tmp1 = fullfile(tempdir, ['dtg_center_' char(java.util.UUID.randomUUID()) '.png']);
        tmp2 = fullfile(tempdir, ['dtg_legend_' char(java.util.UUID.randomUUID()) '.png']);
        success = false;
        try
            % 导出中心绘图区域与图例面板为位图
            exportgraphics(centerPanel, tmp1, 'Resolution', 300, 'BackgroundColor', 'white');
            exportgraphics(legendPanel, tmp2, 'Resolution', 300, 'BackgroundColor', 'white');

            im1 = imread(tmp1);
            im2 = imread(tmp2);

            % 对齐高度：以较大高度为准，上下各填充白色像素行
            h1 = size(im1,1); h2 = size(im2,1);
            H = max(h1,h2);
            if h1 < H
                padTop = floor((H - h1)/2);
                padBottom = H - h1 - padTop;
                im1 = padarray(im1, [padTop,0], 255, 'pre');
                im1 = padarray(im1, [padBottom,0], 255, 'post');
            end
            if h2 < H
                padTop = floor((H - h2)/2);
                padBottom = H - h2 - padTop;
                im2 = padarray(im2, [padTop,0], 255, 'pre');
                im2 = padarray(im2, [padBottom,0], 255, 'post');
            end

            % 横向拼接
            imCombined = [im1, im2];

            [~,~,ext] = fileparts(filename);
            switch lower(ext)
                case {'.png', '.jpg', '.jpeg'}
                    imwrite(imCombined, fullname);
                case '.pdf'
                    % 使用临时标准 figure 将图像作为图像对象绘出再导出为 PDF（矢量化不是必需的，但保证 PDF 可用）
                    hf = figure('Visible','off','PaperPositionMode','auto','Color','white','Units','pixels','Position',[100 100 size(imCombined,2) size(imCombined,1)]);
                    axIm = axes(hf,'Units','normalized','Position',[0 0 1 1]);
                    imshow(imCombined,'Parent',axIm);
                    axis(axIm,'off');
                    try
                        exportgraphics(hf, fullname, 'ContentType','image');
                    catch
                        print(hf, fullname, '-dpdf', '-r300');
                    end
                    close(hf);
                case '.fig'
                    hf = figure('Visible','off','Color','white','Units','pixels','Position',[100 100 size(imCombined,2) size(imCombined,1)]);
                    axIm = axes(hf,'Units','normalized','Position',[0 0 1 1]);
                    imshow(imCombined,'Parent',axIm);
                    axis(axIm,'off');
                    savefig(hf, fullname);
                    close(hf);
                otherwise
                    % fallback to centerPanel export if unknown extension
                    exportgraphics(centerPanel, fullname, 'Resolution', 300, 'BackgroundColor', 'white');
            end

            success = true;
            uialert(fig,sprintf('Figure saved to %s', fullname),'Saved');
        catch ME
            % 回退：如果合并保存失败则尝试直接保存 centerPanel（老行为）
            try
                [~,~,ext] = fileparts(filename);
                switch lower(ext)
                    case {'.png','.jpg'}
                        exportgraphics(centerPanel, fullname, 'Resolution', 300, 'BackgroundColor', 'white');
                    case '.pdf'
                        exportgraphics(centerPanel, fullname, 'ContentType','vector','BackgroundColor','white');
                    case '.fig'
                        % 无法直接保存 uifigure 为 .fig；创建标准 figure 并复制图像
                        tmpA = fullfile(tempdir, ['dtg_center_fallback_' char(java.util.UUID.randomUUID()) '.png']);
                        exportgraphics(centerPanel, tmpA, 'Resolution', 300, 'BackgroundColor', 'white');
                        img = imread(tmpA);
                        hf = figure('Visible','off','Color','white','Units','pixels','Position',[100 100 size(img,2) size(img,1)]);
                        axIm = axes(hf,'Units','normalized','Position',[0 0 1 1]);
                        imshow(img,'Parent',axIm); axis(axIm,'off');
                        savefig(hf, fullname);
                        close(hf);
                    otherwise
                        exportgraphics(centerPanel, fullname, 'Resolution', 300, 'BackgroundColor', 'white');
                end
                uialert(fig,sprintf('Figure saved to %s (legend merge fallback used)', fullname),'Saved');
                success = true;
            catch ME2
                uialert(fig,sprintf('Failed to save: %s\nFallback error: %s', ME.message, ME2.message),'Save Error');
                success = false;
            end
        end

        % 清理临时文件
        try
            if exist(tmp1,'file'), delete(tmp1); end
            if exist(tmp2,'file'), delete(tmp2); end
        catch
        end

        if success
            statusLabel.Text = sprintf('Status: Saved figure to %s', fullname);
        else
            statusLabel.Text = 'Status: Save failed';
        end
    end

    %% 新增：可重用的批量导出辅助函数
    function savedCount = exportGroupData(ds, groupName, outFolder, format)
        savedCount = 0;
        if strcmp(format, 'Excel (XLSX)')
            excelFileName = fullfile(outFolder, sprintf('DTG_Group_%s_Data.xlsx', groupName));
            if exist(excelFileName, 'file'), delete(excelFileName); end
        end
        usedSheetNames = containers.Map();

        for stageIdx = 1:3
            shortNames = ds.shortNames{stageIdx};
            stepsCells = ds.StepsCells{stageIdx};
            validIdx = ds.validIdx{stageIdx};
            % 遍历该阶段所有有效的样本
            for ii = 1:numel(validIdx)
                idx = validIdx(ii);
                steps = stepsCells{idx};

                % 定义要从 steps 结构体中提取的字段名
                fields_to_extract = {'truncated', 'cleaned_truncated', 'normalized', 'smoothed_loess', 'dtg_loess_from_loess'};
                % 定义最终在表格中显示的列名
                column_names = {'truncated', 'cleaned_truncated', 'normalized', 'smoothed_loess', 'SGdtg_from_loess'};
                
                arrays = cell(1,numel(fields_to_extract)); 
                maxL = 0;
                for f = 1:numel(fields_to_extract)
                    val = [];
                    fieldName = fields_to_extract{f};
                    if isfield(steps, fieldName), val = steps.(fieldName); end
                    if isempty(val), val = nan(0,1); else val = val(:); end
                    arrays{f} = val;
                    maxL = max(maxL, numel(val));
                end

                for f = 1:numel(fields_to_extract)
                    v = arrays{f};
                    if numel(v) < maxL, v(end+1:maxL) = NaN; arrays{f} = v; end
                end

                tbl = table();
                for f = 1:numel(fields_to_extract)
                    % 使用我们定义的 column_names 作为列标题
                    tbl.(column_names{f}) = arrays{f};
                end

                baseShort = shortNames{idx};
                baseShortClean = regexprep(baseShort,'[\s\(\)]','_');
                fnameBase = sprintf('%s_%s_%s', groupName, ds.stages{stageIdx}, baseShortClean);
                fnameBase = matlab.lang.makeValidName(fnameBase);

                if strcmp(format,'Excel (XLSX)')
                    try
                        sheetName = strtrim(shortNames{idx});
                        sheetName = regexprep(sheetName, '[\\/:*?"<>|]', '_');
                        sheetName = [ds.stages{stageIdx} '_' sheetName];
                        if ~isletter(sheetName(1)), sheetName = ['S_' sheetName]; end
                        if length(sheetName) > 31, sheetName = sheetName(1:31); end
                        if isKey(usedSheetNames, sheetName)
                            count = usedSheetNames(sheetName) + 1; usedSheetNames(sheetName) = count;
                            suffix = ['_' num2str(count)];
                            if length(sheetName) + length(suffix) <= 31, sheetName = [sheetName suffix];
                            else sheetName = [sheetName(1:31-length(suffix)) suffix]; end
                        else
                            usedSheetNames(sheetName) = 1;
                        end
                        writetable(tbl, excelFileName, 'Sheet', sheetName);
                        savedCount = savedCount + 1;
                    catch ME
                        warning('Failed to write Excel sheet for %s: %s', fnameBase, ME.message);
                    end
                end

                if strcmp(format,'MAT')
                    try
                        matName = fullfile(outFolder, [fnameBase '.mat']);
                        save(matName, 'steps', '-v7.3');
                        savedCount = savedCount + 1;
                    catch ME
                        warning('Failed to save MAT for %s: %s', fnameBase, ME.message);
                    end
                end

                if strcmp(format,'CSV')
                    try
                        csvName = fullfile(outFolder, [fnameBase '.csv']);
                        writetable(tbl, csvName);
                        savedCount = savedCount + 1;
                    catch ME
                        warning('Failed to write CSV for %s: %s', fnameBase, ME.message);
                    end
                end
            end
        end
    end

    %% Export data table (含 SG/LOESS 平滑及基于它们的导数/差分)
    function onExport()
        app = getappdata(fig,'app');
        if isempty(app.currentGroup), uialert(fig,'Please select a group first.','No Group'); return; end
        safeName = matlab.lang.makeValidName(app.currentGroup);
        if ~isfield(app.detailStruct, safeName), uialert(fig,'Selected group not loaded. Please (Re)Load first.','No Data'); return; end
        ds = app.detailStruct.(safeName);
        sel = sampleList.Value;
        if isempty(sel), uialert(fig,'Please select one or more samples in the right list to export.','No Selection'); return; end

        outFolder = uigetdir(pwd, sprintf('Select folder to save exported data for group %s', app.currentGroup));
        if isequal(outFolder,0), return; end

        choice = questdlg('Choose export format','Export Format','CSV','MAT','Excel (XLSX)','Excel (XLSX)');
        if isempty(choice), return; end

        START_IDX = 60;
        nSaved = 0;
        if strcmp(choice, 'Excel (XLSX)')
            excelFileName = fullfile(outFolder, sprintf('DTG_Group_%s_Data.xlsx', app.currentGroup));
            if exist(excelFileName, 'file'), delete(excelFileName); end
        end
        usedSheetNames = containers.Map();

        for stageIdx = 1:3
            shortNames = ds.shortNames{stageIdx};
            stepsCells = ds.StepsCells{stageIdx};
            validIdx = ds.validIdx{stageIdx};
            for ii = 1:numel(validIdx)
                idx = validIdx(ii);
                itemName = ['  ' shortNames{idx}];
                if ~any(strcmp(sel, itemName)), continue; end
                steps = stepsCells{idx};

                % fields to export:
                fields_to_extract = {'truncated', 'cleaned_truncated', 'normalized', 'smoothed_loess', 'dtg_loess_from_loess'};
                column_names = {'truncated', 'cleaned_truncated', 'normalized', 'smoothed_loess', 'SGdtg_from_loess'};

                arrays = cell(1,numel(fields_to_extract)); 
                maxL = 0;
                for f = 1:numel(fields_to_extract)
                    val = [];
                    fieldName = fields_to_extract{f};
                    if isfield(steps, fieldName), val = steps.(fieldName); end
                    if isempty(val), val = nan(0,1); else val = val(:); end
                    arrays{f} = val;
                    maxL = max(maxL, numel(val));
                end

                % ensure same length
                for f = 1:numel(fields_to_extract)
                    v = arrays{f};
                    if numel(v) < maxL, v(end+1:maxL) = NaN; arrays{f} = v; end
                end

                tbl = table();
                for f = 1:numel(fields_to_extract)
                    tbl.(column_names{f}) = arrays{f};
                end

                baseShort = shortNames{idx};
                baseShortClean = regexprep(baseShort,'[\s\(\)]','_');
                fnameBase = sprintf('%s_%s_%s', app.currentGroup, ds.stages{stageIdx}, baseShortClean);
                fnameBase = matlab.lang.makeValidName(fnameBase);

                if strcmp(choice,'Excel (XLSX)')
                    try
                        sheetName = strtrim(shortNames{idx});
                        sheetName = regexprep(sheetName, '[\\/:*?"<>|]', '_');
                        sheetName = [ds.stages{stageIdx} '_' sheetName];
                        if ~isletter(sheetName(1)), sheetName = ['S_' sheetName]; end
                        if length(sheetName) > 31, sheetName = sheetName(1:31); end
                        if isKey(usedSheetNames, sheetName)
                            count = usedSheetNames(sheetName) + 1; usedSheetNames(sheetName) = count;
                            suffix = ['_' num2str(count)];
                            if length(sheetName) + length(suffix) <= 31, sheetName = [sheetName suffix];
                            else sheetName = [sheetName(1:31-length(suffix)) suffix]; end
                        else
                            usedSheetNames(sheetName) = 1;
                        end
                        writetable(tbl, excelFileName, 'Sheet', sheetName);
                        nSaved = nSaved + 1;
                    catch ME
                        warning('Failed to write Excel sheet for %s: %s', fnameBase, ME.message);
                    end
                end

                if strcmp(choice,'MAT')
                    try
                        matName = fullfile(outFolder, [fnameBase '.mat']);
                        save(matName, 'steps', '-v7.3');
                        nSaved = nSaved + 1;
                    catch ME
                        warning('Failed to save MAT for %s: %s', fnameBase, ME.message);
                    end
                end

                if strcmp(choice,'CSV')
                    try
                        csvName = fullfile(outFolder, [fnameBase '.csv']);
                        writetable(tbl, csvName);
                        nSaved = nSaved + 1;
                    catch ME
                        warning('Failed to write CSV for %s: %s', fnameBase, ME.message);
                    end
                end
            end
        end

        if nSaved > 0
            uialert(fig, sprintf('Export completed. %d sample(s) exported to:\n%s', nSaved, outFolder), 'Export Done');
        else
            uialert(fig, 'No samples were exported.', 'Export Done');
        end
        statusLabel.Text = sprintf('Status: Exported %d samples to %s', nSaved, outFolder);
    end

    %% 新增：一键导出所有 Group 数据的回调函数 (增加状态恢复和终止功能)
    function onExportAllGroups(~,~)
        app = getappdata(fig,'app');
        allGroups = groupList.Items;
        if isempty(allGroups)
            uialert(fig, 'No groups found in the list.', 'Export Error');
            return;
        end
        
        % =================================================================
        % ***** 核心修正点 1: 保存当前状态 *****
        % =================================================================
        originalGroup = groupList.Value; % 记录批量操作前选中的Group
        
        outFolder = uigetdir(pwd, 'Select a FOLDER to save all exported group data');
        if isequal(outFolder,0), return; end

        choice = questdlg('Choose export format for ALL groups','Export Format','CSV','MAT','Excel (XLSX)','Excel (XLSX)');
        if isempty(choice), return; end
        
        totalSamplesSaved = 0;
        originalStatus = statusLabel.Text;
        
        try
            hWait = uiprogressdlg(fig,'Title','Batch Exporting','Message','Starting...','Cancelable','on','Indeterminate','on');
            
            for i = 1:numel(allGroups)
                if hWait.CancelRequested
                    break;
                end
                
                groupBase = allGroups{i};
                
                msg = sprintf('Processing group %d/%d: %s', i, numel(allGroups), groupBase);
                statusLabel.Text = msg; hWait.Message = msg; drawnow;
                
                loadGroup(groupBase, true);
                
                currentApp = getappdata(fig,'app');
                safeName = matlab.lang.makeValidName(groupBase);
                if ~isfield(currentApp.detailStruct, safeName), continue; end
                ds = currentApp.detailStruct.(safeName);
                
                savedCount = exportGroupData(ds, groupBase, outFolder, choice);
                totalSamplesSaved = totalSamplesSaved + savedCount;
            end
            
            wasCanceled = hWait.CancelRequested;
            close(hWait);
            
            if wasCanceled
                uialert(fig, sprintf('Batch export CANCELED by user.\n\n%d sample(s) were exported.', totalSamplesSaved), 'Operation Canceled');
            else
                uialert(fig, sprintf('Batch export complete.\n\nTotal samples exported: %d', totalSamplesSaved), 'Export Done');
            end
            
        catch ME
            if exist('hWait','var') && isvalid(hWait), close(hWait); end
            uialert(fig, sprintf('An error occurred during batch export:\n\n%s', ME.message), 'Error');
            statusLabel.Text = originalStatus;
            rethrow(ME);
        end
        
        % =================================================================
        % ***** 核心修正点 2: 恢复原始状态 *****
        % =================================================================
        % 无论成功、失败还是取消，都尝试重新加载批量操作之前的那个Group
        if ~isempty(originalGroup)
            statusLabel.Text = 'Status: Restoring original selection...'; drawnow;
            loadGroup(originalGroup, true);
        else
            statusLabel.Text = 'Status: Batch operation finished.';
        end
    end

    %% processRawThroughStep: reuse旧逻辑（保持兼容）
    function yproc = processRawThroughStep(rawSeg, stepData, selectedStep, app)
        yproc = [];
        if isempty(rawSeg), return; end
        rawSeg = rawSeg(:);
        if numel(rawSeg) < app.TARGET_LENGTH
            rawSeg = [rawSeg; repmat(rawSeg(end), app.TARGET_LENGTH - numel(rawSeg), 1)];
        end
        switch selectedStep
            case 'Raw (pre-repair)'
                yproc = rawSeg; return;
            case 'Truncated'
                yproc = rawSeg; return;
            case 'Normalized'
                try, yproc = absMaxNormalize(rawSeg,0,100); catch, mx = max(abs(rawSeg)); if mx < eps, yproc=zeros(size(rawSeg)); else yproc=100*rawSeg/mx; end; end; return;
            case 'Smoothed'
                try, normData = absMaxNormalize(rawSeg,0,100); catch, mx=max(abs(rawSeg)); if mx < eps, normData=zeros(size(rawSeg)); else normData=100*rawSeg/mx; end; end
                try
                    if strcmp(app.smoothMethod,'SG')
                        [s,~] = sg_smooth_tga(normData, app.sgWindow, app.sgPoly); yproc = s; return;
                    else
                        sp = app.loessSpan;
                        if sp>0 && sp<1, win = max(3, round(sp*numel(normData))); else win = max(3, round(sp)); end
                        yproc = smoothdata(normData,'loess',win); return;
                    end
                catch
                    yproc = normData; return;
                end
            case 'DTG (derivative)'
                try
                    normData = absMaxNormalize(rawSeg,0,100);
                catch
                    mx = max(abs(rawSeg)); if mx < eps, normData=zeros(size(rawSeg)); else normData=100*rawSeg/mx; end
                end
                % derivative based on chosen smoothing+diff method
                base_smoothed = [];
                if strcmp(app.smoothMethod,'SG')
                    try [s,~] = sg_smooth_tga(normData, app.sgWindow, app.sgPoly); base_smoothed = s; end
                else
                    try
                        sp = app.loessSpan; if sp>0 && sp<1, win = max(3, round(sp*numel(normData))); else win = max(3, round(sp)); end
                        base_smoothed = smoothdata(normData,'loess',win);
                    catch
                    end
                end
                if isempty(base_smoothed), yproc = []; return; end
                if strcmp(app.diffMethod,'First difference')
                    tmp = diff(base_smoothed); yproc = [tmp; NaN]; return;
                else
                    try
                        [~, dcalc] = sg_smooth_tga(base_smoothed, app.sgWindow, app.sgPoly); yproc = dcalc; return;
                    catch
                        tmp = diff(base_smoothed); yproc = [tmp; NaN]; return;
                    end
                end
            otherwise
                yproc = [];
        end
    end

    %% 新增辅助函数：根据选择的步骤提取数据
    function [y, labelSuffix] = getStepData(stepData, selectedStep, smoothDisplay, app)
        y = [];
        labelSuffix = '';
        switch selectedStep
            case 'Raw (pre-repair)'
                y = stepData.rawSeg; if isempty(y), y = stepData.cleanedRaw; end
            case 'Truncated'
                y = stepData.truncated;
            case 'Normalized'
                y = stepData.normalized;
            case 'Smoothed'
                switch smoothDisplay
                    case 'SG (only)'
                        if isfield(stepData,'smoothed_sg') && ~isempty(stepData.smoothed_sg)
                            y = stepData.smoothed_sg; labelSuffix = ' (SG)';
                        end
                    case 'LOESS (only)'
                        if isfield(stepData,'smoothed_loess') && ~isempty(stepData.smoothed_loess)
                            y = stepData.smoothed_loess; labelSuffix = ' (LOESS)';
                        end
                    case 'Both' % 'Both'时默认也用LOESS作为基础
                        if isfield(stepData,'smoothed_loess') && ~isempty(stepData.smoothed_loess)
                            y = stepData.smoothed_loess; labelSuffix = ' (LOESS)';
                        end
                    case 'None'
                        y = stepData.smoothed;
                end
                % Fallback
                if isempty(y) 
                    if isfield(stepData,'smoothed_loess') && ~isempty(stepData.smoothed_loess)
                        y = stepData.smoothed_loess; labelSuffix = ' (LOESS fallback)';
                    elseif isfield(stepData,'smoothed_sg') && ~isempty(stepData.smoothed_sg)
                         y = stepData.smoothed_sg; labelSuffix = ' (SG fallback)';
                    end
                end
            case 'DTG (derivative)'
                base_smoothed = [];
                if strcmp(app.smoothMethod,'LOESS')
                    if isfield(stepData,'smoothed_loess') && ~isempty(stepData.smoothed_loess)
                        base_smoothed = stepData.smoothed_loess;
                    end
                else % SG or fallback
                    if isfield(stepData,'smoothed_sg') && ~isempty(stepData.smoothed_sg)
                        base_smoothed = stepData.smoothed_sg;
                    end
                end
                % Fallback
                if isempty(base_smoothed)
                    if isfield(stepData,'smoothed_loess') && ~isempty(stepData.smoothed_loess)
                        base_smoothed = stepData.smoothed_loess;
                    elseif isfield(stepData,'smoothed_sg') && ~isempty(stepData.smoothed_sg)
                        base_smoothed = stepData.smoothed_sg;
                    end
                end

                if ~isempty(base_smoothed)
                    if strcmp(app.diffMethod,'First difference')
                        tmp = diff(base_smoothed); y = [tmp; NaN]; labelSuffix = sprintf(' (%s->first diff)', app.smoothMethod);
                    else
                        try
                            [~, dcalc] = sg_smooth_tga(base_smoothed, app.sgWindow, app.sgPoly);
                            y = dcalc; labelSuffix = sprintf(' (%s->SGderiv)', app.smoothMethod);
                        catch
                            tmp = diff(base_smoothed); y = [tmp; NaN]; labelSuffix = ' (fallback->diff)';
                        end
                    end
                end
            otherwise
                y = [];
        end
    end

end
