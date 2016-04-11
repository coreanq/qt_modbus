#include <QApplication>
#include "mainwindow.h"
#include "version.h"



int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    QCoreApplication::setOrganizationName("LSIS");
    QCoreApplication::setOrganizationDomain("LV Drvie");
    QCoreApplication::setApplicationName(QString("%1").arg(PROGRAM_TITLE));

    w.setWindowTitle(QString("%1 ver %2")
                     .arg(PROGRAM_TITLE)
                     .arg(VER_NUMBER));
    w.show();
    return a.exec();
}
