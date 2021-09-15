#ifndef COMHANDLER_H
#define COMHANDLER_H

#include <QObject>
#include <QIODevice>
#include <QThread>


enum class ComHandlerStatus
{
    STOPPED,
    IDLE,
    SEND,
    RECEIVE
};

class ComHandler : public QObject
{
    Q_OBJECT
public:
    ~ComHandler();
    explicit ComHandler(QIODevice* _ioDev = nullptr, QObject *parent = nullptr);

    //void    setIoDev(QIODevice *_ioDev);
    void    start(void);
    bool    isOpen(void);
    ComHandlerStatus getStatus(void) {return f_status;}
    //QThread *getThread();
    void    threadExit();
signals:
    void    S_lineReceived(QString line);
    void    S_transferProgress(int perc);
    void    S_transferEnded(bool err);

public slots:
    void    close(void);
    void    sendCmd(QString line);

private slots:
    void    readyRead(void);

private:
    QIODevice       *f_ioDev;
    ComHandlerStatus f_status;
    QString         f_lineBuff;
    QThread         f_thread;

    void            processLine(void);

    int getBuffer(quint8 *rx_buf, int buffSize, int tmout);
    int sendBuffer(quint8 *tx_buf, int tx_buf_len);
    int getPacket(int timeout, int packetSize, quint8 *buffer);
    int getFirstPacket(int timeout, int packetSize, quint8 *buffer);
    int getAck(quint32 *lastAckPacket, bool fast);

    int fileSend(QDataStream &stream, int len, const QString &fileName, bool fastMode);
    int fileReceive(QDataStream &stream);
};

#endif // COMHANDLER_H
