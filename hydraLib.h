#ifndef HYDRALIB_H
#define HYDRALIB_H

#include <QIODevice>
#include <QHostAddress>
#include <QTcpSocket>
#include <QNetworkProxy>
#include <QtSerialPort/QSerialPort>

#include "hydraConn.h"

QIODevice *makeLanConnection(const QHostAddress &address, quint16 port = 1444);
QIODevice *makeUartConnection(const QString &portName);

#endif // HYDRALIB_H
