/*
 *
 */

#include "hyperdrivediscoveryservice_p.h"

#include "hyperdrivecore.h"
#include "hyperdrivediscoverymanager.h"

namespace Hyperdrive {

void DiscoveryServicePrivate::initHook()
{
    // Delegate
    Hemera::AsyncInitObjectPrivate::initHook();
}

DiscoveryService::DiscoveryService(DiscoveryServicePrivate &dd, QObject* parent)
    : AsyncInitObject(dd, parent)
{
}

DiscoveryService::~DiscoveryService()
{
}

QByteArray DiscoveryService::transportMedium() const
{
    Q_D(const DiscoveryService);
    return d->transportMedium;
}

void DiscoveryService::setName(const QByteArray& name)
{
    Q_D(DiscoveryService);
    d->name = name;
}

void DiscoveryService::setTransportMedium(const QByteArray& name)
{
    Q_D(DiscoveryService);
    d->transportMedium = name;
}

DiscoveryManager *DiscoveryService::discoveryManager()
{
    Q_D(DiscoveryService);
    return d->discoveryManager;
}

}

#include "moc_hyperdrivediscoveryservice.cpp"
