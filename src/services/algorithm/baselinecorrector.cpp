// #include "BaselineCorrector.h"
// #include "Logger.h"
// #include <QtMath>
// #include <QDebug>
// #include <QElapsedTimer>
// #include <limits>

// QString BaselineCorrector::stepName() const {
//     return "baseline_correction";
// }

// QString BaselineCorrector::userVisibleName() const {
//     return QObject::tr("基线校正");
// }

// QVariantMap BaselineCorrector::defaultParameters() const {
//     QVariantMap params;
//     params["lambda"] = 1e5;
//     params["order"] = 2;
//     params["p"] = 0.05;
//     params["itermax"] = 50;
//     params["wep"] = 0.0; // 权重例外比例（保护起始与结束区域的权重），范围 [0, 0.5]
//     params["cg_max_iter"] = 120;  // 精度优先：更高的CG迭代上限
//     params["cg_tol"] = 1e-6;      // 精度优先：更严格的收敛容差
//     params["warm_start"] = true; // 使用上一次基线作为初值，加速收敛
//     params["use_preconditioner"] = true; // 启用对角预条件（PCG），明显加速大样本收敛
//     params["max_weight"] = 80.0;         // 负残差权重上限（更高以贴近MATLAB）
//     params["dssn_rel_stop"] = 0.001;     // 提前停止阈值（相对总幅度），默认 0.1%
//     params["lambda_scale_by_n"] = true;  // 按样本长度缩放lambda（更贴近MATLAB取值习惯）
//     params["baseline_mode"] = QString("trend"); // 基线模式 trend/envelope；趋势模式反映慢趋势
//     params["trend_p"] = 0.2;                   // 趋势模式下正残差权重
//     params["trend_alpha"] = 2.0;               // 趋势模式下负残差线性权重系数
//     params["trend_itermax"] = 10;              // 趋势模式迭代次数（温和校正）
//     return params;
// }

// // CG 求解全局配置（由 process 中参数设置）
// static int g_cg_max_iter = 120;
// static double g_cg_tol = 1e-6;
// static bool g_warm_start = true;
// static bool g_use_precond = true;
// static double g_max_weight = 80.0;
// static double g_dssn_rel_stop = 0.001;
// static bool g_lambda_scale_by_n = true;
// static QString g_baseline_mode = "trend";
// static double g_trend_p = 0.2;
// static double g_trend_alpha = 2.0;
// static int g_trend_itermax = 10;

// ProcessingResult BaselineCorrector::process(const QList<Curve*>& inputCurves, const QVariantMap& params, QString& error)
// {
//     DEBUG_LOG << "BaselineCorrector::process()";
//     ProcessingResult result;

//     const double lambda = params.value("lambda", 1e5).toDouble();
//     const int order = params.value("order", 2).toInt();
//     const double p = params.value("p", 0.05).toDouble();
//     const int itermax = params.value("itermax", 50).toInt();
//     const double wep = params.value("wep", 0.0).toDouble();
//     const int cgMaxIter = params.value("cg_max_iter", 120).toInt();
//     const double cgTol = params.value("cg_tol", 1e-6).toDouble();
//     const bool warmStart = params.value("warm_start", true).toBool();
//     const bool usePrecond = params.value("use_preconditioner", true).toBool();
//     const double maxWeight = params.value("max_weight", 80.0).toDouble();
//     const double dssnRelStop = params.value("dssn_rel_stop", 0.001).toDouble();
//     const bool lambdaScaleByN = params.value("lambda_scale_by_n", true).toBool();
//     const QString baselineMode = params.value("baseline_mode", "trend").toString();
//     const double trendP = params.value("trend_p", 0.2).toDouble();
//     const double trendAlpha = params.value("trend_alpha", 2.0).toDouble();
//     const int trendIterMax = params.value("trend_itermax", 10).toInt();
//     g_cg_max_iter = cgMaxIter;
//     g_cg_tol = cgTol;
//     g_warm_start = warmStart;
//     g_use_precond = usePrecond;
//     g_max_weight = maxWeight;
//     g_dssn_rel_stop = dssnRelStop;
//     g_lambda_scale_by_n = lambdaScaleByN;
//     g_baseline_mode = baselineMode;
//     g_trend_p = trendP;
//     g_trend_alpha = trendAlpha;
//     g_trend_itermax = trendIterMax;

