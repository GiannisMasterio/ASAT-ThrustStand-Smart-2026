#include "thruststandgui.h" 
#include "containers.h"
#include "modehandler.h"
#include "constants.h"
#include <QDebug>
#include <QDateTime>
#include <QFileDialog>
#include <QMessageBox>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStyleFactory>
#include <QScreen>
#include <QPushButton>
#include <QSettings>
#include <QtMath>
#include <QCloseEvent>
#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include <QTextStream>
#include <QFileInfo>
#include <QProcess>
#include <QDir>
#include <QtSerialPort/QSerialPortInfo>
#include <QFile>
#include <QApplication>
#include <QCoreApplication>
#include <QObject>
#include <QDesktopServices>
#include <QUrl>
#include <QtMqtt/QMqttTopicFilter> 
#include <qtoolbutton.h>

QString HOVERED_STYLE = ThrustStandGUI::getHoverStyle(THEME_COLOR);
QString HOVERED_STYLE_HOME = ThrustStandGUI::getHoverStyle(HOME_BUTTON_COLOR);

QString HOVERED_STYLE_SETTINGS = QString(
    "QPushButton { background-color: %1; color: white; font - weight: bold; }"
    "QPushButton:hover { background-color: blue; }"
).arg(THEME_COLOR);

ThrustStandGUI::ThrustStandGUI(QWidget* parent) : QMainWindow(parent)
{
    setWindowTitle(WINDOW_NAME);
    resize(RES_WIDTH, RES_HEIGHT);
    this->setWindowIcon(QIcon(ICONPATH));

    serial_port = new QSerialPort(this);
    graphic_timer = new QTimer(this);
    mqtt_data_timer = new QTimer(this);

    createHomeScene();
    initExcelFile();
    connect(graphic_timer, &QTimer::timeout, this, &ThrustStandGUI::graphicUpdates);
    connect(mqtt_data_timer, &QTimer::timeout, this, &ThrustStandGUI::updateMqttData);
    mqtt_data_timer->start(MQTT_DELAY);
    start_mqtt();
}

ThrustStandGUI::~ThrustStandGUI()
{
    on_closing();
}

void ThrustStandGUI::set_colors() {

    QApplication::setStyle(QStyleFactory::create("Fusion"));

    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::WindowText, Qt::white);
    darkPalette.setColor(QPalette::Base, QColor(25, 25, 25));
    darkPalette.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::Text, Qt::white);
    darkPalette.setColor(QPalette::Button, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::ButtonText, Qt::white);
    darkPalette.setColor(QPalette::Highlight, QColor(66, 134, 244)); // THEME_COLOR equivalent
    darkPalette.setColor(QPalette::HighlightedText, Qt::white);

    QApplication::setPalette(darkPalette);

}

void ThrustStandGUI::closeEvent(QCloseEvent* event) {
    on_closing();
    event->accept();
}

void ThrustStandGUI::on_closing() {
    running = false;
    logToFile(false);
    handleSerial(false);
    disconnect_mqtt();
}

void ThrustStandGUI::start_mqtt() {

    if (mqtt_client) {
        mqtt_client->disconnectFromBroker();
        delete mqtt_client;
        mqtt_client = nullptr;
    }
    mqtt_client = new MqttClient(mqtt_broker, mqtt_port, mqtt_receive_topic, mqtt_send_topic, this);
    // ΣΥΝΔΕΟΥΜΕ ΤΑ SIGNALS ΤΟΥ WRAPPER
    connect(mqtt_client, &MqttClient::onConnected, this, &ThrustStandGUI::onMqttConnected);
    connect(mqtt_client, &MqttClient::onDisconnected, this, &ThrustStandGUI::onMqttDisconnected);
    connect(mqtt_client, &MqttClient::on_mqtt_message, this, &ThrustStandGUI::onMqttMessageReceived);

    mqtt_client->connectToBroker();

}

void ThrustStandGUI::disconnect_mqtt() {

    if (mqtt_client == nullptr) {
        qDebug() << "MQTT client was never initialized.";
        return;
    }

    if (mqtt_client->getState() == QMqttClient::Connected) {

        mqtt_client->unsubscribe(QMqttTopicFilter(mqtt_receive_topic));
        mqtt_client->unsubscribe(QMqttTopicFilter(mqtt_send_topic));

        mqtt_client->disconnectFromHost();

        qDebug() << "Unsubscribed and requested disconnection from MQTT Broker.";
    }
}

void ThrustStandGUI::createHomeScene() {

    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QString home_scene_backgroundStyle = QString("QWidget {"
        "background-image: url(%1);"
        "background-position: center;"
        "background-repeat: no-repeat;"
        "}").arg(HOMESCENE_BG);

    centralWidget->setStyleSheet(home_scene_backgroundStyle);

    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setAlignment(Qt::AlignCenter);

    QWidget* selectionFrame = new QWidget(centralWidget);
    selectionFrame->setStyleSheet("background: transparent;");
    selectionFrame->setAttribute(Qt::WA_TranslucentBackground, true);

    QHBoxLayout* selectionLayout = new QHBoxLayout(selectionFrame);
    selectionLayout->setSpacing(40);

    QPushButton* experiment_mode_Button = new QPushButton("Experiment", selectionFrame);
    experiment_mode_Button->setFixedSize(300, 80);
    connect(experiment_mode_Button, &QPushButton::clicked, this, [this]() { onHomeButtonClicked("Experiment"); });

    QPushButton* basic_mode_Button = new QPushButton("Basic", selectionFrame);
    basic_mode_Button->setFixedSize(300, 80);
    connect(basic_mode_Button, &QPushButton::clicked, this, [this]() { onHomeButtonClicked("Basic"); });

    QString buttonStyle = HOVERED_STYLE_HOME;

    experiment_mode_Button->setStyleSheet(buttonStyle);
    basic_mode_Button->setStyleSheet(buttonStyle);

    selectionLayout->addWidget(experiment_mode_Button);
    selectionLayout->addWidget(basic_mode_Button);

    mainLayout->addStretch(1);
    mainLayout->addWidget(selectionFrame, 0, Qt::AlignCenter);

}

