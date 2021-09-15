#include "hydraConn.h"
#include <QCoreApplication>
#include <QSemaphore>

HydraConn::HydraConn(QIODevice* ioDev, QObject *parent) : QObject(parent), f_status(HydraStatus::OK),
    f_commHandler(nullptr), f_execCmdMode(false), f_isSerial(false), f_closing(false)
{
    assert(ioDev != nullptr);
    f_commHandler = new ComHandler(ioDev, this);
    f_lastErr = 0;

    start();
}

HydraConn::HydraConn(QIODevice* ioDev, bool isSerial, QObject *parent) : QObject(parent), f_status(HydraStatus::OK),
    f_commHandler(nullptr), f_execCmdMode(false), f_isSerial(isSerial), f_closing(false)
{
    assert(ioDev != nullptr);
    f_commHandler = new ComHandler(ioDev, this);
    f_lastErr = 0;

    start();
}


HydraConn::~HydraConn()
{
    f_closing = true;
    f_execCmdMode = -1;

    if (f_commHandler != nullptr)
    {
       QMetaObject::invokeMethod(f_commHandler, "close", Qt::QueuedConnection);
       f_commHandler->threadExit();
       delete f_commHandler;
       f_commHandler = nullptr;
    }
}

void HydraConn::closeAndDelete()
{
    f_closing = true;
    f_execCmdMode = -1;
    disconnect(f_commHandler, &ComHandler::S_lineReceived, this, &HydraConn::lineReceived);
    disconnect(f_commHandler, &ComHandler::S_transferProgress, this, &HydraConn::S_transferProgress);

    deleteLater();
}

void HydraConn::start()
{
    f_commHandler->start();
    connect(f_commHandler, &ComHandler::S_lineReceived, this, &HydraConn::lineReceived, Qt::QueuedConnection);
    connect(f_commHandler, &ComHandler::S_transferProgress, this, &HydraConn::S_transferProgress, Qt::QueuedConnection);
}

bool HydraConn::isIdle() const
{
    return (f_status == HydraStatus::OK || f_status == HydraStatus::ERR);
}

bool HydraConn::isOpen() const
{
    return f_commHandler->isOpen();
}

bool HydraConn::postCmd(QString cmd)
{
    cmd = cmd.trimmed();
    if (isIdle())
    {
        HydraStatus newStatus;
        if (cmd.startsWith(QLatin1String("RUN")) || cmd.startsWith(QLatin1String("BATCH")))
            newStatus = HydraStatus::RUN;
        else
            newStatus = HydraStatus::CMD;
        setStatus(newStatus);
        sendCmd(cmd);
        f_CmdStartTime.start();
        return true;
    }
    else
        return false;
}

void HydraConn::sendCmd(const QString &line)
{
    QMetaObject::invokeMethod(f_commHandler, "sendCmd", Qt::QueuedConnection, Q_ARG(QString, line));
    emit S_logLine(line, 0);
}


void HydraConn::lineReceived(const QString &line)
{
    if(f_closing)
        return;
    HydraStatus newStatus = f_status;
    bool cmdEnded = false;
    if (line.endsWith('>'))
    {
        newStatus = HydraStatus::OK;
        cmdEnded = true;
    }
    else if (line.endsWith('$'))
    {
        newStatus = HydraStatus::ERR;
        cmdEnded = true;
    }
    if (newStatus == HydraStatus::ERR)
        f_lastErr = line.midRef(line.length() - 9, 8).toInt(nullptr, 16);
    else
        f_lastErr = 0;

    if (f_execCmdMode > 0)
    {
        f_execCmdText += line + '\n';
    }
    else
    {
        if (cmdEnded && (f_status == HydraStatus::CMD || f_status == HydraStatus::RUN))
            emit S_logLine(line, f_CmdStartTime.elapsed());
        else
            emit S_logLine(line, 0);
        emit S_lineReceived(line);
    }
    setStatus(newStatus);
}

void HydraConn::setStatus(HydraStatus newStatus)
{
    if (newStatus != f_status)
    {
        HydraStatus old = f_status;
        f_status = newStatus;
        emit S_statusChanged(old, newStatus);
        if (f_execCmdMode > 0 && (old == HydraStatus::RUN || old == HydraStatus::CMD) &&
                                 (newStatus == HydraStatus::OK || newStatus == HydraStatus::ERR))
        {
            f_execCmdMode = 0;
        }
    }
}

QString HydraConn::execCommand(const QString &cmd, int timeout)
{
    QTime   tm;
    f_execCmdText.clear();
    f_execCmdMode = 1;
    if (!postCmd(cmd))
    {
        f_execCmdMode = 0;
        return QString();
    }
    tm.start();
    while (f_execCmdMode > 0)
    {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
        if (f_execCmdMode == -1)
            return QString();
        QThread::msleep(100);
        if (f_closing)
            return QString();
        if (timeout > 0 && tm.elapsed() > timeout && f_execCmdMode == 1)
            f_execCmdMode = -1;
    }
    if (f_execCmdMode == 0)
        return f_execCmdText;
    else
        return QString();
}

QString HydraConn::parseResp(const QString &resp, quint32 *errCode) const
{
    QRegExp re("<\n(.+)>");
    if (re.indexIn(resp) != -1)
    {
        if (errCode != nullptr)
            *errCode = 0;
        return re.cap(1);
    }
    re.setPattern(QStringLiteral("<\n(.+)([0..9A..F]+)$"));
    if (re.indexIn(resp) != -1)
    {
        if (errCode != nullptr)
            *errCode = re.cap(2).toUInt(nullptr, 16);
        return re.cap(1);
    }
   if (errCode != nullptr)
       *errCode = 0;
   return resp;
}