//     for (Curve* inCurve : inputCurves) {
//         const QVector<QPointF>& originalData = inCurve->data();
//         if (originalData.isEmpty()) continue;

//         // --- 提取 y 数据 ---
//         QVector<double> y;
//         y.reserve(originalData.size());
//         for (const QPointF& pt : originalData)
//             y.append(pt.y());

//         // --- 执行 airPLS 算法 ---
//         QVector<double> yb;
//         try {
//             // 在算法内部将读取 cgMaxIter/cgTol/warmStart，这里通过静态变量传递或局部捕获
//             yb = airPLS(y, lambda, order, wep, p, itermax);
//         } catch (...) {
//             error = "airPLS 算法执行失败。";
//             return result;
//         }

//         // --- 计算校正后曲线 ---
//         QVector<QPointF> corrected;
//         corrected.reserve(y.size());
//         for (int i = 0; i < y.size(); ++i)
//             corrected.append(QPointF(originalData[i].x(), y[i] - yb[i]));

//         // --- 生成校正后曲线 ---
//         Curve* correctedCurve = new Curve(corrected, inCurve->name() + QObject::tr(" (基线校正)"));
//         correctedCurve->setSampleId(inCurve->sampleId());
//         result.namedCurves["baseline_corrected"].append(correctedCurve);

//         // --- 输出基线曲线 ---
//         QVector<QPointF> baselinePts;
//         baselinePts.reserve(yb.size());
//         for (int i = 0; i < yb.size(); ++i)
//             baselinePts.append(QPointF(originalData[i].x(), yb[i]));
//         Curve* baselineCurve = new Curve(baselinePts, inCurve->name() + QObject::tr(" (基线)"));
//         baselineCurve->setSampleId(inCurve->sampleId());
//         result.namedCurves["baseline"].append(baselineCurve);
//     }

//     return result;
// }

// // =================== 辅助函数实现 ===================

// // 计算差分系数（等同于 MATLAB 的 diff(speye(n), order) 的核系数）
// static QVector<double> computeDiffCoeffs(int order)
// {
//     QVector<double> coeffs(order + 1, 0.0);
//     auto binom = [](int n, int k) -> double {
//         if (k < 0 || k > n) return 0.0;
//         double res = 1.0;
//         for (int i = 1; i <= k; ++i) {
//             res = res * (n - (k - i)) / i;
//         }
//         return res;
//     };
//     for (int k = 0; k <= order; ++k) {
//         double c = binom(order, k);
//         double sign = ((order - k) % 2 == 0) ? 1.0 : -1.0;
//         coeffs[k] = sign * c;
//     }
//     return coeffs;
// }

// // 应用 D' * D 到向量 x（不显式构造矩阵），返回 lambda*(D'*D)x
// static QVector<double> applyDD(const QVector<double>& x, const QVector<double>& coeffs, double lambda)
// {
//     const int n = x.size();
//     const int m = coeffs.size() - 1;
//     QVector<double> y(n, 0.0);
//     if (n == 0) return y;
//     if (m <= 0) return y;

//     const int rows = n - m;
//     if (rows <= 0) return y;

//     QVector<double> temp(rows, 0.0);
//     for (int j = 0; j < rows; ++j) {
//         double t = 0.0;
//         for (int k = 0; k <= m; ++k) {
//             t += coeffs[k] * x[j + k];
//         }
//         temp[j] = t;
//     }
//     for (int j = 0; j < rows; ++j) {
//         const double tj = temp[j] * lambda;
//         for (int k = 0; k <= m; ++k) {
//             y[j + k] += tj * coeffs[k];
//         }
//     }
//     return y;
// }