void ThrustStandGUI::onHomeButtonClicked(const QString& mode) {

    function_mode = mode;
    this->createMainScene();

}

QFrame* ThrustStandGUI::createBottomToolbar() {

    QFrame* toolbarFrame = new QFrame();
    toolbarFrame->setFixedHeight(100); 
    toolbarFrame->setFixedWidth(INITIAL_RESOLUTION_ARR[0]);
    toolbarFrame->setStyleSheet(
        "QFrame {"
        "   background-image: none;"          
        "   background-color: #121212;" 
        "   border-top: 1px solid #444;"      
        "   border-bottom: none;"
        "   border-left: none;"
        "   border-right: none;"
        "}"
    );

    QHBoxLayout* toolbarLayout = new QHBoxLayout(toolbarFrame);
    toolbarLayout->setContentsMargins(20, 0, 20, 0);
    toolbarLayout->setSpacing(10);

    auto createIcon = [&](const QString& iconPath) {
        QPushButton* btn = new QPushButton();
        btn->setFixedSize(90, 90);
        btn->setIcon(QIcon(iconPath));
        btn->setIconSize(QSize(70, 70));
        btn->setStyleSheet(
            "QPushButton {"
            "   background: transparent;"
            "   border: none;"
            "   border-radius: 10px;"
            "}"
            "QPushButton:hover {"
            "   background-color: rgba(255, 255, 255, 30);" 
            "}"
        );

        if (iconPath== ICONPATHPNG) QObject::connect(btn, SIGNAL(clicked()), this, SLOT(changeFunctionMode()));

        if (iconPath == second_icon_path) {
            secondIconButton = btn;
            QObject::connect(btn, SIGNAL(clicked()), this, SLOT(changeScene()));
        }
        if (iconPath == third_icon_path) {
            thirdIconButton = btn;
            QObject::connect(btn, SIGNAL(clicked()), this, SLOT(escapeExperiment()));
        }

        if (iconPath == SETTINGSICONPATH) {
            QObject::connect(btn, SIGNAL(clicked()), this, SLOT(chooseSettings()));
        }

        if (iconPath == PROPELLERICONPATH) {
                //QObject::connect(btn, SIGNAL(clicked()), this, SLOT(chooseSettings()));
            }

        return btn;

        };

    toolbarLayout->addWidget(createIcon(ICONPATHPNG));
    toolbarLayout->addWidget(createIcon(second_icon_path));
    toolbarLayout->addWidget(createIcon(third_icon_path));
    //toolbarLayout->addWidget(createIcon(PROPELLERICONPATH));

    toolbarLayout->addStretch(2); 

    auto createBottomButton = [&](const QString& text, const char* slot) {
        QPushButton* btn = new QPushButton(text);
        if (text == "Choose XCel Location")
         btn->setFixedSize(310, 70);
        else btn->setFixedSize(270, 70);
        btn->setStyleSheet(HOVERED_STYLE);
        QObject::connect(btn, SIGNAL(clicked()), this, slot);
        return btn;
        };

    toolbarLayout->addWidget(createBottomButton("Open Excel File", SLOT(openExcelFile())));
    toolbarLayout->addWidget(createBottomButton("Choose XCel Location", SLOT(chooseExcelLocation())));
    toolbarLayout->addWidget(createBottomButton("Choose Com Port", SLOT(chooseComPort())));
    toolbarLayout->addWidget(createIcon(SETTINGSICONPATH));

    return toolbarFrame;
}

void ThrustStandGUI::escapeExperiment() {

     if (experiment_running) {
        experiment_running = false;
        this->updateBottomToolbar();
     } 

}

void ThrustStandGUI::updateBottomToolbar() {

    if (current_scene == "GRAPH") {
        second_icon_path = HOMEICONPATH;
    }
    else {
        second_icon_path = GRAPHICONPATH;
    }

    if (secondIconButton) {
        secondIconButton->setIcon(QIcon(second_icon_path));
    }

    if (experiment_running) {
        third_icon_path = EXPERIMENT_RUNNING_ICONPATH;
    }
    else {
        third_icon_path = EXPERIMENT_NOT_RUNNING_ICONPATH;
    }

    if (thirdIconButton) {
        thirdIconButton->setIcon(QIcon(third_icon_path));
    }
}

