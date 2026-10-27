#include "PropellerWindow.h"
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QChart>
#include <QVBoxLayout>
#include <QPushButton>
#include <QFile>
#include <QFileDialog>
PropellerWindow::PropellerWindow(QWidget* parent) : QWidget(parent) {
    setupUI();
    setWindowTitle("Propeller Analysis Graphs");
    resize(800, 600);
}

void PropellerWindow::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    

    // --- 2. Log Section ---
    //m_logDisplay = new QTextEdit();
    //m_logDisplay->setReadOnly(true);
    //m_logDisplay->setPlaceholderText("Propeller logs will appear here...");

    // --- 3. Buttons ---
    QPushButton* exportBtn = new QPushButton("Export to CSV");
    connect(exportBtn, &QPushButton::clicked, this, &PropellerWindow::handleExport);

    // Layout Arrangement
    
    //mainLayout->addWidget(m_logDisplay, 1);
    mainLayout->addWidget(exportBtn);
}

void PropellerWindow::addDataPoint(double rpm, double thrust) {

}

void PropellerWindow::updateLog(const QString& data) {
    m_logDisplay->append(data);
}

void PropellerWindow::handleExport() {
    /*QString fileName = QFileDialog::getSaveFileName(this, "Save Log", "", "CSV Files (*.csv);;Text Files (*.txt)");

    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
  
        out << m_logDisplay->toPlainText();
        file.close();
    } */
}