// // 计算 D' * D 的对角近似，用于 PCG 的对角预条件
// static QVector<double> computeDiagDD(int n, const QVector<double>& coeffs, double lambda)
// {
//     const int m = coeffs.size() - 1;
//     QVector<double> diag(n, 0.0);
//     const int rows = n - m;
//     if (rows <= 0) return diag;
//     for (int j = 0; j < rows; ++j) {
//         for (int k = 0; k <= m; ++k) {
//             int idx = j + k;
//             double c = coeffs[k];
//             diag[idx] += c * c * lambda;
//         }
//     }
//     return diag;
// }

// // 共轭梯度法求解 (W + DD) * baseline = weights .* y（支持热启动）
// static QVector<double> solveWithCG(
//     const QVector<double>& b,
//     const QVector<double>& weights,
//     const QVector<double>& coeffs,
//     double lambda,
//     int maxIter,
//     double tol,
//     const QVector<double>* initialX = nullptr,
//     int* usedIters = nullptr,
//     const QVector<double>* M_inv_diag = nullptr)
// {
//     const int n = b.size();
//     QVector<double> x(n, 0.0);
//     if (initialX && initialX->size() == n) x = *initialX; // 热启动
//     auto applyA = [&](const QVector<double>& v) {
//         QVector<double> y = applyDD(v, coeffs, lambda);
//         for (int i = 0; i < n; ++i) {
//             y[i] += weights[i] * v[i];
//         }
//         return y;
//     };
//     QVector<double> Ax = applyA(x);
//     QVector<double> r(n, 0.0);
//     for (int i = 0; i < n; ++i) r[i] = b[i] - Ax[i]; // 初始残差
//     QVector<double> z(n, 0.0);
//     bool usePCG = (M_inv_diag && M_inv_diag->size() == n);
//     if (usePCG) {
//         for (int i = 0; i < n; ++i) z[i] = (*M_inv_diag)[i] * r[i];
//     } else {
//         z = r;
//     }

//     QVector<double> p = z;
//     QVector<double> Ap(n, 0.0);

//     auto dot = [](const QVector<double>& a, const QVector<double>& b) {
//         double s = 0.0;
//         const int len = a.size();
//         for (int i = 0; i < len; ++i) s += a[i] * b[i];
//         return s;
//     };

//     double rr = usePCG ? dot(r, z) : dot(r, r);
//     const double eps = 1e-12;
//     int it = 0;
//     const int cgMax = (maxIter > 0 ? maxIter : 120); // 使用外部限次，避免随 n 增长
//     QElapsedTimer timer;
//     timer.start();
//     while (it < cgMax) {
//         Ap = applyA(p);
//         double denom = dot(p, Ap) + eps;
//         double alpha = rr / denom;
//         for (int i = 0; i < n; ++i) {
//             x[i] += alpha * p[i];
//             r[i] -= alpha * Ap[i];
//         }
//         double rr_new;
//         if (usePCG) {
//             for (int i = 0; i < n; ++i) z[i] = (*M_inv_diag)[i] * r[i];
//             rr_new = dot(r, z);
//             if (std::sqrt(rr_new) < tol) break;
//             double beta = rr_new / (rr + eps);
//             for (int i = 0; i < n; ++i) {
//                 p[i] = z[i] + beta * p[i];
//             }
//             rr = rr_new;
//         } else {
//             rr_new = dot(r, r);
//             if (std::sqrt(rr_new) < tol) break;
//             double beta = rr_new / (rr + eps);
//             for (int i = 0; i < n; ++i) {
//                 p[i] = r[i] + beta * p[i];
//             }
//             rr = rr_new;
//         }
//         ++it;
//     }
//     if (usedIters) *usedIters = it;
//     DEBUG_LOG << "CG 求解完成: iters=" << it << " tol=" << tol << " elapsed_ms=" << timer.elapsed();
//     return x;
// }

