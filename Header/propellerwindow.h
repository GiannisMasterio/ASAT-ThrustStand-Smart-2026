#ifndef PROPELLERWINDOW_H
#define PROPELLERWINDOW_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include "backendhandler.h"
#include "graphcontainer.h"

struct PropellerData {
    double thrust = 0.0;
    double rpm = 0.0;
    double current = 0.0;
    double voltage = 0.0;
    int throttle = 0.0;
    double power = 0.0;
    double torque = 0.0;
    double efficiency;
    double advance_ratio;
};


class PropellerWindow : public QWidget {
    Q_OBJECT

public:
    explicit PropellerWindow(BackendHandler* backend, QWidget* parent = nullptr);
    ~PropellerWindow();

private slots:
    void loadLogData();
    void loadApcReference();
    void exportGraphs();
    void updateHeaders();

private:
    BackendHandler* backend_handler;

    // UI Elements
    QComboBox* propellerBox;

    struct GraphGroup {
        QComboBox* x_sel;
        QComboBox* y_sel;
        GraphContainer* graph;
    };
    QList<GraphGroup*> groups;

    // Data buffers
    QVector<PropellerData> currentMeanData;
    QVector<PropellerData> apcReferenceData;

    // Helper functions
    void setupUI();
    QVector<PropellerData> calculateMeanValues(const QVector<LogEntry>& rawEntries, double rpm_tolerance = 50.0);
    QVector<PropellerData> parseApcReferenceData(const QString& filepath);
    void refreshPlots();
};

#endif // PROPELLERWINDOW_H