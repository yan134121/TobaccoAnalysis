#include "BadPointRepair.h"
#include "core/entities/Curve.h"
#include "Logger.h"

#include <QtMath>
#include <algorithm>
#include <limits>

QString BadPointRepair::stepName() const {
    return "bad_point_repair";
}

QString BadPointRepair::userVisibleName() const {
    return QObject::tr("坏点去除与修复");
}

QVariantMap BadPointRepair::defaultParameters() const {
    QVariantMap params;
    params["window"] = 15;            // 局部窗口
    params["n_sigma"] = 4.0;          // 局部异常阈值（Hampel）
    params["jump_threshold"] = 2.0;   // 跳变阈值
    params["global_n_sigma"] = 4.0;   // 全局偏差阈值（MAD）
    params["mono_start"] = 20;        // 单调性校正起点（索引，0基）
    params["mono_end"] = 120;         // 单调性校正终点（索引，0基，闭区间）
    params["anchor_win"] = 8;         // 局部拟合锚点窗口大小
    params["fit_type"] = QStringLiteral("linear"); // 局部拟合类型：linear/quad
    params["eps_scale"] = 1e-9;       // 平台段严格递减的微小斜率尺度
    params["interp_method"] = QStringLiteral("pchip"); // 插值方法：pchip/linear
    return params;
}

// 判断字符坏点或者无效点（这里主要用于 Curve 中已经保证 double，只处理 NaN/inf）
bool BadPointRepair::isBadPoint(double v)
{
    return qIsNaN(v) || qIsInf(v);
}

/**
 * 判断局部异常点（类似 Hampel filter）
 */
bool BadPointRepair::isLocalOutlier(const QVector<double>& y, int idx, int win, double nSigma)
{
    int N = y.size();
    int half = win / 2;
    int lo = qMax(0, idx - half);
    int hi = qMin(N - 1, idx + half);

    QVector<double> winvals;
    winvals.reserve(hi - lo + 1);

    for (int i = lo; i <= hi; ++i)
        winvals.append(y[i]);

    std::sort(winvals.begin(), winvals.end());
    double med = winvals[winvals.size() / 2];

    QVector<double> absdev;
    absdev.reserve(winvals.size());
    for (double v : winvals)
        absdev.append(qAbs(v - med));

    std::sort(absdev.begin(), absdev.end());
    double mad = absdev[absdev.size() / 2];

    double sigma = 1.4826 * mad;
    if (sigma < 1e-12)
        sigma = 1e-6;

    return qAbs(y[idx] - med) > nSigma * sigma;
}

