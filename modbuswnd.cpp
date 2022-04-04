#include "modbuswnd.h"
#include "ui_form.h"
#include "version.h"
#include "CRC.h"



ModbusWnd::ModbusWnd(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Form),
    m_modelTxPktQueue(new QStandardItemModel(this) ),
    m_modelTxRxResult(new QStandardItemModel(this) ),
    m_threadForSerial(new QThread(this) ),
    m_timerAutoSend(new QTimer(this)),
    m_timer100msec(new QTimer(this)),
    m_reHex("[a-fA-F0-9]+", QRegularExpression::CaseInsensitiveOption),
    m_recvedCount(0),
    m_sendedCount(0),
    m_1msCount(1)
{
    ui->setupUi(this);
    onBtnRefreshClicked();
    m_timerAutoSend->setSingleShot(true);
    m_timer100msec->setInterval(100);

    ui->cmbBaudrate->setCurrentText("9600");

    QStringList headers;
    headers << "ENABLE" << "HEX" << "ASCII" << "COMMENT";
    m_modelTxPktQueue->setHorizontalHeaderLabels(headers);
    ui->viewTxQueueData->setModel(m_modelTxPktQueue);
    ui->viewTxQueueData->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->viewTxQueueData->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->viewTxQueueData->setSelectionBehavior(QAbstractItemView::SelectRows);

    headers.clear();
    headers << "TIME STAMP" << "STATUS" << "MODE" << "RAW DATA" <<  "CRC" << "S ADDR" << "FUNC" << "INFO";
    m_modelTxRxResult->setHorizontalHeaderLabels(headers);
    ui->viewTxRxResult->setModel(m_modelTxRxResult);
    ui->viewTxRxResult->setEditTriggers(QAbstractItemView::NoEditTriggers);
//    ui->viewTxRxResult->horizontalHeader()->setStretchLastSection(true);
    ui->viewTxRxResult->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->viewTxRxResult->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->viewTxRxResult->setSelectionBehavior(QAbstractItemView::SelectRows);


    m_rxCount = 0;
    m_txCount = 0;
    m_errorCount = 0;

    m_txRxResultMaxRow = 20000;

    createConnection();
    readSettings();

    ui->lineWriteData->setValidator(new QRegularExpressionValidator(m_reHex) );
    ui->lineAddr->setValidator(new QRegularExpressionValidator(m_reHex) );
}

ModbusWnd::~ModbusWnd()
{
    delete ui;
    m_threadForSerial->exit();
}

void ModbusWnd::createConnection()
{
    m_port = new SerialPort();
    // for serial
    connect(m_port,                     SIGNAL(sgReceivedData(QByteArray)),     this,               SLOT(onRecvedData(QByteArray) ));
    connect(m_port,                     SIGNAL(sgReceivedErrorData(QByteArray)),     this,          SLOT(onRecvedErrorData(QByteArray) ));
    connect(m_port,                     SIGNAL(sgSendedData(QByteArray)),       this,               SLOT(onSendedData(QByteArray)   ));
    connect(m_threadForSerial,          SIGNAL(finished()),                     m_port,             SLOT(deleteLater())                 );
    connect(m_threadForSerial,          SIGNAL(started()),                      m_port,             SLOT(init())                 );

    connect(m_port,                     SIGNAL(sgConnected()),                  this,               SLOT(onPortConnected()                        ));
    connect(m_port,                     SIGNAL(sgDisconnected()),               this,               SLOT(onPortDisconnected()                     ));
    connect(m_port,                     SIGNAL(sgResponseTimeout()),            this,               SLOT(onPortResponseTimeout()                  ));
    connect(m_port,                     SIGNAL(sgT15IntervalChanged()),         this,               SLOT(onT15IntervalChanged()                   ));
    connect(m_port,                     SIGNAL(sgT35IntervalChanged()),         this,               SLOT(onT35IntervalChanged()                   ));

    // 타이밍이 중요 하므로 쓰레드 우선순위를 높임
    m_port->moveToThread(m_threadForSerial);
    m_threadForSerial->start(QThread::TimeCriticalPriority);

    connect(ui->btnOpen,                SIGNAL(clicked()),                      this, SLOT(onBtnOpenClicked()                       ));
    connect(ui->btnClose,               SIGNAL(clicked()),                      this, SLOT(onBtnCloseClicked())                     );

    connect(ui->cmbComPort,             SIGNAL(currentTextChanged(QString)),    this, SLOT(onCmbComportTextChanged(QString)         ));
    connect(ui->cmbBaudrate,            SIGNAL(currentTextChanged(QString)),    this, SLOT(onCmbBaudrateTextChanged(QString)        ));
    connect(ui->cmbDataBits,            SIGNAL(currentTextChanged(QString)),    this, SLOT(onCmbDataBitsTextChanged(QString)        ));
    connect(ui->cmbParity,              SIGNAL(currentTextChanged(QString)),    this, SLOT(onCmbParityTextChanged(QString)          ));
    connect(ui->cmbStopBits,            SIGNAL(currentTextChanged(QString)),    this, SLOT(onCmbStopBitsTextChanged(QString)        ));

    connect(ui->lineDecimalChange,      SIGNAL(editingFinished()),              this, SLOT(onLineDecimalChanged() ));
    connect(ui->lineHexChange,          SIGNAL(editingFinished()),              this, SLOT(onLineHexChanged()   ));


    connect(ui->cmbCommMode,                SIGNAL(currentTextChanged(QString)),this, SLOT(onCmbCommModeCurrentTextChanged(QString)     ));
    connect(ui->cmbFunction,            SIGNAL(currentTextChanged(QString)),    this, SLOT(onCmbFunctionCurrentTextChanged(QString) ));
    connect(ui->cmbSendCount,           SIGNAL(currentTextChanged(QString)),    this, SLOT(onCmbSendCountTextChanged(QString)       ));
    connect(ui->chkAutoSend,            SIGNAL(toggled(bool)),                  this, SLOT(onChkAutoSendToggled(bool)               ));
    connect(ui->chkIgnoreAck,           SIGNAL(toggled(bool)),                  this, SLOT(onChkIgnoreAckToggled(bool)               ));    


    connect(ui->lineModbusTimeout,      SIGNAL(textChanged(QString)),           this, SLOT(onLineModbusTimeoutChanged(QString) ));
    connect(ui->lineModbusTimeout,      SIGNAL(editingFinished()),              this, SLOT(onLineModbusEditingFinished()));

    connect(ui->btnManualSend,          SIGNAL(clicked()    )              ,    this, SLOT(onBtnManualSendClicked()                 ));
    connect(ui->btnScreenClear,         SIGNAL(clicked()    )              ,    this, SLOT(onBtnScreenClearClicked()                ));
    connect(ui->btnRefresh,             SIGNAL(clicked()    )              ,    this, SLOT(onBtnRefreshClicked()                    ));

    connect(this,                       SIGNAL(sgRxCountChanged()),             this, SLOT(onRxCountChanged()                       ));
    connect(this,                       SIGNAL(sgTxCountChanged()),             this, SLOT(onTxCountChanged()                       ));
    connect(this,                       SIGNAL(sgErrorCountChanged()),          this, SLOT(onErrorCountChanged()                       ));

    connect(ui->btnResultFileSave,      SIGNAL(clicked() ),                     this, SLOT(onBtnResultFileSaveClicked()             ));

    connect(ui->btnTxQueueAdd,          SIGNAL(clicked() ),                     this, SLOT(btnTxQueueAddClicked()                   ));
    connect(ui->btnTxQueueRemove,       SIGNAL(clicked() ),                     this, SLOT(btnTxQueueRemoveClicked()                ));
    connect(ui->btnTxQueueUp,           SIGNAL(clicked() ),                     this, SLOT(btnTxQueueUpClicked()                    ));
    connect(ui->btnTxQueueDown,         SIGNAL(clicked() ),                     this, SLOT(btnTxQueueDownClicked()                  ));

    connect(ui->btnCheckSum,            SIGNAL(clicked() ),                     this, SLOT(btnCheckSumClicked()                     ));
    connect(ui->btnCRC16,               SIGNAL(clicked() ),                     this, SLOT(btnCRC16Clicked()                        ));
    connect(ui->btnCRC32,               SIGNAL(clicked() ),                     this, SLOT(btnCRC32Clicked()                        ));
    connect(ui->btnDataInputClear,      SIGNAL(clicked() ),                     ui->lineDataInput, SLOT(clear() ));




    connect(ui->lineAscToHexAscInput,   SIGNAL(returnPressed()),                this, SLOT(onLineAscToHexAscInputClicked()   ));
    connect(ui->lineAscToHexHexInput,   SIGNAL(returnPressed()),                this, SLOT(onLineAscToHexHexInputClicked()   ));

    connect(ui->btnAscInput,            SIGNAL(clicked() ),                     ui->lineAscToHexAscInput,         SLOT(clear() ));
    connect(ui->btnHexInput,            SIGNAL(clicked() ),                     ui->lineAscToHexHexInput,         SLOT(clear() ));


    connect(m_modelTxPktQueue,        SIGNAL(dataChanged(QModelIndex,QModelIndex)),
            this, SLOT(onModelTxPktQueueDataChanged(QModelIndex, QModelIndex) ));




    connect(m_timerAutoSend,            SIGNAL(timeout()),                      this, SLOT(onTimerAutoSendTimeout()                 ));
    connect(m_timer100msec,             SIGNAL(timeout()),                      this, SLOT(onTimer100msTimeout()                 ));

    connect(m_modelTxRxResult,          SIGNAL(rowsInserted(const QModelIndex &, int, int)),
            this, SLOT(onModelTxResultRowInserted(const QModelIndex &, int, int)));


    connect(ui->viewTxQueueData->selectionModel(),
            SIGNAL(currentRowChanged(const QModelIndex&, const QModelIndex&)),                 this,
            SLOT(onViewTxQueueDataSelectionCurrentRowChanged(const QModelIndex &, const QModelIndex &) ));

}

