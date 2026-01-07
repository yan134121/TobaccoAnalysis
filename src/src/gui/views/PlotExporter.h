#ifndef PLOTEXPORTER_H
#define PLOTEXPORTER_H

#include <QString>
#include <QWidget>
#include "qcustomplot.h"
#include "third_party/QXlsx/header/xlsxdocument.h"

/**
 * @brief 绘图导出工具类
 * 
 * 该类提供了将QCustomPlot导出为图像和数据的功能
 */
class PlotExporter
{
public:
    /**
     * @brief 构造函数
     * @param parent 父窗口，用于显示对话框
     */
    PlotExporter(QWidget* parent = nullptr);
    
    /**
     * @brief 导出绘图为图像
     * @param plot 要导出的QCustomPlot对象
     * @param defaultPath 默认保存路径
     * @return 是否导出成功
     */
    bool exportImage(QCustomPlot* plot, const QString& defaultPath = QString());
    
    /**
     * @brief 导出绘图数据为CSV、TXT或XLSX
     * @param plot 要导出的QCustomPlot对象
     * @param defaultPath 默认保存路径
     * @return 是否导出成功
     */
    bool exportData(QCustomPlot* plot, const QString& defaultPath = QString());
    
    /**
     * @brief 设置是否显示成功消息
     * @param show 是否显示
     */
    void setShowSuccessMessage(bool show);
    
private:
    QWidget* m_parentWidget;
    bool m_showSuccessMessage;
};

#endif // PLOTEXPORTER_H
