
#include "MyWidget.h"
#include "Folder.h"

#include <QMessageBox>
#include <QEvent>
#include <QCloseEvent>
#include <QString>
#include <QLocale>
#include <QIcon>
#include <QtDebug>
#include <QDebug>

#include "core/singletons/StringManager.h"

MyWidget::MyWidget(const QString &label, QWidget *parent, const QString name, Qt::WindowFlags f)
    : QMdiSubWindow(parent, f)
{
    DEBUG_LOG << "MyWidget::MyWidget(const QString &label, QWidget *parent, const QString name, Qt::WindowFlags f)";
    w_label = label;
    caption_policy = Both;
    askOnClose = true;
    w_status = Normal;
    setObjectName(QString(name));
}

void MyWidget::updateCaption()
{
    switch (caption_policy) {
    case Name:
        setWindowTitle(name());
        break;

    case Label:
        if (!windowLabel().isEmpty())
            setWindowTitle(windowLabel());
        else
            setWindowTitle(name());
        break;

    case Both:
        if (!windowLabel().isEmpty())
            setWindowTitle(name() + " - " + windowLabel());
        else
            setWindowTitle(name());
        break;
    }
}

void MyWidget::closeEvent(QCloseEvent *e)
{
    if (askOnClose) {
        // switch (QMessageBox::information(this, tr("SciDAVis"),
        //                                  tr("Do you want to hide or delete") + "<p><b>'"
        //                                          + objectName() + "'</b> ?",
        //                                  tr("Delete"), tr("Hide"), tr("Cancel"), 0, 2)) {
        switch (QMessageBox::information(this, STR("SciDAVis"),
                                         STR("Do you want to hide or delete") + "<p><b>'"
                                                 + objectName() + "'</b> ?",
                                         STR("Delete"), STR("Hide"), STR("Cancel"), 0, 2)) {
                                            
        case 0:
            emit closedWindow(this);
            e->accept();
            break;

        case 1:
            e->ignore();
            emit hiddenWindow(this);
            break;

        case 2:
            e->ignore();
            break;
        }
    } else {
        emit closedWindow(this);
        e->accept();
    }
}

QString MyWidget::aspect()
{
    QString s = tr("Normal");
    switch (w_status) {
    case Hidden:
        return tr("Hidden");
        break;

    case Normal:
        break;

    case Minimized:
        return tr("Minimized");
        break;

    case Maximized:
        return tr("Maximized");
        break;
    }
    return s;
}

void MyWidget::changeEvent(QEvent *event)
{
    if (!isHidden() && event->type() == QEvent::WindowStateChange) {
        if (((QWindowStateChangeEvent *)event)->oldState() == windowState())
            return;
        if (windowState() & Qt::WindowMinimized)
            w_status = Minimized;
        else if (windowState() & Qt::WindowMaximized)
            w_status = Maximized;
        else
            w_status = Normal;
        emit statusChanged(this);
    }
    QMdiSubWindow::changeEvent(event);
}

void MyWidget::contextMenuEvent(QContextMenuEvent *e)
{
    if (!this->widget()->geometry().contains(e->pos())) {
        emit showTitleBarMenu();
        e->accept();
    }
}

void MyWidget::setStatus(Status s)
{
    if (w_status == s)
        return;

    w_status = s;
    emit statusChanged(this);
}

void MyWidget::setHidden()
{
    w_status = Hidden;
    emit statusChanged(this);
    hide();
}

void MyWidget::setNormal()
{
    showNormal();
    w_status = Normal;
    emit statusChanged(this);
}

void MyWidget::setMinimized()
{
    showMinimized();
    w_status = Minimized;
    emit statusChanged(this);
}

void MyWidget::setMaximized()
{
    showMaximized();
    w_status = Maximized;
    emit statusChanged(this);
}
