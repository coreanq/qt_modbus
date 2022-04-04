#ifndef MODBUSWND_H
#define MODBUSWND_H

#include <QWidget>
#include <QSerialPortInfo>
#include <serialport.h>
#include <QDateTime>
#include <QSettings>
#include <QStandardItemModel>
#include <QFile>
#include <QDir>
#include <QMessageBox>
#include <QFileDialog>
#include <QTimer>
#include <QRegularExpression>


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
        INFO                ,
        COUNT
    };
}
namespace MODBUS_WND_TX_QUEUE_COLUMNS
{
    enum {
        ENABLE           ,
        HEX              ,
        ASCII           ,
        COMMENT          ,
        COUNT
    };
}



namespace Ui {
class Form;
}

class ModbusWnd : public QWidget
{
    Q_OBJECT

public:
    explicit ModbusWnd(QWidget *parent = 0);
    ~ModbusWnd();
    static bool check_ModbusRTU_CRC16(QByteArray buf, quint16 &result);
    static quint16 ModbusRTU_CRC16 (const char *buf, quint16 wLength);
    static quint8 LSBUS_sum(QByteArray buf);
    static QByteArray toHexString(QByteArray buf);

    void createConnection();

    void resetRxCount() { m_rxCount = 0; emit sgRxCountChanged(); }
    void resetTxCount() { m_txCount = 0; emit sgTxCountChanged(); }
    void resetErrorCount() { m_errorCount = 0; emit sgErrorCountChanged(); }
    void addRxCount() { m_rxCount++; emit sgRxCountChanged(); }
    void addTxCount() { m_txCount++; emit sgTxCountChanged(); }
    void addErrorCount() { m_errorCount++; emit sgErrorCountChanged(); }


    int onParseData(QStringList &parsedData, QByteArray data);

    void readSettings();

    void closeEvent(QCloseEvent * event);
    void addDataToTxRxResultModel(QStringList info);
    void addDataToTxPktQueueModel(QStringList parsedData);

    QByteArray makeRTUFrame(QByteArray slaveAddr, QByteArray functionCode, QByteArray startAddr,
                                       QByteArray numOfRegister, QByteArray byteCount, const QByteArray writeData);
    QByteArray makeLSBUSFrame(QByteArray slaveAddr, QByteArray functionCode, QByteArray startAddr,
                                       QByteArray numOfRegister, QByteArray byteCount, const QByteArray writeData);

public slots:

    void onPortConnected();
    void onPortDisconnected();
    void onPortResponseTimeout();

    void onBtnOpenClicked();
    void onBtnCloseClicked();
    void onRecvedData(QByteArray data);
    void onSendedData(QByteArray data);
    void onRecvedErrorData(QByteArray data);

    void onCmbComportTextChanged(QString str);
    quint32 onCmbBaudrateTextChanged(QString str);
    quint32 onCmbDataBitsTextChanged(QString str);
    quint32 onCmbParityTextChanged(QString str);
    quint32 onCmbStopBitsTextChanged(QString str);

    void onLineDecimalChanged();
    void onLineHexChanged();

    void onCmbCommModeCurrentTextChanged(QString str);
    void onCmbFunctionCurrentTextChanged(QString str);
    void onCmbSendCountTextChanged(QString str);
    void onChkAutoSendToggled(bool checked);
    void onChkIgnoreAckToggled(bool checked);
    void onLineModbusTimeoutChanged(QString str);
    void onLineModbusEditingFinished();

    void onBtnManualSendClicked();
    void onBtnScreenClearClicked();
    void removeModelTxRxResult();
    void onBtnRefreshClicked();

    void onRxCountChanged();
    void onTxCountChanged();
    void onErrorCountChanged();
    void onT15IntervalChanged();
    void onT35IntervalChanged();

    void onBtnResultFileSaveClicked();

    void btnTxQueueAddClicked()       ;
    void btnTxQueueRemoveClicked()    ;
    void btnTxQueueUpClicked()        ;
    void btnTxQueueDownClicked()      ;
    void btnTxQueueFileOpenClicked()  ;
    void btnTxqueueFileSaveClicked()  ;

    void btnCheckSumClicked();
    void btnCRC16Clicked();
    void btnCRC32Clicked(); 

    void onLineAscToHexAscInputClicked();
    void onLineAscToHexHexInputClicked();
    void onModelTxPktQueueDataChanged(QModelIndex topLeft, QModelIndex bottomRight);





    void onTimerAutoSendTimeout();
    void onTimer100msTimeout();
    void onModelTxResultRowInserted(const QModelIndex &parent, int first, int last);
    void onViewTxQueueDataSelectionCurrentRowChanged(const QModelIndex &current, const QModelIndex &previous);
    void sendModelTxPktQueue(int index );
    void onReadyEntered();

signals:
    void sgRxCountChanged();
    void sgTxCountChanged();
    void sgErrorCountChanged();
    void sgConnected(QString str);
    void sgDisconnected(QString str);

private:
    SerialPort*             m_port;
    QStandardItemModel*     m_modelTxPktQueue;
    QStandardItemModel*     m_modelTxRxResult;
    quint64                 m_rxCount;
    quint64                 m_txCount;
    quint64                 m_errorCount;
    quint64                 m_txRxResultMaxRow;
    QThread*                m_threadForSerial;

    QTimer*                 m_timerAutoSend;
    QTimer* 				m_timer100msec;
    QRegularExpression		m_reHex;

    quint32 				m_recvedCount;
    quint32					m_sendedCount;
    quint64					m_1msCount;
    quint64					m_previousTimeStamp;
    qint64					m_recvedTimeStamp;
    qint64					m_sendedTimeStamp;

    Ui::Form *ui;
};

#endif // MODBUSWND_H
