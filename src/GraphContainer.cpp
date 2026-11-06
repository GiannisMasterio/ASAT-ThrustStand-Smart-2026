#include "graphcontainer.h"
#include "constants.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QFont>
#include <QLinearGradient>

int title_label_size = 20;

GraphContainer::GraphContainer(const QString& title, const QString& unit, QWidget* parent)
    : QFrame(parent), m_unit(unit) {

    setObjectName("GraphCard");
    setStyleSheet(
        "QFrame#GraphCard {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "              stop:0 rgba(30,41,59,200), stop:1 rgba(10,16,30,225));"
        "  border: 1px solid rgba(255,255,255,40);"
        "  border-radius: 24px;"
        "}");

    setupUI(title);
}

void GraphContainer::setupUI(const QString& title) {
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 16, 18, 14);
    layout->setSpacing(6);

    // --- Τίτλος ---
    titleLabel = new QLabel(title.toUpper(), this);
    titleLabel->setAlignment(Qt::AlignCenter);
    QFont tf("Segoe UI", title_label_size, QFont::DemiBold);
    tf.setLetterSpacing(QFont::AbsoluteSpacing, 3);
    titleLabel->setFont(tf);
    titleLabel->setStyleSheet("color: rgba(203,213,225,230); border: none; background: transparent;");
    layout->addWidget(titleLabel);

    // --- Plot: διάφανο, ώστε να φαίνεται η κάρτα ---
    customPlot = new QCustomPlot(this);
    customPlot->setBackground(QBrush(Qt::transparent));
    customPlot->axisRect()->setBackground(QBrush(Qt::transparent));
    customPlot->setStyleSheet("background: transparent; border: none;");
    customPlot->setAttribute(Qt::WA_TranslucentBackground);

    // --- Γραμμή + gradient γέμισμα ---
    customPlot->addGraph();
    QColor lineColor(THEME_COLOR);
    customPlot->graph(0)->setPen(QPen(lineColor, 2.5));

    QLinearGradient fill(0, 0, 0, 1);
    fill.setCoordinateMode(QGradient::ObjectBoundingMode);
    QColor top = lineColor;    top.setAlpha(90);
    QColor bottom = lineColor; bottom.setAlpha(0);
    fill.setColorAt(0, top);
    fill.setColorAt(1, bottom);
    customPlot->graph(0)->setBrush(QBrush(fill));

    // --- Τρέχουσα τιμή μέσα στο γράφημα ---
    valueLabel = new QCPItemText(customPlot);
    valueLabel->setPositionAlignment(Qt::AlignTop | Qt::AlignLeft);
    valueLabel->position->setType(QCPItemPosition::ptAxisRectRatio);
    valueLabel->position->setCoords(0.02, 0.02);
    valueLabel->setText("0.00 " + m_unit);
    valueLabel->setFont(QFont("Segoe UI", 18, QFont::Bold));
    valueLabel->setColor(Qt::white);
    valueLabel->setPadding(QMargins(6, 4, 6, 4));

    // --- Άξονες: λεπτοί, απαλό γκρι ---
    const QColor axisColor(148, 163, 184);
    const QFont  tickFont("Segoe UI", 10);

    for (QCPAxis* ax : { customPlot->xAxis, customPlot->yAxis }) {
        ax->setBasePen(QPen(QColor(255, 255, 255, 40), 1));
        ax->setTickPen(Qt::NoPen);
        ax->setSubTickPen(Qt::NoPen);
        ax->setTickLabelColor(axisColor);
        ax->setLabelColor(axisColor);
        ax->setTickLabelFont(tickFont);
        ax->setTickLength(0, 0);
        ax->setSubTickLength(0, 0);
    }

    customPlot->xAxis->grid()->setVisible(false);
    customPlot->yAxis->grid()->setVisible(true);
    customPlot->yAxis->grid()->setPen(QPen(QColor(255, 255, 255, 28), 1, Qt::DotLine));
    customPlot->yAxis->grid()->setSubGridVisible(false);
    customPlot->yAxis->grid()->setZeroLinePen(Qt::NoPen);

    layout->addWidget(customPlot, 1);
}

void GraphContainer::addDataPoint(double time, double value) {
    if (!customPlot) return;

    data_value = value;
    valueLabel->setText(QString::number(data_value, 'f', 2) + " " + m_unit);

    customPlot->graph(0)->addData(time, value);
    customPlot->graph(0)->rescaleValueAxis(true);

    customPlot->xAxis->setRange(time, 10, Qt::AlignRight);
    customPlot->replot();
}

void GraphContainer::setFullData(const QVector<double>& xData, const QVector<double>& yData) {
    if (!customPlot || xData.isEmpty()) return;

    customPlot->graph(0)->setData(xData, yData);

    customPlot->graph(0)->rescaleAxes();

    customPlot->xAxis->scaleRange(1.1, customPlot->xAxis->range().center());
    customPlot->yAxis->scaleRange(1.1, customPlot->yAxis->range().center());

    valueLabel->setText(QString::number(yData.last(), 'f', 2) + " " + m_unit);

    customPlot->replot();
}

void GraphContainer::setTitle(const QString& title) {
    if (titleLabel) titleLabel->setText(title.toUpper());
}

void GraphContainer::setAxisLabels(const QString& xLabel, const QString& yLabel) {
    if (customPlot) {
        customPlot->xAxis->setLabel(xLabel);
        customPlot->yAxis->setLabel(yLabel);
        customPlot->replot();
    }
}