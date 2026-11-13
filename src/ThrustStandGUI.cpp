#include "thruststandgui.h"
#include "containers.h"
#include "backendhandler.h"
#include "constants.h"
#include <functional>

ThrustStandGUI::ThrustStandGUI(QWidget* parent) : QMainWindow(parent)
{
    backend_handler = new BackendHandler(this, nullptr);
    setupSettingsFields();
    setupExperimentFields();
    loadSettings();

    if (backend_handler->registered_controllers.contains(active_controller_name))
        backend_handler->setActiveController(active_controller_name);

    setWindowTitle(WINDOW_NAME);
    resize(RES_WIDTH, RES_HEIGHT);
    setWindowIcon(QIcon(ICONPATH));

    graphic_timer = new QTimer(this);

    createMainScene();

    connect(backend_handler, &BackendHandler::telemetryUpdated, this, &ThrustStandGUI::updateBottomToolbar);
    connect(backend_handler, &BackendHandler::loggingStateChanged, this, [this](bool) { this->updateBottomToolbar(); });
    connect(backend_handler, &BackendHandler::armedStateChanged, this, &ThrustStandGUI::updateArmedState);
    connect(backend_handler, &BackendHandler::experimentFinished, this, &ThrustStandGUI::onExperimentFinished);
    connect(graphic_timer, &QTimer::timeout, this, &ThrustStandGUI::graphicUpdates);
}

ThrustStandGUI::~ThrustStandGUI()
{
    on_closing();
}

// SYSTEM
void ThrustStandGUI::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_F11) {
        if (isFullScreen()) showNormal();
        else showFullScreen();
    }
    else if (event->key() == Qt::Key_Escape && isFullScreen()) {
        showNormal();
    }
    else {
        QMainWindow::keyPressEvent(event);
    }
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
    darkPalette.setColor(QPalette::Highlight, QColor(66, 134, 244));
    darkPalette.setColor(QPalette::HighlightedText, Qt::white);

    QApplication::setPalette(darkPalette);
}

void ThrustStandGUI::closeEvent(QCloseEvent* event) {
    on_closing();
    event->accept();
}

void ThrustStandGUI::on_closing() {
    backend_handler->running = false;
    backend_handler->logToFile(false);
    backend_handler->handleSerial(false);
    backend_handler->disconnect_mqtt();
}

// INTERFACE

