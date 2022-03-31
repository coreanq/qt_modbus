#include "serialport.h"

SerialPort::SerialPort(QObject *parent) :
    QObject(parent),
    m_port(new QSerialPort(this) ),
    m_timerResponseTimeout(new QBasicTimer()),
    m_timerRTU35Timeout(new QBasicTimer() ),
    m_timerElapsedRTUTimeout(new QElapsedTimer() )
{
    setT15IntervalUsec(750);
    setT35IntervalUsec(1500);
    m_lastSendPktSize = 0;
    qRegisterMetaType<QSerialPort::SerialPortError>("QSerialPort::SerialPortError");
    m_isIgnoreAck = false;
}
void SerialPort::init()
{
    createConnection();
    createState();
}

SerialPort::~SerialPort()
{
    if( m_timerResponseTimeout != 0 )
        delete m_timerResponseTimeout;

    if( m_timerElapsedRTUTimeout != 0 )
        delete m_timerElapsedRTUTimeout;

    if( m_timerRTU35Timeout != 0 )
        delete m_timerRTU35Timeout;

}

void SerialPort::createConnection()
{
    connect(m_port,             SIGNAL(bytesWritten(qint64)),           this,               SLOT(onBytesWritten(qint64))        );
    connect(m_port,             SIGNAL(readyRead()),                    this,               SLOT(onReadyRead())                 );
    connect(m_port,             SIGNAL(error(QSerialPort::SerialPortError)), this,          SLOT(onError(QSerialPort::SerialPortError) ));
}


void SerialPort::createState()
{
    m_state = new QStateMachine(this);

    QState *mainState = new QState(m_state);
    QFinalState *finalState = new QFinalState(m_state);

    QState *init = new QState(mainState);
    QState *connected = new QState(mainState);
    QState *disconnected = new QState(mainState);

    QState *ready = new QState(connected);
    QState *sending    = new QState(connected);
    QState *waitAck = new QState(connected);

    m_state->setInitialState(mainState);

    mainState->setInitialState(init);
    connected->setInitialState(ready);

    mainState->addTransition(this,          SIGNAL(sgStateStop()            ),  finalState);

    init->addTransition(this,               SIGNAL(sgInitOk()               ),  disconnected);
    init->addTransition(this,               SIGNAL(sgReInit()               ),  init );

    disconnected->addTransition(this,       SIGNAL(sgConnected()           ),  connected );

    connected->addTransition(this,          SIGNAL(sgTryDisconnect()        ),  disconnected );
    connected->addTransition(this,          SIGNAL(sgError(QString)         ),  disconnected );

    ready->addTransition(this,              SIGNAL(sgSendPktQueued()),          ready);
    ready->addTransition(this,              SIGNAL(sgSendData()      ),         sending);

    sending->addTransition(this,            SIGNAL(sgSendingComplete()      ),  waitAck);
    sending->addTransition(this,            SIGNAL(sgIgnoreAck()            ),  ready);

    waitAck->addTransition(this,            SIGNAL(sgReponseReceived()          ),  ready);
    waitAck->addTransition(this,            SIGNAL(sgResponseTimeout()           ),  ready);

    connect(mainState,      SIGNAL(entered()), this, SLOT(mainStateEntered()        ));
    connect(finalState,     SIGNAL(entered()), this, SLOT(finalStateEntered()       ));

    connect(init,           SIGNAL(entered()), this, SLOT(initEntered()             ));
    connect(connected,      SIGNAL(entered()), this, SLOT(connectedEntered()        ));
    connect(disconnected,   SIGNAL(entered()), this, SLOT(disconnectedEntered()     ));

    connect(ready,          SIGNAL(entered()), this, SLOT(readyEntered()            ));

    connect(sending,        SIGNAL(entered()), this, SLOT(sendingEntered()          ));
    connect(waitAck,        SIGNAL(entered()), this, SLOT(waitAckEntered()          ));


    // history 의 경우 Entered 할수 없음.
    m_state->start();
}



