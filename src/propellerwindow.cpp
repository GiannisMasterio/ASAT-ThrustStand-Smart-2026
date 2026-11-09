#include "PropellerWindow.h"
#include "Constants.h"

PropellerWindow::PropellerWindow(BackendHandler* backend, QWidget* parent)
    : QWidget(parent, Qt::Window), backend_handler(backend)
{
    setObjectName("ModernDialog");
    setWindowTitle("Propeller Data Analysis & APC Comparison");
    setWindowIcon(QIcon(ICONPATH));
    setFixedSize(1600, 800);
    setWindowModality(Qt::ApplicationModal);
    setAttribute(Qt::WA_DeleteOnClose);
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(DIALOG_STYLE);

    setupUI();
}

PropellerWindow::~PropellerWindow() {
    qDeleteAll(groups);
    groups.clear();
}

void PropellerWindow::setupUI() {
    QVBoxLayout* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(30, 24, 30, 24);
    main_layout->setSpacing(18);

    // Μικρά labels με κεφαλαία και απόσταση γραμμάτων
    auto makeSectionLabel = [this](const QString& text) {
        QLabel* l = new QLabel(text, this);
        l->setObjectName("SectionLabel");
        QFont f("Segoe UI", 10, QFont::DemiBold);
        f.setLetterSpacing(QFont::AbsoluteSpacing, 2);
        l->setFont(f);
        return l;
        };

    // --- Epilogh propellas ---
    QHBoxLayout* top_row = new QHBoxLayout();
    QLabel* prop_label = makeSectionLabel("SELECTED PROPELLER");

    propellerBox = new QComboBox(this);
    propellerBox->setEditable(true);
    propellerBox->setFixedWidth(280);
    propellerBox->setMinimumHeight(40);
    for (const auto& prop : backend_handler->cfg.propeller_list) {
		propellerBox->addItem(prop); // την προσθετω στο combobox αν ειναι στο υπαρχον list
    }

    int propeller_idx = propellerBox->findText(backend_handler->cfg.propeller_name);
    if (propeller_idx != -1) propellerBox->setCurrentIndex(propeller_idx);
    else propellerBox->setCurrentText(backend_handler->cfg.propeller_name);

    top_row->addStretch();
    top_row->addWidget(prop_label);
    top_row->addSpacing(14);
    top_row->addWidget(propellerBox);
    top_row->addStretch();
    main_layout->addLayout(top_row);

    // --- Γραφήματα ---
    QHBoxLayout* graphs_layout = new QHBoxLayout();
    graphs_layout->setSpacing(24);
    QStringList log_params = { "RPM", "Throttle(%)" , "Advance Ratio (J)", "Thrust (N)", "Power (W)", "Efficiency", "Torque (N*m)", "Voltage (V)", "Current (A)"};

    auto createGraphColumn = [&](QString defaultX, QString defaultY) {
        QVBoxLayout* col = new QVBoxLayout();
        col->setSpacing(12);

        GraphGroup* g = new GraphGroup();
        g->x_sel = new QComboBox();
        g->y_sel = new QComboBox();
        g->x_sel->addItems(log_params);
        g->y_sel->addItems(log_params);
        g->x_sel->setMinimumWidth(170);
        g->y_sel->setMinimumWidth(170);
        g->x_sel->setCurrentText(defaultX);
        g->y_sel->setCurrentText(defaultY);

        g->graph = new GraphContainer("", "", this);
        groups.append(g);

        QHBoxLayout* selectors = new QHBoxLayout();
        selectors->setSpacing(8);
        selectors->addStretch();
        selectors->addWidget(makeSectionLabel("X"));
        selectors->addWidget(g->x_sel);
        selectors->addSpacing(20);
        selectors->addWidget(makeSectionLabel("Y"));
        selectors->addWidget(g->y_sel);
        selectors->addStretch();

        connect(g->x_sel, &QComboBox::currentTextChanged, this, &PropellerWindow::updateHeaders);
        connect(g->y_sel, &QComboBox::currentTextChanged, this, &PropellerWindow::updateHeaders);

        col->addLayout(selectors);
        col->addWidget(g->graph, 1);
        return col;
        };

    graphs_layout->addLayout(createGraphColumn("RPM", "Thrust (N)"), 1);
    graphs_layout->addLayout(createGraphColumn("RPM", "Power (W)"), 1);
    main_layout->addLayout(graphs_layout, 1);
    updateHeaders();

    // --- Footer ---
    QHBoxLayout* footer = new QHBoxLayout();
    footer->setSpacing(16);
    QPushButton* btn_export = new QPushButton("Export Graphs", this);
    QPushButton* btn_apc = new QPushButton("Load APC Reference", this);
    QPushButton* btn_load = new QPushButton("Load Log Data", this);

    for (auto btn : { btn_export, btn_apc, btn_load }) {
        btn->setFixedSize(210, 46);
        btn->setCursor(Qt::PointingHandCursor);
    }
    btn_load->setObjectName("PrimaryButton");   // το κύριο κουμπί, γεμάτο μπλε

    footer->addStretch();
    footer->addWidget(btn_export);
    footer->addWidget(btn_apc);
    footer->addWidget(btn_load);
    footer->addStretch();
    main_layout->addLayout(footer);

    connect(btn_load, &QPushButton::clicked, this, &PropellerWindow::loadLogData);
    connect(btn_apc, &QPushButton::clicked, this, &PropellerWindow::loadApcReference);
    connect(btn_export, &QPushButton::clicked, this, &PropellerWindow::exportGraphs);
}