// PCHIP（单调保形分段三次插值）实现，基于 Fritsch-Carlson 方法
static void pchipInterpolateInPlace(QVector<double>& y)
{
    int N = y.size();
    if (N <= 1) return;

    // 收集有效点索引与值
    QVector<int> gx; QVector<double> gy;
    gx.reserve(N); gy.reserve(N);
    for (int i = 0; i < N; ++i) {
        if (!qIsNaN(y[i]) && !qIsInf(y[i])) { gx.append(i); gy.append(y[i]); }
    }
    int M = gy.size();
    if (M < 2) return;

    // 计算分段斜率 di 与区间长度 dx
    QVector<double> dx(M - 1), di(M - 1);
    for (int i = 0; i < M - 1; ++i) {
        dx[i] = double(gx[i + 1] - gx[i]); if (dx[i] <= 0) dx[i] = 1.0; // 防御
        di[i] = (gy[i + 1] - gy[i]) / dx[i];
    }

    // 计算各节点导数 m，保形/单调
    QVector<double> m(M);
    // 端点导数（使用非中心权重）
    if (M == 2) {
        m[0] = di[0]; m[1] = di[0];
    } else {
        // m0
        double d0 = di[0], d1 = di[1];
        if (d0 * d1 <= 0) m[0] = 0.0;
        else m[0] = ((2 * dx[0] + dx[1]) * d0 - dx[0] * d1) / (dx[0] + dx[1]);
        // m_{M-1}
        double dn_2 = di[M - 2], dn_3 = (M - 3 >= 0) ? di[M - 3] : di[M - 2];
        if (dn_2 * dn_3 <= 0) m[M - 1] = 0.0;
        else m[M - 1] = ((2 * dx[M - 2] + (M - 3 >= 0 ? dx[M - 3] : dx[M - 2])) * dn_2 - dx[M - 2] * dn_3)
                         / (dx[M - 2] + (M - 3 >= 0 ? dx[M - 3] : dx[M - 2]));
        // 中间节点导数（加权调和均值）
        for (int i = 1; i <= M - 2; ++i) {
            double d_im1 = di[i - 1];
            double d_i = di[i];
            if (d_im1 * d_i <= 0) m[i] = 0.0;
            else {
                double w1 = 2 * dx[i] + dx[i - 1];
                double w2 = dx[i] + 2 * dx[i - 1];
                m[i] = (w1 + w2) / (w1 / d_im1 + w2 / d_i);
            }
        }
    }

    // 对每个间隔做三次 Hermite 插值
    for (int seg = 0; seg < M - 1; ++seg) {
        int i0 = gx[seg], i1 = gx[seg + 1];
        double y0 = gy[seg], y1 = gy[seg + 1];
        double m0 = m[seg], m1 = m[seg + 1];
        int len = i1 - i0;
        if (len <= 1) continue;
        for (int k = i0 + 1; k < i1; ++k) {
            double t = double(k - i0) / double(len);
            double t2 = t * t, t3 = t2 * t;
            double h00 = 2 * t3 - 3 * t2 + 1;
            double h10 = t3 - 2 * t2 + t;
            double h01 = -2 * t3 + 3 * t2;
            double h11 = t3 - t2;
            y[k] = h00 * y0 + h10 * (len * m0) + h01 * y1 + h11 * (len * m1);
        }
    }
}

/**
 * 对坏点区域做局部线性拟合修复
 */
void BadPointRepair::localLinearRepair(
        QVector<double>& y,
        const QVector<bool>& badMask)
{
    int N = y.size();
    int i = 0;

    while (i < N) {
        if (!badMask[i]) {
            i++;
            continue;
        }

        // 找坏点段起点
        int start = i;
        while (i < N && badMask[i])
            i++;
        int end = i - 1;

        // 找左右锚点
        int left = start - 1;
        while (left >= 0 && badMask[left])
            left--;

        int right = end + 1;
        while (right < N && badMask[right])
            right++;

        if (left >= 0 && right < N && left < right) {
            double x1 = left;
            double y1 = y[left];
            double x2 = right;
            double y2 = y[right];

            double slope = (y2 - y1) / (x2 - x1);

            for (int k = start; k <= end; ++k) {
                y[k] = y1 + slope * (k - x1); // 简单线性修复
            }
        }
        // 若锚点不存在：不修复，让后面的插值处理
    }
}

/**
 * 主处理流程
 */
