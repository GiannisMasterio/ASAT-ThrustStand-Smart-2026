#pragma once
#include <QWidget>
#include <QPainter>
#include <QMap>
#include <QString>
#include <QColor>
#include <QFont>
#include <functional>
#include <QVBoxLayout>

class ValueGraphic : public QWidget {
public:
    using QWidget::QWidget;
    virtual void setRange(double minV, double maxV) { Q_UNUSED(minV); Q_UNUSED(maxV); }
    virtual void setValue(double v) = 0;
};

class HundaiGaugeGraphic : public ValueGraphic {
public:
    explicit HundaiGaugeGraphic(QWidget* parent = nullptr) : ValueGraphic(parent) {
        setMinimumSize(180, 180);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

    void setRange(double minval, double maxval) override {
        minV = minval;
        maxV = (maxval > minval) ? maxval : minval + 1.0;
        update();
    }

    void setValue(double new_value) override {
        if (qFuzzyCompare(value + 1.0, new_value + 1.0)) return;
        value = new_value;
        update();
    }

    // Τι δείχνει ο αριθμός: τιμή / divisor, με "decimals" δεκαδικά, και η μονάδα από κάτω
    void setDisplay(double divisor, int decimals, const QString& unitText) {
        displayDivisor = (divisor != 0.0) ? divisor : 1.0;
        displayDecimals = decimals;
        unit = unitText;
        update();
    }

    void setAccent(const QColor& c) { color_accent = c; update(); }

    // Από ποιο ποσοστό της κλίμακας και μετά οι γραμμές γίνονται κόκκινες (>= 1.0 = ποτέ)
    void setRedlineStart(double ratio) { redlineStart = ratio; update(); }

    void setTickCount(int n) { tickCount = qMax(10, n); update(); }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        const int side = qMin(width(), height());
        p.translate(width() / 2.0, height() * 0.53);
        p.scale(side / 180.0, side / 180.0);

        const double ratio = qBound(0.0, (value - minV) / (maxV - minV), 1.0);
        const double startDeg = 150.0, spanDeg = 240.0;

        // Λεπτό εξωτερικό τόξο
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(color_accent, 1.6));
        p.drawArc(QRectF(-92, -92, 184, 184), 210 * 16, -240 * 16);

        // Ακτινικές γραμμές
        QColor dim = color_accent;  dim.setAlpha(110);
        const QColor redline("#ef4444");
        for (int i = 0; i <= tickCount; ++i) {
            const double t = double(i) / tickCount;
            const bool lit = (t <= ratio + 1e-9);
            QColor c = lit ? ((t > redlineStart) ? redline : color_accent) : dim;

            p.save();
            p.rotate(startDeg + spanDeg * t);
            p.setPen(QPen(c, 3.0, Qt::SolidLine, Qt::FlatCap));
            p.drawLine(QPointF(60, 0), QPointF(86, 0));
            p.restore();
        }

        // Αριθμός: μικραίνει αυτόματα αν δεν χωράει (π.χ. "1234.56")
        const QString numText = QString::number(value / displayDivisor, 'f', displayDecimals);
        QFont nf;
        nf.setFamilies({ "Bahnschrift", "Segoe UI" });
        nf.setWeight(QFont::DemiBold);
        int px = 46;
        nf.setPixelSize(px);
        while (px > 18 && QFontMetricsF(nf).horizontalAdvance(numText) > 112) {
            nf.setPixelSize(--px);
        }
        p.setFont(nf);
        p.setPen(Qt::white);
        p.drawText(QRectF(-62, -40, 124, 56), Qt::AlignCenter, numText);

        // Μονάδα
        if (!unit.isEmpty()) {
            QFont uf;
            uf.setFamilies({ "Segoe UI" });
            uf.setPixelSize(13);
            uf.setWeight(QFont::DemiBold);
            p.setFont(uf);
            p.setPen(QColor(226, 232, 240));
            p.drawText(QRectF(-62, 18, 124, 20), Qt::AlignCenter, unit);
        }
    }

private:
    double  value = 0.0, minV = 0.0, maxV = 100.0;
    double  displayDivisor = 1.0;
    int     displayDecimals = 1;
    QString unit;
    QColor  color_accent = QColor("#1e90ff");
    double  redlineStart = 2.0;     // 2.0 = ποτέ κόκκινο
    int     tickCount = 60;
};

// Ένα GraphicFactory είναι μια συνάρτηση που παίρνει έναν γονέα (QWidget*) και επιστρέφει ένα καινούριο γραφικό (ValueGraphic*).

using GraphicFactory = std::function<ValueGraphic* (QWidget*)>;  // orisma o parent - > ftiaxnw to graphic factory

inline ValueGraphic* makeGauge(QWidget* parent, double divisor, int decimals,
    const QString& unit, const QColor& accent,
    double redlineStart = 2.0) {
    auto* g = new HundaiGaugeGraphic(parent);
    g->setDisplay(divisor, decimals, unit);
    g->setAccent(accent);
    g->setRedlineStart(redlineStart);
    return g;
}

inline const QMap<QString, GraphicFactory>& containerGraphics() {
    static const QMap<QString, GraphicFactory> registry = [] {
        QMap<QString, GraphicFactory> m;

        m.insert("RPM", [](QWidget* p) -> ValueGraphic* {
            return makeGauge(p, 1.0, 0, "rpm", QColor("#f07b63"), 0.85);
            });
        m.insert("Throttle", [](QWidget* p) -> ValueGraphic* {
            return makeGauge(p, 1.0, 0, "%", QColor("#1e90ff"));
            });

        return m;
        }();
    return registry;
}