#ifndef BIGBATTERYWIDGET_H
#define BIGBATTERYWIDGET_H

#include <QWidget>
#include <QPainter>
#include <QLinearGradient>

class BigBatteryWidget : public QWidget {
    Q_OBJECT

public:
    explicit BigBatteryWidget(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumSize(320, 80);
    }

    void setPercentage(int pct) {
        percentage = qBound(0, pct, 100);
        update();
    }

protected:
    void paintEvent(QPaintEvent* event) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        painter.setBrush(Qt::NoBrush);
        painter.setPen(Qt::NoPen);

        QRect batteryRect(10, 10, 300, 60);

        // 2. Σχεδίαση του περιγράμματος (Main Body)
        painter.setPen(QPen(Qt::white, 3));
        painter.setBrush(QColor(20, 20, 20, 150)); // Ημιδιάφανο εσωτερικό
        painter.drawRoundedRect(batteryRect, 10, 10);

        // 3. Το "κλιπ" (θετικός πόλος) στα δεξιά
        painter.setBrush(Qt::white);
        painter.drawRoundedRect(batteryRect.right() + 2, batteryRect.top() + 20, 8, 20, 2, 2);

        // 4. Σχεδίαση των επιπέδων (Bars)
        int numBars = 20;
        int activeBars = (percentage * numBars) / 100;

        // Υπολογισμός πλάτους για κάθε bar (με κενό ανάμεσα)
        double totalInternalWidth = batteryRect.width() - 10;
        double barWidth = totalInternalWidth / numBars;

        for (int i = 0; i < activeBars; ++i) {
            QLinearGradient grad(0, batteryRect.top(), 0, batteryRect.bottom());

            grad.setColorAt(0, QColor(200, 255, 255));
            grad.setColorAt(0.5, QColor(0, 220, 255));
            grad.setColorAt(1, QColor(0, 150, 200));

            painter.setBrush(grad);
            painter.setPen(Qt::NoPen); 

            painter.drawRect(batteryRect.left() + 5 + (i * barWidth) + 1,
                batteryRect.top() + 5,
                barWidth - 2,
                batteryRect.height() - 10);
        }
    }

private:
    int percentage = 0;
};

#endif