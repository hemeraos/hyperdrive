#ifndef _HYPERDISCOVERY_SERVICE_H_
#define _HYPERDISCOVERY_SERVICE_H_

#include <QtCore/QByteArray>
#include <QtNetwork/QHostAddress>

class Service
{
public:
    int timestamp;
    int ttl;
    int port;
    QByteArray interface;
    QHostAddress address;
};

#endif

