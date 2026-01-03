#include "modehandler.h"
#include <QDebug>
#include <QtMath> 
#include <QPushButton> 
#include <QLabel>
#include <QDialog>
#include <QVBoxLayout>
#include <QSlider>
#include <qlineedit.h>
#include <QDoubleValidator>
#include "ThrustStandGUI.h"
#include <algorithm>

QString default_mode = "MANUAL";

ModeHandler::ModeHandler(QObject* parent,
    double initialValue,
    QObject* defaultValue,
    double target_rpm,
    double desired_throttle,
    ThrustStandGUI* masterGUI)
      : QObject(parent),
    value(initialValue),
    mode(default_mode),
    target_rpm(target_rpm),
    desired_throttle(desired_throttle),
    master(nullptr),
    m_guiMaster(masterGUI) 
{

    mode_value_limits = {
    {"MANUAL", {min_throttle,max_throttle}},
    {"AUTO",{0.0,max_rpm}}
    };

    connect(m_guiMaster, &ThrustStandGUI::limitsChanged, this, &ModeHandler::updateLimits);
}

double ModeHandler::getThrottlePercentage() const { 
    return 100.0 * (value - min_throttle) / (max_throttle - min_throttle);
}

void ModeHandler::updateLimits(double newMinThrottle, double newMaxThrottle, double newMaxRpm, int newCurrentLimit, double newRpmClemency, double newRpmStep, double newThrustClemency, int newStage4_wait_time_ms, double newThrottle_increase_step, int newThrust_stab_wait_time_ms)
{
    mode_value_limits["MANUAL"] = { newMinThrottle, newMaxThrottle };
    mode_value_limits["AUTO"] = { 0.0, newMaxRpm };


    min_throttle = newMinThrottle;
    max_throttle = newMaxThrottle;
    max_rpm = newMaxRpm;
    current_limit = newCurrentLimit;
    rpm_clemency = newRpmClemency;
    rpm_div = newRpmStep;
    thrust_clemency = newThrustClemency;
    stage4_wait_time_ms = newStage4_wait_time_ms;
    throttle_increase_step = newThrottle_increase_step;
    thrust_stab_wait_time_ms = newThrust_stab_wait_time_ms;

}

void ModeHandler::updateMqttSendTopic(const QString& newTopic) {
    mqtt_send_topic = newTopic;
    qDebug() << "ModeHandler MQTT Topic updated to:" << newTopic;
}