void ModbusWnd::readSettings()
{

    QSettings settings(APP_CONFIG_FILE, QSettings::IniFormat);

    QString cmbComport = settings.value("cmbComport").toString();
    QString cmbBaudrate = settings.value("cmbBaudrate").toString();
    QString cmbDataBits = settings.value("cmdDataBits").toString();
    QString cmbParity  = settings.value("cmbParity").toString();
    QString cmbStopBits = settings.value("cmbStopBits").toString();
    QString lineAddr = settings.value("lineAddr").toString();
    QString lineInterval = settings.value("lineInterval").toString();
    QString lineSlaveAddr = settings.value("lineSlaveAddr").toString();
    QString lineWriteData = settings.value("lineWriteData").toString();
    QString cmbFunction = settings.value("cmbFunction").toString();
    QString cmbSendCount = settings.value("cmbSendCount").toString();
    bool chkAutoSend = settings.value("chkAutoSend").toBool();
    QString lineModbusTimeout = settings.value("lineModbusTimeout").toString();
    QString cmbCommMode = settings.value("cmbCommMode").toString();
    QStringList txQueueData = settings.value("txQueueData").toStringList();

    foreach(QString str, txQueueData)
    {
        QList<QStandardItem*> items;
        QStringList tempStrs = str.split(",");

        for(int i = 0; i < MODBUS_WND_TX_QUEUE_COLUMNS::COUNT; i++ )
        {
            QStandardItem* insertedItem = new QStandardItem();

            switch(i)
            {
            case MODBUS_WND_TX_QUEUE_COLUMNS::ENABLE:
                insertedItem->setCheckable(true);
                insertedItem->setEditable(false);
                break;
            case MODBUS_WND_TX_QUEUE_COLUMNS::ASCII:
                insertedItem->setEditable(false);
                insertedItem->setEditable(false);
                insertedItem->setText(tempStrs.at(i-1).toLatin1());
            default:
                insertedItem->setText(tempStrs.at(i-1));
            }
            items << insertedItem;
        }

        m_modelTxPktQueue->appendRow(items);


    }

    qDebug() << Q_FUNC_INFO << cmbComport << cmbBaudrate << cmbDataBits << cmbParity << cmbStopBits <<
                lineAddr << lineInterval << lineSlaveAddr << lineWriteData << cmbFunction << cmbSendCount << chkAutoSend << lineModbusTimeout;
    if( cmbComport.isEmpty() == false)
        ui->cmbComPort->setCurrentText(cmbComport);
    if( cmbBaudrate.isEmpty() == false)
        ui->cmbBaudrate->setCurrentText(cmbBaudrate);
    if( cmbDataBits.isEmpty() == false)
        ui->cmbDataBits->setCurrentText(cmbDataBits);
    if( cmbParity.isEmpty() == false)
        ui->cmbParity->setCurrentText(cmbParity);
    if( cmbStopBits.isEmpty() == false)
        ui->cmbStopBits->setCurrentText(cmbStopBits);


    if( lineAddr.isEmpty() == false)
        ui->lineAddr->setText(lineAddr);
    if( lineInterval.isEmpty() == false)
        ui->lineInterval->setText(lineInterval);
    if( lineSlaveAddr.isEmpty() == false)
        ui->lineSlaveAddr->setText(lineSlaveAddr);
    if( lineWriteData.isEmpty() == false)
        ui->lineWriteData->setText(lineWriteData);
    if( cmbFunction.isEmpty() == false)
        ui->cmbFunction->setCurrentText(cmbFunction);
    if( cmbSendCount.isEmpty() == false)
    {
        ui->cmbSendCount->setCurrentText(cmbSendCount);        
    }
    onCmbFunctionCurrentTextChanged(cmbFunction);
//    ui->chkAutoSend->setChecked(chkAutoSend);

    if( lineModbusTimeout.isEmpty() == false)
    {
        ui->lineModbusTimeout->setText(lineModbusTimeout);
        m_port->setResponseTimeout(lineModbusTimeout.toInt() );
    }
    else
    {
        ui->lineModbusTimeout->setText("500");
        m_port->setResponseTimeout(500);
    }

    if( cmbCommMode.isEmpty() == false)
    {
        ui->cmbCommMode->setCurrentText(cmbCommMode);
        m_port->setMode(cmbCommMode);
    }

    if( ui->cmbCommMode->currentText().contains("RTU") == true )
    {
        ui->chkAddrMinusOne->setVisible(true);
    }
    else
    {
        ui->chkAddrMinusOne->setVisible(false);
    }

}

