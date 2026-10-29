#include "containers.h"
#include "constants.h"
#include "backendhandler.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QCheckBox>
#include <QPushButton>
#include "ThrustStandGUI.h"
#include "BackendHandler.h"

Containers::Containers(const QString& title, int titleSize, const QString& titleColor,
    double initialValue, bool hasBar, BackendHandler* backend_handler, QWidget* parent)
    : QFrame(parent), title_text(title), title_label_size(titleSize), title_label_color(titleColor),
    value(initialValue), unit(units_map.value(title)), has_bar(hasBar), quantity(actual_quantity.value(title)),
    backend_handler(backend_handler)   
{
    QString function_mode = backend_handler->get_function_mode();
    low_bound = bounds_map.value(quantity).first;
    high_bound = bounds_map.value(quantity).second;
   
    QVector<QString> colors = progress_bar_colors_map.value(quantity);

    if (colors.size() >= 3) {
        low_color = colors[0];
        mid_color = colors[1];
        high_color = colors[2];
    }
    else {
        low_color = THEME_COLOR;
        mid_color = THEME_COLOR;
        high_color = THEME_COLOR;
        qWarning() << "Warning: Missing color definitions for quantity:" << quantity;
    }

    design();

    if ((quantity == "Throttle") && (function_mode == "Basic")){ // αναλογα το text που βαζω στο title καταλαβαινω το quantity
        setupControlElements();
        QObject::connect(backend_handler, &BackendHandler::modeChanged, this, &Containers::handleModeChange);
    }

    setObjectName("Containers");
    setStyleSheet("QFrame#Containers {" "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
    "stop:0 rgba(30,41,59,200), stop:1 rgba(10,16,30,225));" "border: 1px solid rgba(255,255,255,40);"
    "border-radius: 28px;" "}");
}

void Containers::design() {

    QVBoxLayout* frameLayout = new QVBoxLayout(this);
    frameLayout->setAlignment(Qt::AlignCenter);
    frameLayout->setContentsMargins(24, 22, 24, 22);   // ήταν default
    frameLayout->setSpacing(14);

    if (containerGraphics().contains(quantity)) {
        graphic = containerGraphics().value(quantity)(this);
    }

    const bool useGraphic = containerGraphics().contains(quantity);
    if (!useGraphic) frameLayout->addStretch(1);

    frameLayout->addStretch(1);

    // Τίτλος: κεφαλαία, απαλό γκρι, απόσταση γραμμάτων
    title_label = new QLabel(title_text.toUpper(), this);
    title_label->setAlignment(Qt::AlignCenter);
    QFont tf("Segoe UI", title_label_size, QFont::DemiBold);
    tf.setLetterSpacing(QFont::AbsoluteSpacing, 3);
    title_label->setFont(tf);
    title_label->setStyleSheet("color: rgba(203,213,225,230); border: none; background: transparent;");
    frameLayout->addWidget(title_label, 0, Qt::AlignCenter);

    if (useGraphic) {
        double maxV = value_limit.value(quantity);

        if (quantity == "Throttle") {
            maxV = 100.0;             
        }
        else if (quantity == "RPM" && backend_handler->cfg.max_rpm > 0) {
            maxV = backend_handler->cfg.max_rpm;
        }

        graphic->setRange(0.0, maxV);
        graphic->setValue(toDisplayValue(value));  

        frameLayout->addWidget(graphic, 1);
        return;
    }

    // Τιμή + μονάδα
    double shown = toDisplayValue(value);

    value_label = new QLabel(formatValueText(shown), this);
    value_label->setTextFormat(Qt::RichText);
    value_label->setAlignment(Qt::AlignCenter);
    value_label->setFont(QFont("Segoe UI"));
    value_label->setStyleSheet("border: none; background: transparent;");
    frameLayout->addWidget(value_label, 0, Qt::AlignCenter);

    footer = new QWidget(this);
    footer->setObjectName("CardFooter");
    footer->setFixedHeight(44);
    footer->setStyleSheet("QWidget#CardFooter { background: transparent; border: none; }");
    QVBoxLayout* footerLayout = new QVBoxLayout(footer);
    footerLayout->setContentsMargins(0, 0, 0, 0);
    footerLayout->setAlignment(Qt::AlignCenter);

    if (has_bar) {
        progress_bar = new QProgressBar(this);
        progress_bar->setFixedHeight(10);
        progress_bar->setMinimumWidth(320);
        progress_bar->setRange(0, qRound(value_limit.value(quantity)));
        progress_bar->setValue(qRound(value));
        progress_bar->setTextVisible(false);
        progress_bar->setStyleSheet(barStyle(THEME_COLOR));
        frameLayout->addSpacing(10);
        frameLayout->addWidget(progress_bar);
    }

    frameLayout->addStretch(1);
}