void PropellerWindow::updateHeaders() {
    for (auto g : groups) {
        g->graph->setTitle(QString("%1 VS %2").arg(g->y_sel->currentText(), g->x_sel->currentText()));
        g->graph->setAxisLabels(g->x_sel->currentText(), g->y_sel->currentText());
    }
    refreshPlots();
}

void PropellerWindow::loadLogData() {
    backend_handler->playAudio(backend_handler->audio_click_sfx_volume, "MainClick");
    QString targetProp = propellerBox->currentText();

    QString initialPath = QCoreApplication::applicationDirPath() + QDir::separator() + "Logs";
    QString filepath = QFileDialog::getOpenFileName(this, "Select Log Data CSV", initialPath, "CSV Files (*.csv);;Text Files (*.txt);;All Files (*.*)");

    if (filepath.isEmpty()) return;

    QFile file(filepath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Could not open log file:" << filepath;
        return;
    }

    QTextStream in(&file);
    QVector<LogEntry> filteredEntries;
    bool isFirstLine = true;

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;

        // Κόβουμε τη γραμμή με βάση το ερωτηματικό ή το κόμμα (ανάλογα τι χρησιμοποιεί το CSV σου)
        QStringList parts = line.split(QRegularExpression("[,;]"), Qt::SkipEmptyParts);

        // Χρησιμοποιούμε contains("Timestamp") για ασφάλεια, μήπως υπάρχει κάποιος κρυφός χαρακτήρας
        if (isFirstLine && parts.at(0).contains("Timestamp")) {
            isFirstLine = false;
            continue;
        }

        // Τώρα η Προπέλα είναι στο index 8 και συνολικά έχουμε 11 στήλες (0 έως 10)
        if (parts.size() >= 11 && parts.at(8).trimmed() == targetProp) {
            LogEntry entry;
            entry.thrust = parts.at(1).toDouble();
            entry.torque = parts.at(2).toDouble();
            entry.voltage = parts.at(3).toDouble();
            entry.current = parts.at(4).toDouble();
            entry.rpm = parts.at(5).toDouble();
            entry.throttle = parts.at(6).toDouble();
            entry.power = parts.at(7).toDouble();

            filteredEntries.append(entry);
        }
    }
    file.close();

    // Αντικατάσταση του MeanValues1.m
    currentMeanData = calculateMeanValues(filteredEntries, 50.0); // 50 rpm ανοχή
    refreshPlots();
}

QVector<PropellerData> PropellerWindow::calculateMeanValues(const QVector<LogEntry>& rawEntries, double rpm_tolerance) {
    QVector<PropellerData> meanDataList; // ΛΙΣΤΑ ΜΕΣΩΝ ΟΡΩΝ
    if (rawEntries.isEmpty()) return meanDataList;

    double currentTargetRpm = rawEntries.first().rpm;
    QVector<LogEntry> currentGroup;

    auto processGroup = [&](const QVector<LogEntry>& group) {
        if (group.isEmpty()) return;

        PropellerData meanData; // Οι αρχικές τιμές είναι ήδη 0 χάρη στο struct σου

        for (const auto& g : group) {
            meanData.thrust += g.thrust;
            meanData.rpm += g.rpm;
            meanData.current += g.current;
            meanData.voltage += g.voltage;
            meanData.throttle += g.throttle;
            meanData.power += g.power;
            // Αν προσθέσεις το torque στο LogEntry, το βάζεις κι αυτό εδώ
        }

        double n = group.size();
        meanData.thrust /= n;
        meanData.rpm /= n;
        meanData.current /= n;
        meanData.voltage /= n;
        meanData.throttle /= n;
        meanData.power /= n;

        // Υπολογισμός Απόδοσης & Advance Ratio
        double airspeed = backend_handler->cfg.airspeed; // Ή group.first().airspeed αν το παίρνεις από το log
        double prop_diameter_m = 0.254; // TODO: Φέρε τη διάμετρο της προπέλας (σε μέτρα) από τα settings

        meanData.efficiency = (meanData.power > 0.1) ? (meanData.thrust * airspeed / meanData.power) : 0.0;

        meanData.advance_ratio = (meanData.rpm > 0 && prop_diameter_m > 0) ?
            (airspeed / ((meanData.rpm / 60.0) * prop_diameter_m)) : 0.0;

        meanDataList.append(meanData);
        };

    for (const auto& entry : rawEntries) {
        if (std::abs(entry.rpm - currentTargetRpm) <= rpm_tolerance) {
            currentGroup.append(entry);
        }
        else {
            processGroup(currentGroup);
            currentGroup.clear();
            currentGroup.append(entry);
            currentTargetRpm = entry.rpm;
        }
    }
    // Μην ξεχάσεις το τελευταίο group!
    processGroup(currentGroup);

    return meanDataList;
}

