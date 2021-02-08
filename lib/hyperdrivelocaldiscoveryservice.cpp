/*
 *
 */

#include "hyperdrivelocaldiscoveryservice_p.h"

#include "hyperdrivecore.h"
#include "hyperdrivediscoverymanager.h"

namespace Hyperdrive {

void LocalDiscoveryServicePrivate::initHook()
{
    // Connect signals
    Q_Q(LocalDiscoveryService);

    QObject::connect(q->discoveryManager(), &Hyperdrive::DiscoveryManager::localInterfacesRegistered, q, &LocalDiscoveryService::onLocalInterfacesRegistered);
    QObject::connect(q->discoveryManager(), &Hyperdrive::DiscoveryManager::localInterfacesUnregistered, q, &LocalDiscoveryService::onLocalInterfacesUnregistered);

    // Delegate
    DiscoveryServicePrivate::initHook();
}

LocalDiscoveryService::LocalDiscoveryService(QObject* parent)
    : DiscoveryService(*new LocalDiscoveryServicePrivate(this), parent)
{
}

LocalDiscoveryService::~LocalDiscoveryService()
{
}

void LocalDiscoveryService::setInterfacesAnnounced(const QList< QByteArray >& interfaces)
{
    Q_D(LocalDiscoveryService);
    d->manager->setInterfacesAnnounced(this, interfaces);
}

void LocalDiscoveryService::setInterfacesExpired(const QList< QByteArray >& interfaces)
{
    Q_D(LocalDiscoveryService);
    d->manager->setInterfacesExpired(this, interfaces);
}

void LocalDiscoveryService::setInterfacesPurged(const QList< QByteArray >& interfaces)
{
    Q_D(LocalDiscoveryService);
    d->manager->setInterfacesPurged(this, interfaces);
}

void LocalDiscoveryService::setInterfaceIntrospected(const QByteArray& interface, const QList< QByteArray >& servicesUrl)
{
    Q_D(LocalDiscoveryService);
    d->manager->setInterfaceIntrospected(this, interface, servicesUrl);
}

}

#include "moc_hyperdrivelocaldiscoveryservice.cpp"