void SerialPort::mainStateEntered(){}

void SerialPort::finalStateEntered()
{
    qDebug() << Q_FUNC_INFO;
}

void SerialPort::initEntered()
{
    qDebug() << Q_FUNC_INFO;
    emit sgInitOk();
}
void SerialPort::disconnectedEntered()
{
    qDebug() << Q_FUNC_INFO << "is port open?" << m_port->isOpen() <<
                m_port->portName() << m_port->baudRate() << m_port->handle() << m_port->parity();
    if( m_port->isOpen() == true )
    {
        m_port->close();
    }
    m_timerResponseTimeout->stop();

    if( m_recvPkt.isEmpty() == false )
    {
        emit sgReceivedErrorData(m_recvPkt);
    }
    m_queueSendPkt.clear();
    m_recvPkt.clear();


    emit sgDisconnected();


}

void SerialPort::connectedEntered()
{
    qDebug() << Q_FUNC_INFO << "is port open?" << m_port->isOpen() <<
                m_port->portName() <<
                m_port->baudRate() <<
                m_port->dataBits() <<
                m_port->parity() <<
                m_port->stopBits() <<
                m_port->handle() ;

}

void SerialPort::readyEntered()
{
//    qDebug() << Q_FUNC_INFO;
    emit sgReadyEntered();
    if( m_queueSendPkt.count() )
    {
        m_lastSendPkt = m_queueSendPkt.takeFirst();
        m_lastSendPktSize = m_lastSendPkt.size();
        emit sgSendData();
    }
}

void SerialPort::sendingEntered()
{
//    qDebug() << Q_FUNC_INFO << m_lastSendPkt.toHex();
    m_port->write(m_lastSendPkt);
}

void SerialPort::waitAckEntered()
{
    m_timerResponseTimeout->start(m_msec_responseTimeout, Qt::PreciseTimer, this);
}


void SerialPort::tryConnect(QString portName, quint32 baudrate, quint32 dataBits, quint32 parity, quint32 stopBits )
{
    m_port->setPortName(portName);

    m_port->setBaudRate(baudrate);
    m_port->setDataBits((QSerialPort::DataBits)dataBits);
    m_port->setParity((QSerialPort::Parity)parity);
    m_port->setStopBits((QSerialPort::StopBits)stopBits);
    m_port->setFlowControl(QSerialPort::NoFlowControl);

    if( m_port->open(QIODevice::ReadWrite) == true )
    {
        emit sgConnected();
    }
    else
    {
        emit sgDisconnected();
    }
}

void SerialPort::tryDisconnect()
{
    if( m_port->isOpen() == true )
    {
        m_port->close();
    }

    emit sgTryDisconnect();
}

void SerialPort::changeBaudrate(quint32 baudrate )
{
    m_port->setBaudRate(baudrate);
    const int multiplier = 1;
    if( baudrate > QSerialPort::Baud19200 )
    {
        // defined in modbus-rtu specification.
        setT15IntervalUsec(750 * multiplier);
        setT35IntervalUsec(1500 * multiplier);
//        qDebug() << Q_FUNC_INFO << "interval changed to default t15" <<  t15IntervalUsec() << "t35" << t35IntervalUsec();
    }
    else
    {
        // 1 byte 구성     start bit + data bits + parity bit + stopbits
        int oneBytesBits = (1 +
                       m_port->dataBits() +
                       (m_port->parity() == QSerialPort::NoParity ? 0 : 1) +
                       (m_port->stopBits() == QSerialPort::OneStop ? 1 : 2 ) );

        setT15IntervalUsec( (1000000 / (m_port->baudRate()  / oneBytesBits)) * 1.5  * multiplier);
        setT35IntervalUsec( (1000000 / (m_port->baudRate() / oneBytesBits)) * 3.5 * multiplier);
//        qDebug() << Q_FUNC_INFO << "interval changed to t15" << t15IntervalUsec() << "t35" << t35IntervalUsec();
    }
}
void SerialPort::changeDataBits(quint32 dataBits)
{
    m_port->setDataBits((QSerialPort::DataBits) dataBits);
}
void SerialPort::changeParity(quint32 parity)
{
    m_port->setParity((QSerialPort::Parity) parity );
}
void SerialPort::changeStopBits(quint32 stopBits)
{
    m_port->setStopBits((QSerialPort::StopBits) stopBits);
}

