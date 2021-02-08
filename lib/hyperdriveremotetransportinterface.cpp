/*
 *
 */

#include "hyperdriveremotetransportinterface.h"
#include "hyperdriveprotocol.h"

#include "hyperdrivetransport_p.h"

#include <HyperspaceCore/Socket>

#include <QtCore/QDebug>
#include <QtCore/QLoggingCategory>

Q_LOGGING_CATEGORY(hyperdriveRemoteTransportInterfaceDC, "hyperdrive.remotetransportinterface", DEBUG_MESSAGES_DEFAULT_LEVEL)

namespace Hyperdrive
{

class RemoteTransportInterfacePrivate : public TransportPrivate
{
    Q_DECLARE_PUBLIC(RemoteTransportInterface)
public:
    RemoteTransportInterfacePrivate(RemoteTransportInterface *q) : TransportPrivate(q) {}

    Hyperspace::Socket *socket;
};

RemoteTransportInterface::RemoteTransportInterface(Hyperspace::Socket *socket, const QString &name, QObject *parent)
    : Transport(*new RemoteTransportInterfacePrivate(this), name, parent)
{
    Q_D(RemoteTransportInterface);
    d->socket = socket;
}

RemoteTransportInterface::~RemoteTransportInterface()
{
}

void RemoteTransportInterface::initImpl()
{
    // Nothing to do
    setReady();
}

void RemoteTransportInterface::rebound(const Hyperspace::Rebound &rebound, int fd)
{
    Q_D(RemoteTransportInterface);

    // Send the rebound over the socket
    QByteArray msg;
    QDataStream out(&msg, QIODevice::WriteOnly);

    out << Hyperdrive::Protocol::Control::rebound() << rebound.serialize() << Hyperdrive::Protocol::Control::messageTerminator();

    int written = d->socket->write(msg, fd);
    Q_UNUSED(written); // It might come useful to debug this at times.
}

void RemoteTransportInterface::fluctuation(const Hyperspace::Fluctuation &fluctuation)
{
    Q_D(RemoteTransportInterface);

    // Send the fluctuation over the socket
    QByteArray msg;
    QDataStream out(&msg, QIODevice::WriteOnly);

    out << Hyperdrive::Protocol::Control::fluctuation() << fluctuation.serialize();

    d->socket->write(msg);
}

void RemoteTransportInterface::cacheMessage(const CacheMessage &cacheMessage)
{
    Q_D(RemoteTransportInterface);

    // Send the cacheMessage over the socket
    QByteArray msg;
    QDataStream out(&msg, QIODevice::WriteOnly);

    out << Hyperdrive::Protocol::Control::cacheMessage() << cacheMessage.serialize();

    d->socket->write(msg);
}

void RemoteTransportInterface::routeWave(const Hyperspace::Wave &wave, int fd)
{
    Q_UNUSED(wave);
    Q_UNUSED(fd);
    qCWarning(hyperdriveRemoteTransportInterfaceDC) << "Warning: this function should never be invoked!!";
}

void RemoteTransportInterface::bigBang()
{
    Q_D(RemoteTransportInterface);

    // Send the cacheMessage over the socket
    QByteArray msg;
    QDataStream out(&msg, QIODevice::WriteOnly);

    out << Hyperdrive::Protocol::Control::bigBang();

    d->socket->write(msg);
}

}
