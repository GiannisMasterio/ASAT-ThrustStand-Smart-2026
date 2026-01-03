#include <QApplication>
#include "ThrustStandGUI.h" 

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    ThrustStandGUI window;

    window.show();
    return app.exec();
}