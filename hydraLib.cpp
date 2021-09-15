#include "hydraLib.h"

QIODevice *makeLanConnection(const QHostAddress &address, quint16 port)
{
    QTcpSocket *sock = new QTcpSocket();
    QNetworkProxyFactory::setUseSystemConfiguration(false);
    sock->setSocketOption(QAbstractSocket::KeepAliveOption, 1);
    sock->connectToHost(address, port);

    if (!sock->waitForConnected(5000))
    {
        delete sock;
        return nullptr;
    }

    return sock;
}
