function idx = getComparisonIndices(len, selection)
    if ischar(selection) || (isstring(selection) && selection=='all')
        idx = 1:len; return;
    end
    if isnumeric(selection) && numel(selection)==2
        s = round(selection(1)); e = round(selection(2));
        s = max(1, min(len, s)); e = max(1, min(len, e));
        if s <= e, idx = s:e; else idx = e:s; end
        return;
    end
    idx = 1:len;
end