void ThrustStandGUI::createMainScene() {
    graphic_timer->stop();

    auto& cfg = backend_handler->cfg;
    auto& mqttcfg = backend_handler->mqttcfg;
    auto& tel = backend_handler->tel;
    auto& cal = backend_handler->cal;

    if (centralWidget()) {
        firstIconButton = nullptr;
        secondIconButton = nullptr;
        thirdIconButton = nullptr;
        statusIconButton = nullptr;
        saveIconButton = nullptr;
        loggingIconButton = nullptr;
        delete centralWidget();
    }

    QWidget* centralWidget = new QWidget(this);
    centralWidget->setObjectName("CentralWidget");
    setCentralWidget(centralWidget);

    QString backgroundStyle = QString(
        "QWidget#CentralWidget {" "background-image: url(%1);"
        "background-position: center;""background-repeat: no-repeat;"
        "}").arg(MAINSCENE_BG);
    centralWidget->setStyleSheet(backgroundStyle);

    QVBoxLayout* mainVLayout = new QVBoxLayout(centralWidget);
    mainVLayout->setContentsMargins(0, 0, 0, 0);
    mainVLayout->setSpacing(0);

    QWidget* topSection = new QWidget(centralWidget);
    topSection->setStyleSheet("background: transparent;");

    QGridLayout* gridLayout = new QGridLayout(topSection);
    gridLayout->setContentsMargins(50, 35, 50, 20);
    gridLayout->setSpacing(20);

    const int TITLE_SIZE = 36;

    instantthrustGUI = new Containers("Instant Thrust", TITLE_SIZE, "white", tel.thrust, true, backend_handler, topSection);
    instantcurrentGUI = new Containers("Instant Current", TITLE_SIZE, "white", tel.current, true, backend_handler, topSection);
    instanttorqueGUI = new Containers("Instant Torque", TITLE_SIZE, "white", tel.torque, false, backend_handler, topSection);
    instantrpmGUI = new Containers("Instant RPM", TITLE_SIZE, "white", tel.rpm, false, backend_handler, topSection);
    instantthrottleGUI = new Containers("Instant Throttle", TITLE_SIZE, "white", (double)tel.throttle, false, backend_handler, topSection);
    instantpowerGUI = new Containers("Instant Power", TITLE_SIZE, "white", tel.power, false, backend_handler, topSection);
    batteryWidget = new BatteryWidget("Instant Voltage", true, topSection, tel.voltage);

    double minV = bounds_map.value("Voltage").first;
    double maxV = bounds_map.value("Voltage").second;
    int initial_battery_pct = std::clamp((int)((tel.voltage - minV) / (maxV - minV) * 100), 0, 100);
    batteryWidget->setPercentage(initial_battery_pct);

    gridLayout->addWidget(instantthrustGUI, 0, 0);
    gridLayout->addWidget(instanttorqueGUI, 0, 2);

    gridLayout->addWidget(instantcurrentGUI, 1, 0);
    gridLayout->addWidget(instantpowerGUI, 1, 2);

    gridLayout->addWidget(instantthrottleGUI, 2, 0);
    gridLayout->addWidget(instantrpmGUI, 2, 2);

    gridLayout->addWidget(batteryWidget, 0, 1, 3, 1);

    gridLayout->setColumnStretch(0, 3);
    gridLayout->setColumnStretch(1, 2);
    gridLayout->setColumnStretch(2, 3);

    gridLayout->setRowStretch(0, 1);
    gridLayout->setRowStretch(1, 1);
    gridLayout->setRowStretch(2, 1);

    mainVLayout->addWidget(topSection, 1);

    QFrame* bottomBar = createBottomToolbar();
    mainVLayout->addWidget(bottomBar);

    this->updateBottomToolbar();

    graphic_timer->start(GRAPHIC_UPDATES_REFRESH_RATE);

    if (backend_handler->get_function_mode() == "Experiment" && !backend_handler->experiment_running) {
        customizeExperiment();
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
    chartsGrid->setContentsMargins(40, 30, 40, 20);
    chartsGrid->setSpacing(24);

    voltageGraph = new GraphContainer("Instant Voltage", "V", chartsArea);
    currentGraph = new GraphContainer("Instant Current", "A", chartsArea);
    powerGraph = new GraphContainer("Instant Power", "W", chartsArea);
    thrustGraph = new GraphContainer("Instant Thrust", "N", chartsArea);
    throttleGraph = new GraphContainer("Instant Throttle", "%", chartsArea);
    rpmGraph = new GraphContainer("Instant RPM", "rpm", chartsArea);

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

    for (int r = 0; r < 3; ++r) chartsGrid->setRowStretch(r, 1);
    chartsGrid->setColumnStretch(0, 1);
    chartsGrid->setColumnStretch(1, 1);

    mainVLayout->addWidget(chartsArea, 1);

    QFrame* bottomBar = createBottomToolbar();
    mainVLayout->addWidget(bottomBar);

    if (graphic_timer && !graphic_timer->isActive()) {
        graphic_timer->start(GRAPHIC_UPDATES_REFRESH_RATE);
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
        instanttorqueGUI = nullptr;
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

    this->updateBottomToolbar();
}

void ThrustStandGUI::graphicUpdates() {
    if (!backend_handler->process_begin) return;

    const auto& tel = backend_handler->tel;

    double currentTime = graphtimeCounter + ((double)GRAPHIC_UPDATES_REFRESH_RATE / 1000.0);
    graphtimeCounter = currentTime;

    auto appendData = [&](QVector<double>& buffer, double value) {
        buffer.append(value); if (buffer.size() > MAX_POINTS) buffer.removeFirst(); };

    timeHistory.append(currentTime);
    if (timeHistory.size() > MAX_POINTS) timeHistory.removeFirst();

    double throttle_percentage = backend_handler->getThrottlePercentage(tel.throttle);

    appendData(thrustHistory, tel.thrust);
    appendData(rpmHistory, tel.rpm);
    appendData(voltageHistory, tel.voltage);
    appendData(currentHistory, tel.current);
    appendData(powerHistory, tel.power);
    appendData(throttleHistory, throttle_percentage);

    if (current_scene == "MAIN" && instantthrustGUI != nullptr) {
        instantthrustGUI->updateData(tel.thrust);
        instantcurrentGUI->updateData(tel.current);
        instanttorqueGUI->updateData(tel.torque);
        instantthrottleGUI->updateData((double)tel.throttle);
        instantrpmGUI->updateData(tel.rpm);
        instantpowerGUI->updateData(tel.power);

        if (batteryWidget) {
            double minV = bounds_map.value("Voltage").first;
            double maxV = bounds_map.value("Voltage").second;

            int pct = 0;
            pct = std::clamp((int)((tel.voltage - minV) / (maxV - minV) * 100.0), 0, 100);
            batteryWidget->setPercentage(pct);
            batteryWidget->setVoltage(tel.voltage);
        }

    }
    else if (current_scene == "GRAPH" && thrustGraph != nullptr) {
        thrustGraph->addDataPoint(graphtimeCounter, tel.thrust);
        rpmGraph->addDataPoint(graphtimeCounter, tel.rpm);
        voltageGraph->addDataPoint(graphtimeCounter, tel.voltage);
        currentGraph->addDataPoint(graphtimeCounter, tel.current);
        powerGraph->addDataPoint(graphtimeCounter, tel.power);
        throttleGraph->addDataPoint(graphtimeCounter, throttle_percentage);
    }
}

void ThrustStandGUI::calibrate_parameters_window() {
    auto& cal = backend_handler->cal;

    QWidget* cal_window = new QWidget(nullptr, Qt::Window);
    cal_window->setObjectName("ModernDialog");
    cal_window->setWindowTitle("Sensor Calibration");
    cal_window->setMinimumSize(560, 850);
    cal_window->setWindowModality(Qt::ApplicationModal);
    cal_window->setAttribute(Qt::WA_DeleteOnClose);
    cal_window->setAttribute(Qt::WA_StyledBackground, true);
    cal_window->setWindowIcon(QIcon(ICONPATH));
    cal_window->setStyleSheet(DIALOG_STYLE);

    QVBoxLayout* mainLayout = new QVBoxLayout(cal_window);
    mainLayout->setContentsMargins(32, 28, 32, 28);
    mainLayout->setSpacing(8);

    // --- Helpers ---
    auto makeSectionLabel = [cal_window](const QString& text) {
        QLabel* l = new QLabel(text, cal_window);
        l->setObjectName("SectionLabel");
        QFont f("Segoe UI", 10, QFont::DemiBold);
        f.setLetterSpacing(QFont::AbsoluteSpacing, 2);
        l->setFont(f);
        return l;
        };

    auto makeGroupTitle = [cal_window](const QString& text) {
        QLabel* l = new QLabel(text, cal_window);
        l->setObjectName("DialogHeader");
        QFont f("Segoe UI", 13, QFont::DemiBold);
        f.setLetterSpacing(QFont::AbsoluteSpacing, 3);
        l->setFont(f);
        return l;
        };

    auto makeHint = [cal_window](const QString& text) {
        QLabel* l = new QLabel(text, cal_window);
        l->setObjectName("HintLabel");
        l->setWordWrap(true);
        return l;
        };

    auto makeDivider = [cal_window]() {
        QFrame* line = new QFrame(cal_window);
        line->setFixedHeight(1);
        line->setStyleSheet("background-color: rgba(255,255,255,40); border: none;");
        return line;
        };

    auto makeNumberField = [cal_window](double value, int decimals) {
        QLineEdit* e = new QLineEdit(QString::number(value, 'f', decimals), cal_window);
        e->setMinimumHeight(40);
        QDoubleValidator* v = new QDoubleValidator(-10000000.0, 10000000.0, 10, e);
        v->setNotation(QDoubleValidator::StandardNotation);
        v->setLocale(QLocale::c());
        e->setValidator(v);
        return e;
        };

    // --- Επικεφαλίδα ---
    QLabel* header = new QLabel("SENSOR CALIBRATION (PC-SIDE)", cal_window);
    header->setObjectName("DialogHeader");
    QFont hf("Segoe UI", 15, QFont::DemiBold);
    hf.setLetterSpacing(QFont::AbsoluteSpacing, 3);
    header->setFont(hf);
    header->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(header);
    mainLayout->addSpacing(14);

    // =========== CURRENT ===========
    mainLayout->addWidget(makeGroupTitle("CURRENT"));
    mainLayout->addSpacing(4);

    QCheckBox* checkManualCurrent = new QCheckBox("Enable PC-Side Current Calibration", cal_window);
    checkManualCurrent->setChecked(cal.manual_current_calibration);
    checkManualCurrent->setCursor(Qt::PointingHandCursor);
    checkManualCurrent->setMinimumHeight(34);
    mainLayout->addWidget(checkManualCurrent);

    mainLayout->addWidget(makeSectionLabel("SELECTED LSB (A/BIT)"));
    QLineEdit* editLsbSelected = makeNumberField(cal.current_lsb_user_selected, 8);
    mainLayout->addWidget(editLsbSelected);
    mainLayout->addWidget(makeHint(QString("ESP/Arduino default LSB: %1")
        .arg(cal.current_lsb_user_esp, 0, 'f', 8)));

    mainLayout->addSpacing(12);
    mainLayout->addWidget(makeDivider());
    mainLayout->addSpacing(12);

    // =========== THRUST ===========
    mainLayout->addWidget(makeGroupTitle("THRUST (LOAD CELL)"));
    mainLayout->addSpacing(4);

    QCheckBox* checkManualThrust = new QCheckBox("Enable PC-Side Thrust Calibration", cal_window);
    checkManualThrust->setChecked(cal.manual_thrust_calibration);
    checkManualThrust->setCursor(Qt::PointingHandCursor);
    checkManualThrust->setMinimumHeight(34);
    mainLayout->addWidget(checkManualThrust);

    mainLayout->addWidget(makeSectionLabel("THRUST CALIBRATION FACTOR (STEPS/GRAM)"));
    QLineEdit* editThrustCalibrationFactor = makeNumberField(cal.thrust_calibration_factor, 6);
    mainLayout->addWidget(editThrustCalibrationFactor);
    mainLayout->addSpacing(6);

    mainLayout->addWidget(makeSectionLabel("TARE OFFSET (RAW ZERO VALUE)"));
    QLineEdit* editTareNewtonOffset = makeNumberField(cal.tare_newton_offset, 2);
    mainLayout->addWidget(editTareNewtonOffset);
    mainLayout->addSpacing(6);

    mainLayout->addWidget(makeSectionLabel("ROTATION DIRECTION"));
    QComboBox* comboDirection = new QComboBox(cal_window);
    comboDirection->setMinimumHeight(40);
    comboDirection->addItem("Standard (1.0)", 1.0);
    comboDirection->addItem("Inverted (-1.0)", -1.0);
    int dirIndex = (cal.direction_sign > 0) ? 0 : 1;
    comboDirection->setCurrentIndex(dirIndex);
    mainLayout->addWidget(comboDirection);

    mainLayout->addSpacing(12);
    mainLayout->addWidget(makeDivider());
    mainLayout->addSpacing(12);

    // =========== TORQUE ===========
    mainLayout->addWidget(makeGroupTitle("TORQUE (LOAD CELLS)"));
    mainLayout->addSpacing(4);

    QCheckBox* checkManualTorque = new QCheckBox("Enable PC-Side Torque Calibration", cal_window);
    checkManualTorque->setChecked(cal.manual_torque_calibration);
    checkManualTorque->setCursor(Qt::PointingHandCursor);
    checkManualTorque->setMinimumHeight(34);
    mainLayout->addWidget(checkManualTorque);

    mainLayout->addWidget(makeSectionLabel("TORQUE CALIBRATION FACTOR"));
    QLineEdit* editTorqueCalibrationFactor = makeNumberField(cal.torque_calibration_factor, 6);
    mainLayout->addWidget(editTorqueCalibrationFactor);
    mainLayout->addSpacing(6);

    mainLayout->addWidget(makeSectionLabel("TARE TORQUE OFFSET (RAW ZERO VALUE)"));
    QLineEdit* editTareTorqueOffset = makeNumberField(cal.tare_torque_offset, 2);
    mainLayout->addWidget(editTareTorqueOffset);

    mainLayout->addStretch();

    // --- Apply ---
    QPushButton* btnConfirm = new QPushButton("Apply Calibration", cal_window);
    btnConfirm->setObjectName("PrimaryButton");
    btnConfirm->setFixedHeight(48);
    btnConfirm->setCursor(Qt::PointingHandCursor);
    mainLayout->addWidget(btnConfirm);

    connect(btnConfirm, &QPushButton::clicked, this, [this, &cal,
        checkManualCurrent, editLsbSelected,
        checkManualThrust, editThrustCalibrationFactor, comboDirection, editTareNewtonOffset,
        checkManualTorque, editTorqueCalibrationFactor, editTareTorqueOffset,
        cal_window]() {

            // Διαβάζουμε τις τιμές του UI
            bool  next_manual_current = checkManualCurrent->isChecked();
            float next_lsb = editLsbSelected->text().toFloat();

            bool  next_manual_thrust = checkManualThrust->isChecked();
            float next_thrust_factor = editThrustCalibrationFactor->text().toFloat();
            float next_direction = comboDirection->currentData().toFloat();
            float next_tare_thrust = editTareNewtonOffset->text().toFloat();

            bool  next_manual_torque = checkManualTorque->isChecked();
            float next_torque_factor = editTorqueCalibrationFactor->text().toFloat();
            float next_tare_torque = editTareTorqueOffset->text().toFloat();

            // Έλεγχος αν άλλαξε οτιδήποτε για το Unsaved Indicator
            if (cal.manual_current_calibration != next_manual_current ||
                cal.current_lsb_user_selected != next_lsb ||
                cal.manual_thrust_calibration != next_manual_thrust ||
                (cal.thrust_calibration_factor != next_thrust_factor && next_thrust_factor != 0) ||
                cal.direction_sign != next_direction ||
                cal.tare_newton_offset != next_tare_thrust ||
                cal.manual_torque_calibration != next_manual_torque ||
                (cal.torque_calibration_factor != next_torque_factor && next_torque_factor != 0) ||
                cal.tare_torque_offset != next_tare_torque)
            {
                backend_handler->unsaved_settings_changes = true;
                this->updateBottomToolbar();
            }

            // Εφαρμογή Current
            cal.manual_current_calibration = next_manual_current;
            cal.current_lsb_user_selected = next_lsb;

            // Εφαρμογή Thrust
            cal.manual_thrust_calibration = next_manual_thrust;
            if (next_thrust_factor != 0) cal.thrust_calibration_factor = next_thrust_factor;
            cal.direction_sign = next_direction;
            cal.tare_newton_offset = next_tare_thrust;

            // Εφαρμογή Torque
            cal.manual_torque_calibration = next_manual_torque;
            if (next_torque_factor != 0) cal.torque_calibration_factor = next_torque_factor;
            cal.tare_torque_offset = next_tare_torque;

            backend_handler->playAudio(backend_handler->audio_click_sfx_volume, "MainClick");

            cal_window->close();
        });

    cal_window->show();
}

void ThrustStandGUI::tune_controller_window() {
    QWidget* tune_controller_window = new QWidget(nullptr, Qt::Window);
    tune_controller_window->setObjectName("ModernDialog");
    tune_controller_window->setWindowTitle("Controller Tuning");
    tune_controller_window->setAttribute(Qt::WA_DeleteOnClose);
    tune_controller_window->setWindowIcon(QIcon(ICONPATH));
    tune_controller_window->setAttribute(Qt::WA_StyledBackground, true);
    tune_controller_window->setStyleSheet(DIALOG_STYLE);
    tune_controller_window->setMinimumSize(760, 520);
    tune_controller_window->setWindowModality(Qt::ApplicationModal);

    QVBoxLayout* mainLayout = new QVBoxLayout(tune_controller_window);
    mainLayout->setContentsMargins(32, 28, 32, 28);
    mainLayout->setSpacing(18);

    QLabel* tuneHeader = new QLabel("SELECT & TUNE CONTROLLER");
    tuneHeader->setObjectName("DialogHeader");
    QFont hf("Segoe UI", 15, QFont::DemiBold);
    hf.setLetterSpacing(QFont::AbsoluteSpacing, 3);
    tuneHeader->setFont(hf);
    tuneHeader->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(tuneHeader);

    QComboBox* selectControllerBox = new QComboBox(tune_controller_window);
    selectControllerBox->setMinimumHeight(40);
    for (const QString& controller_name : backend_handler->registered_controllers.keys()) {
        selectControllerBox->addItem(controller_name);
    }
    mainLayout->addWidget(selectControllerBox);

    QFrame* dynamicFieldsFrame = new QFrame(tune_controller_window);
    QVBoxLayout* dynamicLayout = new QVBoxLayout(dynamicFieldsFrame);
    dynamicLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(dynamicFieldsFrame);

    QSharedPointer<QMap<QString, QLineEdit*>> dynamic_input_fields(new QMap<QString, QLineEdit*>());

    auto populateFields = [=](const QString& controller_name) {
        QLayoutItem* item;
        while ((item = dynamicLayout->takeAt(0)) != nullptr) {
            if (item->widget()) delete item->widget();
            delete item;
        }
        dynamic_input_fields->clear();

        Controller* ctrl = backend_handler->registered_controllers.value(controller_name, nullptr);
        if (!ctrl) return;

        QFormLayout* formLayout = new QFormLayout();
        formLayout->setHorizontalSpacing(24);
        formLayout->setVerticalSpacing(18);
        formLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        formLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);

        QVector<QPair<QString, double>> params = ctrl->return_parameter_list();

        for (const auto& param : params) {
            QString param_name = param.first;
            double param_value = param.second;

            double max_bound = (std::abs(param_value) > 0.001) ? std::abs(param_value) * 3.0 : 1.0;

            QHBoxLayout* rowLayout = new QHBoxLayout();
            QSlider* slider = new QSlider(Qt::Horizontal);
            slider->setRange(0, 1000);
            slider->setMinimumWidth(260);
            slider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            slider->setValue(qRound((param_value / max_bound) * 1000.0));

            QLineEdit* editField = new QLineEdit(QString::number(param_value, 'f', 4));
            auto* validator = new QDoubleValidator(-10000.0, 10000.0, 4, editField);
            validator->setNotation(QDoubleValidator::StandardNotation);
            validator->setLocale(QLocale::c());
            editField->setValidator(validator);
            editField->setFixedWidth(110);

            connect(slider, &QSlider::valueChanged, [editField, max_bound](int val) {
                double real_val = (val / 1000.0) * max_bound;
                editField->setText(QString::number(real_val, 'f', 4));
                });

            connect(editField, &QLineEdit::textChanged, [slider, max_bound](const QString& text) {
                double val = text.toDouble();
                int slider_val = qRound((val / max_bound) * 1000.0);
                slider->blockSignals(true);
                slider->setValue(slider_val);
                slider->blockSignals(false);
                });

            rowLayout->addWidget(slider);
            rowLayout->addWidget(editField);

            formLayout->addRow(param_name + ":", rowLayout);
            dynamic_input_fields->insert(param_name, editField);
        }

        QWidget* formContainer = new QWidget();
        formContainer->setLayout(formLayout);
        formContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        dynamicLayout->addWidget(formContainer);
        };

    connect(selectControllerBox, &QComboBox::currentTextChanged, tune_controller_window, populateFields);

    QString initialController = active_controller_name;
    if (!backend_handler->registered_controllers.contains(initialController))
        initialController = selectControllerBox->itemText(0);

    selectControllerBox->blockSignals(true);
    selectControllerBox->setCurrentText(initialController);
    selectControllerBox->blockSignals(false);
    populateFields(initialController);

    mainLayout->addStretch();
    QPushButton* btnConfirm = new QPushButton("Apply Tuning");
    btnConfirm->setObjectName("PrimaryButton");
    btnConfirm->setFixedHeight(48);
    btnConfirm->setCursor(Qt::PointingHandCursor);
    mainLayout->addWidget(btnConfirm);


    connect(btnConfirm, &QPushButton::clicked, this, [=]() {
        QString selectedName = selectControllerBox->currentText();
        Controller* ctrl = backend_handler->registered_controllers.value(selectedName, nullptr);

        if (ctrl) {
            // 1. Παίρνουμε τις παλιές παραμέτρους για να δούμε αν άλλαξε κάτι
            QVector<QPair<QString, double>> old_params = ctrl->return_parameter_list();
            QMap<QString, double> old_map;
            for (const auto& p : old_params) {
                old_map.insert(p.first, p.second);
            }

            QVector<QPair<QString, double>> updated_params;
            bool changed = (active_controller_name != selectedName);

            auto& c = backend_handler->ctrl_cfg;
            auto& cfg = backend_handler->cfg;

            for (auto it = dynamic_input_fields->constBegin(); it != dynamic_input_fields->constEnd(); ++it) {
                QString key = it.key();
                double new_val = it.value()->text().replace(",", ".").toDouble();
                updated_params.push_back(qMakePair(key, new_val));

                // Έλεγχος αν η τιμή άλλαξε σε σχέση με την προηγούμενη
                if (std::abs(old_map.value(key, 0.0) - new_val) > 1e-6) {
                    changed = true;
                }

                // 2. Συγχρονισμός με τις μεταβλητές που διαβάζει η saveSettings()
                if (key == "Throttle Increase Step") {
                    cfg.throttle_increase_step = static_cast<int>(qRound(new_val));
                }
                else if (key == "Kp Value") c.Kp = new_val;
                else if (key == "Ki Value") c.Ki = new_val;
                else if (key == "Kd Value") c.Kd = new_val;
                else if (key == "Clamping Authority (For Clamping Anti-Windup)") c.clamping_authority = new_val;
                else if (key == "b0 (System Gain)") c.b0 = new_val;
                else if (key == "Wc (Controller Bandwidth)") c.wc = new_val;
                else if (key == "Wo (Observer Bandwidth)") c.wo = new_val;
            }

            ctrl->set_new_parameters(updated_params);
            this->backend_handler->setActiveController(selectedName);

            active_controller_name = selectedName;
            updateControllerIndicator();

            // 3. Αν υπήρξε αλλαγή, ενεργοποιούμε το κουμπί Save στο toolbar
            if (changed) {
                backend_handler->unsaved_settings_changes = true;
                this->updateBottomToolbar();
            }

            backend_handler->playAudio(backend_handler->audio_click_sfx_volume, "MainClick");
            qDebug() << "Tuned and activated Controller:" << selectedName;
        }
        tune_controller_window->close();
        });

    tune_controller_window->show();
}

QFrame* ThrustStandGUI::createBottomToolbar() {
    QFrame* toolbarFrame = new QFrame();
    toolbarFrame->setObjectName("BottomToolbar");
    toolbarFrame->setFixedHeight(100);
    toolbarFrame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    toolbarFrame->setStyleSheet(
        "QFrame#BottomToolbar {"
        "   background-color: rgba(8, 14, 28, 230);"
        "   border-top: 1px solid rgba(255, 255, 255, 40);"
        "   border-bottom: none; border-left: none; border-right: none;"
        "}"
    );

    QHBoxLayout* toolbarLayout = new QHBoxLayout(toolbarFrame);
    toolbarLayout->setContentsMargins(20, 0, 20, 0);
    toolbarLayout->setSpacing(10);

    int clickVol = backend_handler->audio_click_sfx_volume;

    auto createIcon = [&](const QString& iconPath, IconType type) {
        QPushButton* btn = new QPushButton();
        btn->setFixedSize(90, 90);
        btn->setIcon(QIcon(iconPath));
        btn->setIconSize(QSize(70, 70));
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(
            "QPushButton { background: transparent; border: none; border-radius: 18px; }"
            "QPushButton:hover { background-color: rgba(255, 255, 255, 28); }"
            "QPushButton:pressed { background-color: rgba(255, 255, 255, 50); }"
        );

        switch (type) {
        case Mode:
            firstIconButton = btn;
            QObject::connect(btn, &QPushButton::clicked, this, [this]() {
                backend_handler->playAudio(backend_handler->audio_click_sfx_volume, "MainClick");
                this->changeFunctionMode();
                });
            break;
        case Scene:
            secondIconButton = btn;
            QObject::connect(btn, &QPushButton::clicked, this, [this]() {
                backend_handler->playAudio(backend_handler->audio_click_sfx_volume, "MainClick");
                this->changeScene();
                });
            break;
        case EscapeExperiment:
            thirdIconButton = btn;
            QObject::connect(btn, &QPushButton::clicked, this, [this]() {
                backend_handler->playAudio(backend_handler->audio_click_sfx_volume, "MainClick");
                this->escapeExperiment();
                });
            break;
        case Settings:
            QObject::connect(btn, &QPushButton::clicked, this, [this]() {
                backend_handler->playAudio(backend_handler->audio_click_sfx_volume, "MainClick");
                this->chooseSettings();
                });
            break;
        case Calibration:
            QObject::connect(btn, &QPushButton::clicked, this, [this]() {
                backend_handler->playAudio(backend_handler->audio_click_sfx_volume, "MainClick");
                this->calibrate_parameters_window();
                }); 
            break;
        case Propeller:
            QObject::connect(btn, &QPushButton::clicked, this, [this]() {
                backend_handler->playAudio(backend_handler->audio_click_sfx_volume, "MainClick");
                this->inspect_propeller_logs();
                });
            break;
        case Logging:
            loggingIconButton = btn;
            QObject::connect(btn, &QPushButton::clicked, this, [this]() {
                this->updateBottomToolbar();
                });
            break;
        case Tuning:
            controllerTuningIconButton = btn;
            QObject::connect(btn, &QPushButton::clicked, this, [this]() {
                backend_handler->playAudio(backend_handler->audio_click_sfx_volume, "MainClick");
                this->tune_controller_window();
                });
            break;
        case StopMotor:
            stopMotorIconButton = btn;
            QObject::connect(btn, &QPushButton::clicked, this, [this]() {
                backend_handler->playAudio(backend_handler->audio_click_sfx_volume, "MainClick");
                this->stopMotor();
                });
            break;
        case Status:
            statusIconButton = btn;
            break;
        case Save:
            saveIconButton = btn;
            QObject::connect(btn, &QPushButton::clicked, this, [this]() {
                if (backend_handler->unsaved_settings_changes) {
                    backend_handler->playAudio(backend_handler->audio_click_sfx_volume, "MainClick");
                    this->saveSettings();
                }
                });
            break;
        }

        return btn;
        };

    toolbarLayout->addWidget(createIcon(first_icon_path, Mode));
    toolbarLayout->addWidget(createIcon(second_icon_path, Scene));
    toolbarLayout->addWidget(createIcon(third_icon_path, EscapeExperiment));
    toolbarLayout->addWidget(createIcon(controller_tuning_icon_path, Tuning));
    toolbarLayout->addWidget(createIcon(CALIBRATIONICONPATH, Calibration));
    toolbarLayout->addWidget(createIcon(PROPELLERICONPATH, Propeller));
    toolbarLayout->addWidget(createIcon(stop_motor_icon_path, StopMotor));
    toolbarLayout->addWidget(createIcon(status_icon_path, Status));
    toolbarLayout->addWidget(createIcon(logging_icon_path, Logging));
    controllerIndicator = new QLabel();
    controllerIndicator->setObjectName("ControllerPill");
    controllerIndicator->setAlignment(Qt::AlignCenter);
    controllerIndicator->setFixedHeight(36);
    controllerIndicator->setMinimumWidth(170);
    controllerIndicator->setToolTip("Active controller");
    controllerIndicator->setStyleSheet(
        "QLabel#ControllerPill {"
        "  background: rgba(30,144,255,28); color: #7dc3ff;"
        "  border: 1px solid rgba(30,144,255,120); border-radius: 18px;"
        "  padding: 0 16px; font-family: 'Segoe UI'; font-size: 14px; font-weight: 700;"
        "}");
    toolbarLayout->addSpacing(8);
    toolbarLayout->addWidget(controllerIndicator);
    updateControllerIndicator();

    toolbarLayout->addStretch(2);

    auto createBottomButton = [&](const QString& text, std::function<void()> callback) {
        QPushButton* btn = new QPushButton(text);
        btn->setFixedHeight(52);
        btn->setMinimumWidth(150);
        btn->setMaximumWidth(text == "Choose Log Location" ? 290 : 250);
        btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(QString(
            "QPushButton {"
            "   background-color: rgba(255, 255, 255, 12);"
            "   color: #e2e8f0;"
            "   border: 1px solid rgba(255, 255, 255, 70);"
            "   border-radius: 26px;"
            "   padding: 0 14px;"
            "   font-family: 'Segoe UI'; font-size: 18px; font-weight: 600;"
            "}"
            "QPushButton:hover { background-color: rgba(30, 144, 255, 60); border-color: %1; color: white; }"
            "QPushButton:pressed { background-color: rgba(30, 144, 255, 110); }"
        ).arg(THEME_COLOR));
        QObject::connect(btn, &QPushButton::clicked, this, [this, callback]() {
            backend_handler->playAudio(backend_handler->audio_click_sfx_volume, "MainClick");
            callback();
            });
        return btn;
        };

    toolbarLayout->addWidget(createBottomButton("Open Log File", [this]() { backend_handler->openLogFile(); }));
    toolbarLayout->addWidget(createBottomButton("Choose Log Location", [this]() { backend_handler->chooseLogLocation(); }));
    toolbarLayout->addWidget(createBottomButton("Serial Control", [this]() { this->serial_control(); }));
    toolbarLayout->addWidget(createIcon(save_icon_path, Save));
    toolbarLayout->addWidget(createIcon(SETTINGSICONPATH, Settings));

    return toolbarFrame;
}

void ThrustStandGUI::updateControllerIndicator() {
    if (controllerIndicator)
        controllerIndicator->setText("●  " + active_controller_name.toUpper());
}

void ThrustStandGUI::updateBottomToolbar() {
    if (current_scene == "GRAPH") second_icon_path = HOMEICONPATH;
    else second_icon_path = GRAPHICONPATH;

    if (backend_handler->experiment_running) third_icon_path = EXPERIMENT_RUNNING_ICONPATH;
    else third_icon_path = EXPERIMENT_NOT_RUNNING_ICONPATH;

    if (backend_handler->is_armed) status_icon_path = ARMED_ICONPATH;
    else status_icon_path = DISARMED_ICONPATH;

    if (backend_handler->unsaved_settings_changes) save_icon_path = SAVEICONPATH;
    else save_icon_path = SAVEICON_LOW_OPACITY_PATH;

    if (backend_handler->write_logs) logging_icon_path = LOGGING_ON_ICONPATH;
    else logging_icon_path = LOGGING_OFF_ICONPATH;

    if (firstIconButton) {
        first_icon_path = (backend_handler->get_function_mode() == "Basic") ? BASIC_MODE_ICONPATH : EXPERIMENT_MODE_ICONPATH;
        firstIconButton->setIcon(QIcon(first_icon_path));
    }
    if (secondIconButton) secondIconButton->setIcon(QIcon(second_icon_path));
    if (thirdIconButton) thirdIconButton->setIcon(QIcon(third_icon_path));
    if (statusIconButton) statusIconButton->setIcon(QIcon(status_icon_path));
    if (saveIconButton) saveIconButton->setIcon(QIcon(save_icon_path));
    if (loggingIconButton) loggingIconButton->setIcon(QIcon(logging_icon_path));
}

void ThrustStandGUI::changeFunctionMode() {
    QString function_mode = backend_handler->get_function_mode();
    if (lastFunctionModeChange.isValid() && lastFunctionModeChange.elapsed() < 100) {
        return;
    }
    lastFunctionModeChange.start();

    if (current_scene != "GRAPH") {
        function_mode = (function_mode == "Basic") ? "Experiment" : "Basic";
        backend_handler->set_function_mode(function_mode);
        this->updateBottomToolbar();
        this->createMainScene();
    }
    else {
        backend_handler->playAudio(100, "ChangeToHomeFirst");
    }
}

// DIFFERENT
void ThrustStandGUI::updateControllers() {

    auto& ctrl = backend_handler->ctrl_cfg;
    auto& cfg = backend_handler->cfg;

    if (backend_handler->step_controller) {
        backend_handler->step_controller->set_new_parameters({
            qMakePair(QString("Throttle Increase Step"), (double)cfg.throttle_increase_step)
            });
    }
    if (backend_handler->pid_controller) {
        backend_handler->pid_controller->set_new_parameters({
            qMakePair(QString("Kp Value"), ctrl.Kp),
            qMakePair(QString("Ki Value"), ctrl.Ki),
            qMakePair(QString("Kd Value"), ctrl.Kd),
            qMakePair(QString("Clamping Authority (For Clamping Anti-Windup)"), ctrl.clamping_authority)
            });
    }
    if (backend_handler->adrc_controller) {
        backend_handler->adrc_controller->set_new_parameters({
            qMakePair(QString("b0 (System Gain)"), ctrl.b0),
            qMakePair(QString("Wc (Controller Bandwidth)"), ctrl.wc),
            qMakePair(QString("Wo (Observer Bandwidth)"), ctrl.wo)
            });
    }

    updateBottomToolbar();
    qDebug() << "Settings loaded successfully!";
}

void ThrustStandGUI::stopMotor() {
    backend_handler->executeEmergencyStop();
}

void ThrustStandGUI::escapeExperiment() {
    if (backend_handler->experiment_running) {
        if (backend_handler->auto_mode_timer) {
            backend_handler->auto_mode_timer->stop();
        }
        onExperimentFinished();
        backend_handler->playAudio(backend_handler->audio_click_sfx_volume, "ExperimentCancelled");
    }
}

void ThrustStandGUI::inspect_propeller_logs() {
    PropellerWindow* propWindow = new PropellerWindow(backend_handler);
    propWindow->show();
} 

void ThrustStandGUI::updateArmedState(bool new_arm_status) {

    backend_handler->is_armed = new_arm_status;

    if (firstIconButton) {
        this->updateBottomToolbar();
    }

    if (new_arm_status) {
        if (!backend_handler->isSoundPlaying("TelemetryConnected")) {
            backend_handler->playAudio(100, "Armed");
        }
    }
    else {
        backend_handler->playAudio(100, "Disarmed");
    }
}

double ThrustStandGUI::getThrottlePercentage(int value) {
    return backend_handler->getThrottlePercentage(value);
}

void ThrustStandGUI::serial_control() {
    QWidget* selection_window = new QWidget(nullptr, Qt::Window);
    selection_window->setObjectName("ModernDialog");
    selection_window->setWindowTitle("Serial Control");
    selection_window->setMinimumSize(600, 560);
    selection_window->setWindowModality(Qt::ApplicationModal);
    selection_window->setAttribute(Qt::WA_DeleteOnClose);
    selection_window->setAttribute(Qt::WA_StyledBackground, true);
    selection_window->setWindowIcon(QIcon(ICONPATH));
    selection_window->setStyleSheet(DIALOG_STYLE);

    QVBoxLayout* layout = new QVBoxLayout(selection_window);
    layout->setContentsMargins(32, 28, 32, 28);
    layout->setSpacing(12);

    // Μικρά labels με κεφαλαία και απόσταση γραμμάτων
    auto makeSectionLabel = [selection_window](const QString& text) {
        QLabel* l = new QLabel(text, selection_window);
        l->setObjectName("SectionLabel");
        QFont f("Segoe UI", 10, QFont::DemiBold);
        f.setLetterSpacing(QFont::AbsoluteSpacing, 2);
        l->setFont(f);
        return l;
        };

    // --- Επικεφαλίδα ---
    QLabel* header = new QLabel("SERIAL CONTROL", selection_window);
    header->setObjectName("DialogHeader");
    QFont hf("Segoe UI", 15, QFont::DemiBold);
    hf.setLetterSpacing(QFont::AbsoluteSpacing, 3);
    header->setFont(hf);
    header->setAlignment(Qt::AlignCenter);
    layout->addWidget(header);
    layout->addSpacing(10);

    // --- COM port ---
    layout->addWidget(makeSectionLabel("AVAILABLE COM PORTS"));

    QComboBox* dropdown1 = new QComboBox(selection_window);
    dropdown1->setMinimumHeight(40);

    QStringList ports = backend_handler->getAvailablePorts();
    dropdown1->addItems(ports);

    QString current_port = backend_handler->serial_com_port_tag;
    if (!current_port.isEmpty() && ports.contains(current_port)) {
        dropdown1->setCurrentText(current_port);
    }
    layout->addWidget(dropdown1);
    layout->addSpacing(6);

    // --- Baudrate ---
    layout->addWidget(makeSectionLabel("BAUDRATE"));

    QComboBox* dropdown2 = new QComboBox(selection_window);
    dropdown2->setMinimumHeight(40);
    dropdown2->addItems(backend_handler->baudrates);
    dropdown2->setCurrentText(QString::number(backend_handler->serial_baudrate));
    layout->addWidget(dropdown2);
    layout->addSpacing(8);

    // --- Connect ---
    QPushButton* confirmButton = new QPushButton("Connect", selection_window);
    confirmButton->setObjectName("PrimaryButton");
    confirmButton->setFixedHeight(46);
    confirmButton->setCursor(Qt::PointingHandCursor);
    layout->addWidget(confirmButton);

    // --- Διαχωριστική γραμμή ---
    layout->addSpacing(10);
    QFrame* line = new QFrame(selection_window);
    line->setFixedHeight(1);
    line->setStyleSheet("background-color: rgba(255,255,255,40); border: none;");
    layout->addWidget(line);
    layout->addSpacing(10);

    // --- Αποστολή εντολής ---
    layout->addWidget(makeSectionLabel("SEND COMMAND"));

    QLineEdit* commandInput = new QLineEdit(selection_window);
    commandInput->setMinimumHeight(40);
    commandInput->setPlaceholderText("Enter command...");
    layout->addWidget(commandInput);

    QPushButton* sendButton = new QPushButton("Send", selection_window);
    sendButton->setFixedHeight(46);
    sendButton->setCursor(Qt::PointingHandCursor);
    sendButton->setEnabled(backend_handler->isSerialConnected());
    layout->addWidget(sendButton);

    layout->addStretch();

    // --- Connect ---
    connect(confirmButton, &QPushButton::clicked, this, [this, dropdown1, dropdown2, sendButton, confirmButton]() {
        bool success = backend_handler->connectSerial(dropdown1->currentText(), dropdown2->currentText().toInt());

        if (success) {
            sendButton->setEnabled(true);
            confirmButton->setText("Connected ✓");
            confirmButton->setStyleSheet(
                "QPushButton { background-color: rgba(16,185,129,45); color: #34d399;"
                "  border: 1px solid #10b981; border-radius: 22px; font-size: 15px; font-weight: 600; }");
        }
        else {
            confirmButton->setText("Connection Failed");
            confirmButton->setStyleSheet(
                "QPushButton { background-color: rgba(239,68,68,45); color: #f87171;"
                "  border: 1px solid #ef4444; border-radius: 22px; font-size: 15px; font-weight: 600; }");
        }
        });

    // --- Send ---
    auto sendAction = [this, commandInput]() {
        QString cmd = commandInput->text();
        if (!cmd.isEmpty()) {
            backend_handler->serial_sendCommand(cmd);
            commandInput->clear();
        }
        };

    connect(sendButton, &QPushButton::clicked, this, sendAction);
    connect(commandInput, &QLineEdit::returnPressed, this, sendAction);

    selection_window->show();
}

// SETTINGS FUNCTIONS

void ThrustStandGUI::setupSettingsFields() {
    
    auto& cfg = backend_handler->cfg;
    auto& cal = backend_handler->cal;
    auto& mqttcfg = backend_handler->mqttcfg;
    auto& ctrl = backend_handler->ctrl_cfg;

    settings_fields = QVector<SettingsElements>{
        // --- Data & MQTT ---
        {"Data Source:", "", "", false, &backend_handler->selected_data_source, SettingsElements::String},
        {"Receive Topic (Subscription):", "", "", false, &mqttcfg.mqtt_receive_topic, SettingsElements::String},
        {"Send Topic (Publishing):", "", "", false, &mqttcfg.mqtt_send_topic, SettingsElements::String},

        // --- Configuration ---
        {"Propeller Name:", "", "", false, &cfg.propeller_name, SettingsElements::String},
        {"Angle Of Attack (deg):", "", "Enter AOA", true, &cfg.angle_of_attack, SettingsElements::Double},
        {"Airspeed (m/sec):", "", "Enter airspeed", true, &cfg.airspeed, SettingsElements::Double},
        {"Min Throttle (us):", "", "", true, &cfg.min_throttle, SettingsElements::Int},
        {"Max Throttle (us):", "", "", true, &cfg.max_throttle, SettingsElements::Int},
        {"Max RPM:", "", "", true, &cfg.max_rpm, SettingsElements::Int},
        {"RPM Clemency:", "", "", true, &cfg.rpm_clemency, SettingsElements::Int},
        {"RPM Step:", "", "", true, &cfg.rpm_step, SettingsElements::Int},
        {"Thrust Stabilizing WaitTime (ms):", "", "", true, &cfg.thrust_stab_wait_time_ms, SettingsElements::Int},
        {"Wait After Thrust (ms):", "", "", true, &cfg.record_data_wait_time_ms, SettingsElements::Int},
        {"Current Limit (A):", "", "", true, &cfg.current_limit, SettingsElements::Double},

        // ==========================================
        // --- CALIBRATION (NEW FIELDS FOR SAVING)---
        // ==========================================
        { "Thrust Offset (N):", "", "", true, &cal.thrust_offset, SettingsElements::Double },
        { "Current Offset (A):", "", "", true, &cal.current_offset, SettingsElements::Double },

        // Manual Bools
        { "Manual Current Calibration:", "", "", false, &cal.manual_current_calibration, SettingsElements::Bool },
        { "Manual Thrust Calibration:", "", "", false, &cal.manual_thrust_calibration, SettingsElements::Bool },
        { "Manual Torque Calibration:", "", "", false, &cal.manual_torque_calibration, SettingsElements::Bool },

        // Tare & Factors
        { "Thrust Cal Factor:", "", "", true, &cal.thrust_calibration_factor, SettingsElements::Double },
        { "Thrust Tare (RAW):", "", "", true, &cal.tare_newton_offset, SettingsElements::Double },
        { "Direction Sign:", "", "", true, &cal.direction_sign, SettingsElements::Double },
        { "Torque Cal Factor:", "", "", true, &cal.torque_calibration_factor, SettingsElements::Double },
        { "Torque Tare (RAW):", "", "", true, &cal.tare_torque_offset, SettingsElements::Double },
        { "Current LSB Selected:", "", "", true, &cal.current_lsb_user_selected, SettingsElements::Double },
        // ==========================================

        // --- Toggles / Booleans ---
        {"Sound:", "", "", false, &(backend_handler->soundOn), SettingsElements::Bool},
        {"Log Only On Experiment:", "", "", false, &(cfg.log_only_during_experiment), SettingsElements::Bool},
        {"RPM Filter:", "", "", false, &(backend_handler->use_rpm_filter), SettingsElements::Bool},

        // --- STEP CONTROLLER ---
        { "Throttle Increase Step:", "", "Throttle Step", true, &ctrl.throttle_step, SettingsElements::Int},
        // --- PID CONTROLLER ---
        { "Kp:", "", "Proportional Gain", true, &ctrl.Kp, SettingsElements::Double },
        { "Ki:", "", "Integral Gain", true, &ctrl.Ki, SettingsElements::Double },
        { "Kd:", "", "Derivative Gain", true, &ctrl.Kd, SettingsElements::Double },
        { "Clamping Authority:", "", "0.0 - 1.0", true, &ctrl.clamping_authority, SettingsElements::Double },

         // --- ADRC CONTROLLER ---
        { "ADRC: b0 (Gain):", "", "System Gain", true, &ctrl.b0, SettingsElements::Double },
        { "ADRC: Wc (Bandwidth):", "", "Controller Bandwidth", true, &ctrl.wc, SettingsElements::Double },
        { "ADRC: Wo (Observer):", "", "Observer Bandwidth", true, &ctrl.wo, SettingsElements::Double },

        { "Active Controller:", "", "", false, &active_controller_name, SettingsElements::String }
    };
}

void ThrustStandGUI::chooseSettings() {
    auto& cfg = backend_handler->cfg;
    auto& cal = backend_handler->cal;
    auto& tel = backend_handler->tel;
    auto& mqttcfg = backend_handler->mqttcfg;

    for (auto& field : settings_fields) {
        field.entry = nullptr;
        field.label = nullptr;
    }

    QWidget* settings_window = new QWidget(nullptr, Qt::Window);
    settings_window->setObjectName("ModernDialog");
    settings_window->setWindowTitle("Settings");
    settings_window->setFixedSize(620, 780);
    settings_window->setWindowModality(Qt::ApplicationModal);
    settings_window->setAttribute(Qt::WA_DeleteOnClose);
    settings_window->setAttribute(Qt::WA_StyledBackground, true);
    settings_window->setWindowIcon(QIcon(ICONPATH));
    settings_window->setStyleSheet(DIALOG_STYLE);

    QVBoxLayout* main_layout = new QVBoxLayout(settings_window);
    main_layout->setContentsMargins(28, 24, 28, 24);
    main_layout->setSpacing(16);

    QLabel* header = new QLabel("SETTINGS", settings_window);
    header->setObjectName("DialogHeader");
    QFont hf("Segoe UI", 15, QFont::DemiBold);
    hf.setLetterSpacing(QFont::AbsoluteSpacing, 3);
    header->setFont(hf);
    header->setAlignment(Qt::AlignCenter);
    main_layout->addWidget(header);

    QTabWidget* tabWidget = new QTabWidget(settings_window);
    tabWidget->setDocumentMode(false);

    QWidget* generalTab = new QWidget(), * mqttTab = new QWidget(), * limitTab = new QWidget(),
        * audioTab = new QWidget(), * expTab = new QWidget(), * controllerTab = new QWidget();

    QVBoxLayout* generalLayout = new QVBoxLayout(generalTab), * mqttLayout = new QVBoxLayout(mqttTab),
        * limitLayout = new QVBoxLayout(limitTab), * audioLayout = new QVBoxLayout(audioTab),
        * expLayout = new QVBoxLayout(expTab), * controllerLayout = new QVBoxLayout(controllerTab);

    for (auto l : { generalLayout, mqttLayout, limitLayout, audioLayout, expLayout, controllerLayout }) {
        l->setContentsMargins(22, 20, 22, 20);
        l->setSpacing(6);
    }

    QGridLayout* ctrlGrid = new QGridLayout();
    ctrlGrid->setHorizontalSpacing(16);
    ctrlGrid->setVerticalSpacing(12);
    controllerLayout->addLayout(ctrlGrid);
    int ctrlCellCount = 0;

    for (auto& field : settings_fields) {
        if (field.labeltext == "Manual Current Calibration:" ||
            field.labeltext == "Manual Thrust Calibration:" ||
            field.labeltext == "Manual Torque Calibration:" ||
            field.labeltext == "Thrust Cal Factor:" ||
            field.labeltext == "Thrust Tare (RAW):" ||
            field.labeltext == "Direction Sign:" ||
            field.labeltext == "Torque Cal Factor:" ||
            field.labeltext == "Torque Tare (RAW):" ||
            field.labeltext == "Current LSB Selected:") {
            continue;
        }

        QString shownText = field.labeltext;
        if (shownText.endsWith(":")) shownText.chop(1);
        field.label = new QLabel(shownText.toUpper(), settings_window);
        field.label->setObjectName("SectionLabel");
        QFont lf("Segoe UI", 9, QFont::DemiBold);
        lf.setLetterSpacing(QFont::AbsoluteSpacing, 1.5);
        field.label->setFont(lf);

        bool isControllerField = field.labeltext.startsWith("PID") ||
            field.labeltext == "Active Controller:" ||
            field.labeltext.startsWith("ADRC") ||
            field.labeltext.startsWith("Step") ||
            field.labeltext.startsWith("Throttle Increase") ||
            field.labeltext.startsWith("Kp") ||
            field.labeltext.startsWith("Ki") ||
            field.labeltext.startsWith("Kd") ||
            field.labeltext.startsWith("Clamping") ||
            field.labeltext.startsWith("Integral");

        if (field.type == SettingsElements::Bool) {
            QCheckBox* checkBox = new QCheckBox("Enabled", settings_window);
            checkBox->setChecked(*static_cast<bool*>(field.targetVar));
            checkBox->setCursor(Qt::PointingHandCursor);
            checkBox->setMinimumHeight(34);
            field.entry = checkBox;
        }
        else if (field.labeltext == "Data Source:") {
            QComboBox* comboBox = new QComboBox(settings_window);
            comboBox->addItems({ "MQTT", "SERIAL" });
            comboBox->setCurrentText(*static_cast<QString*>(field.targetVar));
            comboBox->setMinimumHeight(40);
            field.entry = comboBox;
        }
        else if (field.labeltext == "Active Controller:") {
            QComboBox* comboBox = new QComboBox(settings_window);
            comboBox->addItems(backend_handler->registered_controllers.keys());
            comboBox->setCurrentText(*static_cast<QString*>(field.targetVar));
            comboBox->setMinimumHeight(40);
            field.entry = comboBox;
        }
        else {
            QLineEdit* lineEdit = new QLineEdit(settings_window);
            lineEdit->setMinimumHeight(40);
            if (!field.placeholder.isEmpty()) lineEdit->setPlaceholderText(field.placeholder);

            if (field.type == SettingsElements::String) {
                lineEdit->setText(*static_cast<QString*>(field.targetVar));
            }
            else if (field.type == SettingsElements::Int) {
                lineEdit->setText(QString::number(*static_cast<int*>(field.targetVar)));
                lineEdit->setValidator(new QIntValidator(0, 100000, lineEdit));
            }
            else if (field.type == SettingsElements::Double) {
                int decimals = isControllerField ? 4 : 2;
                lineEdit->setText(QString::number(*static_cast<double*>(field.targetVar), 'f', decimals));
                lineEdit->setValidator(new QDoubleValidator(lineEdit));
            }
            field.entry = lineEdit;
        }

        if (isControllerField) {
            QWidget* cell = new QWidget(settings_window);
            QVBoxLayout* cl = new QVBoxLayout(cell);
            cl->setContentsMargins(0, 0, 0, 0);
            cl->setSpacing(4);
            cl->addWidget(field.label);
            cl->addWidget(field.entry);
            ctrlGrid->addWidget(cell, ctrlCellCount / 2, ctrlCellCount % 2);
            ctrlCellCount++;
            continue;
        }

        QVBoxLayout* target = generalLayout;
        if (field.labeltext == "Sound:") {
            target = audioLayout;
        }
        else if (field.labeltext.contains("Topic")) {
            target = mqttLayout;
        }
        else if (field.labeltext.contains("Max") || field.labeltext.contains("Min") || field.labeltext.contains("Limit")) {
            target = limitLayout;
        }
        else if ((field.labeltext.contains("RPM") && field.labeltext != "RPM Filter:") ||
            field.labeltext.contains("Step") ||
            field.labeltext.contains("WaitTime") ||
            field.labeltext.contains("After")) {
            target = expLayout;
        }

        target->addWidget(field.label);
        target->addWidget(field.entry);
        target->addSpacing(10);
    }

    for (auto l : { generalLayout, mqttLayout, limitLayout, audioLayout, expLayout, controllerLayout }) l->addStretch();

    tabWidget->addTab(generalTab, "General");
    tabWidget->addTab(expTab, "Experiment");
    tabWidget->addTab(controllerTab, "Controllers");
    tabWidget->addTab(limitTab, "Limits");
    tabWidget->addTab(mqttTab, "MQTT");
    tabWidget->addTab(audioTab, "Audio");

    QPushButton* confirmButton = new QPushButton("Confirm Settings", settings_window);
    confirmButton->setObjectName("PrimaryButton");
    confirmButton->setFixedHeight(48);
    confirmButton->setCursor(Qt::PointingHandCursor);

    main_layout->addWidget(tabWidget, 1);
    main_layout->addWidget(confirmButton);

    connect(confirmButton, &QPushButton::clicked, this, [this, settings_window]() mutable {

        QString new_rx, new_tx;
        for (const auto& f : settings_fields) {
            if (!f.entry) continue;

            if (f.labeltext == "Receive Topic (Subscription):") new_rx = qobject_cast<QLineEdit*>(f.entry)->text().trimmed();
            if (f.labeltext == "Send Topic (Publishing):") new_tx = qobject_cast<QLineEdit*>(f.entry)->text().trimmed();
        }

        if (new_rx.isEmpty() || new_tx.isEmpty()) {
            QMessageBox::warning(settings_window, "Error", "MQTT Topics cannot be empty.");
            return;
        }

        double old_thrust_offset = backend_handler->cal.thrust_offset;
        double old_current_offset = backend_handler->cal.current_offset;
        QString old_rx_topic = backend_handler->mqttcfg.mqtt_receive_topic;
        QString old_tx_topic = backend_handler->mqttcfg.mqtt_send_topic;
        const QString old_controller_name = active_controller_name;

        bool changed = false;

        for (const auto& field : settings_fields) {
            if (!field.entry) continue;

            if (field.type == SettingsElements::Bool) {
                QCheckBox* cb = qobject_cast<QCheckBox*>(field.entry);
                if (cb) {
                    bool newVal = cb->isChecked();
                    bool* target = static_cast<bool*>(field.targetVar);
                    if (*target != newVal) { *target = newVal; changed = true; }
                }
            }
            else if (field.labeltext == "Data Source:" || field.labeltext == "Active Controller:") {
                QComboBox* cb = qobject_cast<QComboBox*>(field.entry);
                if (cb) {
                    QString newVal = cb->currentText();
                    QString* target = static_cast<QString*>(field.targetVar);
                    if (*target != newVal) { *target = newVal; changed = true; }
                }
            }
            else {
                QLineEdit* le = qobject_cast<QLineEdit*>(field.entry);
                if (le) {
                    QString textVal = le->text().trimmed();

                    if (field.type == SettingsElements::String) {
                        QString* target = static_cast<QString*>(field.targetVar);
                        if (*target != textVal) { *target = textVal; changed = true; }
                    }
                    else if (field.type == SettingsElements::Int) {
                        int newVal = textVal.toInt();
                        int* target = static_cast<int*>(field.targetVar);
                        if (*target != newVal) { *target = newVal; changed = true; }
                    }
                    else if (field.type == SettingsElements::Double) {
                        double newVal = textVal.replace(",", ".").toDouble();
                        double* target = static_cast<double*>(field.targetVar);
                        if (*target != newVal) { *target = newVal; changed = true; }
                    }
                }
            }
        }

        if (changed) {
            backend_handler->unsaved_settings_changes = true;
            this->updateBottomToolbar();
        }

        if (active_controller_name != old_controller_name &&
            backend_handler->registered_controllers.contains(active_controller_name)) {
            backend_handler->setActiveController(active_controller_name);
        }
        updateControllerIndicator();

        auto& ctrl = backend_handler->ctrl_cfg;

        if (backend_handler->step_controller) {
            backend_handler->step_controller->set_new_parameters({
                qMakePair(QString("Throttle Increase Step"), (double)backend_handler->cfg.throttle_increase_step)
                });
        }
        if (backend_handler->pid_controller) {
            backend_handler->pid_controller->set_new_parameters({
                qMakePair(QString("Kp Value"), ctrl.Kp),
                qMakePair(QString("Ki Value"), ctrl.Ki),
                qMakePair(QString("Kd Value"), ctrl.Kd),
                qMakePair(QString("Clamping Authority (For Clamping Anti-Windup)"), ctrl.clamping_authority)
                });
        }
        if (backend_handler->adrc_controller) {
            backend_handler->adrc_controller->set_new_parameters({
                qMakePair(QString("b0 (System Gain)"), ctrl.b0),
                qMakePair(QString("Wc (Controller Bandwidth)"), ctrl.wc),
                qMakePair(QString("Wo (Observer Bandwidth)"), ctrl.wo)
                });
        }

        auto& tel = backend_handler->tel;
        auto& cal = backend_handler->cal;

        if (cal.thrust_offset != old_thrust_offset) {
            tel.thrust = (tel.thrust - old_thrust_offset) + cal.thrust_offset;
            if (!backend_handler->process_begin && instantthrustGUI) {
                instantthrustGUI->updateData(tel.thrust);
            }
        }

        if (cal.current_offset != old_current_offset) {
            tel.current = (tel.current - old_current_offset) + cal.current_offset;
            if (!backend_handler->process_begin && instantcurrentGUI) {
                instantcurrentGUI->updateData(tel.current);
            }
        }

        backend_handler->setSoundStatus(backend_handler->soundOn);
        backend_handler->playAudio(backend_handler->audio_click_sfx_volume, "MainClick");

        if (old_rx_topic != backend_handler->mqttcfg.mqtt_receive_topic ||
            old_tx_topic != backend_handler->mqttcfg.mqtt_send_topic) {
            backend_handler->change_mqtt_topic();
        }

        settings_window->close();
        });

    settings_window->show();
}

void ThrustStandGUI::saveSettings() {

    int label_width = 30;
    auto& save_file = backend_handler->save_file;
    auto& save_file_path = backend_handler->save_file_path;
    if (save_file == nullptr) {
        save_file = new QFile(save_file_path, this);

        if (!save_file->open(QIODevice::Append | QIODevice::Text)) {
            delete save_file;
            save_file = nullptr;
            return;
        }
        QTextStream out(save_file);
        out << "\nLast Save Settings Session: "
            << QDateTime::currentDateTime().toString("dd/MM/yyyy HH:mm:ss")
            << "\n";
    }

    QTextStream out(save_file);
    QString timestamp = QDateTime::currentDateTime().toString("HH:mm:ss");

    for (const auto& field : settings_fields) {
        if (field.targetVar == nullptr) continue; // Ασφάλεια: αν ξεχάσεις κάποιον pointer

        QString valueStr;

        if (field.type == SettingsElements::Bool) {
            bool val = *(static_cast<bool*>(field.targetVar));
            valueStr = val ? "Yes" : "No";
        }
        else if (field.type == SettingsElements::Int) {
            int val = *(static_cast<int*>(field.targetVar));
            valueStr = QString::number(val);
        }
        else if (field.type == SettingsElements::Double) {
            double val = *(static_cast<double*>(field.targetVar));
            valueStr = QString::number(val, 'f', 2);
        }
        else if (field.type == SettingsElements::String) {
            QString* target = static_cast<QString*>(field.targetVar);
            valueStr = *target;
        }

        QString cleanLabel = field.labeltext;
        cleanLabel.remove(":");
        cleanLabel = cleanLabel.trimmed();

        out << QString("%1 : %2\n").arg(cleanLabel, -label_width).arg(valueStr);
    }
    out << "------------------------------------------------\n";
    backend_handler->unsaved_settings_changes = false;
    this->updateBottomToolbar();
    save_file->flush();
}

void ThrustStandGUI::loadSettings() {

    auto& save_file = backend_handler->save_file;
    auto& save_file_path = backend_handler->save_file_path;

    QFile file(save_file_path);

    if (!file.exists()) {
        qDebug() << "No settings file found. Using defaults.";
        return;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Could not open settings file for reading.";
        return;
    }

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine();

        if (line.startsWith("-") || line.trimmed().isEmpty()) continue;

        // Κόβουμε τη γραμμή στο πρώτο ':' 
        // (χρησιμοποιούμε indexOf αντί για split σε περίπτωση που πχ το MQTT URL έχει μέσα ':')
        int colonIndex = line.indexOf(":");
        if (colonIndex == -1) continue;


        QString key = line.left(colonIndex).trimmed();
        QString value = line.mid(colonIndex + 1).trimmed();

        for (const auto& field : settings_fields) {

            // Αφαιρούμε το ':' και τα κενά από το labeltext για να ταιριάζει με το key του txt
            QString expectedKey = field.labeltext;
            expectedKey.remove(":");
            expectedKey = expectedKey.trimmed();

            if (expectedKey == key && field.targetVar != nullptr) {

                if (field.type == SettingsElements::Bool) {
                    *(static_cast<bool*>(field.targetVar)) = (value.toLower() == "yes" || value.toLower() == "true");
                }
                else if (field.type == SettingsElements::Int) {
                    *(static_cast<int*>(field.targetVar)) = value.toInt();
                }
                else if (field.type == SettingsElements::Double) {
                    *(static_cast<double*>(field.targetVar)) = value.toDouble();
                }
                else if (field.type == SettingsElements::String) {
                    *(static_cast<QString*>(field.targetVar)) = value;
                }

                break;
            }
        }
    }

    file.close();

    this->updateControllers();
    this->updateBottomToolbar();
    backend_handler->audio_handler->setSoundStatus(backend_handler->soundOn);

    qDebug() << "Settings loaded successfully!";
}

