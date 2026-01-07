// Lightweight width-aware numeric display delegate
#pragma once

#include <QStyledItemDelegate>
#include <QPainter>
#include <QStyle>
#include <QApplication>

class WidthAwareNumberDelegate : public QStyledItemDelegate {
public:
    explicit WidthAwareNumberDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QStyleOptionViewItem opt(option);
        initStyleOption(&opt, index);

        QString text = index.data(Qt::DisplayRole).toString();
        bool ok = false;
        double value = text.toDouble(&ok);
        QFontMetrics fm(opt.font);
        int avail = opt.rect.width() - 6; // padding

        auto fits = [&](const QString& s) { return fm.horizontalAdvance(s) <= avail; };

        if (ok) {
            if (!fits(text)) {
                QString best = text;
                for (int d = 16; d >= 0; --d) {
                    QString s = QString::number(value, 'f', d);
                    if (fits(s)) { best = s; break; }
                }
                if (!fits(best)) {
                    for (int d = 12; d >= 3; --d) {
                        QString s = QString::number(value, 'g', d);
                        if (fits(s)) { best = s; break; }
                    }
                }
                if (!fits(best)) {
                    best = fm.elidedText(QString::number(value, 'g', 12), Qt::ElideRight, avail);
                }
                opt.text = best;
            }
        } else {
            opt.text = fm.elidedText(text, Qt::ElideRight, avail);
        }

        if (const QWidget* widget = opt.widget) {
            widget->style()->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);
        } else {
            QStyle* style = QApplication::style();
            style->drawControl(QStyle::CE_ItemViewItem, &opt, painter);
        }
    }
};