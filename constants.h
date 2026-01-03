#pragma once
#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <QString>
#include <QMap>
#include <QIcon>
#include <QPair>
#include <QDir>
#include <QVector>

constexpr const char* INITIAL_RESOLUTION = "1920x1080";
const int INITIAL_RESOLUTION_ARR[2] = { 1920, 1080 };
constexpr const char* WINDOW_NAME = "Asat ThrustStand Smart";

constexpr const char* ICONPATH = "./assets/AsatIcon.ico";
constexpr const char* ICONPATHPNG = "./assets/AsatIcon.png";
constexpr const char* EXPERIMENT_RUNNING_ICONPATH = "./assets/ExperimentRunningIcon.png";
constexpr const char* EXPERIMENT_NOT_RUNNING_ICONPATH = "./assets/ExperimentNotRunningIcon.png";
constexpr const char* GRAPHICONPATH = "./assets/GraphIcon.png";
constexpr const char* SETTINGSICONPATH = "./assets/SettingsIcon.png";
constexpr const char* PROPELLERICONPATH = "./assets/PropellerIcon.png";
constexpr const char* HOMEICONPATH = "./assets/HomeIcon.png";
constexpr const char* HOMESCENE_BG = "./assets/HomeScreenBG.png";
constexpr const char* MAINSCENE_BG = "./assets/BG2.png";

constexpr const char* DEFAULT_MQTT_BROKER = "127.0.0.1"; 
constexpr const char* DEFAULT_MQTT_SEND_TOPIC = "thrust-toesp";
constexpr const char* DEFAULT_MQTT_RECEIVE_TOPIC = "thrust-fromesp";

constexpr const char* DEFAULT_PROPELLER_NAME = "1 x 16x12E";
constexpr double DEFAULT_AOA = 0;
constexpr double DEFAULT_AIRSPEED = 0;
constexpr int DEFAULT_MQTT_PORT = 1883; 
constexpr int MQTT_DELAY = 10; // ms
constexpr int GRAPHIC_UPDATES_REFRESH_RATE = 80; // den exei nohma na einai mikrotero apo to mqtt delay

constexpr int INCREASE_THROTTLE_STEP = 1;
constexpr double MIN_THROTTLE = 1100.0;
constexpr double MAX_THROTTLE = 2000.0;
constexpr double MAX_RPM = 10000.0;
constexpr int RPM_CLEMENCY = 40;
constexpr int RPM_DIV = 5;
constexpr int THRUST_CLEMENCY = 3;
constexpr int THRUST_WAIT_TIME = 5; 
const int STAGE4_WAIT_TIME_MS = 5000;
const int THRUST_STAB_WAIT_TIME_MS = 3000;

constexpr int RES_WIDTH = 1920;
constexpr int RES_HEIGHT = 1080;

constexpr const char* LOW_COLOR = "red";
constexpr const char* MID_COLOR = "orange";
constexpr const char* HIGH_COLOR = "green";

constexpr const char* THEME_COLOR = "dodgerblue";
constexpr const char* HOME_BUTTON_COLOR = "blue";

constexpr const char* BORDER_COLOR = THEME_COLOR;
constexpr const char* CONTAINER_COLOR = "#363636";
constexpr const char* NEON_GREEN = "#64CC54";
constexpr const char* DARK_GRAY = "gray40";

constexpr int AUTO_MODE_TIMER_INTERVAL_MS = GRAPHIC_UPDATES_REFRESH_RATE; // i set that same to graphic updates timer 

const QMap<QString, QVector<QString>> progress_bar_colors_map = {
    {"Voltage",  {"red", "orange","green"}},
    {"Current",  {"green", "orange","red"}},
    {"Thrust",   {"red", "orange", "green"}},
    {"RPM",      {"red", "orange", "green"}},
    {"Throttle", {"red", "orange","green"}}
};

const QMap<QString, QPair<double, double>> bounds_map = {
    {"Voltage", {10.5, 12.0}},
    {"Current", {12.0, 20.0}},
    {"Thrust", {5.0, 10.0}},
    {"RPM", {3000.0, 8000.0}},
    {"Throttle", {4.0, 12.0}}
};

const QMap<QString, QString> units_map = {
    {"Instant RPM", "rpm"},
    {"Instant Throttle", "%"},
    {"Instant Voltage", "V"},
    {"Instant Current", "A"},
    {"Instant Thrust", "N"},
    {"Instant Power", "W"}
};

const QMap<QString, QString> actual_quantity = {
    {"Instant RPM", "RPM"},
    {"Instant Throttle", "Throttle"},
    {"Instant Voltage", "Voltage"},
    {"Instant Current", "Current"},
    {"Instant Thrust", "Thrust"}
};

const QMap<QString, double> value_limit = {
    {"RPM", 10.0},
    {"Throttle", 100.0},
    {"Voltage", 13.4},
    {"Current", 35.0},
    {"Thrust", 20.0} 
};

#endif 