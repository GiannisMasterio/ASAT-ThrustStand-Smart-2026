#include "containers.h"
#include "constants.h"
#include "modehandler.h"
#include <QDebug>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QCheckBox>
#include <QPushButton>
#include <QFont>
#include <QtMath>
#include "ThrustStandGUI.h"

Containers::Containers(const QString& title, int titleSize, const QString& titleColor,
    double initialValue, bool hasBar, const QString& mode, ThrustStandGUI* guiMaster,
    QWidget* parent)
    : QFrame(parent),
    title_text(title),
    title_label_size(titleSize),
    title_label_color(titleColor),
    value(initialValue),
    unit(units_map.value(title)),
    has_bar(hasBar),
    quantity(actual_quantity.value(title)),
    function_mode(mode),
    m_guiMaster(guiMaster)
{

    low_bound = bounds_map.value(quantity).first;
    high_bound = bounds_map.value(quantity).second;

    QVector<QString> colors = progress_bar_colors_map.value(quantity);

    if (colors.size() >= 3) {
        low_color = colors[0];
        mid_color = colors[1];
        high_color = colors[2];
    }
    else {
        // Default ÙÈÏ›Ú ·Ì Í‹ÙÈ ‹ÂÈ ÛÙÒ·‚‹
        low_color = THEME_COLOR;
        mid_color = THEME_COLOR;
        high_color = THEME_COLOR;
        qWarning() << "Warning: Missing color definitions for quantity:" << quantity;
    }


    if (quantity == "Throttle") { // ¡Ã¡ ≈…Ã¡… ”‘œ CONTAINER ‘œ’ THROTTLE »≈ÀŸ ≈Õ¡ ¡Õ‘… ≈…Ã≈Õœ Õ¡ ƒ≈…◊Õ≈… ”≈ MODEHANDLER
        mode_handler = new ModeHandler(
            this, 
            value,
            nullptr, 
            0.0, 
            m_guiMaster->min_throttle,
            guiMaster 
        );
    }

    design();

    if ((quantity == "Throttle") && (function_mode == "Basic")){
        setupControlElements();
        if (mode_handler) {
            QObject::connect(mode_handler, &ModeHandler::modeChanged,
                this, &Containers::handleModeChange);
        }
    }

    setStyleSheet(QString("QFrame { background-color: %1; border-radius: 27px; border: 2px solid %2; }").arg(CONTAINER_COLOR).arg(BORDER_COLOR));
}

double Containers::getThrottlePercentage() const {
    return 100.0 * (value - m_guiMaster->min_throttle) / (m_guiMaster->max_throttle - m_guiMaster->min_throttle);
}

void Containers::design() {

    QVBoxLayout* frameLayout = new QVBoxLayout(this);
    frameLayout->setAlignment(Qt::AlignCenter);

    title_label = new QLabel(title_text, this);
    title_label->setAlignment(Qt::AlignCenter);
    title_label->setFont(QFont("Arial", title_label_size, QFont::Bold));
    title_label->setStyleSheet(QString("color: %1; border: none;").arg(title_label_color));

    frameLayout->addWidget(title_label, 0, Qt::AlignCenter);

    QString initialValueText;

    if (quantity == "Throttle") {
        double percentage = getThrottlePercentage();
        if (percentage < 0) percentage = 0;
        initialValueText = QString("%1 %2").arg(percentage, 0, 'f', 2).arg(unit);
    }
    else {
        initialValueText = QString("%1 %2").arg(value, 0, 'f', 2).arg(unit);
    }

    value_label = new QLabel(initialValueText, this);
    value_label->setAlignment(Qt::AlignCenter);
    value_label->setFont(QFont("Arial", 54, QFont::Bold));
    value_label->setStyleSheet(QString("color: %1; border: none;").arg(THEME_COLOR));
    frameLayout->addWidget(value_label, 0, Qt::AlignCenter);

    if (has_bar) {
        progress_bar = new QProgressBar(this);
        progress_bar->setFixedSize(400, 20);
        progress_bar->setRange(0, qRound(value_limit.value(quantity)));
        progress_bar->setValue(qRound(value));
        progress_bar->setTextVisible(false);
        progress_bar->setStyleSheet(QString("QProgressBar { background-color: white; border-radius: 10px; } QProgressBar::chunk { background-color: %1; }").arg(THEME_COLOR));
        frameLayout->addWidget(progress_bar, 0, Qt::AlignCenter);
    }
}

