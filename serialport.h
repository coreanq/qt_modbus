#ifndef SERIALPORT_H
#define SERIALPORT_H

#include <QObject>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QDebug>

#include <QStateMachine>
#include <QFinalState>
#include <QElapsedTimer>
#include <QDateTime>
#include <QThread>
#include <QBasicTimer>
#include <QCoreApplication>
#include <QtMath>

class SerialPort : public QObject
{
    Q_OBJECT
public:
    explicit SerialPort(QObject *parent = 0);
    ~SerialPort();

    void setResponseTimeout(int msec) { m_msec_responseTimeout = msec; }
    int responseTimeout() { return m_msec_responseTimeout; }
    void setT15IntervalUsec(int usec) { m_usec_interval_t15 = usec; emit sgT15IntervalChanged(); }
    void setT35IntervalUsec(int usec) { m_usec_interval_t35 = usec; emit sgT35IntervalChanged(); }
    int t15IntervalUsec() { return m_usec_interval_t15; }
    int t35IntervalUsec() { return m_usec_interval_t35; }
    void setMode(QString mode) { m_commMode = mode; }
    QString mode() { return m_commMode; }
    void setIgnoreAck(bool ignore){ m_isIgnoreAck = ignore; }
    bool isIgnoreAck() { return m_isIgnoreAck; }

    bool check_LSBUS_sum(QByteArray buf);
    bool check_ModbusRTU_CRC16(QByteArray buf);
    quint8 LSBUS_sum (QByteArray buf);
    quint16 ModbusRTU_CRC16 (const char *buf, quint16 wLength);

private:
    void addTxQueue(QByteArray sendData) { m_queueSendPkt.append(sendData); emit sgSendPktQueueCountChanged();}
    void resetTxQueue() { m_queueSendPkt.clear(); emit sgSendPktQueueCountChanged(); }
    void timerEvent(QTimerEvent *event);


signals:
    void sgReceivedErrorData(QByteArray errorData);
    void sgReceivedData(QByteArray recvedData);
    void sgSendedData(QByteArray sendPkt);

    void sgStateStop();
    void sgInitOk();
    void sgReInit();

    void sgConnectingTimeout();
    void sgConnected();
    void sgDisconnected();
    void sgTryDisconnect();

    void sgError(QString error);

    void sgSendPktQueued();
    void sgSendData();

    void sgSendingComplete();

    void sgReponseReceived();
    void sgResponseTimeout();

    void sgSendPktQueueCountChanged();
    void sgT15IntervalChanged();
    void sgT35IntervalChanged();

    void sgReadyEntered();
    void sgIgnoreAck();

public slots:
    void init();

    void tryConnect(QString portName, quint32 baudrate , quint32 dataBits, quint32 parity, quint32 stopBits);
    void tryDisconnect();
    void sendData(QByteArray sendData);
    void changeBaudrate(quint32 baudrate);
    void changeDataBits(quint32 dataBits);
    void changeParity(quint32 parity);
    void changeStopBits(quint32 stopBits);
    QString errorString();

    void onBytesWritten(qint64)  ;
    void onReadyRead();
    void mainStateEntered();
    void finalStateEntered();

    void initEntered();
    void connectedEntered();
    void disconnectedEntered();

    void readyEntered();

    void sendingEntered();
    void waitAckEntered();

    void onError(QSerialPort::SerialPortError serialError);

private:
    void createConnection();
    void createState();

    QSerialPort*                m_port;
    QByteArray                  m_recvPkt;
    QList<QByteArray>           m_queueSendPkt;

    QByteArray                  m_lastSendPkt;
    int                         m_lastSendPktSize;
    QStateMachine*              m_state;

    QStringList                 m_availablePorts;

    QBasicTimer*				m_timerResponseTimeout;
    QBasicTimer*                m_timerRTU35Timeout;
    QElapsedTimer*              m_timerElapsedRTUTimeout;

    int                         m_msec_responseTimeout;
    qreal 						m_usec_interval_t15;
    qreal						m_usec_interval_t35;

    QString                     m_commMode;
    bool                        m_isIgnoreAck;
};

#endif // SERIALPORT_H
