function y = normalizeVector(x, direction)
    mn = min(x); mx = max(x);
    if mx > mn
        y = (x - mn)/(mx - mn);
    else
        y = ones(size(x));
    end
    if strcmp(direction, 'inverse')
        y = 1 - y;
    end
end