/*
 *
 */

#include "hyperdrivelocaltransport_p.h"
#include "hyperdrivecore.h"

#include <QtCore/QUuid>
#include <QtCore/QEventLoop>

namespace Hyperdrive
{

LocalTransport::LocalTransport(const QString &name, QObject *parent)
    : Transport(*new LocalTransportPrivate(this), name, parent)
{
}

LocalTransport::~LocalTransport()
{
}

QHash< QByteArray, Interface > LocalTransport::introspection() const
{
    Q_D(const LocalTransport);
    return d->core->introspection();
}

bool LocalTransport::hyperdriveHasInterface(const QByteArray& interface) const
{
    Q_D(const LocalTransport);

    return d->core->hasInterface(interface);
}

QList< QByteArray > LocalTransport::listGatesForHyperdriveInterface(const QByteArray& interface) const
{
    Q_D(const LocalTransport);

    return d->core->listGatesForInterface(interface);
}

QList< QByteArray > LocalTransport::listHyperdriveInterfaces() const
{
    Q_D(const LocalTransport);

    return d->core->listInterfaces();
}

void LocalTransport::routeWave(const Hyperspace::Wave &wave, int fd)
{
    Q_D(const LocalTransport);

    d->core->routeWave(this, wave, fd);
}

}