void ThrustStandGUI::createMainScene() {
    graphic_timer->stop();

    if (centralWidget()) {
        secondIconButton = nullptr; 
        thirdIconButton = nullptr;
        delete centralWidget();
    }
    
    QWidget* centralWidget = new QWidget(this);
    centralWidget->setObjectName("CentralWidget");
    setCentralWidget(centralWidget);

    QString backgroundStyle = QString(
        "QWidget#CentralWidget {" 
        "background-image: url(%1);"
        "background-position: center;"
        "background-repeat: no-repeat;"
        "}").arg(MAINSCENE_BG);

    centralWidget->setStyleSheet(backgroundStyle);

    QVBoxLayout* mainVLayout = new QVBoxLayout(centralWidget);
    mainVLayout->setContentsMargins(0, 0, 0, 0);
    mainVLayout->setSpacing(0);

    QWidget* topSection = new QWidget(centralWidget);
    topSection->setStyleSheet("background: transparent;");
    topSection->setContentsMargins(50, 35, 50, 0);

    QGridLayout* gridLayout = new QGridLayout(topSection);
    gridLayout->setSpacing(20);

    instantthrustGUI = new Containers("Instant Thrust", 71, "white", current_thrust, true, function_mode, this, topSection);
    instantcurrentGUI = new Containers("Instant Current", 70, "white", current_current, true, function_mode, this, topSection);
    instantvoltageGUI = new Containers("Instant Voltage", 70, "white", current_voltage, true, function_mode, this, topSection);
    instantrpmGUI = new Containers("Instant RPM", 69, "white", current_rpm, false, function_mode, this, topSection);
    instantthrottleGUI = new Containers("Instant Throttle", 74, "white", current_throttle, false, function_mode, this, topSection);
    instantpowerGUI = new Containers("Instant Power", 70, "white", current_power, false, function_mode, this, topSection);

   
    // απαραιτητο connect για να στελνονται τα μυνηματα
    if (instantthrottleGUI->getModeHandler()) {
        connect(instantthrottleGUI->getModeHandler(), &ModeHandler::publishMqtt,
           mqtt_client, &MqttClient::publish);
    }

    gridLayout->addWidget(instantvoltageGUI, 0, 0);
    gridLayout->addWidget(instantcurrentGUI, 1, 0);
    gridLayout->addWidget(instantpowerGUI, 2, 0);

    gridLayout->setColumnMinimumWidth(1, 40);

    gridLayout->addWidget(instantthrustGUI, 0, 2);
    gridLayout->addWidget(instantthrottleGUI, 1, 2);
    gridLayout->addWidget(instantrpmGUI, 2, 2);

    gridLayout->setRowStretch(0, 1);
    gridLayout->setRowStretch(1, 1);
    gridLayout->setRowStretch(2, 1);

    gridLayout->setColumnStretch(1, 1);

    mainVLayout->addWidget(topSection, 1);

    QFrame* bottomBar = createBottomToolbar();
    mainVLayout->addWidget(bottomBar);

    graphic_timer->start(GRAPHIC_UPDATES_REFRESH_RATE);

    if (function_mode == "Experiment" && !experiment_running) {
        customizeExperiment();
    }
}
 
void ThrustStandGUI::changeScene() {
    
    if (lastSceneChange.isValid() && lastSceneChange.elapsed() < 100) {
        return;
    }
    lastSceneChange.start();
    
    if (graphic_timer) graphic_timer->stop();
    
    if (current_scene == "MAIN") {
        current_scene = "GRAPH";
        this->createGraphScene();

        instantthrustGUI = nullptr;
        instantcurrentGUI = nullptr;
        instantvoltageGUI = nullptr;
        instantrpmGUI = nullptr;
        instantthrottleGUI = nullptr;
        instantpowerGUI = nullptr;
    }

    else if (current_scene == "GRAPH") {
        current_scene = "MAIN";
        this->createMainScene();
        voltageGraph = nullptr;
        currentGraph = nullptr;
        powerGraph = nullptr;
        thrustGraph = nullptr;
        throttleGraph = nullptr;
        rpmGraph = nullptr;
    }

    updateBottomToolbar();
}

void ThrustStandGUI::changeFunctionMode() {
    if(lastFunctionModeChange.isValid() && lastFunctionModeChange.elapsed() < 100) {
        return;
    }
    lastFunctionModeChange.start();

    if (current_scene != "GRAPH") {

        if (function_mode == "Basic") {
            function_mode = "Experiment";
            this->createMainScene();
        }

        else if (function_mode == "Experiment") {
            function_mode = "Basic";
            this->createMainScene();
        }

    }
}

void ThrustStandGUI::createGraphScene() {

    QWidget* centralWidget = new QWidget(this);
    centralWidget->setObjectName("CentralWidget");
    setCentralWidget(centralWidget);

    QString backgroundStyle = QString(
        "QWidget#CentralWidget {"
        "background-image: url(%1);"
        "background-position: center;"
        "background-repeat: no-repeat;"
        "}").arg(MAINSCENE_BG);

    centralWidget->setStyleSheet(backgroundStyle);

    QVBoxLayout* mainVLayout = new QVBoxLayout(centralWidget);
    mainVLayout->setContentsMargins(0, 0, 0, 0);
    mainVLayout->setSpacing(0);

    QWidget* chartsArea = new QWidget(centralWidget);
    chartsArea->setStyleSheet("background: transparent;");
    QGridLayout* chartsGrid = new QGridLayout(chartsArea);

    chartsArea->setContentsMargins(50, 20, 50, 20);
    chartsGrid->setSpacing(15);

    voltageGraph = new GraphContainer("Instant Voltage", "V", chartsArea);
    currentGraph = new GraphContainer("Instant Current", "A", chartsArea);
    powerGraph = new GraphContainer("Instant Power", "W", chartsArea);
    thrustGraph = new GraphContainer("Instant Thrust", "N", chartsArea);
    throttleGraph = new GraphContainer("Instant Throttle", "%", chartsArea);
    rpmGraph = new GraphContainer("Instant RPM", "rpm", chartsArea);

    // buffer fill
    voltageGraph->setFullData(timeHistory, voltageHistory);
    currentGraph->setFullData(timeHistory, currentHistory);
    powerGraph->setFullData(timeHistory, powerHistory);
    thrustGraph->setFullData(timeHistory, thrustHistory);
    throttleGraph->setFullData(timeHistory, throttleHistory);
    rpmGraph->setFullData(timeHistory, rpmHistory);

    chartsGrid->addWidget(voltageGraph, 0, 0);
    chartsGrid->addWidget(currentGraph, 1, 0);
    chartsGrid->addWidget(powerGraph, 2, 0);
    chartsGrid->addWidget(thrustGraph, 0, 1);
    chartsGrid->addWidget(throttleGraph, 1, 1);
    chartsGrid->addWidget(rpmGraph, 2, 1);

    chartsGrid->setRowStretch(0, 1);
    chartsGrid->setRowStretch(1, 1);
    chartsGrid->setRowStretch(2, 1);
    chartsGrid->setColumnStretch(0, 1);
    chartsGrid->setColumnStretch(1, 1);

    mainVLayout->addWidget(chartsArea, 1);

    QFrame* bottomBar = createBottomToolbar();
    mainVLayout->addWidget(bottomBar);

    if (graphic_timer && !graphic_timer->isActive()) {
        graphic_timer->start(GRAPHIC_UPDATES_REFRESH_RATE);
    }
}

