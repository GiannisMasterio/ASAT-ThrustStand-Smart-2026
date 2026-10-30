#ifndef THRUSTSTANDGUI_H
#define THRUSTSTANDGUI_H

#include <QMainWindow>
#include <QTimer>
#include <QElapsedTimer>
#include <QFrame>
#include "constants.h"
#include "VarState.h"
#include "containers.h"
#include "graphcontainer.h"
#include "qcustomplot.h"
#include "PropellerWindow.h"
#include "BatteryWidget.h"

class BackendHandler;
class Containers;

struct SettingsElements {
    QString labeltext;
    QString initialValue;
    QString placeholder;
    bool isNumeric = false;

    void* targetVar;

    enum Type {Int, Bool, String, Double} type;

    QLabel* label = nullptr;
    QWidget* entry = nullptr;

    SettingsElements(const QString& l, const QString& v, const QString& p, bool numeric, void* target, Type t) : labeltext(l), initialValue(v), placeholder(p),
        isNumeric(numeric),
        targetVar(target),
        type(t) {
    }
};

struct GraphDataPoint {
    double time;
    double value;
};

struct LogEntry {
    double timeInSeconds;
    double thrust, voltage, current, rpm, throttle, power, aoa, airspeed;
    QString propName;
};

enum IconType { Mode, Scene, Logging, EscapeExperiment, Propeller, Tuning, StopMotor, Status, Calibration, Settings, Save };

class ThrustStandGUI : public QMainWindow
{
    Q_OBJECT

public:
    explicit ThrustStandGUI(QWidget* parent = nullptr);
    ~ThrustStandGUI() override;

    static QString getHoverStyle(const QString& baseColor);

    BackendHandler* backend_handler = nullptr;

private slots:
    void on_closing();
    void graphicUpdates();

    void escapeExperiment();
    void changeFunctionMode();
    void changeScene();

    void chooseSettings();
    void saveSettings();

    void serial_control();
    void onExperimentFinished();

private:
    QVector<double> timeHistory, thrustHistory, rpmHistory, voltageHistory,
        currentHistory, powerHistory, throttleHistory;

    const int MAX_POINTS = 2000;

    QElapsedTimer lastSceneChange;
    QElapsedTimer lastFunctionModeChange;

    qint64 lastLogTimestamp = 0;

    double graphtimeCounter = 0.0;
    int auto_mode_timer_interval_ms = AUTO_MODE_TIMER_INTERVAL_MS;

    QString current_scene = "MAIN";

    QPushButton* firstIconButton = nullptr;
    QString first_icon_path = EXPERIMENT_MODE_ICONPATH;

    QPushButton* secondIconButton = nullptr;
    QString second_icon_path = GRAPHICONPATH;

    QPushButton* thirdIconButton = nullptr;
    QString third_icon_path = EXPERIMENT_NOT_RUNNING_ICONPATH;

    QPushButton* statusIconButton = nullptr;
    QString status_icon_path = DISARMED_ICONPATH;

    QPushButton* saveIconButton = nullptr;
    QString save_icon_path = SAVEICON_LOW_OPACITY_PATH;

    QPushButton* stopMotorIconButton = nullptr;
    QString stop_motor_icon_path = STOP_MOTOR_ICONPATH;

    QPushButton* loggingIconButton = nullptr;
    QString logging_icon_path = LOGGING_OFF_ICONPATH;

    QPushButton* controllerTuningIconButton = nullptr;
    QString controller_tuning_icon_path = CONTROLLER_TUNING_ICONPATH;

    QTimer* graphic_timer = nullptr;

    QVector<SettingsElements> settings_fields;
    QVector<SettingsElements> experiment_fields;

    GraphContainer* thrustGraph = nullptr;
    GraphContainer* rpmGraph = nullptr;
    GraphContainer* throttleGraph = nullptr;
    GraphContainer* powerGraph = nullptr;
    GraphContainer* voltageGraph = nullptr;
    GraphContainer* currentGraph = nullptr;
    GraphContainer* torqueGraph = nullptr;

    BatteryWidget* batteryWidget = nullptr;

    Containers* instantthrustGUI = nullptr;
    Containers* instantcurrentGUI = nullptr;
    Containers* instanttorqueGUI = nullptr;
    Containers* instantthrottleGUI = nullptr;
    Containers* instantrpmGUI = nullptr;
    Containers* instantpowerGUI = nullptr;

    QFrame* createBottomToolbar();
    void updateBottomToolbar();
    void updateArmedState(bool new_arm_status);

    void createMainScene();
    void createGraphScene();

    void windowCreate();
    void set_colors();

    void customizeExperiment();
    void start_experiment();

    double getThrottlePercentage(int value);

    void calibrate_parameters();
    void inspect_propeller_logs();
    void tune_controller();
    void stopMotor();

    void setupSettingsFields();
    void setupExperimentFields();
    void loadSettings();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
};

#endif // THRUSTSTANDGUI_H