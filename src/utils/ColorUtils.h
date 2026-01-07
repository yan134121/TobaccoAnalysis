

// #endif // COLOR_H

#ifndef COLORUTILS_H
#define COLORUTILS_H

// #include "ColorUtils.h"
#include <QColor>

/**
 * @brief 科学可视化配色辅助类
 * 提供统一的色彩方案与智能曲线颜色生成功能。
 */
class ColorUtils {
public:
    // === 饱和度与亮度常量（替代宏定义） ===
    static constexpr int SATURATION_MIN       = 80;
    static constexpr int SATURATION_MEDIUM    = 180;
    static constexpr int SATURATION_HIGH      = 200;

    static constexpr int BRIGHTNESS_DARK      = 120;
    static constexpr int BRIGHTNESS_MEDIUM    = 180;
    static constexpr int BRIGHTNESS_HIGH      = 200;

    // === 常用颜色（标准科学配色） ===
    static QColor sciBlue()   { return QColor::fromHsv(220, SATURATION_MEDIUM, BRIGHTNESS_HIGH); }
    static QColor sciOrange() { return QColor::fromHsv(30,  SATURATION_MEDIUM, BRIGHTNESS_HIGH); }

    // === 智能曲线配色函数 ===
    static QColor setCurveColor(int index);

    // 图表背景颜色
    // static QColor ChartView_plotBackground() { return QColor(240, 240, 240); }
    static QColor ChartView_plotBackground() { return QColor(255, 255, 255); } // 白色背景

    // 绘图区背景颜色
    static QColor ChartView_plotAreaBackground() { return QColor(255, 255, 255); }
};

#endif // COLORUTILS_H