void ModbusWnd::closeEvent(QCloseEvent * event)
{

    Q_UNUSED(event);
    QSettings settings(APP_CONFIG_FILE, QSettings::IniFormat);

    QString cmbComport = ui->cmbComPort->currentText();
    QString cmbBaudrate = ui->cmbBaudrate->currentText();
    QString cmbDataBits = ui->cmbDataBits->currentText();
    QString cmbParity  = ui->cmbParity->currentText();
    QString cmbStopBits = ui->cmbStopBits->currentText();
    QString lineAddr = ui->lineAddr->text();
    QString lineInterval = ui->lineInterval->text();
    QString lineSlaveAddr = ui->lineSlaveAddr->text();
    QString lineWriteData = ui->lineWriteData->text();
    QString cmbFunction = ui->cmbFunction->currentText();
    QString cmbSendCount = ui->cmbSendCount->currentText();
    bool chkAutoSend = ui->chkAutoSend->checkState() == Qt::Unchecked ? false : true;
    QString lineModbusTimeout = ui->lineModbusTimeout->text();
    QStringList txQueueData;

    for( int i = 0; i < m_modelTxPktQueue->rowCount(); i ++ )
    {
        txQueueData << m_modelTxPktQueue->item(i, MODBUS_WND_TX_QUEUE_COLUMNS::HEX)->text() + " ," +
                       m_modelTxPktQueue->item(i, MODBUS_WND_TX_QUEUE_COLUMNS::ASCII)->text() + " ," +
                       m_modelTxPktQueue->item(i, MODBUS_WND_TX_QUEUE_COLUMNS::COMMENT)->text();
    }

    settings.setValue("cmbComport", cmbComport );
    settings.setValue("cmbBaudrate", cmbBaudrate);
    settings.setValue("cmdDataBits", cmbDataBits);
    settings.setValue("cmbParity", cmbParity);
    settings.setValue("cmbStopBits", cmbStopBits);
    settings.setValue("lineAddr", lineAddr);
    settings.setValue("lineInterval", lineInterval);
    settings.setValue("lineSlaveAddr", lineSlaveAddr);
    settings.setValue("lineWriteData", lineWriteData);
    settings.setValue("cmbFunction", cmbFunction);
    settings.setValue("cmbSendCount", cmbSendCount);
    settings.setValue("chkAutoSend", chkAutoSend);
    settings.setValue("lineModbusTimeout", lineModbusTimeout);
    settings.setValue("txQueueData", txQueueData);
    settings.sync();
}

void ModbusWnd::addDataToTxRxResultModel(QStringList info)
{
    QStandardItemModel* model = m_modelTxRxResult;

    QList <QStandardItem*> addItems;
    QBrush foregroundBursh(Qt::black);

    for ( int i = 0 ; i < MODBUS_WND_TXRX_RESULT_COLUMNS::COUNT ; i ++ )
    {
        QStandardItem* tempItem = new QStandardItem();
        switch(i)
        {
        case MODBUS_WND_TXRX_RESULT_COLUMNS::TIMESTAMP:
        {
            QString timeStamp = info.at(MODBUS_WND_TXRX_RESULT_COLUMNS::TIMESTAMP);
            QString status = info.at(MODBUS_WND_TXRX_RESULT_COLUMNS::STATUS);
            if( status.contains("RX") == true )
                foregroundBursh.setColor(Qt::blue);
            tempItem->setText(timeStamp);
            break;
        }
        case MODBUS_WND_TXRX_RESULT_COLUMNS::STATUS           :
        {
            QString status = info.at(MODBUS_WND_TXRX_RESULT_COLUMNS::STATUS);
            if( status.contains("NOK") == true )
                foregroundBursh.setColor(Qt::red);
            tempItem->setText(status);
            break;
        }
        case MODBUS_WND_TXRX_RESULT_COLUMNS::MODE           :
        {
            QString status = info.at(MODBUS_WND_TXRX_RESULT_COLUMNS::MODE);
            tempItem->setText(status);
            break;
        }
        case MODBUS_WND_TXRX_RESULT_COLUMNS::RAWDATA        :
            tempItem->setText(info.at(MODBUS_WND_TXRX_RESULT_COLUMNS::RAWDATA));
            break;
        case MODBUS_WND_TXRX_RESULT_COLUMNS::CRC        :
            tempItem->setText(info.at(MODBUS_WND_TXRX_RESULT_COLUMNS::CRC));
            break;

        case MODBUS_WND_TXRX_RESULT_COLUMNS::SLAVE_ADDR             :
            tempItem->setText("[" +info.at(MODBUS_WND_TXRX_RESULT_COLUMNS::SLAVE_ADDR) + "]");
            break;
        case MODBUS_WND_TXRX_RESULT_COLUMNS::FUNCTION        :
            tempItem->setText("[" + info.at(MODBUS_WND_TXRX_RESULT_COLUMNS::FUNCTION) + "]");
            break;
        case MODBUS_WND_TXRX_RESULT_COLUMNS::INFO        :
            tempItem->setText(info.at(MODBUS_WND_TXRX_RESULT_COLUMNS::INFO));
            break;

        }
        addItems << tempItem;
    }
    foreach(QStandardItem* item, addItems)
    {
        item->setEditable(false);
        item->setForeground(foregroundBursh);
    }

    model->appendRow(addItems);

    if( ui->chkAutoScroll->checkState() == Qt::Checked )
        ui->viewTxRxResult->scrollToBottom();
}

void ModbusWnd::addDataToTxPktQueueModel(QStringList parsedData)
{
    QStandardItemModel* model = m_modelTxPktQueue;

    QList <QStandardItem*> addItems;
    QBrush foregroundBursh(Qt::black);

    for ( int i = 0 ; i < MODBUS_WND_TX_QUEUE_COLUMNS::COUNT ; i ++ )
    {
        QStandardItem* tempItem = new QStandardItem();

        switch(i)
        {
        case MODBUS_WND_TX_QUEUE_COLUMNS::ENABLE:
        {
            tempItem->setCheckable(true);
            tempItem->setCheckState(Qt::Checked);
            tempItem->setEditable(false);
            break;
        }
        case MODBUS_WND_TX_QUEUE_COLUMNS::HEX        :
            tempItem->setEditable(true);
            tempItem->setText(parsedData.at(MODBUS_WND_TXRX_RESULT_COLUMNS::RAWDATA));
            break;

        case MODBUS_WND_TX_QUEUE_COLUMNS::ASCII        :
        {
            tempItem->setEditable(false);

            QByteArray text;
            QByteArray targetText = "";
            text = parsedData.at(MODBUS_WND_TXRX_RESULT_COLUMNS::RAWDATA).toLatin1().trimmed();
            text = text.replace(" " , "");

            for (int i =0; i < text.count() ; i = i+2 )
            {
                targetText +=QByteArray::fromHex(text.mid(i, 2));

            }
            tempItem->setText(targetText);
        }
            break;

        case MODBUS_WND_TX_QUEUE_COLUMNS::COMMENT        :
            tempItem->setCheckable(false);
            tempItem->setEditable(true);
            tempItem->setText("\"comment\"");
            break;

        }
        addItems << tempItem;
    }
    foreach(QStandardItem* item, addItems)
    {
        item->setForeground(foregroundBursh);
    }

    model->appendRow(addItems);
    ui->viewTxQueueData->scrollToBottom();
}