void ModeHandler::manualToggleEvent(int state) {

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

void ModeHandler::handleMode(double current_throttle, double current_rpm) {

    if (mode == "AUTO") {
        double new_throttle = -1.0;
        double lower_bound = target_rpm - rpm_clemency;
        double upper_bound = target_rpm + rpm_clemency;

        if (current_rpm < lower_bound) {
            new_throttle = current_throttle + throttle_increase_step;
        }
        else if (current_rpm > upper_bound) {
            new_throttle = current_throttle - throttle_increase_step;
        }

        if (new_throttle >= min_throttle && new_throttle <= max_throttle) {
            desired_throttle = new_throttle;
            QString payload = QString("%1:%2").arg("Throttle").arg(desired_throttle, 0, 'f', 1);

            emit publishMqtt(mqtt_send_topic, payload);

        }
    }
}

void ModeHandler::openSliderWindow() {

    if (slider_window) {
        slider_window->raise();
        slider_window->activateWindow();
        return;
    }

    slider_window = new QDialog(nullptr, Qt::Window);
    slider_window->setWindowTitle(mode + " Mode Configuration");
    slider_window->setFixedSize(600, 300);
    slider_window->setAttribute(Qt::WA_DeleteOnClose);
    slider_window->setWindowIcon(QIcon(ICONPATH));

    QVBoxLayout* layout = new QVBoxLayout(slider_window);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(20);

    QLineEdit* value_type_input_field = new QLineEdit(slider_window);
    value_type_input_field->setFixedSize(120, 30);
    value_type_input_field->setAlignment(Qt::AlignCenter);
    value_type_input_field->setValidator(new QDoubleValidator(mode_value_limits[mode].first, mode_value_limits[mode].second, 1, value_type_input_field));
    
    this->slider = new QSlider(Qt::Horizontal, slider_window);
    slider->setRange(qRound(mode_value_limits[mode].first), qRound(mode_value_limits[mode].second));
    slider->setSingleStep(1);
    slider->setFixedSize(500, 40);

    QString title_text;

    if (mode == "MANUAL") {
        title_text = QString("Set Throttle PWM Pulse (%1 - %2):").arg(min_throttle).arg(max_throttle);
        slider->setValue(qRound(value));
    }
    else {
        title_text = QString("Set Target RPM (0 - %1):").arg(max_rpm);
        slider->setValue(qRound(target_rpm));
    }

    QLabel* title_label = new QLabel(title_text, slider_window);
    title_label->setFont(QFont("Arial", 16, QFont::Bold));

    layout->addWidget(title_label, 0, Qt::AlignCenter);
    layout->addWidget(value_type_input_field, 0, Qt::AlignCenter);
    layout->addWidget(slider, 0, Qt::AlignCenter);

    slider_value_label = new QLabel(slider_window);
    slider_value_label->setFont(QFont("Arial", 28));
    slider_value_label->setStyleSheet(QString("color: %1;").arg(THEME_COLOR));
    layout->addWidget(slider_value_label, 0, Qt::AlignCenter);

    QObject::connect(slider, &QSlider::valueChanged, this, &ModeHandler::updateSliderValue);
    QObject::connect(slider, &QSlider::valueChanged, value_type_input_field, [value_type_input_field](int value) {value_type_input_field->setText(QString::number(value));});

    QObject::connect(value_type_input_field, &QLineEdit::returnPressed, this, [this, value_type_input_field]() {
        bool ok;
        double newValue = value_type_input_field->text().toDouble(&ok);

        if (ok && newValue >= slider->minimum() && newValue <= slider->maximum()) {
            slider->setValue(qRound(newValue));
        }
        else {
            value_type_input_field->setText(QString::number(slider->value()));
        }
        }
    );

    value_type_input_field->setText(QString::number(slider->value()));

    updateSliderValue(slider->value());

    slider_window->exec();
    slider_window = nullptr;
}

void ModeHandler::updateSliderValue(int sliderValue) {
    double ingoingValue = (double)sliderValue;
    QString payload;

    if (mode == "MANUAL") {
        value = ingoingValue;
        
        if (slider_value_label) {
            slider_value_label->setText(QString::number(getThrottlePercentage(), 'f', 2) + " %");
        }
        payload = QString("%1:%2").arg("Throttle").arg(value, 0, 'f', 1);
        emit publishMqtt(mqtt_send_topic, payload);

    }
    else if (mode == "AUTO") {
        target_rpm = ingoingValue;
        
        if (slider_value_label) {
            slider_value_label->setText(QString::number(target_rpm, 'f', 1) + " rpm");
        }
    }
}

void ModeHandler::automate(double current_throttle, double current_rpm, double current_current, double current_thrust, int current_limit,  QTimer* auto_mode_timer, bool* experiment_running) {

    double new_throttle = -1.0;

    if (!(*experiment_running) || (current_current >= (double)current_limit)) stage = 5;



    switch (stage) {

    case 1:
        target_rpm += max_rpm / rpm_div;
        lower_rpm_bound = target_rpm - rpm_clemency;
        upper_rpm_bound = target_rpm + rpm_clemency;
        stage = 2;
        break;

    case 2:
        if (current_rpm >= lower_rpm_bound && current_rpm <= upper_rpm_bound) {
            stage = 3;
            break;
        }

        if (current_rpm < lower_rpm_bound) {
            new_throttle = current_throttle + throttle_increase_step;
        }
        else if (current_rpm > upper_rpm_bound) {
            new_throttle = current_throttle - throttle_increase_step;
        }

        if (new_throttle >= min_throttle && new_throttle <= max_throttle) {

            desired_throttle = new_throttle;
            QString payload = QString("%1:%2").arg("Throttle").arg(desired_throttle, 0, 'f', 1);
            emit publishMqtt(mqtt_send_topic, payload);

        }

        qDebug() << "Stage: " << stage;
        break;

    case 3: {

        if (thrustSampleBuffer.isEmpty()) {
            thrustSampleBuffer.append(current_thrust);
            stage3_start_time = QDateTime::currentDateTime();
            break;
        }

        thrustSampleBuffer.append(current_thrust);

        if (stage3_start_time.msecsTo(QDateTime::currentDateTime()) < thrust_stab_wait_time_ms) {
            break;
        }

        double min_thrust = *std::min_element(thrustSampleBuffer.constBegin(), thrustSampleBuffer.constEnd());
        double max_thrust = *std::max_element(thrustSampleBuffer.constBegin(), thrustSampleBuffer.constEnd());

        double peak_to_peak_difference = max_thrust - min_thrust;

        if (peak_to_peak_difference <= thrust_clemency) {

            qDebug() << "Thrust stabilized";
            thrustSampleBuffer.clear();
            stage = 4;

        }
        else {
            thrustSampleBuffer.remove(0); // αφαιρω το παλιοτερο element ωστε να εχω συγκεκριμενο αριθμο δειγματων 
        }
        qDebug() << "Stage: " << stage;
        break;
    }
    case 4:

        if (!stage4_start_time_initialized) {
            stage4_start_time = QDateTime::currentDateTime();
            stage4_start_time_initialized = true;
        }

        if (stage4_start_time.msecsTo(QDateTime::currentDateTime()) > stage4_wait_time_ms) {

            if (target_rpm >= (max_rpm - rpm_clemency)) { 

                qDebug() << "Experiment finished at Max RPM (Stage 4 end).";

                desired_throttle = min_throttle; 
                QString payload = QString("%1:%2").arg("Throttle").arg(desired_throttle, 0, 'f', 1);
                emit publishMqtt(mqtt_send_topic, payload);

                auto_mode_timer->stop();
                emit experimentFinished();
                break; 
            }

            stage4_start_time_initialized = false;
            stage = 1;
        }

        break;

    case 5:
         qDebug() << "Experiment was exited by user.";
         desired_throttle = min_throttle;
         QString payload = QString("%1:%2").arg("Throttle").arg(desired_throttle, 0, 'f', 1);
         emit publishMqtt(mqtt_send_topic, payload);

         auto_mode_timer->stop();
         emit experimentFinished();

         stage4_start_time_initialized = false;
         stage = 1;
         break;
    }

}

void ModeHandler::resetExperiment() {
    stage = 1;
    target_rpm = 0.0;
    rpm_reached = false;
    stage4_start_time_initialized = false;
    thrustSampleBuffer.clear();
}