void Containers::setupControlElements() {

    if (!mode_handler) return;
    control_frame = new QWidget(this);
    QHBoxLayout* controlLayout = new QHBoxLayout(control_frame);
    controlLayout->setSpacing(20);
    controlLayout->setAlignment(Qt::AlignCenter);

    QCheckBox* toggle_switch_widget = new QCheckBox("MANUAL Mode", control_frame);
    toggle_switch_widget->setFixedSize(140, 30);

    QObject::connect(toggle_switch_widget, &QCheckBox::checkStateChanged,
        mode_handler, &ModeHandler::manualToggleEvent);

    QPushButton* slider_opener_button_widget = new QPushButton("Set Throttle", control_frame);
    slider_opener_button_widget->setFixedSize(120, 30);
 
    QString buttonStyle = QString("QPushButton { background-color: %1; color: white; border-radius: 10px; font-weight: bold; }" "QPushButton:hover { background-color: blue; }").arg(THEME_COLOR);
    slider_opener_button_widget->setStyleSheet(buttonStyle);

    QObject::connect(slider_opener_button_widget, &QPushButton::clicked,
        mode_handler, &ModeHandler::openSliderWindow);

    controlLayout->addWidget(toggle_switch_widget);
    controlLayout->addWidget(slider_opener_button_widget);

    mode_handler->slider_opener_button = slider_opener_button_widget;

    if (layout()) {
        static_cast<QVBoxLayout*>(layout())->addWidget(control_frame, 0, Qt::AlignCenter);
    }
}

void Containers::checkBoundsAndSetColor(double currentValue) {
    QString progressColor = THEME_COLOR;
    double compareValue = currentValue;

    if (quantity == "Throttle") {
        compareValue = getThrottlePercentage();
    }


    if (quantity != "Throttle" && quantity != "RPM") {
        if (compareValue <= low_bound) {
            progressColor = low_color;
        }
        else if (compareValue < high_bound) {
            progressColor = mid_color;
        }
        else {
            progressColor = high_color;
        }
    }

    if (has_bar ) {
        progress_bar->setStyleSheet(QString("QProgressBar { background-color: white; border-radius: 10px; } QProgressBar::chunk { background-color: %1; }").arg(progressColor));
    }
}

void Containers::updateData(double newValue, double currentRpm) {

    QString displayValue;
    value = newValue;

    if (mode_handler) {
        mode_handler->handleMode(newValue, currentRpm);
        mode_handler->setValue(newValue);
    }

    if (quantity == "Throttle") {
        double percentage = getThrottlePercentage();
        if (percentage < 0) percentage = 0;
        displayValue = QString("%1 %2").arg(percentage, 0, 'f', 2).arg(unit);

    }
    else {
        displayValue = QString("%1 %2").arg(value, 0, 'f', 2).arg(unit);
    }

    value_label->setText(displayValue);
   
    if (has_bar && progress_bar) {
        if (value >= value_limit.value(quantity)) progress_bar->setValue(qRound(value_limit.value(quantity)));
        else if (value <= 0) progress_bar->setValue(0);
        else progress_bar->setValue(qRound(value));
        
        checkBoundsAndSetColor(value);
    }
}

void Containers::handleModeChange(const QString& newMode) {
    if (control_frame && mode_handler) {
        QCheckBox* toggle = control_frame->findChild<QCheckBox*>();
        if (toggle) {
            toggle->setText(newMode + " Mode");
        }

        QPushButton* opener = mode_handler->slider_opener_button;
        if (opener) {
            if (newMode == "MANUAL") {
                opener->setText("Set Throttle");
            }
            else {
                opener->setText("Set RPM");
            }
        }
    }
}