// QVector<double> BaselineCorrector::airPLS(const QVector<double>& y, double lambda, int order, double wep, double p, int itermax)
// {
//     const int n = y.size();
//     if (n == 0) return {};

//     // 参数保护与裁剪
//     if (order < 1) order = 1;
//     if (itermax < 1) itermax = 1;
//     if (lambda <= 0.0) lambda = 1e5;
//     if (p < 0.0) p = 0.0;
//     if (p > 1.0) p = 1.0;
//     if (wep < 0.0) wep = 0.0;
//     if (wep > 0.5) wep = 0.5;

//     // 预计算差分系数
//     const QVector<double> coeffs = computeDiffCoeffs(order);

//     // 初始化权重为 1
//     QVector<double> weights(n, 1.0);
//     QVector<double> baseline(n, 0.0);
//     // PCG 对角预条件（A 的对角近似）
//     QVector<double> M_inv_diag;
//     auto rebuildPrecond = [&](const QVector<double>& wts){
//         if (!g_use_precond) { M_inv_diag.clear(); return; }
//         QVector<double> diagDD = computeDiagDD(n, coeffs, lambda);
//         M_inv_diag.resize(n);
//         for (int i = 0; i < n; ++i) {
//             double diagA = wts[i] + diagDD[i];
//             if (diagA < 1e-12) diagA = 1e-12;
//             M_inv_diag[i] = 1.0 / diagA;
//         }
//     };
//     rebuildPrecond(weights);

//     // 计算边缘区域索引（保护区域）
//     const int protectCount = qCeil(n * wep);
//     const int protectStartEnd = qMax(0, n - protectCount);

//     // 用于收敛判断的总幅度
//     double sumAbsY = 0.0;
//     for (int i = 0; i < n; ++i) sumAbsY += std::abs(y[i]);
//     if (sumAbsY <= 0.0) sumAbsY = 1.0;

//     // 迭代拟合（加入 CG 限迭代与热启动）
//     const int cgMaxIter = g_cg_max_iter;
//     const double cgTol = g_cg_tol;
//     bool warmStart = g_warm_start;
//     // 若需要从外部参数覆盖，使用局部静态缓存或在 process 中传递（此处简化）

//     double dssn_prev = std::numeric_limits<double>::max();
//     int outerIters = (g_baseline_mode == "trend") ? qMax(1, g_trend_itermax) : itermax;
//     // 趋势模式先做一次对称Whittaker平滑作为初值
//     if (g_baseline_mode == "trend") {
//         QVector<double> ones(n, 1.0);
//         rebuildPrecond(ones);
//         QVector<double> b0(n, 0.0);
//         for (int i = 0; i < n; ++i) b0[i] = y[i];
//         int cgUsed0 = 0;
//         double lambda_eff0 = lambda;
//         if (g_lambda_scale_by_n) {
//             double scale = (static_cast<double>(n) / 500.0);
//             lambda_eff0 = lambda * scale * scale;
//         }
//         baseline = solveWithCG(b0, ones, coeffs, lambda_eff0, g_cg_max_iter, g_cg_tol, nullptr, &cgUsed0, g_use_precond ? &M_inv_diag : nullptr);
//         // 初始化权重为趋势模式的温和设置
//         for (int i = 0; i < n; ++i) weights[i] = 1.0;
//         rebuildPrecond(weights);
//     }
//     for (int iter = 1; iter <= outerIters; ++iter) {
//         QElapsedTimer iterTimer;
//         iterTimer.start();

//         // b = weights .* y
//         QElapsedTimer tBuild;
//         tBuild.start();
//         QVector<double> b(n, 0.0);
//         for (int i = 0; i < n; ++i) b[i] = weights[i] * y[i];
//         qint64 build_ms = tBuild.elapsed();