QString SerialPort::errorString()
{
    return QString("[%1] [error num: %2]")
            .arg( m_port->errorString() )
            .arg(m_port->error());

}


void SerialPort::sendData(QByteArray sendData)
{
//    qDebug() << Q_FUNC_INFO << sendData.toHex();

    if( sendData.size() )
    {
        addTxQueue(sendData);
        emit sgSendPktQueued();
    }
}

void SerialPort::onError(QSerialPort::SerialPortError serialError)
{
    if( serialError == QSerialPort::NoError ||
        serialError == QSerialPort::ParityError  ||
        serialError == QSerialPort::FramingError ||
        serialError == QSerialPort::UnknownError )
        return;

    qDebug() << Q_FUNC_INFO << "errorNumber" << serialError <<
                m_port->portName() <<
                m_port->baudRate() <<
                m_port->dataBits() <<
                m_port->stopBits() <<
                m_port->parity() <<
                m_port->handle() <<
                m_port->errorString();

    emit sgError(m_port->errorString() );
}

void SerialPort::onBytesWritten(qint64 count)
{
    m_lastSendPktSize -= count;
    if( m_lastSendPktSize == 0)
    {
//        qDebug() << Q_FUNC_INFO << m_lastSendPkt.toHex();
        emit sgSendedData(m_lastSendPkt);
        if( isIgnoreAck() == true )
        {
            emit sgIgnoreAck();
        }
        else{
            emit sgSendingComplete();
        }

    }
}


void SerialPort::onReadyRead()
{
    //한 바이트와 한바이트 사이의 시간을 측정하기에는 OS 상에서는 역부족이므로 해당 루틴 넣지 않도록
    // QSerialPort 의 경우 async 방식만 사용해야함 -> 같은 쓰레드에서만 동작하도록 구성되어 있기 때문

    QByteArray buffer;
    while(m_port->bytesAvailable())
    {
        if( m_timerResponseTimeout->isActive() == true )
            m_timerResponseTimeout->start(m_msec_responseTimeout, Qt::PreciseTimer, this);
        buffer = m_port->read(1);

        if( m_commMode == "LSBUS")
        {

            if( buffer.at(0) == 0x05 && buffer.at(0) == 0x06 || buffer.at(0) == 0x15 )
            {
                if( m_recvPkt.isEmpty() == false )
                {
                    emit sgReceivedErrorData(m_recvPkt);
                    m_recvPkt.clear();
                }
            }

            m_recvPkt += buffer;

            if( buffer.at(0) == 0x04 )
            {
                m_timerResponseTimeout->stop();

                if(check_LSBUS_sum(m_recvPkt) == true )
                {
                    emit sgReponseReceived();
                    emit sgReceivedData(m_recvPkt);
                }
                else
                {
                    emit sgReponseReceived();
                    emit sgReceivedErrorData(m_recvPkt);
                }
                m_timerResponseTimeout->stop();
                m_recvPkt.clear();
            }
        }
        else // for RTU
        {
            m_recvPkt += buffer;

            if(check_ModbusRTU_CRC16(m_recvPkt) == true )
            {
                emit sgReponseReceived();
                emit sgReceivedData(m_recvPkt);
                m_timerResponseTimeout->stop();
                m_recvPkt.clear();
            }
            else if( m_recvPkt.count() > 255 )
            {
                emit sgReponseReceived();
                emit sgReceivedErrorData(m_recvPkt);
                m_timerResponseTimeout->stop();
                m_recvPkt.clear();
            }


//            qDebug() << buffer.toHex() <<  m_timerElapsedRTUTimeout->elapsed();
//            m_timerElapsedRTUTimeout->start();
//            m_timerRTU35Timeout->start( qCeil( m_usec_interval_t35 / 1000), Qt::PreciseTimer, this );

         }
    }

}