double ThrustStandGUI::getThrottlePercentage(double value) const {
    return (double)(100.0 * (value - min_throttle) / (max_throttle - min_throttle));
}

void ThrustStandGUI::publishMqttMessage(const QString& topic, const QString& payload) {
    if (mqtt_client) {

        qint32 id = mqtt_client->publish(topic, payload); // Αυτό καλεί τη μέθοδο του wrapper!

        if (id != -1) {
            qDebug() << "MQTT PUBLISHED via Wrapper: Topic=" << topic << ", Payload=" << payload << ", ID=" << id;
        }
        else {
            qWarning() << "ERROR publishing via Wrapper. Client state is likely disconnected.";
        }
    }
    else {
        qWarning() << "MQTT Client Wrapper not initialized. Message not sent.";
    }
}

void ThrustStandGUI::onMqttConnected() {
    qDebug() << "Connected to MQTT Broker:" << mqtt_broker << ":" << mqtt_port;
}

void ThrustStandGUI::onMqttDisconnected() {
    qDebug() << "Disconnected from MQTT Broker.";
}

void ThrustStandGUI::onMqttMessageReceived(const QString& data) {
    if (!process_begin) {
        process_begin = true;
    }
  
    last_mqtt_data = data.toUtf8();
    qDebug() << "received (MQTT String Data):" << data;
}

void ThrustStandGUI::updateMqttData() {
    if (!last_mqtt_data.isEmpty()) {
        processData(QString::fromUtf8(last_mqtt_data), mqtt);
        last_mqtt_data.clear();
    }
}

void ThrustStandGUI::processData(const QString& data, QString source) {

    if (source != selected_source) return;

    if (selected_source == mqtt) {

        QString cleanData = data.trimmed().replace(" ", "");
        QMap<QString, double> values;
        QStringList parts = cleanData.split(",");

        for (const QString& part : parts) {
            QStringList pair = part.split(":");
            if (pair.size() == 2) {
                bool ok;
                double val = pair[1].toDouble(&ok);
                if (ok) {
                    values[pair[0]] = val;
                }
            }
        }

        current_thrust = values.value("Thrust", current_thrust) + thrust_offset;
        current_current = values.value("Current", current_current);
        current_voltage = values.value("Voltage", current_voltage);
        current_throttle = values.value("Throttle", current_throttle);
        current_rpm = values.value("RPM", current_rpm);
        

    }
    else {
        QString rawData = data.trimmed();
        QString cleanData = rawData;

        cleanData.replace("V = ", "V:");
        cleanData.replace("I = ", "I:");
        cleanData.replace(" V,", ",");
        cleanData.replace(" A,", ",");
        cleanData.replace(" ", "");

        qDebug() << "FIXED CLEAN LINE:" << cleanData;

        QMap<QString, double> values;
        QStringList parts = cleanData.split(",");

        for (const QString& part : parts) {
            QStringList pair = part.split(":");
            if (pair.size() >= 2) {
                bool ok;
                double val = QLocale::c().toDouble(pair[1], &ok);
                if (ok) {
                    values[pair[0]] = val;
                }
            }
        }

        if (!values.isEmpty()) {
            current_thrust = values.value("Thrust", current_thrust) + thrust_offset;
            current_throttle = values.value("Throttle", current_throttle);
            current_rpm = values.value("RPM", current_rpm);
            current_voltage = values.value("V", current_voltage);
            current_current = values.value("I", current_current);


            if (!process_begin) process_begin = true;
        }
    }

    current_power = current_voltage * current_current;

    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();

    if (!process_begin) {
        process_begin = true;
        lastLogTimestamp = currentTime;
        logToFile(true);
        return;
    }

    if (currentTime - lastLogTimestamp >= logIntervalMs) {
        logToFile(true);
        lastLogTimestamp = currentTime; 
    }
    
}

void ThrustStandGUI::logToFile(bool status) {

    if (status) {
        int label_width = 30;
        if (log_file == nullptr) {
            log_file = new QFile(excel_file_path, this );
           
            bool file_exists = log_file->exists();
            if (!log_file->open(QIODevice::Append | QIODevice::Text)) {
                qWarning() << "Error opening log file for appending:" << log_file->errorString();
                delete log_file;
                log_file = nullptr;
                return;
            }
            
            QTextStream out(log_file);
            if (!file_exists || log_file->size() == 0) {
                out << QString("%1 %2 %3 %4 %5 %6 %7 %8 %9 %10\n")
                    .arg("Timestamp", label_width)
                    .arg("Thrust (N)", label_width)
                    .arg("Voltage (V)", label_width)
                    .arg("Current (A)", label_width)
                    .arg("RPM", label_width)
                    .arg("Throttle (%)", label_width)
                    .arg("Power (W)", label_width)
                    .arg("Propeller Name", label_width)
                    .arg("Angle of Attack (degrees)", label_width)
                    .arg("Airspeed (m/sec)", label_width);
            }
        }

        QTextStream out(log_file);
        QString timestamp = QDateTime::currentDateTime().toString("HH:mm:ss");

        out << QString("%1 %2 %3 %4 %5 %6 %7 %8 %9 %10\n")
            .arg(timestamp, label_width)
            .arg(current_thrust, label_width, 'f', 2)
            .arg(current_voltage, label_width, 'f', 2)
            .arg(current_current, label_width, 'f', 2)
            .arg(current_rpm, label_width, 'f', 0)
            .arg(getThrottlePercentage(current_throttle), label_width, 'f', 2)
            .arg(current_power, label_width, 'f', 2)
            .arg(propeller_name, label_width)
            .arg(angle_of_attack, label_width, 'f', 2)
            .arg(airspeed, label_width, 'f', 2);

        log_file->flush();

    }
    else if (log_file != nullptr) {
        log_file->close();
        delete log_file;
        log_file = nullptr;
        qDebug() << "Log file closed.";
    }
}

