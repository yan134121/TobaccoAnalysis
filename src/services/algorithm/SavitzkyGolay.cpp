// // #include "SavitzkyGolay.h"
// // #include "core/entities/Curve.h"

// // QString SavitzkyGolay::stepName() const {
// //     return "smoothing_sg"; // SG 平滑的唯一ID
// // }
// // QString SavitzkyGolay::userVisibleName() const {
// //     return QObject::tr("Savitzky-Golay 平滑");
// // }
// // QVariantMap SavitzkyGolay::defaultParameters() const {
// //     QVariantMap params;
// //     params["window_size"] = 9;
// //     params["poly_order"] = 2;
// //     // 注意：SG 也可以用于求导，但作为平滑时，deriv_order 是 0
// //     return params;
// // }

// // ProcessingResult SavitzkyGolay::process(const QList<Curve*> &inputCurves, const QVariantMap &params, QString &error)
// // {
// //     ProcessingResult result;
// //     int windowSize = params.value("window_size").toInt();
// //     int polyOrder = params.value("poly_order").toInt();
// //     // 强制 derivative_order 为 0，因为我们在这里只做平滑
// //     int derivOrder = 0; 
    
// //     for (Curve* inCurve : inputCurves) {
// //         // --- 在这里实现 Savitzky-Golay 平滑的数学逻辑 ---
// //         // 1. 获取输入数据 inCurve->data()
// //         // 2. 根据 windowSize, polyOrder, derivOrder=0 计算卷积系数
// //         // 3. 对数据进行卷积操作
// //         // 4. 得到平滑后的数据点 smoothedPoints

// //         // (以下为模拟代码)
// //         QVector<QPointF> smoothedPoints = inCurve->data(); // 暂时返回原始数据
// //         // --- 模拟代码结束 ---
        
// //         Curve* smoothedCurve = new Curve(smoothedPoints, inCurve->name() + QObject::tr(" (SG)"));
// //         result.namedCurves["smoothed"].append(smoothedCurve);
// //     }
    
// //     return result;
// // }

// // #include <Eigen/Dense> // 用 Eigen 计算矩阵伪逆
// #include <QtMath> // pow, tgamma

// #include "SavitzkyGolay.h"
// #include "core/entities/Curve.h"

// // stepName() 和 userVisibleName() 返回的是这个类的通用名称
// QString SavitzkyGolay::stepName() const {
//     return "savitzky_golay"; 
// }
// QString SavitzkyGolay::userVisibleName() const {
//     return QObject::tr("Savitzky-Golay 滤波");
// }

// // defaultParameters() 定义了所有可能的参数
// QVariantMap SavitzkyGolay::defaultParameters() const {
//     QVariantMap params;
//     params["window_size"] = 9;
//     params["poly_order"] = 2;
//     // 【关键】derivative_order 是一个必须的参数，默认值为0（平滑）
//     params["derivative_order"] = 0; 
//     return params;
// }


// // Helper: 生成 SG 卷积系数（平滑或一阶导数）
// static QVector<double> generateSGCoefficients(int windowSize, int polyOrder, int derivOrder)
// {
//     int half = windowSize / 2;
//     QVector<QVector<double>> A(windowSize, QVector<double>(polyOrder + 1));

//     // 构造 Vandermonde 矩阵
//     for (int i = -half; i <= half; ++i) {
//         for (int j = 0; j <= polyOrder; ++j) {
//             A[i + half][j] = qPow(i, j);
//         }
//     }

//     // 用正规方程求 (A^T A)^(-1) A^T 的第一行 / derivOrder 行
//     // 简化处理：对一阶导数，使用标准公式
//     QVector<double> coeff(windowSize, 0.0);

//     if (derivOrder == 0) {
//         // 平滑: 使用最简单的均值卷积 (可改成正规 SG)
//         double sum = 0;
//         for (int i = 0; i < windowSize; ++i) { sum += 1.0; coeff[i] = 1.0; }
//         for (int i = 0; i < windowSize; ++i) { coeff[i] /= sum; }
//     } else if (derivOrder == 1) {
//         // 一阶导数: 使用简化公式生成系数
//         // 参考公式：https://en.wikipedia.org/wiki/Savitzky%E2%80%93Golay_filter#Convolution_coefficients
//         int n = windowSize;
//         double norm = 0;
//         for (int k = 0; k < n; ++k) {
//             int i = k - half;
//             coeff[k] = i;
//             norm += i*i;
//         }
//         for (int k = 0; k < n; ++k) coeff[k] /= norm; // 归一化
//     }
//     return coeff;
// }

