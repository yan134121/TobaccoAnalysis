#include "PlotExporter.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QSet>
#include <QList>
#include <algorithm>

PlotExporter::PlotExporter(QWidget* parent)
    : m_parentWidget(parent), m_showSuccessMessage(true)
{
}

bool PlotExporter::exportImage(QCustomPlot* plot, const QString& defaultPath)
{
    if (!plot) {
        return false;
    }
    
    // 获取保存文件路径
    QString filePath = QFileDialog::getSaveFileName(m_parentWidget, "导出图像", 
        defaultPath.isEmpty() ? QDir::homePath() + "/plot_image.png" : defaultPath, 
        "图像文件 (*.png *.jpg *.bmp *.pdf)");
    
    if (filePath.isEmpty()) {
        return false;  // 用户取消了操作
    }
    
    // 根据文件扩展名选择导出格式（根据扩展名切换导出逻辑）
    QFileInfo fileInfo(filePath);
    QString suffix = fileInfo.suffix().toLower();
    
    bool success = false;
    
    if (suffix == "pdf") {
        // 导出为PDF
        success = plot->savePdf(filePath);
    } else if (suffix == "jpg" || suffix == "jpeg") {
        // 导出为JPG
        success = plot->saveJpg(filePath, plot->width(), plot->height(), 1.0, 100);
    } else if (suffix == "bmp") {
        // 导出为BMP
        success = plot->saveBmp(filePath);
    } else {
        // 默认导出为PNG
        success = plot->savePng(filePath, plot->width(), plot->height(), 1.0, 100);
    }
    
    // 显示成功消息
    if (success && m_showSuccessMessage) {
        QMessageBox::information(m_parentWidget, "导出成功", "图像已成功导出到：\n" + filePath);
    }
    
    return success;
}

bool PlotExporter::exportData(QCustomPlot* plot, const QString& defaultPath)
{
    if (!plot) {
        return false;
    }
    
    // 获取保存文件路径
    QString filePath = QFileDialog::getSaveFileName(m_parentWidget, "导出数据", 
        defaultPath.isEmpty() ? QDir::homePath() + "/plot_data.xlsx" : defaultPath, 
        "Excel文件 (*.xlsx);;CSV文件 (*.csv);;文本文件 (*.txt)");
    
    if (filePath.isEmpty()) {
        return false;  // 用户取消了操作
    }
    
    // 收集所有X值
    QSet<double> xValues;
    for (int i = 0; i < plot->graphCount(); ++i) {
        QCPGraph* graph = plot->graph(i);
        for (int j = 0; j < graph->dataCount(); ++j) {
            xValues.insert(graph->data()->at(j)->key);
        }
    }
    
    // 转换为排序列表
    QList<double> sortedXValues = xValues.values();
    std::sort(sortedXValues.begin(), sortedXValues.end());
    
    // 根据文件扩展名选择导出格式
    QFileInfo fileInfo(filePath);
    QString suffix = fileInfo.suffix().toLower();
    
    if (suffix == "xlsx") {
        // 导出为XLSX格式
        QXlsx::Document xlsx;
        
        // 写入表头（Y 列名改为对应曲线的图例名，空图例名回退为 Y+序号）
        xlsx.write(1, 1, "X");
        for (int i = 0; i < plot->graphCount(); ++i) {
            QCPGraph* graph = plot->graph(i);
            QString colName;
            if (graph && !graph->name().trimmed().isEmpty()) {
                colName = graph->name();
            } else {
                colName = QString("Y%1").arg(i+1);
            }
            xlsx.write(1, i+2, colName);
        }
        
        // 写入数据
        int row = 2;
        for (const double& x : sortedXValues) {
            xlsx.write(row, 1, x);
            
            for (int i = 0; i < plot->graphCount(); ++i) {
                QCPGraph* graph = plot->graph(i);
                
                // 查找对应X值的Y值
                for (int j = 0; j < graph->dataCount(); ++j) {
                    if (qFuzzyCompare(graph->data()->at(j)->key, x)) {
                        xlsx.write(row, i+2, graph->data()->at(j)->value);
                        break;
                    }
                }
            }
            
            row++;
        }
        
        // 保存文件
        if (!xlsx.saveAs(filePath)) {
            if (m_showSuccessMessage) {
                QMessageBox::critical(m_parentWidget, "错误", "无法保存XLSX文件：\n" + filePath);
            }
            return false;
        }
    } else {
        // 导出为CSV或TXT格式（保持与XLSX一致的列名逻辑）
        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            if (m_showSuccessMessage) {
                QMessageBox::critical(m_parentWidget, "错误", "无法打开文件进行写入：\n" + filePath);
            }
            return false;
        }
        
        QTextStream out(&file);
        
        // 写入表头（使用曲线图例名作为列名，空图例名回退）
        out << "X";
        for (int i = 0; i < plot->graphCount(); ++i) {
            QCPGraph* graph = plot->graph(i);
            QString colName;
            if (graph && !graph->name().trimmed().isEmpty()) {
                colName = graph->name();
            } else {
                colName = QString("Y%1").arg(i+1);
            }
            out << "," << colName;
        }
        out << "\n";
        
        // 写入数据
        for (const double& x : sortedXValues) {
            out << QString::number(x) << ",";
            
            for (int i = 0; i < plot->graphCount(); ++i) {
                QCPGraph* graph = plot->graph(i);
                bool found = false;
                
                // 查找对应X值的Y值
                for (int j = 0; j < graph->dataCount(); ++j) {
                    if (qFuzzyCompare(graph->data()->at(j)->key, x)) {
                        out << QString::number(graph->data()->at(j)->value);
                        found = true;
                        break;
                    }
                }
                
                if (!found) {
                    out << ""; // 如果没有对应的Y值，则留空
                }
                
                if (i < plot->graphCount() - 1) {
                    out << ",";
                }
            }
            
            out << "\n";
        }
        
        file.close();
    }
    
    // 显示成功消息
    if (m_showSuccessMessage) {
        QMessageBox::information(m_parentWidget, "导出成功", "数据已成功导出到：\n" + filePath);
    }
    
    return true;
}

void PlotExporter::setShowSuccessMessage(bool show)
{
    m_showSuccessMessage = show;
}
