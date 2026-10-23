#ifndef CONTAINERS_H
#define CONTAINERS_H

#include <QWidget>
#include <QFrame>
#include <QString>
#include "backendhandler.h"


class QLabel;
class QProgressBar;
class QCheckBox;
class QPushButton;
class ThrustStandGUI;

class Containers : public QFrame
{
    Q_OBJECT

public:
    explicit Containers(const QString& title, int titleSize, const QString& titleColor,
        double initialValue, bool hasBar, BackendHandler* backend_handler,
        QWidget* parent = nullptr);

    ~Containers() override = default;

public slots:

    void updateData(double newValue);
    void handleModeChange(const QString& newMode);

private:
    
    BackendHandler* backend_handler = nullptr;

    QString title_text;
    int title_label_size;
    QString title_label_color;
    double value;
    QString unit;
    bool has_bar;
    QString quantity;

    double low_bound = 2.0;
    double high_bound = 5.0;

    QString low_color;
    QString mid_color;
    QString high_color;

    QLabel* title_label;
    QLabel* value_label;
    QProgressBar* progress_bar = nullptr;

    QWidget* control_frame = nullptr;
    QCheckBox* toggle_switch = nullptr;
    QPushButton* slider_opener_button = nullptr;

    void checkBoundsAndSetColor(double currentValue);
    void design();
    void setupControlElements();
    QString formatValueText(double v) const;
    QString barStyle(const QString& color) const;
};

#endif 