void Containers::setupControlElements() {

    control_frame = new QWidget(this);
    QHBoxLayout* controlLayout = new QHBoxLayout(control_frame);
    controlLayout->setSpacing(20);
    controlLayout->setAlignment(Qt::AlignCenter);

    QCheckBox* toggle_switch_widget = new QCheckBox("MANUAL Mode", control_frame);
    toggle_switch_widget->setFixedSize(190, 34);
    toggle_switch_widget->setStyleSheet(
        "QCheckBox { color: rgba(226,232,240,255); font-size: 14px; font-weight: 600; border: none; background: transparent; }"
        "QCheckBox::indicator { width: 20px; height: 20px; border-radius: 6px;"
        "                       border: 2px solid rgba(255,255,255,90); background: transparent; }"
        "QCheckBox::indicator:checked { background: " + QString(THEME_COLOR) + "; border-color: " + QString(THEME_COLOR) + "; }");
    

    QObject::connect(toggle_switch_widget, &QCheckBox::checkStateChanged,
        backend_handler, &BackendHandler::manualToggleEvent);

    QPushButton* slider_opener_button_widget = new QPushButton("Set Throttle", control_frame);
    slider_opener_button_widget->setFixedSize(150, 36);
    slider_opener_button_widget->setCursor(Qt::PointingHandCursor);
    slider_opener_button_widget->setStyleSheet(
        QString("QPushButton { background: %1; color: white; border: none; border-radius: 18px;"
            "              font-size: 14px; font-weight: 700; }"
            "QPushButton:hover { background: #38bdf8; }"
            "QPushButton:pressed { background: #0369a1; }").arg(THEME_COLOR));
 
    QString buttonStyle = QString("QPushButton { background-color: %1; color: white; border-radius: 10px; font-weight: bold; }" "QPushButton:hover { background-color: blue; }").arg(THEME_COLOR);

    QObject::connect(slider_opener_button_widget, &QPushButton::clicked,
        backend_handler, &BackendHandler::openSliderWindow);

    controlLayout->addWidget(toggle_switch_widget);
    controlLayout->addWidget(slider_opener_button_widget);

    backend_handler->slider_opener_button = slider_opener_button_widget;

    if (layout()) {
        static_cast<QVBoxLayout*>(layout())->addWidget(control_frame, 0, Qt::AlignCenter);
    }
}

void Containers::checkBoundsAndSetColor(double currentValue)
{
    if (!has_bar || !progress_bar)
        return;

    double compareValue = currentValue;

    if (quantity == "Throttle") {
        compareValue = backend_handler->getThrottlePercentage(static_cast<int>(currentValue));   
    }

    QString color = THEME_COLOR;

    if (quantity != "RPM") {
        if (compareValue <= low_bound) {
            color = low_color;
        }
        else if (compareValue < high_bound) {
            color = mid_color;
        }
        else  color = high_color;    
    }

    progress_bar->setStyleSheet(
        QString(
            "QProgressBar {"
            " background-color: white;"
            " border: none;"
            " border-radius: 10px;"
            "}"
            "QProgressBar::chunk {"
            " background-color: %1;"
            " border-radius: 10px;"
            "}"
        ).arg(color)
    );
}

void Containers::updateData(double newValue) {

    QString displayValue;
    value = newValue;

    backend_handler->handleMode();
    backend_handler->setValue(newValue);

    if (graphic) {       
        graphic->setValue(quantity == "Throttle" ? qMax(0.0, backend_handler->getThrottlePercentage(backend_handler->tel.throttle)) : value);
        return;
    }

    double shown = value;
    if (quantity == "Throttle") shown = backend_handler->getThrottlePercentage((int)value);
    
    value_label->setText(formatValueText(shown));


    if (has_bar && progress_bar) {
        double displayProgress = value;

        if (quantity == "Voltage") {
            double minV = low_bound; 
            double maxV = value_limit.value(quantity);

            displayProgress = ((value - minV) / (maxV - minV)) * maxV;
        }

        if (displayProgress >= value_limit.value(quantity))
            progress_bar->setValue(qRound(value_limit.value(quantity)));
        else if (displayProgress <= 0)
            progress_bar->setValue(0);
        else
            progress_bar->setValue(qRound(displayProgress));

        checkBoundsAndSetColor(value);
    }

}

void Containers::handleModeChange(const QString& newMode) {

    if (control_frame && backend_handler) {

        QCheckBox* toggle = control_frame->findChild<QCheckBox*>();
        if (toggle) toggle->setText(newMode + " Mode");
        QPushButton* opener = backend_handler->slider_opener_button;

        if (opener) {
            if (newMode == "MANUAL") {
                opener->setText("Set Throttle");
            }
            else opener->setText("Set RPM");
        }
    }
}

double Containers::toDisplayValue(double raw) const {
    if (quantity == "Throttle")
        return qMax(0.0, backend_handler->getThrottlePercentage(static_cast<int>(raw)));
    return raw;
}

QString Containers::formatValueText(double v) const {
    return QString(
        "<span style='font-size:60pt; font-weight:700; color:white;'>%1</span>"
        "<span style='font-size:24pt; font-weight:600; color:%2;'>&nbsp;%3</span>")
        .arg(v, 0, 'f', 2).arg(THEME_COLOR).arg(unit);
}

QString Containers::barStyle(const QString& color) const {
    return QString(
        "QProgressBar { background-color: rgba(255,255,255,30); border: none; border-radius: 5px; }"
        "QProgressBar::chunk { background-color: %1; border-radius: 5px; }").arg(color);
}