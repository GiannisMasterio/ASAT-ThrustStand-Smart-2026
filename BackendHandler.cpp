#include "backendhandler.h"
#include <QDebug>
#include <QPushButton>
#include <QLabel>
#include <QDialog>
#include <QVBoxLayout>
#include <QSlider>
#include <QLineEdit>
#include <QDoubleValidator>
#include <QFileDialog>
#include <QMessageBox>
#include <QDesktopServices>
#include <QCoreApplication>
#include <QUrl>
#include <algorithm>
#include "mqttclient.h"
#include "containers.h"
#include "graphcontainer.h"
#include <QFrame>
#include <QtSerialPort/QSerialPortInfo>

using namespace std;

QString default_mode = "MANUAL";

BackendHandler::BackendHandler(QObject* parent, QObject* defaultValue) :
    QObject(parent),
    mode(default_mode)
{
    serial_port = new QSerialPort(this);
    mqtt_data_timer = new QTimer(this);
    
    initLogFile();

    connect(this, &BackendHandler::mqttSendTopicChanged, this, &BackendHandler::updateMqttSendTopic);
    connect(this, &BackendHandler::publishMqtt, this, &BackendHandler::publishMqttMessage);
    connect(mqtt_data_timer, &QTimer::timeout, this, &BackendHandler::updateMqttData);

    mqtt_data_timer->start(MQTT_DELAY);
    start_mqtt();

    audio_handler = new AudioHandler(this);

    step_controller = new StepController(0, 0, cfg.throttle_increase_step);
    pid_controller = new PidController(Kp, Ki, Kd, integral_term, cfg.min_throttle, cfg.max_throttle);
    adrc_controller = new ADRCController(b0_val, wc_val, wo_val, min_out, max_out);
    registerController("Step Controller", step_controller);
    registerController("PID Controller", pid_controller);
    registerController("ADRC Controller", adrc_controller);

    active_controller = step_controller;
}

void BackendHandler::set_function_mode(QString new_function_mode) {
    this->function_mode = new_function_mode;
}

QString BackendHandler::get_function_mode() {
    return this->function_mode;
}

void BackendHandler::updateTelemetry(QMap<QString, double> value_keeper) {

    tel.thrust = calibrate_thrust(value_keeper.value("Thrust", tel.thrust)) + cal.thrust_offset;
    tel.rpm = value_keeper.value("RPM", tel.rpm);
    tel.current = calibrate_current(value_keeper.value("Current", tel.current)) + cal.current_offset;
    tel.voltage = value_keeper.value("Voltage", tel.voltage);
    tel.throttle = value_keeper.value("Throttle", tel.throttle);
    tel.torque = value_keeper.value("Torque", tel.torque);
    tel.power = tel.voltage * tel.current;

    if (use_rpm_filter) {
        tel.rpm = RPMFilter(tel.rpm);
    }
    else {
        RPMFilter(tel.rpm);
    }

    emit telemetryUpdated();
}

double BackendHandler::RPMFilter(double new_rpm) {
    rpm_buffer.push_back(new_rpm);

    if (rpm_buffer.size() > rpm_filter_window) {
        rpm_buffer.erase(rpm_buffer.begin());
    }

    QVector<double> sorted_buffer = rpm_buffer;
    std::sort(sorted_buffer.begin(), sorted_buffer.end());

    return sorted_buffer[sorted_buffer.size() / 2];
}

double BackendHandler::calibrate_current(double esp_current) {
    if (!cal.manual_current_calibration) return esp_current;
    double rawC_avg = esp_current / cal.current_lsb_user_esp;
    double new_current = rawC_avg * cal.current_lsb_user_selected;
    return new_current;
}

double BackendHandler::calibrate_thrust(double esp_newton) {
    if (!cal.manual_thrust_calibration || cal.thrust_calibration_factor == 0) return esp_newton;
    double esp_grams = esp_newton * 1000.0 / 9.81;
    double raw_avg = cal.esp_actual_tare_offset - esp_grams * cal.esp_thrust_calibration_factor;

    double new_grams = cal.direction_sign * (raw_avg - cal.tare_newton_offset) / cal.thrust_calibration_factor;
    double new_newton = (new_grams / 1000.0) * cal.gravity;
    return new_newton;
}

void BackendHandler::playAudio(int volume, QString audio_tag) {
    if (audio_handler) audio_handler->playSound(volume, audio_tag);
}

