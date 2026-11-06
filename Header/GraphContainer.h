#ifndef GRAPHCONTAINER_H
#define GRAPHCONTAINER_H

#include <QFrame>
#include <QString>
#include "qcustomplot.h"

class QVBoxLayout;
class QLabel;

class GraphContainer : public QFrame {
    Q_OBJECT

public:
    explicit GraphContainer(const QString& title, const QString& unit, QWidget* parent = nullptr);
    ~GraphContainer() override = default;

    void setFullData(const QVector<double>& timeData, const QVector<double>& valueData);
    void addDataPoint(double time, double value);
    void setTitle(const QString& title);
    void setAxisLabels(const QString& xLabel, const QString& yLabel);

    QCustomPlot* getPlot() { return customPlot; }

private:
    QCustomPlot* customPlot;
    QLabel* titleLabel;
    QString m_unit;
    QCPItemText* valueLabel;

    double data_value = 0;

    void setupUI(const QString& title);
};

#endif