// EXPERIMENT FUNCTIONS

void ThrustStandGUI::setupExperimentFields() {

    auto& cfg = backend_handler->cfg;
    auto& cal = backend_handler->cal;
    auto& mqttcfg = backend_handler->mqttcfg;

    experiment_fields = QVector<SettingsElements>{
        // --- General Settings ---
        { "Propeller Name:", "", "", false, &cfg.propeller_name, SettingsElements::String },
        { "Thrust Threshold:", "", "", true, &cfg.stabilize_target_threshold, SettingsElements::Double },

        // --- Aerodynamics / Environment ---
        { "Angle Of Attack (deg):", "", "", true, &cfg.angle_of_attack, SettingsElements::Double },
        { "Airspeed (m/sec):", "", "Enter airspeed in m/sec", true, &cfg.airspeed, SettingsElements::Double },

        // --- Thrust & RPM Parameters ---
        { "Thrust Clemency:", "", "", true, &cfg.thrust_clemency, SettingsElements::Double },
        { "RPM Step:", "", "", true, &cfg.rpm_step, SettingsElements::Int },
        { "Max RPM:", "", "", true, &cfg.max_rpm, SettingsElements::Int },
        { "RPM Clemency:", "", "", true, &cfg.rpm_clemency, SettingsElements::Int },
        { "Use MaxRPM Logic:", "", "", false, &cfg.use_max_rpm_logic, SettingsElements::Bool },

        // --- Timings & Delays ---
        { "Thrust Stabilizing WaitTime (ms):", "", "", true, &cfg.thrust_stab_wait_time_ms, SettingsElements::Int },
        { "Transition Delay Time (ms):", "", "", true, &cfg.transition_delay_ms, SettingsElements::Int },
        { "Wait After Thrust (ms):", "", "", true, &cfg.record_data_wait_time_ms, SettingsElements::Int },

        // --- Calibration ---
        { "Thrust Offset (N):", "", "", true, &cal.thrust_offset, SettingsElements::Double },

        // --- Experiment Bounds ---
        { "Experiment is Static", "", "", false, &cfg.experiment_is_static, SettingsElements::Bool },
        { "First Experiment", "", "", false, &cfg.first_experiment, SettingsElements::Bool },
        { "Last Experiment", "", "", false, &cfg.last_experiment, SettingsElements::Bool }
    };

}

