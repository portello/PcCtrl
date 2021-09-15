#ifndef HYDRACONN_H
#define HYDRACONN_H

#include <QObject>
#include <QTime>

#include "commHandler.h"

enum class HydraStatus
{
    OK,
    RUN,
    CMD,
    ERR,
    TRANSFER,
};

class HydraConn : public QObject
{
    Q_OBJECT
public:
    explicit HydraConn(QIODevice* ioDev, QObject *parent = nullptr);
    explicit HydraConn(QIODevice* ioDev, bool isSerial, QObject *parent = nullptr);
    ~HydraConn();

    void closeAndDelete();
    QString execCommand(const QString &cmd, int timeout = 0);
    QString decodeError(int code) const;
    QString decodeErrorClass(int code) const;
    QString parseResp(const QString &resp, quint32 *errCode) const;
    bool isIdle() const;
    bool isOpen() const;
    bool postCmd(QString cmd);
    HydraStatus getStatus() const {return f_status;}

signals:
    void S_lineReceived(const QString &line);
    void S_logLine(const QString &line, int elapsed);
    void S_statusChanged(HydraStatus old, HydraStatus New);
    void S_transferProgress(int prog);

public slots:
    void sendCmd(const QString &line);

private slots:
    void lineReceived(const QString &line);


private:
    HydraStatus     f_status;
    ComHandler     *f_commHandler;
    int             f_lastErr;
    QTime           f_CmdStartTime;
    QString         f_execCmdText;
    int             f_execCmdMode;
    bool            f_isSerial{false};
    bool            f_closing{false};

    void setStatus(HydraStatus newStatus);
    void start(void);
};

#endif // HYDRACONN_H