void SerialPort::timerEvent(QTimerEvent *event)
{
    if ( event->timerId() == m_timerResponseTimeout->timerId())
    {
//        qDebug() << Q_FUNC_INFO << m_msec_responseTimeout << m_recvPkt.toHex();
        m_timerResponseTimeout->stop();
        m_timerRTU35Timeout->stop();

        emit sgResponseTimeout();
        if( m_recvPkt.isEmpty() == false )
        {
            emit sgReceivedErrorData(m_recvPkt);
            m_recvPkt.clear();
        }
    }
    else if ( event->timerId() == m_timerRTU35Timeout->timerId())
    {
//        qDebug() << m_recvPkt.toHex() <<  m_timerElapsedRTUTimeout->elapsed();
        m_timerResponseTimeout->stop();
        m_timerRTU35Timeout->stop();

    }
    else {
        QObject::timerEvent(event);
    }
}


bool SerialPort::check_ModbusRTU_CRC16(QByteArray buf)
{
    if( buf.size() >= 4 )
    {
        QByteArray srcCrc = buf.mid(buf.size()-2, 2);
        QByteArray dstCrc = "";
        quint16 crc = ModbusRTU_CRC16(buf.constData(), buf.size() -2 );
        dstCrc = QByteArray::fromRawData((char*)&crc, 2);

        if( srcCrc == dstCrc )
        {
            return true;
        }
        else
        {
            return false;
        }
    }
    return false;
}
bool SerialPort::check_LSBUS_sum(QByteArray buf)
{
    if( buf.size() >= 2 )
    {
        if( buf.left(1).at(0) == 0x05 ||
                buf.left(1).at(0) == 0x06 ||
                buf.left(1).at(0) == 0x15)
        {
            if( buf.right(1).at(0) == 0x04 )
            {
                QByteArray srcCrc = buf.right(3).left(2);
                QByteArray dstCrc = "";
                quint8 sum = LSBUS_sum(buf.mid(0, buf.count() - 3));
                dstCrc = QByteArray::fromRawData((char*)&sum, 1).toHex().toUpper();

//                qDebug() << buf << buf.toHex() << srcCrc.toHex() << dstCrc.toHex();
                if( srcCrc == dstCrc )
                {
                    return true;
                }
                else
                {
                    return false;
                }
            }

        }

    }
    return false;
}
quint8 SerialPort::LSBUS_sum (QByteArray buf)
{
    quint8 sum = 0x00;
    for ( int i = 1 ; i < buf.count(); i ++ )
    {
//        qDebug() << QByteArray(1, buf.at(i)).toHex();
        sum += buf.at(i);
    }
//     qDebug() << "sum" << QByteArray(1, sum).toHex();
    return sum;
}