//         // 求解 (W + DD) * baseline = b
//         QElapsedTimer tSolve;
//         tSolve.start();
//         int cgUsed = 0;
//         // 按长度缩放lambda以贴近MATLAB
//         double lambda_eff = lambda;
//         if (g_lambda_scale_by_n) {
//             double scale = (static_cast<double>(n) / 500.0);
//             lambda_eff = lambda * scale * scale;
//         }
//         baseline = solveWithCG(b, weights, coeffs, lambda_eff, cgMaxIter, cgTol, warmStart ? &baseline : nullptr, &cgUsed, g_use_precond ? &M_inv_diag : nullptr);
//         qint64 solve_ms = tSolve.elapsed();

//         // 残差与负偏差总和
//         QElapsedTimer tResidual;
//         tResidual.start();
//         QVector<double> residuals(n, 0.0);
//         double dssn = 0.0;
//         for (int i = 0; i < n; ++i) {
//             residuals[i] = y[i] - baseline[i];
//             if (residuals[i] < 0.0) dssn += std::abs(residuals[i]);
//         }
//         qint64 residual_ms = tResidual.elapsed();

//         // 提前停止条件（相对总幅度）
//         if (dssn < g_dssn_rel_stop * sumAbsY) {
//             DEBUG_LOG << "airPLS 收敛: iter=" << iter
//                       << " dssn=" << dssn
//                       << " cg_used=" << cgUsed
//                       << " build_ms=" << build_ms
//                       << " solve_ms=" << solve_ms
//                       << " residual_ms=" << residual_ms
//                       << " total_ms=" << iterTimer.elapsed();
//             break;
//         }
//         // 若改善幅度极小则提前结束
//         if (std::abs(dssn_prev - dssn) < 1e-6 * sumAbsY) {
//             DEBUG_LOG << "airPLS 改善微弱，提前结束: iter=" << iter << " dssn=" << dssn;
//             break;
//         }
//         dssn_prev = dssn;

//         // 更新权重
//         QElapsedTimer tWeight;
//         tWeight.start();
//         const double denom = (dssn <= 1e-12) ? 1e-12 : dssn;
//         for (int i = 0; i < n; ++i) {
//             if (g_baseline_mode == "trend") {
//                 if (residuals[i] >= 0.0) {
//                     weights[i] = g_trend_p; // 温和约束，避免基线下压
//                 } else {
//                     double w = 1.0 + g_trend_alpha * std::abs(residuals[i]) / denom; // 线性增长，避免指数过猛
//                     if (w > g_max_weight) w = g_max_weight;
//                     weights[i] = w;
//                 }
//             } else {
//                 if (residuals[i] >= 0.0) {
//                     weights[i] = p;
//                 } else {
//                     double w = std::exp(iter * std::abs(residuals[i]) / denom);
//                     if (w > g_max_weight) w = g_max_weight;
//                     weights[i] = w;
//                 }
//             }
//         }
//         // 保护起始与结束区域
//         for (int i = 0; i < protectCount && i < n; ++i) {
//             weights[i] = p;
//         }
//         for (int i = protectStartEnd; i < n; ++i) {
//             weights[i] = p;
//         }
//         // 迭代中动态更新预条件
//         rebuildPrecond(weights);
//         qint64 weight_ms = tWeight.elapsed();

//         DEBUG_LOG << "airPLS 迭代统计: iter=" << iter
//                   << " dssn=" << dssn
//                   << " cg_used=" << cgUsed
//                   << " build_ms=" << build_ms
//                   << " solve_ms=" << solve_ms
//                   << " residual_ms=" << residual_ms
//                   << " weight_ms=" << weight_ms
//                   << " total_ms=" << iterTimer.elapsed();
//     }

//     return baseline;
// }

// // 可扩展差分矩阵实现（当前未使用）
// QVector<double> BaselineCorrector::diffMatrixApply(const QVector<double>& y, int order)
// {
//     QVector<double> res = y;
//     for (int k = 0; k < order; ++k) {
//         QVector<double> diff;
//         for (int i = 1; i < res.size(); ++i)
//             diff.append(res[i] - res[i - 1]);
//         res = diff;
//     }
//     return res;
// }


