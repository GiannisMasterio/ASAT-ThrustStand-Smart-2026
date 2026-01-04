#include "graphcontainer.h"
#include "constants.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QFont>

GraphContainer::GraphContainer(const QString& title, const QString& unit, QWidget* parent)
    : QFrame(parent), m_unit(unit) {

    setStyleSheet(QString("QFrame { background-color: %1; border-radius: 15px; border: 1px solid %2; }")
        .arg(CONTAINER_COLOR).arg(BORDER_COLOR));

    setupUI(title);
}

void GraphContainer::setupUI(const QString& title) {
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);

    titleLabel = new QLabel(title, this);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setFont(QFont("Arial", 20, QFont::Bold));
    titleLabel->setStyleSheet("color: white; border: none;");
    layout->addWidget(titleLabel);

    customPlot = new QCustomPlot(this);
    customPlot->setBackground(QBrush(QColor(25, 25, 25))); 
    customPlot->addGraph();
    customPlot->graph(0)->setPen(QPen(QColor(THEME_COLOR), 2)); 

    valueLabel = new QCPItemText(customPlot);
    valueLabel->setPositionAlignment(Qt::AlignTop | Qt::AlignLeft);
    valueLabel->position->setType(QCPItemPosition::ptAxisRectRatio);
    valueLabel->position->setCoords(0.02, 0.02);
    valueLabel->setText("0.00 " + m_unit);
    valueLabel->setFont(QFont("Arial", 18, QFont::Bold));
    valueLabel->setColor(Qt::white);
    valueLabel->setPadding(QMargins(5, 5, 5, 5));


    QPen axisPen(Qt::white);
    customPlot->xAxis->setBasePen(axisPen);
    customPlot->yAxis->setBasePen(axisPen);
    customPlot->xAxis->setTickLabelColor(Qt::white);
    customPlot->yAxis->setTickLabelColor(Qt::white);
    customPlot->yAxis->setLabelColor(Qt::white);
    customPlot->yAxis->setLabel(m_unit);

    customPlot->xAxis->grid()->setVisible(false);
    customPlot->yAxis->grid()->setPen(QPen(QColor(60, 60, 60)));

    layout->addWidget(customPlot);
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

void GraphContainer::setFullData(const QVector<double>& timeData, const QVector<double>& valueData) {
    if (!customPlot || timeData.isEmpty()) return;

    customPlot->graph(0)->setData(timeData, valueData);
    customPlot->graph(0)->rescaleValueAxis(true);

    customPlot->xAxis->setRange(timeData.last(), 10, Qt::AlignRight);
    customPlot->replot();
}