// ProcessingResult SavitzkyGolay::process(const QList<Curve*> &inputCurves, const QVariantMap &params, QString &error)
// {
//     ProcessingResult result;
//     int windowSize = params.value("window_size").toInt();
//     int polyOrder = params.value("poly_order").toInt();
//     int derivOrder = params.value("derivative_order", 0).toInt();

//     if (windowSize <= polyOrder || windowSize % 2 == 0) {
//         error = "Savitzky-Golay 参数无效";
//         return result;
//     }

//     QVector<double> coeff = generateSGCoefficients(windowSize, polyOrder, derivOrder);
//     int half = windowSize / 2;

//     for (Curve* inCurve : inputCurves) {
//         const QVector<QPointF>& data = inCurve->data();
//         int n = data.size();
//         QVector<QPointF> outData(n);

//         // 卷积计算
//         for (int i = 0; i < n; ++i) {
//             double y = 0;
//             for (int k = -half; k <= half; ++k) {
//                 int idx = qBound(0, i + k, n - 1);
//                 y += data[idx].y() * coeff[k + half];
//             }
//             outData[i] = QPointF(data[i].x(), y);
//         }

//         QString nameSuffix = (derivOrder == 0)
//             ? QObject::tr(" (SG 平滑)")
//             : QObject::tr(" (SG %1阶导)").arg(derivOrder);

//         QString key = (derivOrder == 0) ? "smoothed" : QString("derivative%1").arg(derivOrder);

//         Curve* outCurve = new Curve(outData, inCurve->name() + nameSuffix);
//         result.namedCurves[key].append(outCurve);
//     }

//     return result;
// }


#include "SavitzkyGolay.h"
#include "core/entities/Curve.h"
#include <QtMath>
#include <QVector>


QString SavitzkyGolay::stepName() const {
    return "smoothing_sg"; // SG 平滑的唯一ID
}
QString SavitzkyGolay::userVisibleName() const {
    return QObject::tr("Savitzky-Golay 平滑");
}
QVariantMap SavitzkyGolay::defaultParameters() const {
    QVariantMap params;
    params["window_size"] = 9;
    params["poly_order"] = 2;
    // 注意：SG 也可以用于求导，但作为平滑时，deriv_order 是 0
    return params;
}

// ==== 计算 SG 权重矩阵（返回每个列对应的权重）====
// 说明：正确的权重计算应为 M = (A^T A)^{-1} A^T，其中 A(i,j)=i^j。
//      常数项 c0 对应平滑权重（DC 增益应为 1），c1 对应一阶导权重。
static QVector<QVector<double>> sgMatrix(int polyOrder, int windowSize)
{
    // 仅支持奇数窗口，half 为窗口半宽
    int half = windowSize / 2; // 对奇数窗口与 (windowSize-1)/2 等价

    // 构造 A，尺寸为 [windowSize x (polyOrder+1)]
    QVector<QVector<double>> A(windowSize, QVector<double>(polyOrder + 1));
    for (int i = -half; i <= half; ++i) {
        for (int j = 0; j <= polyOrder; ++j) {
            A[i + half][j] = qPow(i, j);
        }
    }

    // 计算 ATA = A^T A，尺寸为 [(polyOrder+1) x (polyOrder+1)]
    QVector<QVector<double>> ATA(polyOrder + 1, QVector<double>(polyOrder + 1, 0.0));
    for (int i = 0; i <= polyOrder; ++i) {
        for (int j = 0; j <= polyOrder; ++j) {
            double sum = 0.0;
            for (int k = 0; k < windowSize; ++k) {
                sum += A[k][i] * A[k][j];
            }
            ATA[i][j] = sum;
        }
    }

    // 使用 QMatrix4x4 求逆（polyOrder 不超过 3 时有效），得到 invATA
    QMatrix4x4 mat; // 初始化为 0
    for (int i = 0; i <= polyOrder && i < 4; ++i) {
        for (int j = 0; j <= polyOrder && j < 4; ++j) {
            mat(i, j) = ATA[i][j];
        }
    }
    bool invertible = false;
    QMatrix4x4 invATA = mat.inverted(&invertible);
    if (!invertible) {
        // 逆不可用时返回全零，后续调用需检查
        return QVector<QVector<double>>(windowSize, QVector<double>(polyOrder + 1, 0.0));
    }

    // 计算 M = invATA * A^T，尺寸为 [(polyOrder+1) x windowSize]
    QVector<QVector<double>> M(polyOrder + 1, QVector<double>(windowSize, 0.0));
    for (int i = 0; i <= polyOrder && i < 4; ++i) {
        for (int k = 0; k < windowSize; ++k) {
            double sum = 0.0;
            for (int j = 0; j <= polyOrder && j < 4; ++j) {
                sum += invATA(i, j) * A[k][j];
            }
            M[i][k] = sum;
        }
    }

    // 将权重按原有返回格式组织为 G[windowSize x (polyOrder+1)]：
    // G[k][0] = 平滑权重；G[k][1] = 一阶导权重
    QVector<QVector<double>> G(windowSize, QVector<double>(polyOrder + 1, 0.0));
    for (int k = 0; k < windowSize; ++k) {
        G[k][0] = M[0][k]; // c0 行为平滑权重
        if (polyOrder >= 1) {
            G[k][1] = M[1][k]; // c1 行为一阶导权重（单位步长）
        }
    }

    return G;
}