void ModbusWnd::onSendedData(QByteArray data)
{
    QStringList sendedData;
    m_sendedCount += data.size();

    onParseData(sendedData, data);
    sendedData[MODBUS_WND_TXRX_RESULT_COLUMNS::STATUS] = "[TX " + sendedData.at(MODBUS_WND_TXRX_RESULT_COLUMNS::STATUS);

    m_sendedTimeStamp = QDateTime::currentMSecsSinceEpoch();
    addDataToTxRxResultModel(sendedData);
    addTxCount();
}

void ModbusWnd::onRecvedData(QByteArray data)
{
    QStringList receivedData;

    m_recvedCount += data.size();

    onParseData(receivedData, data);
    receivedData[MODBUS_WND_TXRX_RESULT_COLUMNS::STATUS] = "[RX " + receivedData.at(MODBUS_WND_TXRX_RESULT_COLUMNS::STATUS);

    m_recvedTimeStamp = QDateTime::currentMSecsSinceEpoch();

    //qDebug() << m_recvedTimeStamp - m_sendedTimeStamp <<  " " <<   receivedData << "\n";
    addDataToTxRxResultModel(receivedData);
    addRxCount();
}


void ModbusWnd::onRecvedErrorData(QByteArray data)
{
    QStringList receivedData;
    onParseData(receivedData, data);
    receivedData[MODBUS_WND_TXRX_RESULT_COLUMNS::STATUS] = "[RX " + receivedData.at(MODBUS_WND_TXRX_RESULT_COLUMNS::STATUS);
    addDataToTxRxResultModel(receivedData);
    addRxCount();
}

int ModbusWnd::onParseData(QStringList &parsedData, QByteArray data)
{
    QString timeStamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    QString status = "";
    QString rawData = QString(ModbusWnd::toHexString(data));
    QString slaveAddr = "";
    QString function = "";
    QString info = "";
    QString crc = "";
    QString commMode = ui->cmbCommMode->currentText();

    int exception_code = 0x00;
    if( commMode == "RTU")
    {
        if( data.count() >= 4 )
        {
            slaveAddr = QString( data.mid(0, 1).toHex() );
            function = QString( data.mid(1, 1).toHex() );
            crc = QString( data.mid(data.count() -2 , 2).toHex() );

            if( data.at(1) & 0x80 )
                exception_code = data.at(2);

            quint16 crc_result = 0x0ffff;
            if( check_ModbusRTU_CRC16(data, crc_result) == true )
            {
                if( exception_code != 0x00 )
                {
                    info += QString("[ERROR CODE] ") + data.mid(2, data.count() - 4).toHex();
                    exception_code = 0xff;
                }
            }
            else
            {
                info = QString("[ERROR CRC] expected %1").arg(crc_result, 4, 16, QChar('0'));
                exception_code = 0xff;
            }
        }
        else
        {
            exception_code = 0xff;
        }
    }
    else if( commMode == "LSBUS")
    {
        if( data.count() >= 4 )
        {
            QString response_start = QString( data.mid(0, 1).toHex() );
            slaveAddr = QString( data.mid(1, 2) );
            function = QString( data.mid(3, 1) );
            crc = QString( data.mid(data.count() -3, 2) );

            quint8 sum = LSBUS_sum(data.mid(0, data.count() - 3));

            if( sum == crc.toInt(NULL, 16) )
            {
                if( response_start == "15" )
                {
                    info += QString("[ERROR CODE] ") + data.mid(4, 2) + "  ";
                    exception_code = 0xff;
                }
                info += "" + data.mid(0, data.count()) ;
            }
            else
            {
                info = QString("[ERROR SUM] expected %1").arg(sum, 4, 16, QChar('0'));
                exception_code = 0xff;
            }
        }
        else
        {
            exception_code = 0xff;
        }
    }


    if( exception_code != 0x00 )
    {
        status = "NOK]";
    }
    else
    {
        status = "OK]";
    }

    parsedData << timeStamp << status << commMode << rawData << crc << slaveAddr << function << info;
//    qDebug() << parsedData;
    return exception_code;
}

void ModbusWnd::onCmbComportTextChanged(QString str)
{

}

quint32 ModbusWnd::onCmbBaudrateTextChanged(QString str)
{
    QMetaObject::invokeMethod(m_port, "changeBaudrate", Qt::BlockingQueuedConnection,
                              Q_ARG(quint32, str.toInt()  ));

    return str.toInt();
}

quint32 ModbusWnd::onCmbDataBitsTextChanged(QString str)
{
    QMetaObject::invokeMethod(m_port, "changeDataBits", Qt::BlockingQueuedConnection,
                              Q_ARG(quint32, str.toInt()  ));

    return str.toInt();
}
quint32 ModbusWnd::onCmbParityTextChanged(QString str)
{
    QSerialPort::Parity parity = QSerialPort::UnknownParity;

    if( str == "none" )
    {
        parity = QSerialPort::NoParity;
    }
    else if ( str == "even" )
    {
        parity = QSerialPort::EvenParity;
    }
    else if ( str == "odd" )
    {
        parity = QSerialPort::OddParity;
    }

    QMetaObject::invokeMethod(m_port, "changeParity", Qt::BlockingQueuedConnection,
                              Q_ARG(quint32, parity  ));

    return parity;
}
quint32 ModbusWnd::onCmbStopBitsTextChanged(QString str)
{
    QSerialPort::StopBits stopBits = QSerialPort::UnknownStopBits;
    if( str.toInt() == 1 )
        stopBits = QSerialPort::OneStop;
    else if ( str.toInt() == 2)
        stopBits = QSerialPort::TwoStop;

    QMetaObject::invokeMethod(m_port, "changeStopBits", Qt::BlockingQueuedConnection,
                              Q_ARG(quint32, stopBits  ));

    return stopBits;
}


void ModbusWnd::onLineDecimalChanged()
{
    QString numText = ui->lineDecimalChange->text();
    numText = numText.replace(" ", "");
    int number = numText.toInt();
    ui->lineHexChange->setText(QByteArray::number(number, 16).toUpper() );
}

void ModbusWnd::onLineHexChanged()
{
    int number = ui->lineHexChange->text().toInt(0, 16);
    ui->lineDecimalChange->setText(QByteArray::number(number, 10).toUpper() );

}