quint16 SerialPort::ModbusRTU_CRC16 (const char *buf, quint16 wLength)
{
    static const quint16 wCRCTable[] = {
       0X0000, 0XC0C1, 0XC181, 0X0140, 0XC301, 0X03C0, 0X0280, 0XC241,
       0XC601, 0X06C0, 0X0780, 0XC741, 0X0500, 0XC5C1, 0XC481, 0X0440,
       0XCC01, 0X0CC0, 0X0D80, 0XCD41, 0X0F00, 0XCFC1, 0XCE81, 0X0E40,
       0X0A00, 0XCAC1, 0XCB81, 0X0B40, 0XC901, 0X09C0, 0X0880, 0XC841,
       0XD801, 0X18C0, 0X1980, 0XD941, 0X1B00, 0XDBC1, 0XDA81, 0X1A40,
       0X1E00, 0XDEC1, 0XDF81, 0X1F40, 0XDD01, 0X1DC0, 0X1C80, 0XDC41,
       0X1400, 0XD4C1, 0XD581, 0X1540, 0XD701, 0X17C0, 0X1680, 0XD641,
       0XD201, 0X12C0, 0X1380, 0XD341, 0X1100, 0XD1C1, 0XD081, 0X1040,
       0XF001, 0X30C0, 0X3180, 0XF141, 0X3300, 0XF3C1, 0XF281, 0X3240,
       0X3600, 0XF6C1, 0XF781, 0X3740, 0XF501, 0X35C0, 0X3480, 0XF441,
       0X3C00, 0XFCC1, 0XFD81, 0X3D40, 0XFF01, 0X3FC0, 0X3E80, 0XFE41,
       0XFA01, 0X3AC0, 0X3B80, 0XFB41, 0X3900, 0XF9C1, 0XF881, 0X3840,
       0X2800, 0XE8C1, 0XE981, 0X2940, 0XEB01, 0X2BC0, 0X2A80, 0XEA41,
       0XEE01, 0X2EC0, 0X2F80, 0XEF41, 0X2D00, 0XEDC1, 0XEC81, 0X2C40,
       0XE401, 0X24C0, 0X2580, 0XE541, 0X2700, 0XE7C1, 0XE681, 0X2640,
       0X2200, 0XE2C1, 0XE381, 0X2340, 0XE101, 0X21C0, 0X2080, 0XE041,
       0XA001, 0X60C0, 0X6180, 0XA141, 0X6300, 0XA3C1, 0XA281, 0X6240,
       0X6600, 0XA6C1, 0XA781, 0X6740, 0XA501, 0X65C0, 0X6480, 0XA441,
       0X6C00, 0XACC1, 0XAD81, 0X6D40, 0XAF01, 0X6FC0, 0X6E80, 0XAE41,
       0XAA01, 0X6AC0, 0X6B80, 0XAB41, 0X6900, 0XA9C1, 0XA881, 0X6840,
       0X7800, 0XB8C1, 0XB981, 0X7940, 0XBB01, 0X7BC0, 0X7A80, 0XBA41,
       0XBE01, 0X7EC0, 0X7F80, 0XBF41, 0X7D00, 0XBDC1, 0XBC81, 0X7C40,
       0XB401, 0X74C0, 0X7580, 0XB541, 0X7700, 0XB7C1, 0XB681, 0X7640,
       0X7200, 0XB2C1, 0XB381, 0X7340, 0XB101, 0X71C0, 0X7080, 0XB041,
       0X5000, 0X90C1, 0X9181, 0X5140, 0X9301, 0X53C0, 0X5280, 0X9241,
       0X9601, 0X56C0, 0X5780, 0X9741, 0X5500, 0X95C1, 0X9481, 0X5440,
       0X9C01, 0X5CC0, 0X5D80, 0X9D41, 0X5F00, 0X9FC1, 0X9E81, 0X5E40,
       0X5A00, 0X9AC1, 0X9B81, 0X5B40, 0X9901, 0X59C0, 0X5880, 0X9841,
       0X8801, 0X48C0, 0X4980, 0X8941, 0X4B00, 0X8BC1, 0X8A81, 0X4A40,
       0X4E00, 0X8EC1, 0X8F81, 0X4F40, 0X8D01, 0X4DC0, 0X4C80, 0X8C41,
       0X4400, 0X84C1, 0X8581, 0X4540, 0X8701, 0X47C0, 0X4680, 0X8641,
       0X8201, 0X42C0, 0X4380, 0X8341, 0X4100, 0X81C1, 0X8081, 0X4040 };

    quint8 nTemp;
    quint16 CRC16 = 0xFFFF;

    while (wLength--)
    {
        nTemp = *buf++ ^ CRC16;
        CRC16 >>= 8;
        CRC16  ^= wCRCTable[nTemp];
    }
    return CRC16;
} // End: CRC16