void ThrustStandGUI::graphicUpdates() {

    if (!process_begin) return;

    if (process_begin) {

        double currentTime = graphtimeCounter + ((double)GRAPHIC_UPDATES_REFRESH_RATE / 1000.0);
        graphtimeCounter = currentTime;

        auto appendData = [&](QVector<double>& buffer, double value) {
            buffer.append(value);
            if (buffer.size() > MAX_POINTS) buffer.removeFirst();
            };

        timeHistory.append(currentTime);
        if (timeHistory.size() > MAX_POINTS) timeHistory.removeFirst();

        appendData(thrustHistory, current_thrust);
        appendData(rpmHistory, current_rpm);
        appendData(voltageHistory, current_voltage);
        appendData(currentHistory, current_current);
        appendData(powerHistory, current_power);
        appendData(throttleHistory, current_throttle);

        if (current_scene == "MAIN" && instantthrustGUI != nullptr) {

            instantthrustGUI->updateData(current_thrust, current_rpm);
            instantcurrentGUI->updateData(current_current, current_rpm);
            instantvoltageGUI->updateData(current_voltage, current_rpm);
            instantthrottleGUI->updateData(current_throttle, current_rpm);
            instantrpmGUI->updateData(current_rpm, current_rpm);
            instantpowerGUI->updateData(current_power, current_rpm);
        }

        else if (current_scene == "GRAPH" && thrustGraph != nullptr) {
            graphtimeCounter += (double)GRAPHIC_UPDATES_REFRESH_RATE / 1000.0;

            thrustGraph->addDataPoint(graphtimeCounter, current_thrust);
            rpmGraph->addDataPoint(graphtimeCounter, current_rpm);
            voltageGraph->addDataPoint(graphtimeCounter, current_voltage);
            currentGraph->addDataPoint(graphtimeCounter, current_current);
            powerGraph->addDataPoint(graphtimeCounter, current_power);
            throttleGraph->addDataPoint(graphtimeCounter, current_throttle);
        }

    }
}

void ThrustStandGUI::handleSerial(bool status) {
    if (status) {
        if (serial_port->isOpen()) {
            serial_port->close();
            qDebug() << "Closed previous serial port.";
        }

        serial_port->setPortName(com_port);
        serial_port->setBaudRate(baudrate);

        if (serial_port->open(QIODevice::ReadOnly)) {
            connect(serial_port, &QSerialPort::readyRead, this, &ThrustStandGUI::readSerialData);
            qDebug() << "Opened serial port" << com_port << "at" << baudrate;
        }
        else {
            qWarning() << "Cannot open port" << com_port << ":" << serial_port->errorString();
        }
    }
    else {
        if (serial_port->isOpen()) {
            serial_port->close();
            qDebug() << "Serial port closed.";
        }
    }
}

void ThrustStandGUI::readSerialData() {
    
    while (serial_port->canReadLine()) {
        QByteArray data = serial_port->readLine();
        if (!data.isEmpty()) {
            QString serialData = QString::fromUtf8(data).trimmed();
            qDebug() << "Full Line Received:" << serialData;
            processData(serialData, "SERIAL");
        }
    }
}

void ThrustStandGUI::chooseComPort() {

    QWidget* selection_window = new QWidget(nullptr, Qt::Window);
    selection_window->setWindowTitle("Select COM Port");
    selection_window->setFixedSize(600, 400);
    selection_window->setWindowModality(Qt::ApplicationModal);
    selection_window->setAttribute(Qt::WA_DeleteOnClose);
    selection_window->setWindowIcon(QIcon(ICONPATH));


    QVBoxLayout* layout = new QVBoxLayout(selection_window);

    QLabel* label1 = new QLabel("Available COM Ports:", selection_window);
    QComboBox* dropdown1 = new QComboBox(selection_window);

    QStringList ports;
    for (const QSerialPortInfo& info : QSerialPortInfo::availablePorts()) {
        ports << info.portName();
    }
    dropdown1->addItems(ports);
    if (!com_port.isEmpty() && ports.contains(com_port)) {
        dropdown1->setCurrentText(com_port);
    }

    QLabel* label2 = new QLabel("Choose Baudrate:", selection_window);
    QComboBox* dropdown2 = new QComboBox(selection_window);
    
    dropdown2->addItems(baudrates);
    dropdown2->setCurrentText(QString::number(baudrate));

    QPushButton* confirmButton = new QPushButton("Select", selection_window);
    confirmButton->setStyleSheet(HOVERED_STYLE);

    layout->addWidget(label1);
    layout->addWidget(dropdown1);
    layout->addWidget(label2);
    layout->addWidget(dropdown2);
    layout->addWidget(confirmButton);

    connect(confirmButton, &QPushButton::clicked, this, [this, dropdown1, dropdown2, selection_window]() {
        com_port = dropdown1->currentText();
        baudrate = dropdown2->currentText().toInt();
        confirmPort();
        selection_window->close();
        });

    selection_window->show();
}