void ModbusWnd::onCmbCommModeCurrentTextChanged(QString str)
{
    QStringList functions;

    ui->cmbFunction->clear();

    if( ui->cmbCommMode->currentText().contains("RTU"))
    {
        functions << "03 (Read Holding Register)" <<
                     "04 (Read Input Register)" <<
                     "06 (Write Single Register)" <<
                     "10 (Write Multiple Register)" ;

        ui->lblWriteData->setText("Write Data(HEX)");
        ui->chkAddrMinusOne->setVisible(true);

    }
    else
    {
        functions << "52 'R' Read" <<
                     "57 'W' Write" <<
                     "58 'X' Monitor request" <<
                     "59 'Y' Monitor excute" ;

        ui->lblWriteData->setText("Write Data(ASCII)");
        ui->chkAddrMinusOne->setVisible(false);
    }
    m_port->setMode(str);
    ui->cmbFunction->addItems(functions);
}

void ModbusWnd::onCmbFunctionCurrentTextChanged(QString str)
{
    if( str == "06 (Write Single Register)" )
    {
        ui->cmbSendCount->setEnabled(false);
        ui->lineWriteData->setMaxLength(4);
    }
    else
    {
        ui->cmbSendCount->setEnabled(true);
        ui->lineWriteData->setMaxLength(512);
    }

    if( str.contains("Read") )
    {
        ui->lineWriteData->setEnabled(false);
    }
    else
    {
        ui->lineWriteData->setEnabled(true);
    }
}

void ModbusWnd::onCmbSendCountTextChanged(QString str)
{

}

void ModbusWnd::onChkAutoSendToggled(bool checked)
{
    int interval = ui->lineInterval->text().toInt();

    if( checked == true )
    {
        ui->lineInterval->setEnabled(false);

        if( interval == 0 )
        {
            interval = 1;
        }

        m_sendedCount= 0;
        m_recvedCount = 0;
        m_1msCount = 0;
        m_previousTimeStamp = QDateTime::currentMSecsSinceEpoch();


        connect(m_port, SIGNAL(sgReadyEntered()), m_timerAutoSend, SLOT(start()));
        m_timerAutoSend->setInterval( interval );
        m_timerAutoSend->start();
        m_timer100msec->start();

    }
    else
    {
        disconnect(m_port, SIGNAL(sgReadyEntered()), 0, 0);
        m_timerAutoSend->stop();
        m_timer100msec->stop();
        ui->lineInterval->setEnabled(true);
    }
}

void ModbusWnd::onChkIgnoreAckToggled(bool checked)
{
    if( checked == true )
    {
        m_port->setIgnoreAck(true);
    }
    else
    {
       m_port->setIgnoreAck(false);
    }
}

void ModbusWnd::onLineModbusTimeoutChanged(QString str)
{
    if( ui->lineModbusTimeout->text().toInt() < 10 )
    {
        return;
    }
}

void ModbusWnd::onLineModbusEditingFinished()
{
    QString str = ui->lineModbusTimeout->text();

    if( ui->lineModbusTimeout->text().toInt() < 1 )
    {
        QMessageBox::warning(this, "Error", "modbus timeout should not be lower than 1 msec");
        ui->lineModbusTimeout->setText("1");
        return;
    }
    m_port->setResponseTimeout(str.toInt() );
}

void ModbusWnd::onPortConnected()
{
    ui->lineModbusTimeout->setEnabled(false);
    ui->btnOpen->setEnabled(false);
    ui->btnClose->setEnabled(true);
    ui->cmbComPort->setEnabled(false);
    ui->btnRefresh->setEnabled(false);
    ui->chkAutoSend->setChecked(false);

    emit sgConnected("Connected");
}
void ModbusWnd::onPortDisconnected()
{
    ui->lineModbusTimeout->setEnabled(true);
    ui->btnOpen->setEnabled(true);
    ui->btnClose->setEnabled(false);
    ui->btnRefresh->setEnabled(true);
    ui->cmbComPort->setEnabled(true);
    m_timerAutoSend->stop();
    m_timer100msec->stop();

    emit sgDisconnected( m_port->errorString());
}
void ModbusWnd::onPortResponseTimeout()
{

#if 0
    namespace MODBUS_WND_TXRX_RESULT_COLUMNS
    {
        enum {
            TIMESTAMP           ,
            STATUS              ,
            MODE                ,
            RAWDATA             ,
            CRC                 ,
            SLAVE_ADDR          ,
            FUNCTION            ,
            INFO				,
            COUNT
        };
    }
#endif

    QString timeStamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    QString status = "[RX NOK]";
    QString rawData = "";
    QString slaveAddr = "";
    QString function = "";
    QString info = QString("[ERROR TIMEOUT INTERVAL] %1 msec").arg(QString("%1").arg(m_port->responseTimeout()));
    QString crc = "";
    QStringList rowItems;
    QString commMode = ui->cmbCommMode->currentText();
    rowItems << timeStamp << status << commMode << rawData << crc << slaveAddr << function << info ;
    addDataToTxRxResultModel(rowItems);
    addErrorCount();
}


void ModbusWnd::onBtnOpenClicked()
{
    QMetaObject::invokeMethod(m_port, "tryConnect", Qt::QueuedConnection ,
                              Q_ARG(QString, ui->cmbComPort->currentText()),
                              Q_ARG(quint32, onCmbBaudrateTextChanged(ui->cmbBaudrate->currentText()) ),
                              Q_ARG(quint32, onCmbDataBitsTextChanged(ui->cmbDataBits->currentText()) ),
                              Q_ARG(quint32, onCmbParityTextChanged(ui->cmbParity->currentText()) ),
                              Q_ARG(quint32, onCmbStopBitsTextChanged(ui->cmbStopBits->currentText()) )
                              );
}
void ModbusWnd::onBtnCloseClicked()
{
    QMetaObject::invokeMethod(m_port, "tryDisconnect", Qt::QueuedConnection);
}

void ModbusWnd::onBtnManualSendClicked()
{
    QModelIndex currentIndex  = ui->viewTxQueueData->selectionModel()->currentIndex();
    if( currentIndex.row() != -1)
        sendModelTxPktQueue(currentIndex.row());
}

void ModbusWnd::onBtnScreenClearClicked()
{
    resetRxCount();
    resetTxCount();
    resetErrorCount();
    removeModelTxRxResult();
}
void ModbusWnd::removeModelTxRxResult()
{
    int removeCount = m_modelTxRxResult->rowCount();
    for ( int i = 0; i < removeCount; i++ )
    {
        m_modelTxRxResult->removeRow(0);
    }
}

void ModbusWnd::onBtnRefreshClicked()
{
    QList<QSerialPortInfo> portInfos = QSerialPortInfo::availablePorts();
    ui->cmbComPort->clear();
    foreach( QSerialPortInfo portInfo, portInfos)
    {
        QString portName = portInfo.portName();
        if( portInfo.isBusy() )
            ui->cmbComPort->addItem(portName + "(in use)");
        else
            ui->cmbComPort->addItem(portName);
    }
}