// ==== SG 平滑 + 一阶导 ====
ProcessingResult SavitzkyGolay::process(const QList<Curve*> &inputCurves,
                                        const QVariantMap &params,
                                        QString &error)
{
    ProcessingResult result;

    int windowSize = params.value("window_size").toInt();
    int polyOrder = params.value("poly_order").toInt();

    if (windowSize <= polyOrder || windowSize % 2 == 0) {
        error = "Savitzky-Golay 参数无效";
        return result;
    }

    int half = (windowSize - 1) / 2;
    auto G = sgMatrix(polyOrder, windowSize);

    // 提取平滑系数 (第0列) 和一阶导数系数 (第1列)
    QVector<double> smoothCoeff(windowSize), derivCoeff(windowSize);
    for (int i = 0; i < windowSize; ++i) {
        smoothCoeff[i] = G[i][0];
        derivCoeff[i] = (polyOrder >= 1 ? G[i][1] : 0.0);
    }
    // 为避免数值误差导致 DC 增益偏移，对平滑系数进行归一化（权重和=1）
    double wsum = 0.0;
    for (int i = 0; i < windowSize; ++i) wsum += smoothCoeff[i];
    if (wsum != 0.0) {
        for (int i = 0; i < windowSize; ++i) smoothCoeff[i] /= wsum;
    }

    for (Curve* inCurve : inputCurves) {
        const QVector<QPointF>& data = inCurve->data();
        int n = data.size();

        QVector<QPointF> smoothData(n);
        QVector<QPointF> derivData(n);

        // 复制首尾点进行 padding
        QVector<double> y(n + 2 * half);
        for (int i = 0; i < half; ++i) y[i] = data.first().y();
        for (int i = 0; i < n; ++i) y[i + half] = data[i].y();
        for (int i = 0; i < half; ++i) y[n + half + i] = data.last().y();

        // 卷积实现平滑 + 导数
        for (int i = 0; i < n; ++i) {
            double ys = 0, yd = 0;
            for (int k = 0; k < windowSize; ++k) {
                ys += smoothCoeff[k] * y[i + k];
                yd += derivCoeff[k] * y[i + k];
            }
            smoothData[i] = QPointF(data[i].x(), ys);
            derivData[i]  = QPointF(data[i].x(), yd);
        }

        Curve* smoothed = new Curve(smoothData, inCurve->name() + QObject::tr(" (SG 平滑)"));
        Curve* derivative = new Curve(derivData, inCurve->name() + QObject::tr(" (SG 导数)"));

        result.namedCurves["smoothed"].append(smoothed);
        result.namedCurves["derivative1"].append(derivative);
    }

    return result;
}
