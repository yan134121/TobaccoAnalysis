

// InfoAutoClose.h
#include <QWidget>
#include <QLabel>
#include <QTimer>

class InfoAutoClose : public QWidget
{
    Q_OBJECT
public:
    explicit InfoAutoClose(QWidget *parent = nullptr);
    void showMessage(const QString &title, const QString &msg, int autoCloseMs);

private slots:
    void onTimeout();

private:
    QLabel *labelMsg;
    QLabel *labelCountdown;
    QTimer m_timer;
    int m_msLeft;
    int m_updateInterval;
};
