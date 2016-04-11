#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    createConnection();
}
MainWindow::~MainWindow()
{
}
void MainWindow::createConnection()
{
    connect(ui->actionAbout, SIGNAL(triggered()), this, SLOT(onActionAboutClicked() ));

    connect(ui->widget, SIGNAL(sgConnected(QString)),      this, SLOT(onConnected(QString)    ));
    connect(ui->widget, SIGNAL(sgDisconnected(QString)),   this, SLOT(onDisconnected(QString) ));



   setStyleSheet(" QMainWindow {"
                                  " background-color: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #ff0000, stop: 1 #ffff00); "
                                  "}" );


}

void MainWindow::onActionAboutClicked()
{
    QMessageBox::about(this, "Made by", "CharlesPark");
}

void MainWindow::closeEvent(QCloseEvent * event)
{
    ui->widget->closeEvent(event);
}

void MainWindow::onConnected(QString str)
{
    setStyleSheet(" QMainWindow {"
                                  " background-color: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #007f00, stop: 1 #aaffaa); "
                                  "}" );

    ui->statusBar->showMessage(str);


}
void MainWindow::onDisconnected(QString str)
{
    setStyleSheet(" QMainWindow {"
                                  " background-color: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #ff0000, stop: 1 #ffff00); "
                                  "}" );
    ui->statusBar->showMessage(str);
}