ProcessingResult BadPointRepair::process(const QList<Curve*>& inputCurves,
                                         const QVariantMap& params,
                                         QString& error)
{
    DEBUG_LOG << "BadPointRepair::process()";

    ProcessingResult result;

    int win = params.value("window", 15).toInt();
    double nSigma = params.value("n_sigma", 4.0).toDouble();
    double jumpThr = params.value("jump_threshold", 2.0).toDouble();
    double globalNSigma = params.value("global_n_sigma", 4.0).toDouble();
    int monoStart = params.value("mono_start", 20).toInt();
    int monoEnd   = params.value("mono_end", 120).toInt();
    int anchorWin = params.value("anchor_win", 8).toInt();
    QString fitType = params.value("fit_type", QStringLiteral("linear")).toString();
    double epsScale = params.value("eps_scale", 1e-9).toDouble();

    for (Curve* inCurve : inputCurves) {
        QVector<QPointF> raw = inCurve->data();
        if (raw.isEmpty())
            continue;

        int N = raw.size();
        QVector<double> x(N), y(N);
        for (int i = 0; i < N; ++i) {
            x[i] = raw[i].x();
            y[i] = raw[i].y();
        }

        DEBUG_LOG << "BadPointRepair::process() N:" << N;
        // ------ 1. 标记坏点 ------
        QVector<bool> bad(N, false);

        // NaN / inf
        for (int i = 0; i < N; ++i)
            if (isBadPoint(y[i]))
                bad[i] = true;

        // 全局偏差（相对稳健基线的MAD阈值）
        {
            // 稳健基线（中值滤波），窗口取 max(win, N/10) 的奇数
            int medianWin = qMax(win, qMax(3, (int)qRound(N / 10.0)));
            if (medianWin % 2 == 0) medianWin += 1;
            QVector<double> baseline(N);
            int half = medianWin / 2;
            for (int i = 0; i < N; ++i) {
                int lo = qMax(0, i - half);
                int hi = qMin(N - 1, i + half);
                QVector<double> vals;
                vals.reserve(hi - lo + 1);
                for (int k = lo; k <= hi; ++k) vals.append(y[k]);
                std::sort(vals.begin(), vals.end());
                baseline[i] = vals[vals.size() / 2];
            }
            QVector<double> dev(N);
            for (int i = 0; i < N; ++i) dev[i] = qAbs(y[i] - baseline[i]);
            // MAD 估计
            QVector<double> devAbs = dev;
            std::sort(devAbs.begin(), devAbs.end());
            double medDev = devAbs[devAbs.size() / 2];
            double mad = 1.4826 * medDev;
            if (mad < 1e-12) {
                // MAD过小则退化为标准差估计
                double mean = 0.0; for (double d : dev) mean += d; mean /= N;
                double var = 0.0; for (double d : dev) var += (d - mean) * (d - mean); var /= qMax(1, N - 1);
                mad = qMax(1e-9, std::sqrt(var));
            }
            for (int i = 0; i < N; ++i) {
                if (!bad[i] && mad > 0.0 && dev[i] > globalNSigma * mad)
                    bad[i] = true;
            }
        }

        // 局部异常点（Hampel）
        for (int i = 0; i < N; ++i) {
            if (!bad[i] && isLocalOutlier(y, i, win, nSigma))
                bad[i] = true;
        }

        // 跳变点（diff）
        for (int i = 1; i < N; ++i) {
            if (qAbs(y[i] - y[i - 1]) > jumpThr)
                bad[i] = true;
        }

        // ------ 1.1 单调性破坏（仅在指定区间）------
        {
            // 限制区间到有效范围
            int ms = qBound(0, monoStart, N - 1);
            int me = qBound(0, monoEnd, N - 1);
            if (me < ms) me = ms;
            if (N >= 2 && me > ms) {
                for (int i = ms + 1; i <= me; ++i) {
                    if (y[i] >= y[i - 1]) bad[i] = true; // 非递减视为破坏点
                }
            }
        }

        // 【新增】收集坏点坐标用于绘图显示
        {
            QVector<QPointF> badPts;
            badPts.reserve(N / 10);
            for (int i = 0; i < N; ++i) {
                if (bad[i]) {
                    badPts.append(QPointF(x[i], y[i]));
                }
            }
            if (!badPts.isEmpty()) {
                Curve* badCurve = new Curve(badPts, QObject::tr("坏点"));
                // 设置样式为虚线，绘图层可选择散点显示
                badCurve->setLineStyle(Qt::NoPen);
                result.namedCurves["bad_points"].append(badCurve);
            }
        }

        // ------ 2. 局部线性/二次修复（优先针对单调性区间内的坏段） ------
        QVector<double> y_repaired = y;
        {
            // 在 [monoStart, monoEnd] 区间内对坏段尝试局部拟合修复
            int ms = qBound(0, monoStart, N - 1);
            int me = qBound(0, monoEnd, N - 1);
            if (me < ms) me = ms;

            int i = ms;
            while (i <= me) {
                if (!bad[i]) { i++; continue; }

                int blockStart = i;
                while (i <= me && bad[i]) i++;
                int blockEnd = i - 1;

                // 寻找左右锚点（非坏点）
                int anchor1 = blockStart - 1;
                while (anchor1 >= ms && bad[anchor1]) anchor1--;
                int anchor2 = blockEnd + 1;
                while (anchor2 <= me && bad[anchor2]) anchor2++;

                if (anchor1 >= ms && anchor2 <= me && anchor2 > anchor1) {
                    int preStart = qMax(ms, anchor1 - anchorWin + 1);
                    int postEnd = qMin(me, anchor2 + anchorWin - 1);

                    QVector<int> fitIdx;
                    for (int k = preStart; k <= anchor1; ++k) if (!bad[k]) fitIdx.append(k);
                    for (int k = anchor2; k <= postEnd; ++k) if (!bad[k]) fitIdx.append(k);

                    int minFitPts = (fitType.compare("quad", Qt::CaseInsensitive) == 0) ? 3 : 2;
                    if (fitIdx.size() >= minFitPts) {
                        // 标准化 x 以提高数值稳定性
                        QVector<double> x_raw, y_raw;
                        x_raw.reserve(fitIdx.size()); y_raw.reserve(fitIdx.size());
                        for (int idx : fitIdx) { x_raw.append(double(idx)); y_raw.append(y_repaired[idx]); }
                        double mean = 0.0; for (double xv : x_raw) mean += xv; mean /= x_raw.size();
                        double stdv = 0.0; for (double xv : x_raw) stdv += (xv - mean) * (xv - mean); stdv = std::sqrt(stdv / qMax(1, (int)x_raw.size()));
                        if (stdv < 1e-12) stdv = 1.0;
                        QVector<double> x_norm(x_raw.size());
                        for (int k = 0; k < x_raw.size(); ++k) x_norm[k] = (x_raw[k] - mean) / stdv;

                        // 最小二乘拟合（一次或二次）
                        auto polyfit = [&](const QVector<double>& x, const QVector<double>& y, int deg){
                            // 构建正规方程 X^T X a = X^T y
                            int m = x.size();
                            if (deg == 1) {
                                double Sx=0,Sx2=0,Sy=0,Sxy=0;
                                for(int t=0;t<m;++t){ double xi=x[t]; double yi=y[t]; Sx+=xi; Sx2+=xi*xi; Sy+=yi; Sxy+=xi*yi; }
                                double D = m*Sx2 - Sx*Sx; if (qAbs(D) < 1e-12) D = (D>=0?1e-12:-1e-12);
                                double a1 = (m*Sxy - Sx*Sy)/D; // slope
                                double a0 = (Sy - a1*Sx)/m;     // intercept
                                return QVector<double>{a0,a1};
                            } else {
                                double Sx=0,Sx2=0,Sx3=0,Sx4=0,Sy=0,Sx_y=0,Sx2_y=0;
                                for(int t=0;t<m;++t){ double xi=x[t]; double yi=y[t]; Sx+=xi; Sx2+=xi*xi; Sx3+=xi*xi*xi; Sx4+=xi*xi*xi*xi; Sy+=yi; Sx_y+=xi*yi; Sx2_y+=xi*xi*yi; }
                                // 求解 3x3 线性方程组
                                // | m    Sx   Sx2 | |a0| = | Sy    |
                                // | Sx   Sx2  Sx3 | |a1|   | Sx_y  |
                                // | Sx2  Sx3  Sx4 | |a2|   | Sx2_y |
                                double A[3][3] = {{(double)m,Sx,Sx2},{Sx,Sx2,Sx3},{Sx2,Sx3,Sx4}};
                                double B[3] = {Sy,Sx_y,Sx2_y};
                                // 高斯消元
                                for(int r=0;r<3;++r){ int piv=r; double maxv=qAbs(A[r][r]); for(int rr=r+1;rr<3;++rr){ if(qAbs(A[rr][r])>maxv){maxv=qAbs(A[rr][r]); piv=rr;} } if(piv!=r){ for(int c=0;c<3;++c) std::swap(A[r][c],A[piv][c]); std::swap(B[r],B[piv]); }
                                    double diag=A[r][r]; if(qAbs(diag)<1e-12) diag=(diag>=0?1e-12:-1e-12); for(int c=r;c<3;++c) A[r][c]/=diag; B[r]/=diag; for(int rr=0;rr<3;++rr){ if(rr==r) continue; double f=A[rr][r]; for(int c=r;c<3;++c) A[rr][c]-=f*A[r][c]; B[rr]-=f*B[r]; } }
                                return QVector<double>{B[0],B[1],B[2]};
                            }
                        };

                        int deg = (fitType.compare("quad", Qt::CaseInsensitive) == 0) ? 2 : 1;
                        QVector<double> coef = polyfit(x_norm, y_raw, deg);

                        auto polyval = [&](const QVector<double>& coef, double xn){
                            if (coef.size()==2) return coef[0] + coef[1]*xn; // 线性
                            return coef[0] + coef[1]*xn + coef[2]*xn*xn;     // 二次
                        };

                        for (int k = blockStart; k <= blockEnd; ++k) {
                            double xn = (double(k) - mean) / stdv;
                            y_repaired[k] = polyval(coef, xn);
                            bad[k] = false; // 标记为已修复
                        }
                        continue; // 该坏段已修复，继续下一段
                    }
                }

                // 若无法局部拟合修复，则留给后续线性修复与插值
            }
        }

        // 对剩余坏点进行通用线性修复（段内线性过渡）
        localLinearRepair(y_repaired, bad);

        // ------ 3. 插值未修复部分 ------
        // 将坏点置为 NaN，后续统一按线性插值处理
        for (int i = 0; i < N; ++i)
            if (bad[i])
                y_repaired[i] = qQNaN();

        // 处理前段全部为坏点的情况——找到第一个有效点，并用其值填充前导 NaN，避免越界
        int firstGood = -1;
        for (int i = 0; i < N; ++i) {
            if (!qIsNaN(y_repaired[i])) { firstGood = i; break; }
        }
        if (firstGood > 0) {
            for (int k = 0; k < firstGood; ++k)
                y_repaired[k] = y_repaired[firstGood];
        }

        // 用线性插值填充中间的 NaN 段；仅在存在上一个有效点时才进行插值，避免 lastGood=-1 越界
        int lastGood = (firstGood >= 0 ? firstGood : -1);
        for (int i = (firstGood >= 0 ? firstGood + 1 : 0); i < N; ++i) {
            if (qIsNaN(y_repaired[i])) continue;

            if (lastGood >= 0 && lastGood + 1 < i) {
                double y0 = y_repaired[lastGood];
                double y1 = y_repaired[i];
                int L = i - lastGood;

                for (int k = lastGood + 1; k < i; ++k) {
                    double t = double(k - lastGood) / L;
                    y_repaired[k] = y0 * (1 - t) + y1 * t;
                }
            }

            lastGood = i;
        }

        // 处理尾段全部为坏点的情况——若存在最后一个有效点，用其值回填后续 NaN
        if (lastGood >= 0 && lastGood < N - 1) {
            for (int k = lastGood + 1; k < N; ++k)
                y_repaired[k] = y_repaired[lastGood];
        }

        // 插值（线性或 PCHIP 保形三次），并在端点维持 nearest 行为
        QString interpMethod = params.value("interp_method", QStringLiteral("pchip")).toString();
        if (interpMethod.compare("pchip", Qt::CaseInsensitive) == 0) {
            pchipInterpolateInPlace(y_repaired);
        } else {
            // 已完成线性插值（上面的 lastGood 段插值），此处无需重复
        }

        // ------ 4. 单调性 PAVA 与平台段严格递减处理（仅在指定区间）------
        {
            int ms = qBound(0, monoStart, N - 1);
            int me = qBound(0, monoEnd, N - 1);
            if (me < ms) me = ms;
            if (me > ms) {
                QVector<double> sub;
                sub.reserve(me - ms + 1);
                for (int k = ms; k <= me; ++k) sub.append(y_repaired[k]);

                // PAVA 非增（将递增违例块合并并取均值）
                QVector<QVector<double>> blocks;
                blocks.reserve(sub.size());
                for (double v : sub) blocks.append(QVector<double>{v});
                int bi = 0;
                while (bi < blocks.size() - 1) {
                    double avg_i = 0.0; for (double v : blocks[bi]) avg_i += v; avg_i /= blocks[bi].size();
                    double avg_ip1 = 0.0; for (double v : blocks[bi+1]) avg_ip1 += v; avg_ip1 /= blocks[bi+1].size();
                    if (avg_i < avg_ip1) { // 违反非增（当前块均值小于后块均值）
                        blocks[bi] += blocks[bi+1];
                        blocks.removeAt(bi+1);
                        if (bi > 0) bi--; // 回退比较前一对
                    } else {
                        bi++;
                    }
                }
                // 重构非增序列
                QVector<double> iso;
                iso.reserve(sub.size());
                for (const auto& blk : blocks) {
                    double avg = 0.0; for (double v : blk) avg += v; avg /= blk.size();
                    for (int t = 0; t < blk.size(); ++t) iso.append(avg);
                }

                // 将所有水平段转换为严格递减斜坡
                double range_sub = 0.0; for (double v : iso) range_sub = qMax(range_sub, v);
                double minv = 1e18; for (double v : iso) minv = qMin(minv, v);
                range_sub = range_sub - minv; if (range_sub < 1e-12) range_sub = 1.0;
                double strict_epsilon = qMax(epsScale, range_sub * 1e-7);
                strict_epsilon = qMin(strict_epsilon, qMax(1e-6, range_sub * 1e-3));

                int i2 = 0;
                while (i2 < iso.size() - 1) {
                    if (qAbs(iso[i2+1] - iso[i2]) < strict_epsilon / 10.0) {
                        int j2 = i2 + 1;
                        while (j2 < iso.size() - 1 && qAbs(iso[j2+1] - iso[j2]) < strict_epsilon / 10.0) j2++;
                        double start_val = iso[i2];
                        int len_block = j2 - i2 + 1;
                        for (int r = 0; r < len_block; ++r) iso[i2 + r] = start_val - strict_epsilon * r;
                        i2 = j2;
                    }
                    i2++;
                }
                // 最终保险：确保严格递减
                for (int k = 1; k < iso.size(); ++k) {
                    if (iso[k] >= iso[k - 1]) iso[k] = iso[k - 1] - strict_epsilon;
                }

                for (int k = 0; k < iso.size(); ++k) y_repaired[ms + k] = iso[k];

                // 再次检测单调违例并快速微调，确保最终严格递减
                for (int k = ms + 1; k <= me; ++k) {
                    if (y_repaired[k] >= y_repaired[k - 1]) {
                        y_repaired[k] = y_repaired[k - 1] - strict_epsilon;
                    }
                }
            }
        }

        DEBUG_LOG << "BadPointRepair::process() done";

        // ------ 4. 生成新的 Curve ------
        QVector<QPointF> repaired;
        repaired.reserve(N);
        for (int i = 0; i < N; ++i)
            repaired.append(QPointF(x[i], y_repaired[i]));

        Curve* c = new Curve(repaired, inCurve->name() + QObject::tr(" (坏点修复)"));
        c->setSampleId(inCurve->sampleId());

        result.namedCurves["repaired"].append(c);
    }

    DEBUG_LOG << "BadPointRepair::process() done";

    return result;
}
