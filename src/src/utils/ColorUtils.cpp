

#include "ColorUtils.h"
#include <QColor>

/**
 * @brief 根据索引智能生成颜色（避免相邻颜色过近）
 * @param index 序号（从 0 开始）
 * @return QColor 对象（HSV 生成）
 */
QColor ColorUtils::setCurveColor(int index)
{
    // 使用黄金角度（137°）分布 hue，避免颜色冲突
    int hue = (index * 137) % 360;
    int saturation = SATURATION_MEDIUM;
    int brightness = BRIGHTNESS_HIGH;

    // 智能调整不同色系的亮度与饱和度
    if ((hue >= 20 && hue <= 40) || (hue >= 200 && hue <= 220)) {
        // 橙色、蓝色系：降低饱和度和亮度
        saturation = 160;
        brightness = 160;
    } else if (hue >= 330 || hue <= 10) {
        // 红色系：提高亮度
        brightness = 220;
        saturation = 160;
    } else if (hue >= 70 && hue <= 90) {
        // 黄绿色系：保持原值
    }

    return QColor::fromHsv(hue, saturation, brightness);
}
