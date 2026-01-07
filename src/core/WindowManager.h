#ifndef WINDOWMANAGER_H
#define WINDOWMANAGER_H

#include <QObject>
#include <QMap>
#include <QList>
#include <QPointer>
#include <QWidget>

class WindowManager : public QObject
{
    Q_OBJECT
public:
    // 单例
    static WindowManager* instance();

    // 注册窗口
    void registerWindow(QWidget* window, const QString& type);
    void unregisterWindow(QWidget* window);

    // 获取活跃窗口（可按类型）
    QWidget* getActiveWindow(const QString& type = QString());
    // 获取所有窗口（可按类型）
    QList<QWidget*> getAllWindows(const QString& type = QString());

private:
    explicit WindowManager(QObject* parent = nullptr);
    ~WindowManager();

    QMap<QWidget*, QString> m_windows;     // 存储窗口和类型
    QPointer<QWidget> m_activeWindow;      // 当前活跃窗口

private slots:
    void onWindowActivated(QWidget* window);
    void onWindowDestroyed(QObject* obj);
};

#endif // WINDOWMANAGER_H
