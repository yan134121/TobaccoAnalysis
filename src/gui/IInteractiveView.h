#ifndef IINTERACTIVEVIEW_H
#define IINTERACTIVEVIEW_H

#include <QString>
#include <QtPlugin> // 【关键】确保包含了这个头文件

/**
 * @brief IInteractiveView 接口
 * 
 * 定义了所有可与主工具栏交互的视图必须实现的方法。
 * 这使得 MainWindow 可以与任何实现了此接口的视图进行通信，而无需知道其具体类型。
 */
// class IInteractiveView
// {
// public:
//     virtual ~IInteractiveView() {}

//     // 设置当前视图的工具模式（如 "select", "pan", "zoom_rect"）
//     virtual void setCurrentTool(const QString& toolId) = 0;

//     // 执行缩放操作
//     virtual void zoomIn() = 0;
//     virtual void zoomOut() = 0;
//     virtual void zoomReset() = 0;
// };

class IInteractiveView {
public:
    virtual ~IInteractiveView() = default;

    virtual void setCurrentTool(const QString& toolId) = 0;
    virtual void zoomIn() = 0;
    virtual void zoomOut() = 0;
    virtual void zoomReset() = 0;
};

// 【关键】为接口声明一个唯一的标识符字符串
#define IInteractiveView_iid "com.YourCompany.TobaccoAnalysis.IInteractiveView/1.0"

// 【关键】使用 Q_DECLARE_INTERFACE 宏将 C++ 接口注册到 Qt 元对象系统
Q_DECLARE_INTERFACE(IInteractiveView, IInteractiveView_iid)


// 在需要 qobject_cast 的地方，需要一个 Q_DECLARE_INTERFACE
// Q_DECLARE_INTERFACE(IInteractiveView, "com.YourCompany.IInteractiveView") // 可选，但推荐

#endif // IINTERACTIVEVIEW_H