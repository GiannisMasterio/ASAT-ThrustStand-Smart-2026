#ifndef BACKENDHANDLER_H
#define BACKENDHANDLER_H

#include "constants.h"
#include "AutoControl.h"
#include "audiohandler.h"
#include <QSerialPort>
#include "varstate.h"

class QWidget;
class QPushButton;
class QSlider;
class QLabel;
class QDialog; 

enum ExperimentStage {
    IncrementRPMTarget,
    AdjustThrottle,
    StabilizeThrust,
    RecordData,
    EmergencyStop ,
    WindTunnelTransitionStage
};

class BackendHandler : public QObject
{
    Q_OBJECT

public:

    explicit BackendHandler(QObject* parent, QObject* defaultValue);
       
    ~BackendHandler() override = default;

    ExperimentStage getExperimentStage() const {
        return static_cast<ExperimentStage>(experiment_stage);
    }

    QSlider* slider;
    QPushButton* slider_opener_button = nullptr;
    QMap<QString, QPair<unsigned int, unsigned int>> mode_value_limits;

    void execute_experiment();
    void resetExperiment();
    void updateTelemetry(QMap<QString, double> value_keeper);
    void executeEmergencyStop();

    TelemetryData tel;
    ExperimentConfig cfg;
    CalibrationConfig cal;
    MqttConfig mqttcfg;

    Controller* active_controller = nullptr;
    StepController* step_controller = nullptr;
    PidController* pid_controller = nullptr;
    ADRCController* adrc_controller = nullptr;

    AudioHandler* audio_handler = nullptr;

    QMap<QString, Controller*> registered_controllers; 

    void registerController(const QString& name, Controller* controller);
    void setActiveController(const QString& name);

    void playAudio(int volume, QString audio_tag);
    bool isSoundPlaying(QString audio);
    void setSoundStatus(bool sound_status);

    void initLogFile();
    void start_mqtt();
    void disconnect_mqtt();
    void change_mqtt_topic();

    void set_function_mode(QString new_function_mode);
    QString get_function_mode();

    QStringList getAvailablePorts();
    bool connectSerial(const QString& portName, int baudrate);
    bool isSerialConnected() const;

    void receiveTelemetry(const QString& data, QString source);

    void logToFile(bool status);

    double getThrottlePercentage(int current_throttle);

    QTimer* auto_mode_timer = nullptr;

    QStringList baudrates = { "300", "1200", "2400", "4800", "9600", "14400", "19200", "38400", "57600", "115200" };

    QString selected_propeller;

    bool experiment_running = false;
    bool log_only_during_experiment = true;
    bool experiment_is_transitioning = false;
    bool experiment_is_static = false;

    int audio_click_sfx_volume = 70;

    QString log_file_path = "ASAT ThrustStand Records.csv";
    QString save_file_path = "ASAT ThrustStandSmart Save.txt";
    QFile* log_file = nullptr;
    QFile* save_file = nullptr;

    QString selected_data_source = "MQTT";

    int logIntervalMs = 500;

    bool running = true;
    bool process_begin = false;
    bool unsaved_settings_changes = false;
    bool is_armed = false;
    bool soundOn = true;

    bool write_logs = false;

    bool use_rpm_filter = false;
    QVector<double> rpm_buffer; //used for rpm filtering
    int rpm_filter_window = 8;

    QSerialPort* serial_port = nullptr;
    QString serial_com_port_tag;

    int serial_baudrate = 9600;

protected:

    QDateTime stage3_start_time;
    QDateTime record_data_wait_time;
    QDateTime initial_delay_start_time;

    bool record_data_wait_time_initialized = false;
    bool initial_delay_initialized = false;

    QVector<double> thrustSampleBuffer;
    QVector<double> transitionSampleBuffer;

    const int MONITOR_DURATION_MS = 3000;

    double slider_stored_value;
    double target_rpm = 0;
    int desired_throttle = MIN_THROTTLE;

    double lower_rpm_bound = 0;
    double upper_rpm_bound = 0;

    QString mode;

    QDialog* slider_window = nullptr; 
    QLabel* slider_value_label = nullptr;

private:
    QString function_mode = "Basic";
    bool experiment_just_started = true;
    bool max_throttle_reached = false;
    bool stabilizing_thrust = false;
    bool target_rpm_reached = false;
    bool ending_transition = false;

    bool stabilizing_transition = false;

    double battery_removed_voltage_value = 204.8;

    int record_data_wait_time_ms = RECORD_DATA_WAIT_TIME_MS;
    int thrust_stab_wait_time_ms = THRUST_STAB_WAIT_TIME_MS;
    int transition_stab_wait_time_ms = 3000;

    QTimer* mqtt_data_timer = nullptr;

    bool use_max_rpm_logic = false;

    double Kp = 0.005;
    double Ki = 0.001; 
    double Kd = 0; // Just Zero for ThrustStand
    double integral_term = 0.0;
    double prev_error = 0.0;

    double b0_val = 0, wc_val = 0, wo_val = 0, min_out = 0, max_out = 0;

    int max_thrust_samples = 0;
    int max_transition_samples = 0;

    int experiment_stage = IncrementRPMTarget; // το αρχικοποιώ σε αυτήν την κατάσταση

    int transition_delay_ms = TRANSITION_DELAY_TIME_MS;

    double calibrate_current(double value);
    double calibrate_thrust(double value);
    double RPMFilter(double new_rpm);

    void createSliderWindow();
    void initialize_stabilizing_thrust();

signals:
    void publishMqtt(const QString& topic, const QString& payload);
    void modeChanged(const QString& newMode);

    void experimentFinished();

    void mqttSendTopicChanged(const QString& newTopic);
    
    void telemetryUpdated();
    void armedStateChanged(bool armed);
    void loggingStateChanged(bool logging);

public slots:

    void manualToggleEvent(int state);
    void handleMode();
    void openSliderWindow();
    void updateSliderValue(int sliderValue);
    void setValue(double newValue) { slider_stored_value = newValue; }

    void updateMqttSendTopic(const QString& newTopic);

    void onMqttConnected();
    void onMqttDisconnected();
    void onMqttMessageReceived(const QString& message);

    void updateMqttData();
    void chooseLogLocation();
    void openLogFile();

    void confirmPort();
    void serial_sendCommand(QString command);
    void handleSerial(bool status);
    void readSerialData();

    void publishMqttMessage(const QString& topic, const QString& payload);

};

#endif