#include "BaselineCorrector.h"
#include "Logger.h"
#include <QtMath>
#include <QDebug>
#include <QElapsedTimer>
#include <cmath>
#include <limits>

QString BaselineCorrector::stepName() const {
    return "baseline_correction";
}

QString BaselineCorrector::userVisibleName() const {
    return QObject::tr("基线校正");
}

QVariantMap BaselineCorrector::defaultParameters() const {
    QVariantMap params;
    params["lambda"] = 1e5;
    params["order"] = 2;
    params["p"] = 0.05;
    params["itermax"] = 50;
    params["wep"] = 0.0;
    return params;
}

ProcessingResult BaselineCorrector::process(const QList<Curve*>& inputCurves, const QVariantMap& params, QString& error)
{
    DEBUG_LOG << "BaselineCorrector::process()";
    ProcessingResult result;

    const double lambda = params.value("lambda", 1e5).toDouble();
    const int order = params.value("order", 2).toInt();
    const double p = params.value("p", 0.05).toDouble();
    const int itermax = params.value("itermax", 50).toInt();
    const double wep = params.value("wep", 0.0).toDouble();

    for (Curve* inCurve : inputCurves) {
        const QVector<QPointF>& originalData = inCurve->data();
        if (originalData.isEmpty()) continue;

        QVector<double> y;
        y.reserve(originalData.size());
        for (const QPointF& pt : originalData)
            y.append(pt.y());

        QVector<double> yb;
        try {
            yb = airPLS(y, lambda, order, wep, p, itermax);
        } catch (...) {
            error = "airPLS 算法执行失败。";
            return result;
        }

        QVector<QPointF> corrected;
        corrected.reserve(y.size());
        for (int i = 0; i < y.size(); ++i)
            corrected.append(QPointF(originalData[i].x(), y[i] - yb[i]));

        Curve* correctedCurve = new Curve(corrected, inCurve->name() + QObject::tr(" (基线校正)"));
        correctedCurve->setSampleId(inCurve->sampleId());
        result.namedCurves["baseline_corrected"].append(correctedCurve);

        QVector<QPointF> baselinePts;
        baselinePts.reserve(yb.size());
        for (int i = 0; i < yb.size(); ++i)
            baselinePts.append(QPointF(originalData[i].x(), yb[i]));
        Curve* baselineCurve = new Curve(baselinePts, inCurve->name() + QObject::tr(" (基线)"));
        baselineCurve->setSampleId(inCurve->sampleId());
        baselineCurve->setColor(QColor(Qt::darkGray)); // 算法输出的基线统一使用深灰色，保证通用可读性
        result.namedCurves["baseline"].append(baselineCurve);
    }

    return result;
}

static QVector<double> computeDiffCoeffs(int order)
{
    QVector<double> coeffs(order + 1, 0.0);
    auto binom = [](int n, int k) -> double {
        if (k < 0 || k > n) return 0.0;
        double res = 1.0;
        for (int i = 1; i <= k; ++i) {
            res = res * (n - (k - i)) / i;
        }
        return res;
    };
    for (int k = 0; k <= order; ++k) {
        double c = binom(order, k);
        double sign = ((order - k) % 2 == 0) ? 1.0 : -1.0;
        coeffs[k] = sign * c;
    }
    return coeffs;
}

static QVector<double> applyDD(const QVector<double>& x, const QVector<double>& coeffs, double lambda)
{
    const int n = x.size();
    const int m = coeffs.size() - 1;
    QVector<double> y(n, 0.0);
    if (n == 0 || m <= 0) return y;
    
    const int rows = n - m;
    if (rows <= 0) return y;
    
    QVector<double> scaledCoeffs(m + 1);
    for (int k = 0; k <= m; ++k) {
        scaledCoeffs[k] = lambda * coeffs[k];
    }
    
    QVector<double> temp(rows, 0.0);
    for (int j = 0; j < rows; ++j) {
        double t = 0.0;
        const double* x_ptr = x.constData() + j;
        const double* c_ptr = scaledCoeffs.constData();
        for (int k = 0; k <= m; ++k) {
            t += c_ptr[k] * x_ptr[k];
        }
        temp[j] = t;
    }
    
    for (int j = 0; j < rows; ++j) {
        const double tj = temp[j];
        double* y_ptr = y.data() + j;
        const double* c_ptr = coeffs.constData();
        for (int k = 0; k <= m; ++k) {
            y_ptr[k] += tj * c_ptr[k];
        }
    }
    return y;
}