void ThrustStandGUI::confirmPort() {
    qDebug() << "Selected COM port:" << com_port;
    qDebug() << "Selected baudrate:" << baudrate;
    handleSerial(running);
}

void ThrustStandGUI::chooseSettings() {

    QWidget* settings_window = new QWidget(nullptr, Qt::Window);
    settings_window->setWindowTitle("Settings");
    settings_window->setFixedSize(500, 800);
    settings_window->setWindowModality(Qt::ApplicationModal);
    settings_window->setAttribute(Qt::WA_DeleteOnClose);
    settings_window->setWindowIcon(QIcon(ICONPATH));

    QVBoxLayout* layout = new QVBoxLayout(settings_window);

    QVector<SettingsElements> fields = {
            {"Data Source:", mqtt , "", false},
            {"Receive Topic (Subscription):", mqtt_receive_topic, "", false},
            {"Send Topic (Publishing):", mqtt_send_topic, "", false},
            {"Propeller Name:", propeller_name, "", false},
            {"Angle Of Attack (deg):", QString::number(angle_of_attack), "Enter AOA in degrees", true},
            {"Airspeed (m/sec):", QString::number(airspeed), "Enter airspeed in m/sec", true},
            {"Min Throttle (us):", QString::number(min_throttle), "", true},
            {"Max Throttle (us):", QString::number(max_throttle), "", true},
            {"Max RPM:", QString::number(max_rpm), "", true},
            {"RPM Clemency:", QString::number(rpm_clemency), "", true},
            {"Throttle Increase Step:", QString::number(throttle_increase_step), "", true},
            {"Thrust Stabilizing WaitTime (ms):", QString::number(thrust_stab_wait_time_ms), "", true},
            {"Wait After Thrust (ms):", QString::number(stage4_wait_time_ms), "", true},
            {"Current Limit (A):", QString::number(current_limit), "", true},
            {"Thrust Offset (N):", QString::number(thrust_offset), "", true}
            

    };

    QPushButton* confirmButton = new QPushButton("Confirm Settings", settings_window);

    for (auto& field : fields) { 

        field.label = new QLabel(field.labeltext, settings_window);

        if (field.labeltext != "Data Source:") {
            QLineEdit* lineEdit= new QLineEdit(field.initialValue, settings_window);
            if (field.placeholder != "") {
                lineEdit->setPlaceholderText(field.placeholder);
            }
            field.entry = lineEdit;
        }
        else {
            QComboBox* comboBox = new QComboBox(settings_window);
            comboBox->addItem("MQTT");
            comboBox->addItem("SERIAL");

            int currentIndex = comboBox->findText(selected_source);
            if (currentIndex != -1) {
                comboBox->setCurrentIndex(currentIndex);
            }

            field.entry = comboBox;
        }
        
        layout->addWidget(field.label);
        layout->addWidget(field.entry);
    }

    confirmButton->setStyleSheet(HOVERED_STYLE_SETTINGS);
    layout->addWidget(confirmButton);


    connect(confirmButton, &QPushButton::clicked, this,
        [this,fields, settings_window]() mutable { 

           auto findEntryByLabel = [&](const QString& label) -> QWidget* {
                for (const auto& field : fields) {
                    if (field.labeltext == label) {
                        return field.entry;
                    }
                }
                return nullptr;
                };

           auto getEntryText = [&](const QString& label)-> QString {

                for (const auto& field : fields) {
                    if (field.labeltext == label && field.labeltext != "Data Source:") {
                       // επειδη το field.entry einai πιο γενικου τυπου Widget, που δεν εχει γενικα το text().trimmed() method kanw explicit casting
                       // pairnw ena deikth field.entry typoy QWidget* kai to metatrepw se pointer se QLineEdit
                       // an den einai QLineEdit sthn pragmatikothta pairnw nullptr
                       QLineEdit* entry = qobject_cast<QLineEdit*>(field.entry);
                       return entry->text().trimmed();
                       }
                       } 
                return "";
                   
                }; 

           QString new_receive_topic = getEntryText("Receive Topic (Subscription):");
           QString new_send_topic = getEntryText("Send Topic (Publishing):");
           QString new_propeller_name = getEntryText("Propeller Name:");

           QWidget* data_source_widget = findEntryByLabel("Data Source:");
           QComboBox* comboBox = qobject_cast<QComboBox*>(data_source_widget);

           if (comboBox->currentText() != selected_source) {
               selected_source = comboBox->currentText();
               current_thrust = 0;
               current_power = 0; 
               current_voltage = 0;
               current_rpm = 0;
               current_current = 0;
               current_throttle = 0;

           }
           
           double new_aoa = getEntryText("Angle Of Attack (deg):").toDouble();
           double new_airspeed = getEntryText("Airspeed (m/sec):").toDouble();
           double new_min_throttle = getEntryText("Min Throttle (us):").toDouble();
           double new_max_throttle = getEntryText("Max Throttle (us):").toDouble();
           double new_max_rpm = getEntryText("Max RPM:").toDouble();
           double new_rpm_clemency = getEntryText("RPM Clemency:").toDouble();
           double new_throttle_increase_step = getEntryText("Throttle Increase Step:").toDouble();
           int new_stage4_wait_time_ms = getEntryText("Wait After Thrust (ms):").toInt();
           int new_thrust_stab_wait_time_ms = getEntryText("Thrust Stabilizing WaitTime (ms):").toInt();
           int new_current_limit = getEntryText("Current Limit (A):").toInt();
           double new_thrust_offset = getEntryText("Thrust Offset (N):").toDouble();

           if (new_thrust_offset != thrust_offset) {
               current_thrust -= thrust_offset;
               thrust_offset = new_thrust_offset;
               current_thrust += thrust_offset;
           }

           if (new_receive_topic.isEmpty() || new_send_topic.isEmpty()) {
                QMessageBox::warning(settings_window, "Error", "MQTT Topics cannot be empty.");
                return;
           }

           angle_of_attack = new_aoa;
           airspeed = new_airspeed;
           propeller_name = new_propeller_name;
           min_throttle = new_min_throttle;
           max_throttle = new_max_throttle;
           max_rpm = new_max_rpm;
           rpm_clemency = new_rpm_clemency;
           stage4_wait_time_ms = new_stage4_wait_time_ms;
           throttle_increase_step = new_throttle_increase_step;
           thrust_stab_wait_time_ms = new_thrust_stab_wait_time_ms;
           current_limit = new_current_limit;
            
           emit limitsChanged(min_throttle, max_throttle, max_rpm,current_limit, rpm_clemency, rpm_div, thrust_clemency, stage4_wait_time_ms, throttle_increase_step, thrust_stab_wait_time_ms); // εκμπεμπεται ενα σημα

           if (new_receive_topic != mqtt_receive_topic || new_send_topic != mqtt_send_topic) {

                disconnect_mqtt();
                mqtt_receive_topic = new_receive_topic;
                mqtt_send_topic = new_send_topic;
                emit mqttSendTopicChanged(mqtt_send_topic);
                mqtt_client->connectToBroker();
                qDebug() << "Topics changed. Attempting reconnect.";
           }

           settings_window->close();
        });

    settings_window->show();
}