void BackendHandler::receiveTelemetry(const QString& data, QString source) {

    if (source != selected_data_source) return;

    if (selected_data_source == "MQTT") {
        QString cleanData = data.trimmed().replace(" ", "");
        QMap<QString, double> value_keeper;
        QStringList parts = cleanData.split(",");

        for (const QString& part : parts) {
            QStringList pair = part.split(":");
            if (pair.size() == 2) {
                bool ok;
                double val = pair[1].toDouble(&ok);
                if (ok) {
                    value_keeper[pair[0]] = val;
                }
            }
        }
        updateTelemetry(value_keeper);
    }
    else {
        QString cleanData = data.trimmed();
        cleanData.replace("V = ", "V:");
        cleanData.replace("I = ", "I:");
        cleanData.replace(" V,", ",");
        cleanData.replace(" A,", ",");
        cleanData.replace(" ", "");

        QMap<QString, double> value_keeper;
        QStringList parts = cleanData.split(",");

        for (const QString& part : parts) {
            QStringList pair = part.split(":");
            if (pair.size() >= 2) {
                bool ok;
                double val = QLocale::c().toDouble(pair[1], &ok);
                if (ok) {
                    value_keeper[pair[0]] = val;
                }
            }
        }

        if (!value_keeper.isEmpty()) {
            updateTelemetry(value_keeper);
        }
    }

    bool previous_write_status = write_logs;

    if (cfg.windtunnel_transitioning) write_logs = false;
    else if (cfg.log_only_during_experiment) write_logs = experiment_running;
    else write_logs = true;

    if (tel.voltage == battery_removed_voltage_value) tel.voltage = 0;

    if (tel.voltage == 0) {
        write_logs = false;
        if (is_armed) emit armedStateChanged(false); // disarm
    }
    else {
        if (!is_armed) emit armedStateChanged(true); // an den einai armed kanto gia tash!=0
    }

    if (write_logs != previous_write_status) {
        emit loggingStateChanged(write_logs);
    }

    logToFile(true);
}

void BackendHandler::initLogFile() {
    QString fullPath = log_file_path;

    if (!QFileInfo(fullPath).isAbsolute() || fullPath.isEmpty()) {
        fullPath = QCoreApplication::applicationDirPath() + QDir::separator() + log_file_path;
    }

    QFile file(fullPath);

    if (!file.exists()) {
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << "                     Timestamp                  Thrust (N)               Torque (N*m)               Voltage (V)                Current (A)               RPM               Throttle (%)                  Power (W)              Propeller Name            AOA (deg)           Airspeed (m/s)\n";
            file.close();
            qDebug() << "Created initial log file:" << fullPath;
        }
    }

    log_file_path = fullPath;
}

void BackendHandler::logToFile(bool status) {
    if (!status || !write_logs) return;

    int label_width = 26;
    QFile file(log_file_path);
    bool write_header = (!file.exists() || file.size() == 0);

    if (!file.open(QIODevice::Append | QIODevice::Text)) {
        qWarning() << "Error opening log file:" << file.errorString();
        return;
    }

    QString timestamp = QDateTime::currentDateTime().toString("HH:mm:ss");
    double throttle_pct = getThrottlePercentage(tel.throttle);

    // ΔΥΝΑΜΙΚΗ ΛΙΣΤΑ LOGGING: Προσθέτεις/αφαιρείς ό,τι θες εδώ σε 1 γραμμή!
    QVector<QPair<QString, QString>> log_columns = {
        {"Timestamp",                 timestamp},
        {"Thrust (N)",                QString::number(tel.thrust, 'f', 2)},
        {"Torque (N*m)",              QString::number(tel.torque, 'f', 3)},
        {"Voltage (V)",               QString::number(tel.voltage, 'f', 2)},
        {"Current (A)",               QString::number(tel.current, 'f', 2)},
        {"RPM",                       QString::number(tel.rpm, 'f', 0)},
        {"Throttle (%)",              QString::number(throttle_pct, 'f', 2)},
        {"Power (W)",                 QString::number(tel.power, 'f', 2)},
        {"Propeller Name",            cfg.propeller_name},
        {"AOA (deg)",                 QString::number(cfg.angle_of_attack, 'f', 2)},
        {"Airspeed (m/sec)",          QString::number(cfg.airspeed, 'f', 2)}
    };

    QTextStream out(&file);

    // Αν το αρχείο είναι άδειο, γράφει αυτόματα τις επικεφαλίδες από τη λίστα!
    if (write_header) {
        for (const auto& col : log_columns) {
            out << QString("%1 ").arg(col.first, label_width);
        }
        out << "\n";
    }

    // Γράφει τις τιμές
    for (const auto& col : log_columns) {
        out << QString("%1 ").arg(col.second, label_width);
    }
    out << "\n";

    file.close();
}