// 构造 DD = lambda * (D' * D) 的带状下三角存储（半带宽=order）
// band[d][i] 表示矩阵元素 A(i, i-d)，其中 d=0..order 且 i>=d
static QVector<QVector<double>> buildPenaltyBand(int n, const QVector<double>& coeffs, double lambda)
{
    const int order = coeffs.size() - 1;
    QVector<QVector<double>> band(order + 1, QVector<double>(n, 0.0));
    if (n <= 0 || order <= 0) return band;

    const int rows = n - order; // MATLAB: D = diff(speye(n), order) -> (n-order) x n
    if (rows <= 0) return band;

    for (int j = 0; j < rows; ++j) {
        for (int k = 0; k <= order; ++k) {
            const int i = j + k;
            const double ck = coeffs[k];
            for (int l = 0; l <= k; ++l) { // 仅累加下三角
                const int col = j + l;
                const int d = i - col;     // 0..order
                band[d][i] += lambda * ck * coeffs[l];
            }
        }
    }
    return band;
}

static bool choleskyFactorBandedInPlace(QVector<QVector<double>>& band)
{
    const int bw = band.size() - 1;
    if (bw < 0) return false;
    const int n = band.isEmpty() ? 0 : band[0].size();
    if (n <= 0) return false;

    for (int i = 0; i < n; ++i) {
        const int jStart = qMax(0, i - bw);
        for (int j = jStart; j <= i; ++j) {
            const int dij = i - j;
            double aij = band[dij][i];

            double sum = 0.0;
            const int kStart = qMax(0, qMax(i - bw, j - bw));
            for (int k = kStart; k < j; ++k) {
                const int dik = i - k;
                const int djk = j - k;
                if (dik > bw || djk > bw) continue;
                sum += band[dik][i] * band[djk][j];
            }

            double v = aij - sum;
            if (i == j) {
                if (!(v > 0.0)) {
                    return false;
                }
                band[0][i] = std::sqrt(v);
            } else {
                const double ljj = band[0][j];
                if (!(ljj > 0.0)) return false;
                band[dij][i] = v / ljj;
            }
        }
    }
    return true;
}

static QVector<double> solveCholeskyBanded(const QVector<QVector<double>>& Lband, const QVector<double>& b)
{
    const int bw = Lband.size() - 1;
    const int n = b.size();
    QVector<double> y(n, 0.0);
    QVector<double> x(n, 0.0);

    // 前代：L * y = b
    for (int i = 0; i < n; ++i) {
        double sum = 0.0;
        const int kStart = qMax(0, i - bw);
        for (int k = kStart; k < i; ++k) {
            const int dik = i - k;
            sum += Lband[dik][i] * y[k];
        }
        y[i] = (b[i] - sum) / Lband[0][i];
    }

    // 回代：L' * x = y
    for (int i = n - 1; i >= 0; --i) {
        double sum = 0.0;
        const int kEnd = qMin(n - 1, i + bw);
        for (int k = i + 1; k <= kEnd; ++k) {
            const int dki = k - i;
            sum += Lband[dki][k] * x[k];
        }
        x[i] = (y[i] - sum) / Lband[0][i];
    }
    return x;
}

