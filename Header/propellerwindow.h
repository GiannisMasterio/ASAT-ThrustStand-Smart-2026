#ifndef PROPELLERWINDOW_H
#define PROPELLERWINDOW_H

#include <QWidget>
#include <QTextEdit>


class PropellerWindow : public QWidget {
    Q_OBJECT

public:
    explicit PropellerWindow(QWidget* parent = nullptr);
    void updateLog(const QString& data);
    void addDataPoint(double rpm, double thrust);

private slots:
    void handleExport();

private:
    QTextEdit* m_logDisplay;
 

    void setupUI();
};

#endif