void ModbusWnd::onRxCountChanged()
{
    ui->lblRxCount->setText( QString("%1").arg(m_rxCount) );
}

void ModbusWnd::onTxCountChanged()
{
    ui->lblTxCount->setText( QString("%1").arg(m_txCount) );
}
void ModbusWnd::onErrorCountChanged()
{
    ui->lblErrorCount->setText( QString("%1").arg(m_errorCount) );
}




void ModbusWnd::onT15IntervalChanged()
{
    ui->lblDefault15Time->setText(QString("%1").arg(m_port->t15IntervalUsec()));
}

void ModbusWnd::onT35IntervalChanged()
{
    ui->lblDefault35Time->setText(QString("%1").arg(m_port->t35IntervalUsec()));
}


void ModbusWnd::onBtnResultFileSaveClicked()
{
    QString saveFilePath = QDir::currentPath();
    QString filePath = QFileDialog::getSaveFileName(
                this, tr("FILE OPEN"),
                saveFilePath,
                tr("csv file (*.csv)"));

    if( filePath.isEmpty() == true)
    {
        return;
    }

    QFile file;
    file.setFileName(filePath);

    QIODevice::OpenModeFlag openMode;
    openMode = QIODevice::WriteOnly;

    if( file.open(openMode) == true )
    {
        file.seek(file.size() );
        QString rowData = "";

        for(int i = 0 ; i < ui->viewTxRxResult->horizontalHeader()->count() ; i ++ )
        {
            if( i  != ui->viewTxRxResult->horizontalHeader()->count() - 1)
                rowData += m_modelTxRxResult->horizontalHeaderItem(i)->text() + ",";
            else
                rowData += m_modelTxRxResult->horizontalHeaderItem(i)->text() + "\n";

        }

        file.write(rowData.toLatin1());
        rowData = "";

        for(int i = 0 ; i < m_modelTxRxResult->rowCount(); i ++ )
        {
            rowData = "";
            rowData += " " + m_modelTxRxResult->item(i, MODBUS_WND_TXRX_RESULT_COLUMNS::TIMESTAMP)->text() + ",";
            rowData += m_modelTxRxResult->item(i, MODBUS_WND_TXRX_RESULT_COLUMNS::STATUS)->text() + ",";
            rowData += m_modelTxRxResult->item(i, MODBUS_WND_TXRX_RESULT_COLUMNS::MODE)->text() + ",";
            rowData += m_modelTxRxResult->item(i, MODBUS_WND_TXRX_RESULT_COLUMNS::RAWDATA)->text() + ",";
            rowData += m_modelTxRxResult->item(i, MODBUS_WND_TXRX_RESULT_COLUMNS::CRC)->text() + ",";
            rowData += m_modelTxRxResult->item(i, MODBUS_WND_TXRX_RESULT_COLUMNS::SLAVE_ADDR)->text() + ",";
            rowData += m_modelTxRxResult->item(i, MODBUS_WND_TXRX_RESULT_COLUMNS::FUNCTION)->text() + ",";
            rowData += m_modelTxRxResult->item(i, MODBUS_WND_TXRX_RESULT_COLUMNS::INFO)->text() + "\n";

            file.write(rowData.toLatin1());
        }
        file.flush();
    }
    else
    {
        qWarning() <<  Q_FUNC_INFO << "file save fail" << file.errorString();

    }
    file.close();
}



void ModbusWnd::btnTxQueueAddClicked()
{
    QByteArray slaveAddr = ui->lineSlaveAddr->text().simplified().mid(0, 2).toLatin1();
    QByteArray functionCode = ui->cmbFunction->currentText().simplified().mid(0, 2).toLatin1();
    QByteArray startAddr = ui->lineAddr->text().simplified().mid(0, 4).toLatin1();
    QByteArray writeData = ui->lineWriteData->text().simplified().toLatin1();
    QByteArray numOfRegister =QByteArray("0000" + ui->cmbSendCount->currentText().toLatin1()).right(4);
    QByteArray byteCount = "";
    QString commMode = ui->cmbCommMode->currentText();
    byteCount.setNum(ui->cmbSendCount->currentText().toInt(0, 16) * 2, 16);
    byteCount = "00" + byteCount;
    byteCount = byteCount.right(2);


    QByteArray sendData = "";
    if( commMode.contains("RTU"))
    {
        int tempStartAddr =    startAddr.toInt(NULL, 16);
        if( ui->chkAddrMinusOne->isChecked() == true )
            tempStartAddr--;

        startAddr = QByteArray("0000" + QByteArray::number(tempStartAddr, 16)).right(4);
//        qDebug() << Q_FUNC_INFO << startAddr;
        sendData = makeRTUFrame(slaveAddr, functionCode, startAddr, numOfRegister, byteCount, writeData);

    }
    else
        sendData = makeLSBUSFrame(slaveAddr, functionCode, startAddr, numOfRegister, byteCount, writeData);

//    qDebug() << Q_FUNC_INFO << "commMode" << commMode << "slave" << slaveAddr.toHex() << "function" << functionCode.toHex() <<
//                "startAddr" << startAddr.toHex() << "number of register" << numOfRegister.toHex() << "byte Count" << byteCount.toHex() << "sendData" << sendData.toHex();

    QStringList parsedData;
    onParseData(parsedData, sendData);
    addDataToTxPktQueueModel(parsedData);
}

void ModbusWnd::btnTxQueueRemoveClicked()
{
    QModelIndex index  = ui->viewTxQueueData->selectionModel()->currentIndex();
    m_modelTxPktQueue->removeRow(index.row());
}

void ModbusWnd::btnTxQueueUpClicked()
{
    QModelIndex currentIndex  = ui->viewTxQueueData->currentIndex();
    if( currentIndex.row() == -1 )
        return;
    if( currentIndex.row() != 0 )
    {
        QList<QStandardItem *> currentRow;
        currentRow = m_modelTxPktQueue->takeRow(currentIndex.row());
        m_modelTxPktQueue->insertRow(currentIndex.row() -1 , currentRow);
        ui->viewTxQueueData->setCurrentIndex(m_modelTxPktQueue->index(currentIndex.row() -1, 0));
    }
}

void ModbusWnd::btnTxQueueDownClicked()
{
    QModelIndex currentIndex  = ui->viewTxQueueData->currentIndex();
    if( currentIndex.row() == -1 )
        return;

    if( currentIndex.row() != m_modelTxPktQueue->rowCount() -1 )
    {
        QList<QStandardItem *> currentRow;
        currentRow = m_modelTxPktQueue->takeRow(currentIndex.row());
        m_modelTxPktQueue->insertRow(currentIndex.row() +1 ,currentRow );
        ui->viewTxQueueData->setCurrentIndex(m_modelTxPktQueue->index(currentIndex.row() +1, 0));
    }
    qDebug() << Q_FUNC_INFO << currentIndex.row();
}

void ModbusWnd::btnTxQueueFileOpenClicked()
{
    qDebug() << Q_FUNC_INFO;
}

void ModbusWnd::btnTxqueueFileSaveClicked()
{
    qDebug() << Q_FUNC_INFO;

}