QVector<double> BaselineCorrector::airPLS(const QVector<double>& y, double lambda, int order, double wep, double p, int itermax)
{
    const int n = y.size();
    if (n == 0) return {};
    
    if (order < 1) order = 1;
    if (itermax < 1) itermax = 1;
    if (lambda <= 0.0) lambda = 1e5;
    if (p < 0.0) p = 0.0;
    if (p > 1.0) p = 1.0;
    if (wep < 0.0) wep = 0.0;
    if (wep > 0.5) wep = 0.5;
    
    const QVector<double> coeffs = computeDiffCoeffs(order);
    QVector<double> weights(n, 1.0);
    QVector<double> baseline(n, 0.0);

    // 预计算 DD = lambda * (D' * D)，与 MATLAB 版本一致
    const QVector<QVector<double>> ddBand = buildPenaltyBand(n, coeffs, lambda);
    
    double sumAbsY = 0.0;
    for (int i = 0; i < n; ++i) sumAbsY += std::abs(y[i]);
    if (sumAbsY <= 0.0) sumAbsY = 1.0;

    // MATLAB: startIdx = 1:ceil(n*wep); endIdx = floor(n - n*wep + 1):n
    const int startCount = qCeil(n * wep);
    const int endStart = static_cast<int>(std::floor(n - n * wep));
    
    for (int iter = 1; iter <= itermax; ++iter) {
        QElapsedTimer iterTimer;
        iterTimer.start();
        
        QVector<double> b(n, 0.0);
        for (int i = 0; i < n; ++i) b[i] = weights[i] * y[i];

        // 构造 A = W + DD，并用带状 Cholesky 求解（等价 MATLAB chol / 反斜杠）
        QVector<QVector<double>> aBand = ddBand;
        for (int i = 0; i < n; ++i) aBand[0][i] += weights[i];
        bool ok = choleskyFactorBandedInPlace(aBand);
        if (!ok) {
            // 当矩阵接近奇异时，添加极小对角扰动以继续求解（与 MATLAB 反斜杠的稳健性目的相同）
            const double jitter = 1e-12;
            aBand = ddBand;
            for (int i = 0; i < n; ++i) aBand[0][i] += (weights[i] + jitter);
            ok = choleskyFactorBandedInPlace(aBand);
            if (!ok) {
                qWarning() << "airPLS: Cholesky 分解失败，无法求解基线";
                return baseline;
            }
        }
        baseline = solveCholeskyBanded(aBand, b);
        
        double dssn = 0.0;
        for (int i = 0; i < n; ++i) {
            double residual = y[i] - baseline[i];
            if (residual < 0.0) dssn += -residual;
        }
        
        if (dssn < 0.001 * sumAbsY) {
            DEBUG_LOG << "airPLS 收敛于迭代 " << iter 
                      << ", dssn = " << dssn
                      << ", 总耗时 " << iterTimer.elapsed() << " ms";
            break;
        }
        
        const double denom = (dssn == 0.0) ? std::numeric_limits<double>::min() : dssn;

        // 严格按 MATLAB updateWeights() 的赋值顺序实现
        for (int i = 0; i < n; ++i) {
            double residual = y[i] - baseline[i];
            if (residual >= 0.0) {
                weights[i] = 0.0;
            }
        }
        for (int i = 0; i < startCount && i < n; ++i) {
            weights[i] = p;
        }
        for (int i = qBound(0, endStart, n); i < n; ++i) {
            weights[i] = p;
        }
        for (int i = 0; i < n; ++i) {
            double residual = y[i] - baseline[i];
            if (residual < 0.0) {
                weights[i] = std::exp(iter * std::abs(residual) / denom);
            }
        }

        DEBUG_LOG << "airPLS 迭代 " << iter 
                  << ": dssn = " << dssn
                  << ", 耗时 " << iterTimer.elapsed() << " ms";
    }
    
    return baseline;
}

QVector<double> BaselineCorrector::diffMatrixApply(const QVector<double>& y, int order)
{
    QVector<double> res = y;
    for (int k = 0; k < order; ++k) {
        QVector<double> diff;
        for (int i = 1; i < res.size(); ++i)
            diff.append(res[i] - res[i - 1]);
        res = diff;
    }
    return res;
}