void PropellerWindow::loadApcReference() {
    backend_handler->playAudio(backend_handler->audio_click_sfx_volume, "MainClick");
    QString filepath = QFileDialog::getOpenFileName(this, "Select APC Reference Text", "", "Text Files (*.txt)");
    if (filepath.isEmpty()) return;

    apcReferenceData = parseApcReferenceData(filepath);
    refreshPlots();
}

QVector<PropellerData> PropellerWindow::parseApcReferenceData(const QString& filepath) {
    QVector<PropellerData> apcData;
    QFile file(filepath);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Could not open APC file:" << filepath;
        return apcData;
    }

    QTextStream in(&file);
    bool dataRegion = false;

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();

        // Τα αρχεία της APC συνήθως ξεκινάνε τα νούμερα μετά από μια γραμμή με V, J, κλπ.
        if (line.startsWith("V") || line.contains("RPM")) {
            dataRegion = true;
            continue;
        }

        if (dataRegion && !line.isEmpty() && line.at(0).isDigit()) {
            QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);

            // Βεβαιώσου ότι τα index (1, 2, 3, 4) ταιριάζουν με τις στήλες του δικού σου APC txt.
            if (parts.size() >= 5) {
                PropellerData data; // voltage, current, throttle μένουν 0

                data.rpm = parts[1].toDouble();

                // ΠΡΟΣΟΧΗ: Αν το APC είναι σε αμερικάνικες μονάδες (Lbf), κάνε μετατροπή σε Newtons: * 4.44822
                data.thrust = parts[2].toDouble();

                // ΠΡΟΣΟΧΗ: Αν το APC power είναι σε ίππους (Hp), κάνε μετατροπή σε Watts: * 745.7
                data.power = parts[3].toDouble();

                // Ανάγνωση του Advance Ratio (J) από τη στήλη του txt
                data.advance_ratio = parts[4].toDouble();

                // Υπολογισμός απόδοσης για επαλήθευση (ή διάβασέ την κατευθείαν από το txt αν υπάρχει σε άλλη στήλη)
                double airspeed = backend_handler->cfg.airspeed;
                data.efficiency = (data.power > 0) ? (data.thrust * airspeed / data.power) : 0.0;

                apcData.append(data);
            }
        }
    }
    file.close();
    return apcData;
}

void PropellerWindow::refreshPlots() {
    auto extract = [](const PropellerData& data, const QString& param) -> double {
        if (param == "RPM") return data.rpm;
        if (param == "Advance Ratio (J)") return data.advance_ratio;
        if (param == "Thrust (N)") return data.thrust;
        if (param == "Power (W)") return data.power;
        if (param == "Efficiency") return data.efficiency;
        if (param == "Torque (N*m)") return data.torque;
        if (param == "Voltage (V)") return data.voltage;
        if (param == "Current (A)") return data.current;
        if (param == "Throttle(%)") return data.throttle;
        return 0.0;
        };

    for (auto group : groups) {

        QVector<double> x_vals, y_vals, apc_x, apc_y;
        const QString curX = group->x_sel->currentText();
        const QString curY = group->y_sel->currentText();

		for (const auto& data : currentMeanData) { // φτιαχνω graph με τα mean values απο το log
            x_vals.append(extract(data, curX)); 
            y_vals.append(extract(data, curY));
        }
		for (const auto& data : apcReferenceData) { // φτιαχνω graph με τα δεδομενα απο το APC reference txt
            apc_x.append(extract(data, curX));
            apc_y.append(extract(data, curY));
        }

        group->graph->setFullData(x_vals, y_vals);
        group->graph->addSecondaryData(apc_x, apc_y, "APC reference");
    }
}

void PropellerWindow::exportGraphs() {
    backend_handler->playAudio(backend_handler->audio_click_sfx_volume, "MainClick");
    QDir dir("Exports");
    if (!dir.exists()) dir.mkpath(".");

    for (auto group : groups) {
        QString fileName = QString("Exports/Comparison_%1_vs_%2.png")
            .arg(QString(group->y_sel->currentText()).remove(QRegularExpression("[^A-Za-z0-9]")),
                QString(group->x_sel->currentText()).remove(QRegularExpression("[^A-Za-z0-9]")));
        if (group->graph->getPlot()->savePng(fileName, 1200, 800)) {
            qDebug() << "Saved graph to:" << fileName;
        }
    }
}