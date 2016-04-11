#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSerialPortInfo>
#include <QDebug>
#include <QThread>
#include <QBuffer>
#include <QTimer>
#include <QDateTime>

#include <QStateMachine>
#include <QFinalState>

#include "serialport.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
    void createConnection();
    void closeEvent(QCloseEvent * event);

signals:


private:
    Ui::MainWindow *ui;
    SerialPort*             m_port;


public slots:
    void onActionAboutClicked();
    void onConnected(QString str);
    void onDisconnected(QString str);



};

#endif // MAINWINDOW_H
