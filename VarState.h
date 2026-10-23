#ifndef VARSTATE_H
#define VARSTATE_H

#include <QString>
#include "constants.h"
#include "mqttclient.h"

struct TelemetryData {
    double thrust = 0.0;
    double rpm = 0.0;
    double current = 0.0;
    double voltage = 0.0;
    int throttle = MIN_THROTTLE;
    double power = 0.0;
    double torque = 0.0;
};

struct ExperimentConfig {
    QVector<QString> propeller_list = {
        "12x6","12x10","13x7","10x6","12x7","13x8","10x6","10x6E","10x7", "12x7",
        "13x6.5","13x8e", "1 x 16x12E","2 x 16x14","1 x 15x13.5","1 x 15x11",
        "1 x 15x12","2 x 14x14E","2 x 14x14N","2 x 14x14","1 x 14x12","2 x 14x13N",
        "2 x 14x13","2 x 14x13.5","2 x 13x11","1 x 13x13","1 x 13x10E","12x8e","13x11","11x10e"
    };
    QStringList aoa_list = { "0", "5", "6", "8", "10", "15" };
    QString propeller_name = "12x6";
    double angle_of_attack = DEFAULT_AOA;
    double airspeed = DEFAULT_AIRSPEED;

    double current_limit = 40.0;
    int min_throttle = MIN_THROTTLE;
    int max_throttle = MAX_THROTTLE;
    unsigned int max_rpm = MAX_RPM;
    unsigned int initial_rpm = 0;

    int rpm_clemency = RPM_CLEMENCY;
    int rpm_step = RPM_STEP;
    double thrust_clemency = THRUST_CLEMENCY;
    int throttle_increase_step = INCREASE_THROTTLE_STEP;
    double stabilize_target_threshold = 0.0;

    int record_data_wait_time_ms = RECORD_DATA_WAIT_TIME_MS;
    int thrust_stab_wait_time_ms = THRUST_STAB_WAIT_TIME_MS;
    int transition_stab_wait_time_ms = 3000;
    int transition_delay_ms = TRANSITION_DELAY_TIME_MS;

    bool first_experiment = true;
    bool last_experiment = false;
    bool experiment_is_static = false;
    bool log_only_during_experiment = true;
    bool windtunnel_transitioning = false;
};

struct CalibrationConfig {
    const float current_lsb_user_esp = 0.00003688f;
    float current_lsb_user_selected = 0.00003688f;
    bool manual_current_calibration = false;

    const float esp_actual_tare_offset = 38468.909f;
    const float esp_thrust_calibration_factor = 0.4485f;
    float direction_sign = -1.0f;
    float thrust_calibration_factor = 0.4485f;
    float tare_newton_offset = 38468.909f;
    bool manual_thrust_calibration = false;
    bool auto_tare_thrust_at_begin = false;

    double thrust_offset = 0.0;
    double current_offset = 0.0;

    float gravity = 9.81;
};

struct MqttConfig {
    QString mqtt_receive_topic = DEFAULT_MQTT_RECEIVE_TOPIC;
    QString mqtt_send_topic = DEFAULT_MQTT_SEND_TOPIC;
    int mqtt_port = DEFAULT_MQTT_PORT;
    MqttClient* mqtt_client = nullptr;
    QString mqtt_broker = DEFAULT_MQTT_BROKER;
    QByteArray last_mqtt_data;
};

#endif