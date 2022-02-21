#-------------------------------------------------
#
# Project created by QtCreator 2014-04-09T16:06:22
#
#-------------------------------------------------

QT       += core gui serialport

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = ModbusRTU-ASCII Tester
TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp \
    serialport.cpp \
    modbuswnd.cpp

HEADERS  += mainwindow.h \
    serialport.h \
    version.h \
    modbuswnd.h \
    CRC.h

FORMS    += mainwindow.ui \
    form.ui