void ThrustStandGUI::initExcelFile() {
    QString fullPath = excel_file_path;

    if (!QFileInfo(fullPath).isAbsolute() || fullPath.isEmpty()) {
        fullPath = QCoreApplication::applicationDirPath() + QDir::separator() + "ASAT ThrustStand Records.csv";
    }

    QFile file(fullPath);

    if (!file.exists()) {
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << "                     Timestamp                  Thrust (N)               Voltage (V)                Current (A)               RPM               Throttle (%)                  Power (W)              Propeller Name            AOA (deg)           Airspeed (m/s)\n";
            file.close();
            qDebug() << "Created initial CSV log file:" << fullPath;
        }
    }

    excel_file_path = fullPath;
}

void ThrustStandGUI::chooseExcelLocation() {

    QString initialDir = QFileInfo(excel_file_path).path();
    QString newFilePath = QFileDialog::getSaveFileName(
        this,
        "Select Log File Location",
        initialDir, 
        "CSV Log (*.csv);;Text Log (*.txt);;All files (*.*)"
    );

    if (!newFilePath.isEmpty()) {
        if (!newFilePath.endsWith(".csv", Qt::CaseInsensitive) && !newFilePath.endsWith(".txt", Qt::CaseInsensitive)) {
            newFilePath += ".csv";
        }

        excel_file_path = newFilePath;

        initExcelFile();
        qDebug() << "New log file location set to:" << excel_file_path;
    }
}

void ThrustStandGUI::openExcelFile()
{
    QString fullFilePath = excel_file_path;

    if (!QFileInfo(fullFilePath).isAbsolute() || fullFilePath.isEmpty()) {
        fullFilePath = QCoreApplication::applicationDirPath() + QDir::separator() + "ASAT ThrustStand Records.csv";
    }

    if (!QFile::exists(fullFilePath)) {
        QMessageBox::warning(this, "File Not Found",
            "Log file does not exist at: " + fullFilePath);
        return;
    }

    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(fullFilePath))) {
        QMessageBox::critical(this, "Error",
            "Could not open file with the system default application.");
    }
}

