

// InfoAutoClose.cpp
#include "InfoAutoClose.h"
#include <QVBoxLayout>

InfoAutoClose::InfoAutoClose(QWidget *parent)
    : QWidget(parent), m_msLeft(0), m_updateInterval(50)
{
    labelMsg = new QLabel(this);
    labelCountdown = new QLabel(this);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(labelMsg);
    layout->addWidget(labelCountdown);
    setLayout(layout);

    connect(&m_timer, &QTimer::timeout, this, &InfoAutoClose::onTimeout);

    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
}

void InfoAutoClose::showMessage(const QString &title, const QString &msg, int autoCloseMs)
{
    setWindowTitle(title);
    labelMsg->setText(msg);
    m_msLeft = autoCloseMs;
    labelCountdown->setText(QString("(%1 秒后自动关闭)").arg(m_msLeft / 1000.0, 0, 'f', 1));
    m_timer.start(m_updateInterval);
    show();
}

void InfoAutoClose::onTimeout()
{
    m_msLeft -= m_updateInterval;
    if (m_msLeft <= 0) {
        m_timer.stop();
        close();
        return;
    }
    labelCountdown->setText(QString("(%1 秒后自动关闭)").arg(m_msLeft / 1000.0, 0, 'f', 1));
}
