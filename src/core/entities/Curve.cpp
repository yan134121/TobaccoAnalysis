#include "Curve.h"
#include <algorithm> // For std::min
#include "Logger.h"

// --- 构造函数 ---

Curve::Curve(QObject *parent) 
    : QObject(parent)
{
    // 默认构造，设置一些安全的初始值
    m_pen.setColor(Qt::blue);
    m_pen.setWidth(1);
    m_pen.setStyle(Qt::SolidLine);
}

Curve::Curve(const QVector<double> &xData, const QVector<double> &yData, const QString &name, QObject *parent)
    : QObject(parent), m_name(name)
{
    setData(xData, yData);
    m_pen.setColor(Qt::blue);
    m_pen.setWidth(1);
    m_pen.setStyle(Qt::SolidLine);
}

Curve::Curve(const QVector<QPointF> &data, const QString &name, QObject *parent)
    : QObject(parent), m_data(data), m_name(name)
{
    m_pen.setColor(Qt::blue);
    m_pen.setWidth(1);
    m_pen.setStyle(Qt::SolidLine);
}

// 拷贝构造函数
Curve::Curve(const Curve &other)
    : QObject(other.parent()) // 父对象通常不拷贝
{
    m_data = other.m_data;
    m_sampleId = other.m_sampleId;
    m_dataType = other.m_dataType;
    m_sourceFileName = other.m_sourceFileName;
    m_name = other.m_name;
    m_pen = other.m_pen;
}

// 赋值运算符
Curve& Curve::operator=(const Curve &other)
{
    if (this == &other) {
        return *this; // 处理自我赋值
    }
    // 父对象不赋值
    m_data = other.m_data;
    m_sampleId = other.m_sampleId;
    m_dataType = other.m_dataType;
    m_sourceFileName = other.m_sourceFileName;
    m_name = other.m_name;
    m_pen = other.m_pen;
    
    // 发出信号通知所有属性都可能已改变
    emit dataChanged();
    emit sampleIdChanged();
    emit dataTypeChanged();
    emit sourceFileNameChanged();
    emit nameChanged();
    emit colorChanged();
    emit lineStyleChanged();
    emit lineWidthChanged();

    return *this;
}

// --- 核心数据接口 ---

// ！！！【新增】实现 getDataContainer
QSharedPointer<QCPGraphDataContainer> Curve::getDataContainer() const
{
    return m_dataContainer;
}

const QVector<QPointF>& Curve::data() const
{
    return m_data;
}
// // (可选) 如果你还想保留旧的 data() 方法
// const QVector<QPointF>& Curve::data() const
// {
//     // 这个方法现在效率会变低，因为它需要每次都进行转换
//     // 最好在代码中尽量避免使用它
//     static QVector<QPointF> temp_data; // 静态变量以返回引用
//     temp_data.clear();
//     temp_data.reserve(m_dataContainer->size());
//     for (auto it = m_dataContainer->begin(); it != m_dataContainer->end(); ++it) {
//         temp_data.append(QPointF(it->key, it->value));
//     }
//     return temp_data;
// }

void Curve::setData(const QVector<QPointF> &data)
{
    if (m_data != data) {
        m_data = data;
        emit dataChanged();
    }
}

// // ！！！【修改】更新 setData 方法
// void Curve::setData(const QVector<QPointF>& data)
// {
//     // 清空旧数据并准备填充
//     m_dataContainer->clear();
//     // m_dataContainer->reserve(data.size()); // 预分配内存，提高效率

//     // 遍历 QPointF 并添加到新的容器中
//     for (const QPointF& point : data) {
//         m_dataContainer->add(QCPGraphData(point.x(), point.y()));
//     }
//     emit dataChanged();
//     emit pointCountChanged();
// }


void Curve::setData(const QVector<double> &xData, const QVector<double> &yData)
{
    QVector<QPointF> newData;
    int size = std::min(xData.size(), yData.size());
    newData.reserve(size);
    for (int i = 0; i < size; ++i) {
        newData.append(QPointF(xData.at(i), yData.at(i)));
    }
    setData(newData); // 复用 setData(QVector<QPointF>)
}


// // ！！！【修改】更新另一个 setData 重载
// void Curve::setData(const QVector<double>& xData, const QVector<double>& yData)
// {
//     // ... 实现与上面类似 ...
//     m_dataContainer->clear();
//     int size = qMin(xData.size(), yData.size());
//     // m_dataContainer->reserve(size);
//     for (int i = 0; i < size; ++i) {
//         m_dataContainer->add(QCPGraphData(xData[i], yData[i]));
//     }
//     emit dataChanged();
//     emit pointCountChanged();
// }


int Curve::pointCount() const
{
    return m_data.size();
}

// // ！！！【修改】更新 pointCount
// int Curve::pointCount() const
// {
//     return m_dataContainer->size();
// }


// --- Getters for Properties ---

QString Curve::name() const { return m_name; }
QColor Curve::color() const { return m_pen.color(); }
Qt::PenStyle Curve::lineStyle() const { return m_pen.style(); }
int Curve::lineWidth() const { return m_pen.width(); }
int Curve::sampleId() const { return m_sampleId; }
QString Curve::dataType() const { return m_dataType; }
QString Curve::sourceFileName() const { return m_sourceFileName; }
QPen Curve::pen() const { return m_pen; }

// --- Setters for Properties ---

void Curve::setName(const QString &name)
{
    if (m_name != name) {
        m_name = name;
        DEBUG_LOG << "Emitting nameChanged, receivers:" << receivers(SIGNAL(nameChanged()));
        emit nameChanged();
    }
}

void Curve::setColor(const QColor &color)
{
    if (m_pen.color() != color) {
        m_pen.setColor(color);
        emit colorChanged();
    }
}

void Curve::setLineStyle(Qt::PenStyle style)
{
    if (m_pen.style() != style) {
        m_pen.setStyle(style);
        emit lineStyleChanged();
    }
}

void Curve::setLineWidth(int width)
{
    if (m_pen.width() != width) {
        m_pen.setWidth(width);
        emit lineWidthChanged();
    }
}

void Curve::setSampleId(int id)
{
    if (m_sampleId != id) {
        m_sampleId = id;
        emit sampleIdChanged();
    }
}

void Curve::setDataType(const QString &type)
{
    if (m_dataType != type) {
        m_dataType = type;
        emit dataTypeChanged();
    }
}

void Curve::setSourceFileName(const QString &fileName)
{
    if (m_sourceFileName != fileName) {
        m_sourceFileName = fileName;
        emit sourceFileNameChanged();
    }
}

QString Curve::printCurveData(int limit) const
{
    QString str;
    str += QString("=== Curve Data (%1) ===\n").arg(m_name);
    str += QString("SampleId: %1 | Type: %2 | Points: %3\n\n")
            .arg(m_sampleId)
            .arg(m_dataType)
            .arg(m_data.size());

    int count = (limit < 0) ? m_data.size() : qMin(limit, m_data.size());

    for (int i = 0; i < count; ++i) {
        const auto& p = m_data[i];
        str += QString("[%1]  X=%2,  Y=%3\n")
                .arg(i)
                .arg(p.x())
                .arg(p.y());
    }

    if (limit > 0 && m_data.size() > limit) {
        str += QString("... (%1 more points omitted)\n")
                .arg(m_data.size() - limit);
    }

    return str;
}