void BackendHandler::handleSerial(bool status) {
    if (status) {
        if (serial_port->isOpen()) {
            serial_port->close();
            qDebug() << "Closed previous serial port.";
        }

        serial_port->setPortName(serial_com_port_tag);
        serial_port->setBaudRate(serial_baudrate);

        if (serial_port->open(QIODevice::ReadWrite)) {
            connect(serial_port, &QSerialPort::readyRead, this, &BackendHandler::readSerialData, Qt::UniqueConnection);
            qDebug() << "Opened serial port" << serial_com_port_tag << "at" << serial_baudrate;
        }
        else {
            qWarning() << "Cannot open port" << serial_com_port_tag << ":" << serial_port->errorString();
        }
    }
    else {
        if (serial_port->isOpen()) {
            serial_port->close();
            qDebug() << "Serial port closed.";
        }
    }
}

QStringList BackendHandler::getAvailablePorts() {
    QStringList ports;
    for (const QSerialPortInfo& info : QSerialPortInfo::availablePorts()) {
        ports << info.portName();
    }
    return ports;
}

bool BackendHandler::connectSerial(const QString& portName, int baudrate) {
    serial_com_port_tag = portName;
    serial_baudrate = baudrate;

    playAudio(audio_click_sfx_volume, "MainClick");
    confirmPort(); // Η υπάρχουσα συνάρτησή σου για σύνδεση

    return isSerialConnected();
}

void BackendHandler::confirmPort() {
    qDebug() << "Selected COM port:" << serial_com_port_tag;
    qDebug() << "Selected baudrate:" << serial_baudrate;
    handleSerial(running);
}

bool BackendHandler::isSerialConnected() const {
    return (serial_port && serial_port->isOpen());
}

void BackendHandler::readSerialData() {
    while (serial_port->canReadLine()) {
        QByteArray data = serial_port->readLine();
        if (!data.isEmpty()) {
            QString serialData = QString::fromUtf8(data).trimmed();
            qDebug() << "Full Line Received:" << serialData;
            receiveTelemetry(serialData, "SERIAL");
        }
    }
}

void BackendHandler::serial_sendCommand(QString command) {
    if (!serial_port || !serial_port->isOpen()) {
        qDebug() << "Serial not open!";
        return;
    }

    if (!command.endsWith("\n"))
        command += "\n";

    QByteArray data = command.toUtf8();
    qint64 bytes = serial_port->write(data);
    serial_port->flush();

    qDebug() << "TX:" << data << "bytes:" << bytes;
}

void BackendHandler::chooseLogLocation() {
    QString initialDir = QFileInfo(log_file_path).path();
    QString newFilePath = QFileDialog::getSaveFileName(
        nullptr, // ΔΙΟΡΘΩΣΗ: nullptr αντί για this (επειδή το BackendHandler δεν είναι QWidget)
        "Select Log File Location",
        initialDir,
        "CSV Log (*.csv);;Text Log (*.txt);;All files (*.*)"
    );

    if (!newFilePath.isEmpty()) {
        if (!newFilePath.endsWith(".csv", Qt::CaseInsensitive) && !newFilePath.endsWith(".txt", Qt::CaseInsensitive)) {
            newFilePath += ".csv";
        }

        log_file_path = newFilePath;
        initLogFile();
        qDebug() << "New log file location set to:" << log_file_path;
    }
}

void BackendHandler::openLogFile() {
    QString fullFilePath = log_file_path;

    if (!QFileInfo(fullFilePath).isAbsolute() || fullFilePath.isEmpty()) {
        fullFilePath = QCoreApplication::applicationDirPath() + QDir::separator() + "ASAT ThrustStand Records.csv";
    }

    if (!QFile::exists(fullFilePath)) {
        QMessageBox::warning(nullptr, "File Not Found", "Log file does not exist at: " + fullFilePath);
        return;
    }

    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(fullFilePath))) {
        QMessageBox::critical(nullptr, "Error", "Could not open file with the system default application.");
    }
}