void ThrustStandGUI::customizeExperiment() {

    auto& cfg = backend_handler->cfg;
    auto& cal = backend_handler->cal;
    auto& tel = backend_handler->tel;

    QWidget* settings_window = new QWidget(nullptr, Qt::Window);
    settings_window->setObjectName("ModernDialog");
    settings_window->setWindowTitle("Experiment Settings");
    settings_window->setMinimumSize(880, 700);
    settings_window->setWindowModality(Qt::ApplicationModal);
    settings_window->setAttribute(Qt::WA_DeleteOnClose);
    settings_window->setAttribute(Qt::WA_StyledBackground, true);
    settings_window->setWindowIcon(QIcon(ICONPATH));
    settings_window->setStyleSheet(DIALOG_STYLE);

    QVBoxLayout* layout = new QVBoxLayout(settings_window);
    layout->setContentsMargins(32, 26, 32, 26);
    layout->setSpacing(10);

    QLabel* header = new QLabel("EXPERIMENT SETTINGS", settings_window);
    header->setObjectName("DialogHeader");
    QFont hf("Segoe UI", 15, QFont::DemiBold);
    hf.setLetterSpacing(QFont::AbsoluteSpacing, 3);
    header->setFont(hf);
    header->setAlignment(Qt::AlignCenter);
    layout->addWidget(header);
    layout->addSpacing(8);

    auto createSection = [&](const QString& title) {
        QLabel* t = new QLabel(title, settings_window);
        t->setObjectName("SectionLabel");
        QFont f("Segoe UI", 11, QFont::DemiBold);
        f.setLetterSpacing(QFont::AbsoluteSpacing, 3);
        t->setFont(f);
        layout->addWidget(t);

        QFrame* line = new QFrame(settings_window);
        line->setFixedHeight(1);
        line->setStyleSheet("background-color: rgba(255,255,255,40); border: none;");
        layout->addWidget(line);

        QGridLayout* grid = new QGridLayout();
        grid->setHorizontalSpacing(18);
        grid->setVerticalSpacing(10);
        for (int c = 0; c < 3; ++c) grid->setColumnStretch(c, 1);
        layout->addLayout(grid);
        layout->addSpacing(8);
        return grid;
        };

    QGridLayout* generalGrid = createSection("GENERAL");
    QGridLayout* thrustGrid = createSection("THRUST & RPM");
    QGridLayout* timingGrid = createSection("TIMINGS");

    QHash<QGridLayout*, int> cellCount;
    auto addCell = [&](QGridLayout* grid, QWidget* label, QWidget* entry) {
        QWidget* cell = new QWidget(settings_window);
        QVBoxLayout* cl = new QVBoxLayout(cell);
        cl->setContentsMargins(0, 0, 0, 0);
        cl->setSpacing(4);
        cl->addWidget(label);
        cl->addWidget(entry);
        int n = cellCount[grid]++;
        grid->addWidget(cell, n / 3, n % 3);
        };

    QPushButton* confirmButton = new QPushButton("Confirm Settings", settings_window);
    QHBoxLayout* boundsRow = new QHBoxLayout();
    boundsRow->setSpacing(30);
    boundsRow->addStretch();

    for (auto& field : experiment_fields) {

        if (field.type == SettingsElements::Bool) {
            QCheckBox* checkBox = new QCheckBox(field.labeltext, settings_window);
            checkBox->setChecked(*static_cast<bool*>(field.targetVar)); 
            checkBox->setCursor(Qt::PointingHandCursor);
            checkBox->setMinimumHeight(34);

            field.entry = checkBox;
            field.label = nullptr;

            boundsRow->addWidget(checkBox);
            continue;
        }

        QString shownText = field.labeltext;
        if (shownText.endsWith(":")) shownText.chop(1);
        field.label = new QLabel(shownText.toUpper(), settings_window);
        field.label->setObjectName("SectionLabel");
        QFont lf("Segoe UI", 9, QFont::DemiBold);
        lf.setLetterSpacing(QFont::AbsoluteSpacing, 1.5);
        field.label->setFont(lf);

        if (field.labeltext == "Propeller Name:") {
            QComboBox* propellerBox = new QComboBox(settings_window);
            propellerBox->setEditable(true);
            propellerBox->setMinimumHeight(40);
            propellerBox->lineEdit()->setPlaceholderText("Add new propeller...");
            for (const auto& prop : cfg.propeller_list) propellerBox->addItem(prop);

            int idx = propellerBox->findText(cfg.propeller_name);
            if (idx != -1) propellerBox->setCurrentIndex(idx);
            else propellerBox->setCurrentText(cfg.propeller_name);
            field.entry = propellerBox;
        }
        else if (field.labeltext == "Angle Of Attack (deg):") {
            QComboBox* aoaBox = new QComboBox(settings_window);
            aoaBox->setMinimumHeight(40);
            for (const auto& val : cfg.aoa_list) aoaBox->addItem(val);

            int idx = aoaBox->findText(QString::number(cfg.angle_of_attack));
            if (idx != -1) aoaBox->setCurrentIndex(idx);
            field.entry = aoaBox;
        }
        else {
            QLineEdit* lineEdit = new QLineEdit(field.initialValue, settings_window);
            lineEdit->setMinimumHeight(40);
            if (!field.placeholder.isEmpty()) lineEdit->setPlaceholderText(field.placeholder);

            if (field.type == SettingsElements::String) {
                lineEdit->setText(*static_cast<QString*>(field.targetVar));
            }
            else if (field.type == SettingsElements::Int) {
                lineEdit->setText(QString::number(*static_cast<int*>(field.targetVar)));
                lineEdit->setValidator(new QIntValidator(0, 100000, lineEdit));
            }
            else if (field.type == SettingsElements::Double) {
                lineEdit->setText(QString::number(*static_cast<double*>(field.targetVar), 'f', 2));
                lineEdit->setValidator(new QDoubleValidator(lineEdit));
            }

            field.entry = lineEdit;
        }

        // Επιλογή ενότητας
        QGridLayout* target = thrustGrid;
        if (field.labeltext == "Propeller Name:" ||
            field.labeltext == "Angle Of Attack (deg):" ||
            field.labeltext == "Airspeed (m/sec):") {
            target = generalGrid;
        }
        else if (field.labeltext.contains("WaitTime") ||
            field.labeltext.contains("Delay") ||
            field.labeltext.contains("Wait After")) {
            target = timingGrid;
        }

        addCell(target, field.label, field.entry);
    }

    boundsRow->addStretch();
    layout->addLayout(boundsRow);
    layout->addStretch();

    confirmButton->setObjectName("PrimaryButton");
    confirmButton->setFixedHeight(48);
    confirmButton->setCursor(Qt::PointingHandCursor);
    layout->addWidget(confirmButton);

    connect(confirmButton, &QPushButton::clicked, this, [this, &cfg, &cal, &tel, settings_window]() mutable {

        auto findEntryByLabel = [&](const QString& label) -> QWidget* {
            for (const auto& field : experiment_fields) {
                if (field.labeltext == label) return field.entry;
            }
            return nullptr;
            };
        auto getEntryText = [&](const QString& label) -> QString {
            QLineEdit* entry = qobject_cast<QLineEdit*>(findEntryByLabel(label));
            return entry ? entry->text().trimmed() : "";
            };

        double old_thrust_offset = cal.thrust_offset; // this has to be saved manually

        for (auto& field : experiment_fields) {
            

            if (field.type == SettingsElements::Bool) {
                QCheckBox* new_value_box = qobject_cast<QCheckBox*>(field.entry);
                *static_cast<bool*>(field.targetVar) = new_value_box ? new_value_box->isChecked() : false;
                continue;
            }

            else if (field.type == SettingsElements::Int) {

                *static_cast<int*>(field.targetVar) = getEntryText(field.labeltext).toInt();
                continue;
            }

			else if (field.type == SettingsElements::Double) {
				*static_cast<double*>(field.targetVar) = getEntryText(field.labeltext).toDouble();
				continue;
			}

            else if (field.type == SettingsElements::String) {
                QComboBox* propBox = qobject_cast<QComboBox*>(findEntryByLabel(field.labeltext));
                if (propBox) {
                    *static_cast<QString*>(field.targetVar) = propBox->currentText();
                }
                continue;
            }

        }

		if (!cfg.propeller_name.isEmpty() && // we add the propeller name to the list if it's not already there
            !cfg.propeller_list.contains(cfg.propeller_name)) {

            cfg.propeller_list.append(cfg.propeller_name);
        }

		if (old_thrust_offset != cal.thrust_offset) { // if the thrust offset has changed, we need to adjust the current thrust value accordingly

            tel.thrust += cal.thrust_offset - old_thrust_offset;

            if (!backend_handler->process_begin && instantthrustGUI) {
                instantthrustGUI->updateData(tel.thrust);
            }
        }

        if (cal.auto_tare_thrust_at_begin && cfg.first_experiment) {
            cal.thrust_offset = -1.0 * tel.thrust;
        }

        backend_handler->playAudio(backend_handler->audio_click_sfx_volume, "MainClick");
        qDebug() << "New Experiment Begins";
        backend_handler->experiment_running = true;

        this->updateBottomToolbar();
        this->start_experiment();

        settings_window->close(); 
        } );

    settings_window->show();
}