void ModbusWnd::btnCheckSumClicked()
{
    QByteArray inputData;
    if( ui->radioHex->isChecked() )
    {
        QString inputText;
        inputText = ui->lineDataInput->text();
        inputText = inputText.replace(" ", "");
        inputData = QByteArray::fromHex(inputText.toLatin1());

    }
    else
    {
        QString inputText;
        inputText = ui->lineDataInput->text();
        inputText = inputText.replace(" ", "");
        inputData = inputText.toLatin1();
    }

//    qDebug() << inputData.toHex() << inputData;


    quint8 sum = 0x00;
    for ( int i = 0 ; i < inputData.count() ; i ++ )
    {
        sum += inputData.at(i);
    }

    QByteArray checkSum = "";
    checkSum = QByteArray::fromRawData((char*)&sum, 1);
    ui->lineDataResultHex->setText(checkSum.toHex().toUpper().toHex());
    ui->lineDataResultASCII->setText(checkSum.toHex().toUpper());

}

void ModbusWnd::btnCRC16Clicked()
{
    QByteArray inputData;
    QString inputText;
    inputText = ui->lineDataInput->text();
    inputText = inputText.replace(" ", "");
    
    if( ui->radioHex->isChecked() )
    {
        inputData = QByteArray::fromHex(inputText.toLatin1());
    }
    else
    {
        inputData = inputText.toLatin1();
    }

//    qDebug() << inputData.toHex() << inputData;


    QByteArray dstCrc = "";
    quint16 crc = ModbusWnd::ModbusRTU_CRC16(inputData.constData(), inputData.size());
    dstCrc = QByteArray::fromRawData((char*)&crc, 2);
    ui->lineDataResultHex->setText(dstCrc.toHex().toUpper().toHex());
    ui->lineDataResultASCII->setText(dstCrc.toHex().toUpper());

}

void ModbusWnd::btnCRC32Clicked()
{
    QByteArray inputData;
    QString inputText;
    inputText = ui->lineDataInput->text();
    inputText = inputText.replace(" ", "");
    
    if( ui->radioHex->isChecked() )
    {
        inputData = QByteArray::fromHex(inputText.toLatin1());
    }
    else
    {
        inputData = inputText.toLatin1();
    }

    qDebug() << inputData.toHex() <<  inputData.size();


    QByteArray dstCrc = "";
    uint32_t crc = CRC::Calculate(inputData.constData(), inputData.size(), CRC::CRC_32_MPEG2());
    dstCrc = QByteArray::fromRawData((char*)&crc, 4);
    ui->lineDataResultHex->setText(dstCrc.toHex().toUpper().toHex());
    ui->lineDataResultASCII->setText(dstCrc.toHex().toUpper());

}


void ModbusWnd::onLineAscToHexAscInputClicked()
{
    QByteArray text;
    QByteArray targetText = "";
    text = ui->lineAscToHexAscInput->text().toLatin1().trimmed();
    text = text.replace(" " , "");

    for (int i =0; i < text.count() ; i ++ )
    {
        QByteArray temp;
        targetText += temp.setNum( (int) text.at(i), 16) + " ";
    }
    ui->lineAscToHexHexInput->setText(QString(targetText.toUpper() ));
}

void ModbusWnd::onLineAscToHexHexInputClicked()
{
    QByteArray text;
    QByteArray targetText = "";
    text = ui->lineAscToHexHexInput->text().toLatin1().trimmed();
    text = text.replace(" " , "");

    for (int i =0; i < text.count() ; i = i+2 )
    {
        targetText +=QByteArray::fromHex(text.mid(i, 2));
    }
    ui->lineAscToHexAscInput->setText( targetText );

}

void ModbusWnd::onModelTxPktQueueDataChanged(QModelIndex topLeft, QModelIndex bottomRight )
{
    int row = topLeft.row();
    int col = topLeft.column();

    if( col == MODBUS_WND_TX_QUEUE_COLUMNS::HEX )
    {
        QString str = m_modelTxPktQueue->item(row, col)->text();
        QString asciiStr = "";
        str = str.trimmed();
        str = str.replace(" ", "");
        for(int i = 0; i < str.count()/2 ; i++ )
        {
            asciiStr += QByteArray::fromHex(str.mid(i*2, 2).toLatin1());
        }
        m_modelTxPktQueue->setItem(row, col+1, new QStandardItem(asciiStr));
    }
}

void ModbusWnd::onTimerAutoSendTimeout()
{
    onReadyEntered();
}

void ModbusWnd::onTimer100msTimeout()
{
    // total bytes in 100 ms  * 10 for 1sec
    quint64 currentTimeStamp = QDateTime::currentMSecsSinceEpoch();

    do {

        m_1msCount += currentTimeStamp - m_previousTimeStamp;

        ui->lblBps->setText( QString("%1").arg( (int)( (((double) (m_recvedCount + m_sendedCount)) / (double) m_1msCount) * 1000 )));

        m_previousTimeStamp = currentTimeStamp;

    }while(false);

}


void ModbusWnd::onModelTxResultRowInserted(const QModelIndex & parent, int first, int last)
{
//    qDebug() << Q_FUNC_INFO << first << last << m_modelTxRxResult->rowCount();

    if( m_txRxResultMaxRow *2  < m_modelTxRxResult->rowCount() )
    {

        QString saveFilePath = QDir::currentPath();
        QString filePath = saveFilePath + + "/" + QDateTime::currentDateTime().toString("MM-dd_HH-mm") + ".csv";

//        qDebug() << Q_FUNC_INFO << filePath;
        QFile file;
        file.setFileName(filePath);

        QIODevice::OpenModeFlag openMode;
        openMode = QIODevice::WriteOnly;

        if( file.open(openMode) == true )
        {
            file.seek(file.size() );
            QString rowData = "";

            for(int i = 0 ; i < ui->viewTxRxResult->horizontalHeader()->count() ; i ++ )
            {
                if( i  != ui->viewTxRxResult->horizontalHeader()->count() - 1)
                    rowData += m_modelTxRxResult->horizontalHeaderItem(i)->text() + ",";
                else
                    rowData += m_modelTxRxResult->horizontalHeaderItem(i)->text() + "\n";

            }

            file.write(rowData.toLatin1());
            rowData = "";

            for(int i = 0 ; i < m_txRxResultMaxRow; i ++ )
            {

                QList <QStandardItem*> items;
                items = m_modelTxRxResult->takeRow(0);
                rowData = "";
                rowData += " " + items.at(MODBUS_WND_TXRX_RESULT_COLUMNS::TIMESTAMP)->text() + ",";
                rowData += items.at(MODBUS_WND_TXRX_RESULT_COLUMNS::STATUS)->text() + ",";
                rowData += items.at(MODBUS_WND_TXRX_RESULT_COLUMNS::MODE)->text() + ",";
                rowData += items.at(MODBUS_WND_TXRX_RESULT_COLUMNS::RAWDATA)->text() + ",";
                rowData += items.at(MODBUS_WND_TXRX_RESULT_COLUMNS::CRC)->text() + ",";
                rowData += items.at(MODBUS_WND_TXRX_RESULT_COLUMNS::SLAVE_ADDR)->text() + ",";
                rowData += items.at(MODBUS_WND_TXRX_RESULT_COLUMNS::FUNCTION)->text() + ",";
                rowData += items.at(MODBUS_WND_TXRX_RESULT_COLUMNS::INFO)->text() + "\n";

                file.write(rowData.toLatin1());

                foreach(QStandardItem* item, items)
                {
                    if( item != 0 )
                        delete item;
                }
            }
            file.flush();
        }
        else
        {
            qWarning() <<  Q_FUNC_INFO << "file save fail" << file.errorString();

        }
        file.close();

    }

}

