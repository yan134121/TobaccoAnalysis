#include "WindowManager.h"
#include <QApplication>
#include <QDebug>

WindowManager* WindowManager::instance()
{
    static WindowManager wm;
    return &wm;
}

WindowManager::WindowManager(QObject* parent)
    : QObject(parent)
{
}

WindowManager::~WindowManager()
{
    m_windows.clear();
    m_activeWindow = nullptr;
}

void WindowManager::registerWindow(QWidget* window, const QString& type)
{
    if (!window) return;

    m_windows[window] = type;

    // 连接激活和销毁信号
    connect(window, &QWidget::windowTitleChanged, this, [this, window]() {
        onWindowActivated(window);
    });
    connect(window, &QObject::destroyed, this, &WindowManager::onWindowDestroyed);
}

void WindowManager::unregisterWindow(QWidget* window)
{
    if (!window) return;

    m_windows.remove(window);
    if (m_activeWindow == window)
        m_activeWindow = nullptr;
}

QWidget* WindowManager::getActiveWindow(const QString& type)
{
    if (!type.isEmpty()) {
        // 如果有指定类型，优先返回该类型的活跃窗口
        if (m_activeWindow && m_windows.value(m_activeWindow) == type)
            return m_activeWindow;

        // 如果当前活跃窗口不是该类型，从列表里找一个
        for (auto it = m_windows.begin(); it != m_windows.end(); ++it) {
            if (it.value() == type)
                return it.key();
        }
        return nullptr;
    }

    return m_activeWindow;
}

QList<QWidget*> WindowManager::getAllWindows(const QString& type)
{
    QList<QWidget*> list;
    for (auto it = m_windows.begin(); it != m_windows.end(); ++it) {
        if (type.isEmpty() || it.value() == type)
            list.append(it.key());
    }
    return list;
}

void WindowManager::onWindowActivated(QWidget* window)
{
    if (window && m_windows.contains(window))
        m_activeWindow = window;
}

void WindowManager::onWindowDestroyed(QObject* obj)
{
    QWidget* w = qobject_cast<QWidget*>(obj);
    if (w)
        unregisterWindow(w);
}