void ThrustStandGUI::start_experiment() {

    backend_handler->resetExperiment();

    if (!backend_handler->auto_mode_timer) { // an den exei ginei allocate gia to timer kanto -- efoson den exei ginei den exoyme kanei experiment

        backend_handler->auto_mode_timer = new QTimer(this);
        connect(backend_handler->auto_mode_timer, &QTimer::timeout, this, [this]() { // connecting timer with experiment execution 
            if (this->backend_handler) this->backend_handler->execute_experiment(); });  

        connect(backend_handler, &BackendHandler::requestWindTunnelTransition, this, &ThrustStandGUI::create_approve_window);
    }

    if (!backend_handler->auto_mode_timer->isActive()) {
        backend_handler->auto_mode_timer->start(auto_mode_timer_interval_ms);
    }

}

void ThrustStandGUI::onExperimentFinished() {
    backend_handler->experiment_running = false;
    this->updateBottomToolbar();
    if (backend_handler->get_function_mode() == "Experiment") customizeExperiment();
}

void ThrustStandGUI::create_approve_window() {

    QWidget* approve_window = new QWidget(nullptr, Qt::Window);
    approve_window->setObjectName("ModernDialog");
    approve_window->setWindowTitle("Approve Transition");
    approve_window->setMinimumSize(300, 200);
    approve_window->setWindowModality(Qt::ApplicationModal);
    approve_window->setAttribute(Qt::WA_DeleteOnClose);
    approve_window->setAttribute(Qt::WA_StyledBackground, true);
    approve_window->setWindowIcon(QIcon(ICONPATH));
    approve_window->setStyleSheet(DIALOG_STYLE);

    QVBoxLayout* layout = new QVBoxLayout(approve_window);
    layout->setContentsMargins(32, 28, 32, 28);
    layout->setSpacing(12);

    auto makeSectionLabel = [approve_window](const QString& text) {
        QLabel* l = new QLabel(text, approve_window);
        l->setObjectName("SectionLabel");
        QFont f("Segoe UI", 10, QFont::DemiBold);
        f.setLetterSpacing(QFont::AbsoluteSpacing, 2);
        l->setFont(f);
        return l;
        };

    QLabel* header = new QLabel("APPROVE TRANSITION WHEN WINDTUNNEL IS STABLE", approve_window);
    header->setObjectName("DialogHeader");
    QFont hf("Segoe UI", 15, QFont::DemiBold);
    hf.setLetterSpacing(QFont::AbsoluteSpacing, 3);
    header->setFont(hf);
    header->setAlignment(Qt::AlignCenter);
    layout->addWidget(header);
    layout->addSpacing(10);

    QPushButton* confirmButton = new QPushButton("Approve Transition", approve_window);
    confirmButton->setObjectName("PrimaryButton");
    confirmButton->setFixedHeight(46);
    confirmButton->setCursor(Qt::PointingHandCursor);
    layout->addWidget(confirmButton);

    connect(confirmButton, &QPushButton::clicked, this, [confirmButton, approve_window, this]() {
        backend_handler->cfg.manual_transition_approved = true;
        backend_handler->playAudio(backend_handler->audio_click_sfx_volume, "MainClick");
        approve_window->close();
        });

    approve_window->show();
}
