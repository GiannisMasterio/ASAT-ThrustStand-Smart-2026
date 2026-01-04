#ifndef THRUSTSTANDGUI_H
#define THRUSTSTANDGUI_H

#include <QMainWindow>
#include <QTimer>
#include <QtSerialPort/QSerialPort>
#include "mqttclient.h"
#include <QFile>
#include "constants.h"
#include "containers.h"
#include "graphcontainer.h"
#include <qlineedit.h>
#include <qframe.h>
#include "qcustomplot.h"
#include <QElapsedTimer>

struct SettingsElements {

    QString labeltext;
    QString initialValue;
    QString placeholder;
    bool isNumeric = false;
    QLabel* label = nullptr;
    QWidget* entry = nullptr; // το κανω QWidget που ειναι πιο γενικο
};

struct GraphDataPoint {
    double time;
    double value;
};

enum IconType { Mode, Scene, EscapeExperiment, Propeller , Settings, Save};

class Containers;

class ThrustStandGUI : public QMainWindow
{
    Q_OBJECT

public:
    explicit ThrustStandGUI(QWidget* parent = nullptr);
    ~ThrustStandGUI() override;

    double min_throttle = MIN_THROTTLE; 
    double max_throttle = MAX_THROTTLE; 
    double max_rpm = MAX_RPM;  
    double rpm_clemency = RPM_CLEMENCY;
    double rpm_div = RPM_DIV;
    double thrust_clemency = THRUST_CLEMENCY;
    double throttle_increase_step = INCREASE_THROTTLE_STEP;
    int stage4_wait_time_ms = STAGE4_WAIT_TIME_MS;
    int thrust_stab_wait_time_ms = THRUST_STAB_WAIT_TIME_MS;

    bool experiment_running = false;

    static QString getHoverStyle(const QString& baseColor);

    QTimer* auto_mode_timer = nullptr;

signals:

    void limitsChanged(double newMinThrottle, double newMaxThrottle, double newMaxRpm, int current_limit, double newRpmClemency, double newRpmStep, double thrust_clemency, int stage4_wait_time_ms, double throttle_increase_step,int thrust_stab_wait_time_ms);
    void mqttSendTopicChanged(const QString& newTopic);

private slots:

    void on_closing();
    void graphicUpdates();
    void updateMqttData();
    void chooseLogLocation();
    void openLogFile();
    void serial_control();
    void confirmPort();
    void escapeExperiment();
    void changeFunctionMode();
    void changeScene();

    void chooseSettings();
    void saveSettings();
    void serial_sendCommand(QString command);

    void onHomeButtonClicked(const QString& mode);

    void onMqttConnected();
    void onMqttDisconnected();
    void onMqttMessageReceived(const QString& message);

    void handleSerial(bool status);
    void readSerialData();

    void processData(const QString& data, QString source);
    void logToFile(bool status);

    void publishMqttMessage(const QString& topic, const QString& payload);

    void onExperimentFinished();

private:
    ModeHandler* auto_mode_handler = nullptr;

    QVector<double> timeHistory, thrustHistory, rpmHistory, voltageHistory,
        currentHistory, powerHistory, throttleHistory;

    const int MAX_POINTS = 2000;

    QElapsedTimer lastSceneChange;
    QElapsedTimer lastFunctionModeChange;
    
    qint64 lastLogTimestamp = 0;

    int logIntervalMs = 500; 

    QVector <QString> propeller_list = { "1 x 16x12E","2 x 16x14","1 x 15x13.5","1 x 15x11","1 x 15x12","2 x 14x14E","2 x 14x14N",
    "2 x 14x14","1 x 14x12","2 x 14x13N","2 x 14x13","2 x 14x13.5","2 x 13x11","1 x 13x13","1 x 13x10E" };
    QStringList aoa_list = { "0", "5", "6", "8", "10", "15" };
    QStringList baudrates = { "300", "1200", "2400", "4800", "9600", "14400", "19200", "38400", "57600", "115200" };

    double current_thrust = 0.0;
    double current_rpm = 0.0;
    double current_voltage = 0.0;
    double current_current = 0.0;
    double current_throttle = min_throttle;
    double current_power = 0.0;

    double thrust_offset = 0.0;
    int current_limit = 50; // ΤΙ ΤΙΜΗ ΔΕ ΠΡΕΠΕΙ ΝΑ ΞΕΠΕΡΑΣΕΙ ΤΟ ΡΕΥΜΑ ΓΙΑ ΝΑ ΜΗ ΚΑΕΙ ΤΟ ESC

    double graphtimeCounter = 0.0; // Για τον άξονα Χ του χρόνου
    int auto_mode_timer_interval_ms = AUTO_MODE_TIMER_INTERVAL_MS;

    QString function_mode; 
    QString mqtt = "MQTT";
    QString serial = "SERIAL";
    QString selected_source = mqtt;

    QPushButton* firstIconButton;
    QString first_icon_path = EXPERIMENT_MODE_ICONPATH;

    QPushButton* secondIconButton;
    QString second_icon_path = GRAPHICONPATH;

    QPushButton* thirdIconButton;
    QString third_icon_path = EXPERIMENT_NOT_RUNNING_ICONPATH;

    QTimer* graphic_timer;
    QTimer* mqtt_data_timer;
   
    GraphContainer* thrustGraph = nullptr;
    GraphContainer* rpmGraph = nullptr;
    GraphContainer* throttleGraph = nullptr;
    GraphContainer* powerGraph = nullptr;
    GraphContainer* voltageGraph = nullptr;
    GraphContainer* currentGraph = nullptr;

    Containers* instantthrustGUI = nullptr;
    Containers* instantcurrentGUI = nullptr;
    Containers* instantvoltageGUI = nullptr;
    Containers* instantthrottleGUI = nullptr;
    Containers* instantrpmGUI = nullptr;
    Containers* instantpowerGUI = nullptr;

    QSerialPort* serial_port = nullptr;
    QString com_port;
    int baudrate = 9600;

    MqttClient* mqtt_client = nullptr;
    QString mqtt_broker = DEFAULT_MQTT_BROKER;
    
    QString mqtt_receive_topic = DEFAULT_MQTT_RECEIVE_TOPIC;
    QString mqtt_send_topic = DEFAULT_MQTT_SEND_TOPIC;
    int mqtt_port = DEFAULT_MQTT_PORT;

    QString propeller_name = DEFAULT_PROPELLER_NAME;
    double angle_of_attack = DEFAULT_AOA;
    double airspeed = DEFAULT_AIRSPEED;

    QString log_file_path = "ASAT ThrustStand Records.csv";
    QString save_file_path = "ASAT ThrustStandSmart Save.txt";
    QFile* log_file = nullptr;
    QFile* save_file = nullptr;

    QFrame* createBottomToolbar();

    void createHomeScene();
    void createMainScene();
    void createGraphScene();

    void loadSettings();

    void set_colors();
    void initLogFile();
    void start_mqtt();
    void disconnect_mqtt();
    void customizeExperiment();
    void start_auto();
    void updateBottomToolbar();
    double getThrottlePercentage(double value) const;

    bool running = true;
    bool process_begin = false;
    QByteArray last_mqtt_data;

    QString current_scene = "MAIN"; // θα υπαρχει η MAIN ΚΑΙ Η GRAPH

protected:
    double target_rpm;
    double desired_throttle;
    void closeEvent(QCloseEvent* event) override;
};

#endif