void ThrustStandGUI::customizeExperiment() {

    QWidget* settings_window = new QWidget(nullptr, Qt::Window);
    settings_window->setWindowTitle("Experiment Settings");
    settings_window->setFixedSize(500, 600);
    settings_window->setWindowModality(Qt::ApplicationModal);
    settings_window->setAttribute(Qt::WA_DeleteOnClose);
    settings_window->setWindowIcon(QIcon(ICONPATH));

    QVBoxLayout* layout = new QVBoxLayout(settings_window);

    QVector<SettingsElements> fields = {
            {"Propeller Name:", propeller_name, "", false},
            {"Angle Of Attack (deg):", QString::number(angle_of_attack), "", false},
            {"Airspeed (m/sec):", QString::number(airspeed), "Enter airspeed in m/sec", true},
            {"Thrust Clemency:", QString::number(thrust_clemency), "", true},
            {"RPM Division:", QString::number(rpm_div), "", true},
            {"Max RPM:", QString::number(max_rpm), "", true},
            {"RPM Clemency:", QString::number(rpm_clemency), "", true},
            {"Throttle Increase Step:", QString::number(throttle_increase_step), "", true},
            {"Thrust Stabilizing WaitTime (ms):", QString::number(thrust_stab_wait_time_ms), "", true},
            {"Wait After Thrust (ms):", QString::number(stage4_wait_time_ms), "", true}
    };

    QPushButton* confirmButton = new QPushButton("Confirm Settings", settings_window);

    for (auto& field : fields) {
        field.label = new QLabel(field.labeltext, settings_window);

        if (field.labeltext == "Propeller Name:") {
            QComboBox* propellerBox = new QComboBox(settings_window);
            for (const auto& prop : propeller_list) propellerBox->addItem(prop);

            int idx = propellerBox->findText(propeller_name);
            if (idx != -1) propellerBox->setCurrentIndex(idx);
            field.entry = propellerBox;
        }
        else if (field.labeltext == "Angle Of Attack (deg):") {
            QComboBox* aoaBox = new QComboBox(settings_window);
            for (const auto& val : aoa_list) aoaBox->addItem(val);

            int idx = aoaBox->findText(QString::number(angle_of_attack));
            if (idx != -1) aoaBox->setCurrentIndex(idx);
            field.entry = aoaBox;
        }
        else {
            QLineEdit* lineEdit = new QLineEdit(field.initialValue, settings_window);
            if (!field.placeholder.isEmpty()) lineEdit->setPlaceholderText(field.placeholder);
            field.entry = lineEdit;
        }

        layout->addWidget(field.label);
        layout->addWidget(field.entry);
    }

    confirmButton->setStyleSheet(HOVERED_STYLE_SETTINGS);
    layout->addWidget(confirmButton);

    connect(confirmButton, &QPushButton::clicked, this,
        [this, fields, settings_window]() mutable {

            auto findEntryByLabel = [&](const QString& label) -> QWidget* {
                for (const auto& field : fields) {
                    if (field.labeltext == label) return field.entry;
                }
                return nullptr;
                };

            auto getEntryText = [&](const QString& label)-> QString {
                for (const auto& field : fields) {
                    if (field.labeltext == label &&
                        field.labeltext != "Propeller Name:" &&
                        field.labeltext != "Angle Of Attack (deg):") {

                        QLineEdit* entry = qobject_cast<QLineEdit*>(field.entry);
                        return entry ? entry->text().trimmed() : "";
                    }
                }
                return "";
                };

            QComboBox* propBox = qobject_cast<QComboBox*>(findEntryByLabel("Propeller Name:"));
            QString new_propeller_name = propBox ? propBox->currentText() : propeller_name;

            QComboBox* aoaBox = qobject_cast<QComboBox*>(findEntryByLabel("Angle Of Attack (deg):"));
            double new_aoa = aoaBox ? aoaBox->currentText().toDouble() : angle_of_attack;

            double new_airspeed = getEntryText("Airspeed (m/sec):").toDouble();
            double new_rpm_div = getEntryText("RPM Division:").toDouble();
            double new_max_rpm = getEntryText("Max RPM:").toDouble();
            double new_thrust_clemency = getEntryText("Thrust Clemency:").toDouble();
            double new_rpm_clemency = getEntryText("RPM Clemency:").toDouble();
            double new_throttle_increase_step = getEntryText("Throttle Increase Step:").toDouble();
            int new_stage4_wait_time_ms = getEntryText("Wait After Thrust (ms):").toInt();
            int new_thrust_stab_wait_time_ms = getEntryText("Thrust Stabilizing WaitTime (ms):").toInt();

            angle_of_attack = new_aoa;
            airspeed = new_airspeed;
            propeller_name = new_propeller_name;
            rpm_div = new_rpm_div;
            max_rpm = new_max_rpm;
            rpm_clemency = new_rpm_clemency;
            thrust_clemency = new_thrust_clemency;
            stage4_wait_time_ms = new_stage4_wait_time_ms;
            throttle_increase_step = new_throttle_increase_step;
            thrust_stab_wait_time_ms = new_thrust_stab_wait_time_ms;

            qDebug() << "New Experiment Begins";

            experiment_running = true;
            this->updateBottomToolbar();
            this->start_auto();
            settings_window->close();
        });

    settings_window->show();
}

void ThrustStandGUI::start_auto() {

    if (!auto_mode_handler) {
        auto_mode_handler = new ModeHandler(this, current_throttle, nullptr, 0.0, min_throttle, this);

        connect(this, &ThrustStandGUI::mqttSendTopicChanged,
            auto_mode_handler, &ModeHandler::updateMqttSendTopic);
        connect(auto_mode_handler, &ModeHandler::experimentFinished,
            this, &ThrustStandGUI::onExperimentFinished);
        connect(auto_mode_handler, &ModeHandler::publishMqtt,
            this, &ThrustStandGUI::publishMqttMessage);
    }

    else {
        auto_mode_handler->resetExperiment();
    }

    emit limitsChanged(min_throttle, max_throttle, max_rpm, current_limit, rpm_clemency, rpm_div, thrust_clemency, stage4_wait_time_ms, throttle_increase_step, thrust_stab_wait_time_ms);

    if (!auto_mode_timer) {
        auto_mode_timer = new QTimer(this);

        connect(auto_mode_timer, &QTimer::timeout, this, [this]() {
            if (this->auto_mode_handler) {
                this->auto_mode_handler->automate(
                    this->current_throttle,
                    this->current_rpm,
                    this->current_current,
                    this->current_thrust,
                    this->current_limit,
                    this->auto_mode_timer,
                    &(this->experiment_running)
                );
            }
            });
    }

    if (!auto_mode_timer->isActive()) {
        auto_mode_timer->start(auto_mode_timer_interval_ms);
    }

}

void ThrustStandGUI::onExperimentFinished() {
    experiment_running = false;
    this->updateBottomToolbar();
    if (function_mode == "Experiment") customizeExperiment(); 
}

QString ThrustStandGUI::getHoverStyle(const QString& baseColor) {

    QString styleString =
        "QPushButton {"
        "background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 %1, stop:1 #00c6ff);"
        "color: white;"
        "border: 1px solid %1;"
        "border-radius: 8px;"
        "padding: 10px 20px;"
        "font-weight: bold;"
        "font-size: 25px;"
        "text-align: center;"
        "}"
        "QPushButton:hover {"
        "background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #007bff, stop:1 #0056b3);"
        "border-color: #007bff;"
        "}"
        "QPushButton:pressed {"
        "background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #003c7d, stop:1 #0099cc);"
        "padding-top: 12px;"
        "padding-bottom: 8px;"
        "}";

    return styleString.arg(baseColor);
}
