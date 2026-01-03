#ifndef MODEHANDLER_H
#define MODEHANDLER_H

#include <QObject>
#include <QString>
#include "constants.h"

class QWidget;
class QPushButton;
class QSlider;
class QLabel;
class QDialog; 

class ThrustStandGUI;

class ModeHandler : public QObject
{
    Q_OBJECT

public:

    explicit ModeHandler(QObject* parent,
        double initialValue,
        QObject* defaultValue,
        double target_rpm,
        double desired_throttle,
        ThrustStandGUI* masterGUI);

    ~ModeHandler() override = default;

    QSlider* slider;
    QPushButton* slider_opener_button = nullptr;
    QString getMode() const { return mode; }
    QMap<QString, QPair<double, double>> mode_value_limits;

    double getTargetRpm() const { return target_rpm; }
    double getDesiredThrottle() const { return desired_throttle; }
    double getValue() const { return value; }

    void setMaster(QWidget* masterWidget) { master = masterWidget; }
    void updateLimits(double newMinThrottle, double newMaxThrottle, double newMaxRpm,int newCurrentLimit, double newRpmClemency, double rpm_div, double thrust_clemency, int stage4_wait_time_ms, double newThrottle_increase_step, int newThrust_stab_wait_time_ms);

    void automate(double current_throttle, double current_rpm, double current_current, double current_thrust, int current_limit, QTimer* auto_mode_timer, bool* experiment_running);
    void resetExperiment();

signals:
    void publishMqtt(const QString& topic, const QString& payload);
    void modeChanged(const QString& newMode);

    void experimentFinished();

public slots:

    void manualToggleEvent(int state);
    void handleMode(double current_throttle, double current_rpm);
    void openSliderWindow();
    void updateSliderValue(int sliderValue);
    void setValue(double newValue) { value = newValue; }

    void updateMqttSendTopic(const QString& newTopic);

protected:

    QDateTime stage3_start_time;
    QDateTime stage4_start_time;
    QVector<double> thrustSampleBuffer;
    const int MONITOR_DURATION_MS = 3000;
    QString mqtt_send_topic = DEFAULT_MQTT_SEND_TOPIC;
    bool stage4_start_time_initialized = false;

    double value;
    double target_rpm = 0;
    double desired_throttle;
    double min_throttle = MIN_THROTTLE;
    double max_throttle = MAX_THROTTLE;
    double max_rpm = MAX_RPM;
    double rpm_div = RPM_DIV;
    double rpm_clemency = RPM_CLEMENCY;
    double thrust_clemency = THRUST_CLEMENCY;
    int stage4_wait_time_ms = STAGE4_WAIT_TIME_MS;
    int thrust_stab_wait_time_ms = THRUST_STAB_WAIT_TIME_MS;
    int current_limit = 50;
    double throttle_increase_step = INCREASE_THROTTLE_STEP;

    double lower_rpm_bound = 0;
    double upper_rpm_bound = 0;

    double getThrottlePercentage() const;

    QWidget* master;
    QString mode;

    QDialog* slider_window = nullptr; 
    QLabel* slider_value_label = nullptr;

private:
    bool rpm_reached = false;
    int stage = 1; 
    ThrustStandGUI* m_guiMaster; // με αυτο θα γινει και μια συνδεση connect για update στα slider limits
};

#endif