void BackendHandler::start_mqtt() {
    if (mqttcfg.mqtt_client) {
        mqttcfg.mqtt_client->disconnectFromBroker();
        delete mqttcfg.mqtt_client; // διαγραφη παλιου reference
        mqttcfg.mqtt_client = nullptr;
    }
    mqttcfg.mqtt_client = new MqttClient(mqttcfg.mqtt_broker, mqttcfg.mqtt_port, mqttcfg.mqtt_receive_topic, mqttcfg.mqtt_send_topic, this);

    connect(mqttcfg.mqtt_client, &MqttClient::onConnected, this, &BackendHandler::onMqttConnected);
    connect(mqttcfg.mqtt_client, &MqttClient::onDisconnected, this, &BackendHandler::onMqttDisconnected);
    connect(mqttcfg.mqtt_client, &MqttClient::on_mqtt_message, this, &BackendHandler::onMqttMessageReceived);

    mqttcfg.mqtt_client->connectToBroker();
}

void BackendHandler::publishMqttMessage(const QString& topic, const QString& payload) {
    if (mqttcfg.mqtt_client) {
        qint32 id = mqttcfg.mqtt_client->publish(topic, payload);
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

void BackendHandler::disconnect_mqtt() {
    auto& mqtt_client = mqttcfg.mqtt_client;

    if (mqtt_client == nullptr) {
        qDebug() << "MQTT client was never initialized.";
        return;
    }

    if (mqtt_client->getState() == QMqttClient::Connected) {
        mqtt_client->unsubscribe(QMqttTopicFilter(mqttcfg.mqtt_receive_topic));
        mqtt_client->unsubscribe(QMqttTopicFilter(mqttcfg.mqtt_send_topic));
        mqtt_client->disconnectFromHost();
        qDebug() << "Unsubscribed and requested disconnection from MQTT Broker.";
    }
}

void BackendHandler::change_mqtt_topic() {
    disconnect_mqtt();
    updateMqttSendTopic(mqttcfg.mqtt_send_topic);
    start_mqtt();
}

void BackendHandler::onMqttConnected() {
    qDebug() << "Connected to MQTT Broker:" << mqttcfg.mqtt_broker << ":" << mqttcfg.mqtt_port;
}

void BackendHandler::onMqttDisconnected() {
    qDebug() << "Disconnected from MQTT Broker.";
}

void BackendHandler::onMqttMessageReceived(const QString& data) {
    if (!process_begin) {
        process_begin = true;
        playAudio(100, "TelemetryConnected");
    }

    mqttcfg.last_mqtt_data = data.toUtf8();
    qDebug() << "received (MQTT String Data):" << data;
}

void BackendHandler::updateMqttData() {
    if (!mqttcfg.last_mqtt_data.isEmpty()) {
        receiveTelemetry(QString::fromUtf8(mqttcfg.last_mqtt_data), "MQTT");
        mqttcfg.last_mqtt_data.clear();
    }
}

bool BackendHandler::isSoundPlaying(QString audio) {
    return audio_handler ? audio_handler->isSoundPlaying(audio) : false;
}

void BackendHandler::setSoundStatus(bool sound_status) {
    if (audio_handler) {
        audio_handler->setSoundStatus(sound_status);
    }
}

double BackendHandler::getThrottlePercentage(int current_throttle) {

    if (cfg.max_throttle <= cfg.min_throttle)
        return 0.0;

    return 100.0 * (current_throttle - cfg.min_throttle) / (cfg.max_throttle - cfg.min_throttle);
}
    
void BackendHandler::updateMqttSendTopic(const QString& newTopic) {
    mqttcfg.mqtt_send_topic = newTopic;
    qDebug() << "ModeHandler MQTT Topic updated to:" << newTopic;
}

void BackendHandler::manualToggleEvent(int state) {
    qDebug() << "Toggle State Changed" << state;

    if (state == Qt::Checked) {
        mode = "AUTO";
        qDebug() << "Mode changed to AUTO";
        if (slider_opener_button) {
            slider_opener_button->setText("Set RPM");
        }
    }
    else if (state == Qt::Unchecked) {
        mode = "MANUAL";
        qDebug() << "Mode changed to MANUAL";
        if (slider_opener_button) {
            slider_opener_button->setText("Set Throttle");
        }
    }

    emit modeChanged(mode);
}

void BackendHandler::handleMode() {
    if (mode == "AUTO") {
        int new_throttle = -1.0;
        double lower_bound = target_rpm - cfg.rpm_clemency;
        double upper_bound = target_rpm + cfg.rpm_clemency;

        if (tel.rpm < lower_bound) {
            new_throttle = tel.throttle + cfg.throttle_increase_step;
        }
        else if (tel.rpm > upper_bound) {
            new_throttle = tel.throttle - cfg.throttle_increase_step;
        }

        if (new_throttle >= cfg.min_throttle && new_throttle <= cfg.max_throttle) {
            desired_throttle = new_throttle;
            QString payload = QString("%1:%2").arg("Throttle").arg(desired_throttle);
            emit publishMqtt(mqttcfg.mqtt_send_topic, payload);
        }
    }
}

void BackendHandler::createSliderWindow() {
    if (slider_window) {
        slider_window->raise();
        slider_window->activateWindow();
        return;
    }

    slider_window = new QDialog(nullptr, Qt::Window);
    slider_window->setObjectName("ModernDialog");
    slider_window->setWindowTitle(mode + " Mode Configuration");
    slider_window->setFixedSize(620, 400);
    slider_window->setAttribute(Qt::WA_DeleteOnClose);
    slider_window->setAttribute(Qt::WA_StyledBackground, true);
    slider_window->setWindowIcon(QIcon(ICONPATH));
    slider_window->setStyleSheet(DIALOG_STYLE);

    QVBoxLayout* layout = new QVBoxLayout(slider_window);
    layout->setContentsMargins(36, 30, 36, 30);
    layout->setSpacing(14);

    // --- Τίτλος ---
    const bool isManual = (mode == "MANUAL");
    QString title_text = isManual ? "SET THROTTLE" : "SET RPM";

    QLabel* title_label = new QLabel(title_text, slider_window);
    title_label->setObjectName("DialogHeader");
    QFont hf("Segoe UI", 15, QFont::DemiBold);
    hf.setLetterSpacing(QFont::AbsoluteSpacing, 3);
    title_label->setFont(hf);
    title_label->setAlignment(Qt::AlignCenter);
    layout->addWidget(title_label);

    // --- Μεγάλη τιμή (ενημερώνεται από την updateSliderValue) ---
    slider_value_label = new QLabel(slider_window);
    QFont vf("Segoe UI", 40, QFont::Bold);
    slider_value_label->setFont(vf);
    slider_value_label->setAlignment(Qt::AlignCenter);
    slider_value_label->setStyleSheet(QString("color: %1; background: transparent; border: none;").arg(THEME_COLOR));
    slider_value_label->setMinimumHeight(70);
    layout->addWidget(slider_value_label);

    layout->addSpacing(6);

    // --- Slider ---
    this->slider = new QSlider(Qt::Horizontal, slider_window);
    slider->setSingleStep(1);
    slider->setMinimumHeight(36);
    slider->setCursor(Qt::PointingHandCursor);

    // --- Πεδίο εισαγωγής ---
    QLineEdit* value_type_input_field = new QLineEdit(slider_window);
    value_type_input_field->setFixedSize(130, 40);
    value_type_input_field->setAlignment(Qt::AlignCenter);

    if (isManual) {
        slider->setRange(cfg.min_throttle, cfg.max_throttle);
        int initial_val = std::clamp(tel.throttle, cfg.min_throttle, cfg.max_throttle);
        slider->setValue(initial_val);
        value_type_input_field->setValidator(new QDoubleValidator((double)cfg.min_throttle, (double)cfg.max_throttle, 1, value_type_input_field));
    }
    else {
        slider->setRange(0, cfg.max_rpm);
        value_type_input_field->setValidator(new QDoubleValidator(0, cfg.max_rpm, 1, value_type_input_field));
    }

    layout->addWidget(slider);

    // --- Ελάχιστο / μέγιστο κάτω από το slider ---
    auto makeEndLabel = [this](const QString& text, Qt::Alignment al) {
        QLabel* l = new QLabel(text, slider_window);
        l->setObjectName("SectionLabel");
        l->setFont(QFont("Segoe UI", 10, QFont::DemiBold));
        l->setAlignment(al);
        return l;
        };
    const QString unitText = isManual ? "us" : "rpm";

    QHBoxLayout* endsRow = new QHBoxLayout();
    endsRow->addWidget(makeEndLabel(QString::number(slider->minimum()) + " " + unitText, Qt::AlignLeft | Qt::AlignVCenter));
    endsRow->addStretch();
    endsRow->addWidget(makeEndLabel(QString::number(slider->maximum()) + " " + unitText, Qt::AlignRight | Qt::AlignVCenter));
    layout->addLayout(endsRow);

    layout->addStretch();

    // --- Πεδίο + μονάδα, κεντραρισμένα ---
    QHBoxLayout* inputRow = new QHBoxLayout();
    inputRow->setSpacing(10);
    inputRow->addStretch();

    QLabel* inputCaption = new QLabel("VALUE", slider_window);
    inputCaption->setObjectName("SectionLabel");
    QFont cf("Segoe UI", 10, QFont::DemiBold);
    cf.setLetterSpacing(QFont::AbsoluteSpacing, 2);
    inputCaption->setFont(cf);

    QLabel* inputUnit = new QLabel(unitText, slider_window);
    inputUnit->setObjectName("SectionLabel");
    inputUnit->setFont(QFont("Segoe UI", 11, QFont::DemiBold));

    inputRow->addWidget(inputCaption);
    inputRow->addWidget(value_type_input_field);
    inputRow->addWidget(inputUnit);
    inputRow->addStretch();
    layout->addLayout(inputRow);

    // --- Συνδέσεις (αμετάβλητες) ---
    QObject::connect(slider, &QSlider::valueChanged, this, &BackendHandler::updateSliderValue);
    QObject::connect(slider, &QSlider::valueChanged, value_type_input_field, [value_type_input_field](int val) {
        value_type_input_field->setText(QString::number(val));
        });

    QObject::connect(value_type_input_field, &QLineEdit::returnPressed, this, [this, value_type_input_field]() {
        bool ok;
        double newValue = value_type_input_field->text().toDouble(&ok);

        if (ok && newValue >= slider->minimum() && newValue <= slider->maximum()) {
            slider->setValue(qRound(newValue));
        }
        else {
            value_type_input_field->setText(QString::number(slider->value()));
        }
        });

    value_type_input_field->setText(QString::number(slider->value()));

    // Ενημερώνουμε ΜΟΝΟ το κείμενο στην οθόνη κατά το άνοιγμα, ΔΕΝ στέλνουμε MQTT εντολή!
    if (isManual) {
        double pct = qMax(0.0, getThrottlePercentage(slider->value()));
        slider_value_label->setText(QString::number(pct, 'f', 2) + " %");
    }
    else {
        slider_value_label->setText(QString::number(target_rpm, 'f', 0) + " rpm");
    }

    slider_window->exec();
    slider_window = nullptr;
}

void BackendHandler::openSliderWindow() {
    if (!experiment_running) {
        playAudio(audio_click_sfx_volume, "MainClick");
        this->createSliderWindow();
    }
    else {
        playAudio(audio_click_sfx_volume, "TurnOffExperimentFirst");
    }
}

void BackendHandler::updateSliderValue(int sliderValue) {
    double ingoingValue = (double)sliderValue;
    QString payload;

    if (mode == "MANUAL") {
        double slider_stored_value = ingoingValue;

        if (slider_value_label) {
            double pct = qMax(0.0, getThrottlePercentage(sliderValue));
            slider_value_label->setText(QString::number(pct, 'f', 2) + " %");
        }

        payload = QString("%1:%2").arg("Throttle").arg((int)slider_stored_value);
        emit publishMqtt(mqttcfg.mqtt_send_topic, payload);
    }
    else if (mode == "AUTO") {
        target_rpm = ingoingValue;

        if (slider_value_label) {
            slider_value_label->setText(QString::number(target_rpm, 'f', 0) + " rpm");
        }
    }
}

void BackendHandler::registerController(const QString& name, Controller* controller) {
    if (controller == nullptr) return;
    registered_controllers.insert(name, controller);
}

void BackendHandler::setActiveController(const QString& name) {
    auto controller_iterator = registered_controllers.constFind(name);
    if (controller_iterator == registered_controllers.cend()) {
        return;
    }
    active_controller = controller_iterator.value();
}

void BackendHandler::executeEmergencyStop() {
    this->resetExperiment();
}

void BackendHandler::initialize_stabilizing_thrust() {
    playAudio(100, "StabilizingThrust");
    stabilizing_thrust = true;
    thrustSampleBuffer.clear();

    int dt_ms = auto_mode_timer ? auto_mode_timer->interval() : 100;
    if (dt_ms <= 0) dt_ms = 100;
    max_thrust_samples = (int)(cfg.thrust_stab_wait_time_ms / dt_ms);
}

void BackendHandler::execute_experiment() {
    int new_throttle = 0;

    int dt_ms = auto_mode_timer ? auto_mode_timer->interval() : 100;
    if (dt_ms <= 0) dt_ms = 100;
    double dt_sec = dt_ms / 1000.0;

    if (!experiment_running && !ending_transition) {
        experiment_stage = EmergencyStop;
        experiment_just_started = false;
        playAudio(100, "ExperimentCancelled");
    }
    else if ((tel.current >= cfg.current_limit) && !ending_transition) {
        experiment_stage = EmergencyStop;
        experiment_just_started = false;
        playAudio(100, "CurrentLimitReached");
    }
    else if (tel.throttle == cfg.max_throttle && !ending_transition) {
        experiment_stage = EmergencyStop;
        experiment_just_started = false;
    }
    else if (experiment_just_started) {
        playAudio(100, "ExperimentBeginning");
        experiment_just_started = false;
        experiment_stage = IncrementRPMTarget;
        if (!cfg.first_experiment) experiment_stage = WindTunnelTransitionStage;
    }

    switch (experiment_stage) {

    case IncrementRPMTarget:
        if (use_max_rpm_logic) target_rpm = min(cfg.rpm_step + (int)target_rpm, (int)cfg.max_rpm);
        else target_rpm += cfg.rpm_step;

        lower_rpm_bound = target_rpm - cfg.rpm_clemency;
        upper_rpm_bound = target_rpm + cfg.rpm_clemency;

        if (active_controller) {
            active_controller->set_new_bounds(lower_rpm_bound, upper_rpm_bound); // εδω τα bounds μου φτιαχνονται με τα rpm
        }

        experiment_stage = AdjustThrottle;
        break;

    case AdjustThrottle: {
        if ((tel.rpm >= lower_rpm_bound && tel.rpm <= upper_rpm_bound) || (tel.throttle == cfg.max_throttle)) {
            experiment_stage = StabilizeThrust;
            break;
        }

        if (active_controller) new_throttle = active_controller->update(target_rpm, tel.rpm, tel.throttle, dt_sec); // setpoint με RPM

        desired_throttle = std::clamp(new_throttle, cfg.min_throttle, cfg.max_throttle);
        QString desired_throttle_payload = QString("%1:%2").arg("Throttle").arg(desired_throttle);
        emit publishMqtt(mqttcfg.mqtt_send_topic, desired_throttle_payload);
        break;
    }

    case StabilizeThrust: {
        if (!stabilizing_thrust) initialize_stabilizing_thrust();

        thrustSampleBuffer.append(tel.thrust);
        if (thrustSampleBuffer.size() > max_thrust_samples) {
            thrustSampleBuffer.removeFirst();
        }

        if (thrustSampleBuffer.size() < max_thrust_samples) break;

        double min_thrust = *std::min_element(thrustSampleBuffer.constBegin(), thrustSampleBuffer.constEnd());
        double max_thrust = *std::max_element(thrustSampleBuffer.constBegin(), thrustSampleBuffer.constEnd());
        double peak_to_peak_difference = max_thrust - min_thrust;

        if (peak_to_peak_difference <= cfg.thrust_clemency) {
            qDebug() << "Thrust stabilized";
            thrustSampleBuffer.clear();
            experiment_stage = RecordData;
        }
        break;
    }

    case RecordData:
        if (!target_rpm_reached) {
            playAudio(100, "TargetRPMReached");
            target_rpm_reached = true;
        }

        if (!record_data_wait_time_initialized) {
            record_data_wait_time = QDateTime::currentDateTime();
            record_data_wait_time_initialized = true;
        }

        if (record_data_wait_time.msecsTo(QDateTime::currentDateTime()) > cfg.record_data_wait_time_ms) {

            if (target_rpm >= (cfg.max_rpm - cfg.rpm_clemency) || tel.throttle == cfg.max_throttle) {
                qDebug() << "Experiment finished successfully.";
                playAudio(100, "ExperimentFinished");
                record_data_wait_time_initialized = false;
                stabilizing_thrust = false;
                target_rpm_reached = false;

                if (cfg.experiment_is_static) {
                    desired_throttle = cfg.min_throttle;
                    QString desired_throttle_payload = QString("%1:%2").arg("Throttle").arg(desired_throttle);
                    emit publishMqtt(mqttcfg.mqtt_send_topic, desired_throttle_payload);

                    if (auto_mode_timer) auto_mode_timer->stop();
                    experiment_just_started = true;
                    emit experimentFinished();
                }
                else {
                    ending_transition = true;
                    if (cfg.first_experiment) cfg.first_experiment = false;
                    experiment_stage = WindTunnelTransitionStage;
                }
                break;
            }

            record_data_wait_time_initialized = false;
            stabilizing_thrust = false;
            target_rpm_reached = false;
            experiment_stage = IncrementRPMTarget;
        }
        break;

    case EmergencyStop: {
        qDebug() << "Experiment was exited by user or current limit reached";

        if (cfg.experiment_is_static) {
            desired_throttle = cfg.min_throttle;
            QString desired_throttle_payload = QString("%1:%2").arg("Throttle").arg(desired_throttle);
            emit publishMqtt(mqttcfg.mqtt_send_topic, desired_throttle_payload);

            if (auto_mode_timer) auto_mode_timer->stop();
            emit experimentFinished();

            experiment_just_started = true;
            record_data_wait_time_initialized = false;
            stabilizing_thrust = false;
            target_rpm_reached = false;
            experiment_stage = IncrementRPMTarget;
            break;
        }

        if (cfg.first_experiment) cfg.first_experiment = false;

        record_data_wait_time_initialized = false;
        target_rpm_reached = false;
        stabilizing_thrust = false;

        ending_transition = true;
        experiment_stage = WindTunnelTransitionStage;
        break;
    }

    case WindTunnelTransitionStage: {
        double current_stabilize_value = tel.thrust;
        double stabilize_target_threshold = cfg.stabilize_target_threshold;
        double clemency = cfg.thrust_clemency;

        experiment_is_transitioning = true;

        if (!stabilizing_transition) {
            qDebug() << "Starting transition stabilization...";
            stabilizing_transition = true;
            transitionSampleBuffer.clear();

            if (active_controller) active_controller->set_new_bounds(stabilize_target_threshold - clemency, stabilize_target_threshold + clemency);

            max_transition_samples = (int)(cfg.transition_stab_wait_time_ms / dt_ms);
        }

        if (active_controller) {
            new_throttle = active_controller->update(stabilize_target_threshold, current_stabilize_value, tel.throttle, dt_sec);
        }

        desired_throttle = std::clamp(new_throttle, cfg.min_throttle, cfg.max_throttle);
        QString desired_throttle_payload = QString("%1:%2").arg("Throttle").arg(desired_throttle);
        emit publishMqtt(mqttcfg.mqtt_send_topic, desired_throttle_payload);

        transitionSampleBuffer.append(current_stabilize_value);
        if (transitionSampleBuffer.size() > max_transition_samples) {
            transitionSampleBuffer.removeFirst();
        }

        if (transitionSampleBuffer.size() < max_transition_samples) break;

        double min_value = *std::min_element(transitionSampleBuffer.constBegin(), transitionSampleBuffer.constEnd());
        double max_value = *std::max_element(transitionSampleBuffer.constBegin(), transitionSampleBuffer.constEnd());
        double peak_to_peak = max_value - min_value;
        double distance_from_target = std::abs(current_stabilize_value - stabilize_target_threshold);

        if (peak_to_peak <= 2 * clemency && distance_from_target <= clemency) {
            transitionSampleBuffer.clear();
            cfg.windtunnel_transitioning = false;
            stabilizing_transition = false;

            if (ending_transition) {
                qDebug() << "Ending calibration complete. Ready for next airspeed.";
                ending_transition = false;
                experiment_just_started = true;
                if (auto_mode_timer) auto_mode_timer->stop();
                emit experimentFinished();
            }
            else {
                qDebug() << "Initial transition complete. Starting experiment steps.";
                experiment_stage = IncrementRPMTarget;
            }
        }
        break;
    }
    }
}

void BackendHandler::resetExperiment() {
    experiment_stage = IncrementRPMTarget;
    target_rpm = 0.0;
    record_data_wait_time_initialized = false;
    thrustSampleBuffer.clear();
    experiment_is_transitioning = false;
}