void ModbusWnd::onViewTxQueueDataSelectionCurrentRowChanged(const QModelIndex &current, const QModelIndex &previous)
{
}


void ModbusWnd::sendModelTxPktQueue(int index )
{
    if( index >= m_modelTxPktQueue->rowCount() )
    {
        qDebug() << Q_FUNC_INFO << "index overflow";
        return;
    }

    QByteArray sendData;
    sendData = QByteArray::fromHex( m_modelTxPktQueue->item(index, MODBUS_WND_TX_QUEUE_COLUMNS::HEX )->text().toLatin1() );

//    qDebug() << Q_FUNC_INFO << sendData.toHex();

    QMetaObject::invokeMethod(m_port, "sendData", Qt::BlockingQueuedConnection,
                              Q_ARG(QByteArray, sendData ));
}

void ModbusWnd::onReadyEntered()
{
//    qDebug() << Q_FUNC_INFO;
    static int index = 0;

    if( m_modelTxPktQueue->rowCount() == 0)
    {
        return;
    }

    for(int count = 0; count < m_modelTxPktQueue->rowCount(); count ++)
    {
       int position = (index + count)  % m_modelTxPktQueue->rowCount();

        if( m_modelTxPktQueue->item(position, MODBUS_WND_TX_QUEUE_COLUMNS::ENABLE )->checkState() == Qt::Checked )
        {
            m_timerAutoSend->stop();
            sendModelTxPktQueue(position);
            index = (position+1) % m_modelTxPktQueue->rowCount();
            break;
        }
        else
        {
            m_timerAutoSend->start();
        }
    }
}

QByteArray ModbusWnd::toHexString(QByteArray buf)
{
    QByteArray temp = "";
    for(int i =0; i < buf.count() ; i ++ )
    {
        if( i == buf.count() -1 )
            temp += buf.mid(i, 1).toHex();
        else
            temp += buf.mid(i, 1).toHex() + " ";
    }
    return temp;
}

bool ModbusWnd::check_ModbusRTU_CRC16(QByteArray buf, quint16 &result)
{
    if( buf.size() >= 4 )
    {
        QByteArray srcCrc = buf.mid(buf.size()-2, 2);
        QByteArray dstCrc = "";
        quint16 crc = ModbusWnd::ModbusRTU_CRC16(buf.constData(), buf.size() -2 );
        dstCrc = QByteArray::fromRawData((char*)&crc, 2);
        result = crc;

//        qDebug() << Q_FUNC_INFO << "src crc " << srcCrc.toHex() << "dst crc" << dstCrc.toHex();

        if( srcCrc == dstCrc )
        {
            return true;
        }
        else
        {
            return false;
        }
    }
    result = 0xffff;
    return false;
}

quint16 ModbusWnd::ModbusRTU_CRC16 (const char *buf, quint16 wLength)
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


quint8 ModbusWnd::LSBUS_sum (QByteArray buf)
{
    quint8 sum = 0x00;
    for ( int i = 1 ; i < buf.count() ; i ++ )
    {
//        qDebug() << QByteArray(1, buf.at(i)).toHex();
        sum += buf.at(i);
    }
//     qDebug() << "sum" << QByteArray(1, sum).toHex();
    return sum;
}

QByteArray ModbusWnd::makeRTUFrame(QByteArray slaveAddr, QByteArray functionCode, QByteArray startAddr,
                                   QByteArray numOfRegister, QByteArray byteCount, const QByteArray writeData)
{
    Q_ASSERT(writeData.size() <= 252 * 2); // because of hex string
    QByteArray modbusPDU = "";



    switch( QByteArray::fromHex(functionCode).at(0) )
    {
    case 0x03:
        modbusPDU += functionCode + startAddr + numOfRegister;
        break;
    case 0x04:
        modbusPDU += functionCode + startAddr + numOfRegister;
        break;
    case 0x06:
        modbusPDU += functionCode + startAddr + writeData;
        break;
    case 0x10:
        modbusPDU += functionCode + startAddr + numOfRegister + byteCount + writeData;
        break;
    default:
        modbusPDU += functionCode + startAddr + writeData;
        break;
    }

    QByteArray frame ="";
    QDataStream ds(&frame, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::LittleEndian);


    ds << quint8(QByteArray::fromHex(slaveAddr).at(0));
    // QByteArray 의 경우 0x00 데이터가 있는 경우,  size() 로  check 안되므로 toHex 처리
    ds.writeRawData(QByteArray::fromHex(modbusPDU).constData(), modbusPDU.size() /2 );
    quint16 crc = ModbusRTU_CRC16(frame.constData(), frame.size());

    ds << quint16(crc);

//    qDebug() << slaveAddr <<  functionCode << startAddr << numOfRegister << writeData;
//    qDebug() << Q_FUNC_INFO << modbusPDU << frame.toHex();

    return frame;
}

QByteArray ModbusWnd::makeLSBUSFrame(QByteArray slaveAddr, QByteArray functionCode, QByteArray startAddr,
                                   QByteArray numOfRegister, QByteArray byteCount, const QByteArray writeData)
{
    Q_ASSERT(writeData.size() <= 252);
    QByteArray modbusPDU = "";

    switch( QByteArray::fromHex(functionCode).at(0) )
    {
    case 0x52:
        modbusPDU += slaveAddr + QByteArray::fromHex(functionCode).at(0) + startAddr + numOfRegister.right(1);
        break;
    case 0x57:
        modbusPDU += slaveAddr + QByteArray::fromHex(functionCode).at(0) + startAddr + numOfRegister.right(1) + writeData;
        break;
    case 0x58:
        break;
    case 0x59:
        break;
    default:
        break;
    }

    QByteArray frame ="";
    QDataStream ds(&frame, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::LittleEndian);

    ds << quint8(0x05);
    // QByteArray 의 경우 0x00 데이터가 있는 경우,  size() 로  check 안되므로 toHex 처리
    ds.writeRawData(modbusPDU.constData(), modbusPDU.size());

    quint8 sum = LSBUS_sum(frame);
    ds.writeRawData( QByteArray::number(sum, 16).toUpper().constData(), 2 );

//    qDebug() << slaveAddr <<  QByteArray::fromHex(functionCode).at(0) << startAddr << numOfRegister.right(2) << writeData << QByteArray::number(sum, 16);

    ds << quint8(0x04);
    return frame;
}






