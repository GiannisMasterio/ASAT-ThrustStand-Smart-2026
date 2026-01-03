#ifndef CONTAINERS_H
#define CONTAINERS_H

#include <QWidget>
#include <QFrame>
#include <QString>
#include "modehandler.h"


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
        double initialValue, bool hasBar, const QString& variableQuantity, ThrustStandGUI* guiMaster,
        QWidget* parent = nullptr);

    ~Containers() override = default;

    ModeHandler* getModeHandler() const { return mode_handler; }
    // ειναι για να επιστρεψει pointer στο αντικειμενο και να καλεσω ετσι καποιο method του

public slots:
    void updateData(double newValue, double currentRpm);
    void handleModeChange(const QString& newMode);

private:
    QString title_text;
    int title_label_size;
    QString title_label_color;
    double value;
    QString unit;
    bool has_bar;
    QString quantity;
    QString function_mode;

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

    ModeHandler* mode_handler = nullptr;
    ThrustStandGUI* m_guiMaster;

    void checkBoundsAndSetColor(double currentValue);
    void design();
    void setupControlElements();
    double getThrottlePercentage() const;
};

#endif 
