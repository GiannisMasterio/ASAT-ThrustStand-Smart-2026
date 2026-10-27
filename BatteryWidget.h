#ifndef BATTERYWIDGET_H
#define BATTERYWIDGET_H

#include <QWidget>
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QFontMetrics>
#include <QPushButton>
#include <QString>
#include <algorithm>

class BatteryWidget : public QPushButton {
    Q_OBJECT
public:
    // Προσθέσαμε την παράμετρο "title" ως πρώτο όρισμα
    explicit BatteryWidget(const QString& title = "", bool isVertical = true, QWidget* parent = nullptr, double value = 0.0)
        : QPushButton(parent), titleText(title), vertical(isVertical), value(value) {
        setFlat(true);
        setStyleSheet("background: transparent; border: none;");
        setAttribute(Qt::WA_TranslucentBackground);

        // Αυξήσαμε λίγο το ύψος για να χωράει άνετα ο τίτλος
        if (vertical) {
            setMinimumSize(300, 500);
            setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        }
        else {
            setMinimumSize(450, 250);
        }
    }

    void setPercentage(int newPercentage) {
        percentage = std::clamp(newPercentage, 0, 100);
        update();
    }

    void setTitle(const QString& title) {
        titleText = title;
        update();
    }

    void setOrientation(bool isVertical) {
        vertical = isVertical;
        if (vertical) setMinimumSize(250, 500);
        else setMinimumSize(450, 250);
        update();
    }

    void setVoltage(double newValue) {
        value = newValue;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        // --- Κάρτα (ίδιο στυλ με τα Containers) ---
        QRectF card = QRectF(rect()).adjusted(1, 1, -1, -1);
        QLinearGradient bg(0, 0, 0, height());
        bg.setColorAt(0, QColor(30, 41, 59, 200));
        bg.setColorAt(1, QColor(10, 16, 30, 225));
        p.setBrush(bg);
        p.setPen(QPen(QColor(255, 255, 255, 40), 1));
        p.drawRoundedRect(card, 28, 28);

        const int margin = 28, titleH = 40, pctH = 90;

        // --- Τίτλος ---
        if (!titleText.isEmpty()) {
            QFont f("Segoe UI", 18, QFont::DemiBold);
            f.setLetterSpacing(QFont::AbsoluteSpacing, 3);
            p.setFont(f);
            p.setPen(QColor(203, 213, 225, 230));
            p.drawText(QRect(0, margin, width(), titleH), Qt::AlignCenter, titleText.toUpper());
        }

        // --- Διαστάσεις μπαταρίας (αφήνει χώρο για τον πόλο) ---
        const int poleH = 14;
        int availH = height() - margin - titleH - pctH - margin - poleH - 10;
        int batH = vertical ? availH : qMin(availH, 120);
        int batW = vertical ? qMin(int(batH * 0.40), width() - 100) : qMin(width() - 100, 350);
        int x = (width() - batW) / 2;
        int y = margin + titleH + poleH + 10;

        // --- Πόλος ---
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, 150));
        if (vertical) p.drawRoundedRect(x + batW / 3, y - poleH, batW / 3, poleH + 4, 4, 4);
        else          p.drawRoundedRect(x + batW - 2, y + batH / 3, poleH, batH / 3, 4, 4);

        // --- Σώμα: λεπτό, ημιδιάφανο ---
        const int r = 20;
        p.setBrush(QColor(255, 255, 255, 18));
        p.setPen(QPen(QColor(255, 255, 255, 150), 3));
        p.drawRoundedRect(QRectF(x, y, batW, batH), r, r);

        // --- Χρώμα γεμίσματος με gradient ---
        QColor c1, c2;
        if (percentage > 50) { c1 = QColor("#34d399"); c2 = QColor("#059669"); }
        else if (percentage > 20) { c1 = QColor("#fbbf24"); c2 = QColor("#d97706"); }
        else { c1 = QColor("#f87171"); c2 = QColor("#dc2626"); }

        // --- Γέμισμα (clip στο εσωτερικό ώστε οι γωνίες να μένουν στρογγυλές) ---
        const int pad = 6;
        QRectF inner(x + pad, y + pad, batW - 2 * pad, batH - 2 * pad);
        QPainterPath clip;
        clip.addRoundedRect(inner, r - 6, r - 6);
        p.save();
        p.setClipPath(clip);
        p.setPen(Qt::NoPen);

        if (vertical) {
            double fillH = inner.height() * percentage / 100.0;
            QRectF fill(inner.left(), inner.bottom() - fillH, inner.width(), fillH);
            QLinearGradient g(fill.topLeft(), fill.bottomLeft());
            g.setColorAt(0, c1); g.setColorAt(1, c2);
            p.setBrush(g);
            p.drawRect(fill);
        }
        else {
            double fillW = inner.width() * percentage / 100.0;
            QRectF fill(inner.left(), inner.top(), fillW, inner.height());
            QLinearGradient g(fill.topLeft(), fill.topRight());
            g.setColorAt(0, c2); g.setColorAt(1, c1);
            p.setBrush(g);
            p.drawRect(fill);
        }
        p.restore();

        /*
        QFont nf("Segoe UI", 44, QFont::Bold);
        QFont uf("Segoe UI", 20, QFont::DemiBold);
        QString num = QString::number(percentage);
        int nw = QFontMetrics(nf).horizontalAdvance(num);
        int uw = QFontMetrics(uf).horizontalAdvance("%");
        int tx = (width() - (nw + uw + 4)) / 2;
        int ty = y + batH + 12;

        p.setFont(nf);
        p.setPen(Qt::white);
        p.drawText(QRect(tx, ty, nw, pctH - 10), Qt::AlignVCenter | Qt::AlignLeft, num);
        p.setFont(uf);
        p.setPen(QColor("#1e90ff"));
        p.drawText(QRect(tx + nw + 4, ty + 12, uw + 4, pctH - 20), Qt::AlignVCenter | Qt::AlignLeft, "%"); */

        QFont nf("Segoe UI", 40, QFont::Bold);
        QFont uf("Segoe UI", 20, QFont::DemiBold);
        QString num = QString::number(value, 'f', 2);
        int nw = QFontMetrics(nf).horizontalAdvance(num);
        int uw = QFontMetrics(uf).horizontalAdvance("V");
        int tx = (width() - (nw + uw + 6)) / 2;
        int ty = y + batH + 12;

        p.setFont(nf);
        p.setPen(Qt::white);
        p.drawText(QRect(tx, ty, nw, pctH - 10), Qt::AlignVCenter | Qt::AlignLeft, num);
        p.setFont(uf);
        p.setPen(QColor("#1e90ff"));
        p.drawText(QRect(tx + nw + 6, ty + 10, uw + 4, pctH - 20), Qt::AlignVCenter | Qt::AlignLeft, "V");
    }
private:
    int percentage = 0;
    bool vertical = true;
    double value;
    QString titleText;
};

#endif