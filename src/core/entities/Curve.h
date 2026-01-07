#ifndef CURVE_H
#define CURVE_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QPointF>
#include <QColor>
#include <QPen>

#include "qcustomplot.h" 

class Curve : public QObject
{
    Q_OBJECT

    // --- 属性声明 ---
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
    Q_PROPERTY(Qt::PenStyle lineStyle READ lineStyle WRITE setLineStyle NOTIFY lineStyleChanged)
    Q_PROPERTY(int lineWidth READ lineWidth WRITE setLineWidth NOTIFY lineWidthChanged)
    Q_PROPERTY(int sampleId READ sampleId WRITE setSampleId NOTIFY sampleIdChanged)
    Q_PROPERTY(QString dataType READ dataType WRITE setDataType NOTIFY dataTypeChanged)
    Q_PROPERTY(QString sourceFileName READ sourceFileName WRITE setSourceFileName NOTIFY sourceFileNameChanged)
    Q_PROPERTY(int pointCount READ pointCount NOTIFY pointCountChanged)

public:
    // --- 构造函数 ---
    explicit Curve(QObject* parent = nullptr);

    // 便捷的构造函数，用于从原始数据快速创建
    Curve(const QVector<double>& xData, 
          const QVector<double>& yData, 
          const QString& name = "", 
          QObject* parent = nullptr);

    Curve(const QVector<QPointF>& data,
          const QString& name = "",
          QObject* parent = nullptr);

    // 拷贝构造函数，用于安全复制
    Curve(const Curve& other);
    Curve& operator=(const Curve& other);


    // --- 核心数据接口 ---
    const QVector<QPointF>& data() const;
    void setData(const QVector<QPointF>& data);
    void setData(const QVector<double>& xData, const QVector<double>& yData);
    int pointCount() const;


    QSharedPointer<QCPGraphDataContainer> getDataContainer() const;


    // --- Getters for Properties ---
    QString name() const;
    QColor color() const;
    Qt::PenStyle lineStyle() const;
    int lineWidth() const;
    int sampleId() const;
    QString dataType() const;
    QString sourceFileName() const;
    
    // (可选) 返回一个 QPen 对象，方便绘图组件使用
    QPen pen() const;

    QString printCurveData(int limit = -1) const;//参数空，就打印所有数据点

public slots:
    // --- Setters for Properties ---
    void setName(const QString& name);
    void setColor(const QColor& color);
    void setLineStyle(Qt::PenStyle style);
    void setLineWidth(int width);
    void setSampleId(int id);
    void setDataType(const QString& type);
    void setSourceFileName(const QString& fileName);

signals:
    // --- NOTIFY 信号 ---
    void nameChanged();
    void colorChanged();
    void lineStyleChanged();
    void lineWidthChanged();
    void sampleIdChanged();
    void dataTypeChanged();
    void sourceFileNameChanged();
    void pointCountChanged();
    void dataChanged(); // 当数据点本身发生变化时

private:
    // --- 核心数据 ---
    QVector<QPointF> m_data;  // 存储曲线的 (X, Y) 数据点


    int m_sampleId = -1;             // 关联的原始样本ID (single_tobacco_sample.id)
    QString m_dataType;              // 数据类型, e.g., "大热重", "色谱-平滑后"
    QString m_sourceFileName;        // 原始文件名


    QString m_name;                  // 曲线名称 (用于图例)
    QPen m_pen;                      // 使用 QPen 统一管理颜色、线宽、线型

    // 用 QCustomPlot 的数据容器替换 QVector<QPointF>
    QSharedPointer<QCPGraphDataContainer> m_dataContainer;
};

#endif // CURVE_H