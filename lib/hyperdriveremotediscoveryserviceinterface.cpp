/*
 *
 */

#include "hyperdriveremotediscoveryserviceinterface_p.h"

#include "hyperdrivediscoveryservice_p.h"
#include "hyperdrivediscoverymanager.h"

#include "hyperdrivecore.h"
#include "hyperdriveprotocol.h"

#include <HyperspaceCore/Global>
#include <HyperspaceCore/Socket>

#include <QtCore/QDebug>

namespace Hyperdrive {

void RemoteDiscoveryServiceInterfacePrivate::initHook() {
    // Connect signals
    Q_Q(RemoteDiscoveryServiceInterface);

    QObject::connect(q->discoveryManager(), &Hyperdrive::DiscoveryManager::localInterfacesRegistered,
                     q, &RemoteDiscoveryServiceInterface::onLocalInterfacesRegistered);
    QObject::connect(q->discoveryManager(), &Hyperdrive::DiscoveryManager::localInterfacesUnregistered,
                     q, &RemoteDiscoveryServiceInterface::onLocalInterfacesUnregistered);

    // Delegate
    DiscoveryServicePrivate::initHook();
}

RemoteDiscoveryServiceInterface::RemoteDiscoveryServiceInterface(Hyperspace::Socket* socket, const QString& name, QObject* parent)
    : DiscoveryService(*new RemoteDiscoveryServiceInterfacePrivate(this), parent)
{
    Q_D(RemoteDiscoveryServiceInterface);

    d->name = name;
    d->socket = socket;
}

RemoteDiscoveryServiceInterface::~RemoteDiscoveryServiceInterface()
{
}

void RemoteDiscoveryServiceInterface::initImpl()
{
    // Nothing to do
    setReady();
}

void RemoteDiscoveryServiceInterface::setInterfaceIntrospected(const QByteArray& interface, const QList< QByteArray >& servicesUrl)
{
    Q_UNUSED(interface);
    Q_UNUSED(servicesUrl);
    qWarning() << "Warning: this function should never be invoked!!";
}

void RemoteDiscoveryServiceInterface::setInterfacesPurged(const QList< QByteArray >& interfaces)
{
    Q_UNUSED(interfaces);
    qWarning() << "Warning: this function should never be invoked!!";
}

void RemoteDiscoveryServiceInterface::setInterfacesExpired(const QList< QByteArray >& interfaces)
{
    Q_UNUSED(interfaces);
    qWarning() << "Warning: this function should never be invoked!!";
}

void RemoteDiscoveryServiceInterface::setInterfacesAnnounced(const QList< QByteArray >& interfaces)
{
    Q_UNUSED(interfaces);
    qWarning() << "Warning: this function should never be invoked!!";
}

void RemoteDiscoveryServiceInterface::introspectInterface(const QByteArray& interface)
{
    Q_D(RemoteDiscoveryServiceInterface);

    QByteArray msg;
    QDataStream out(&msg, QIODevice::WriteOnly);

    out << Hyperspace::Protocol::Discovery::introspectRequest() << interface;

    d->socket->write(msg);
}

void RemoteDiscoveryServiceInterface::onLocalInterfacesUnregistered(const QList< QByteArray >& interfaces)
{
    Q_D(RemoteDiscoveryServiceInterface);

    QByteArray msg;
    QDataStream out(&msg, QIODevice::WriteOnly);

    out << Hyperdrive::Protocol::Discovery::interfacesUnregistered() << interfaces;

    d->socket->write(msg);
}

void RemoteDiscoveryServiceInterface::onLocalInterfacesRegistered(const QList< QByteArray >& interfaces)
{
    Q_D(RemoteDiscoveryServiceInterface);

    QByteArray msg;
    QDataStream out(&msg, QIODevice::WriteOnly);

    out << Hyperdrive::Protocol::Discovery::interfacesRegistered() << interfaces;

    d->socket->write(msg);
}

void RemoteDiscoveryServiceInterface::scanInterfaces(const QList< QByteArray >& interfaces)
{
    Q_D(RemoteDiscoveryServiceInterface);

    QByteArray msg;
    QDataStream out(&msg, QIODevice::WriteOnly);

    out << Hyperspace::Protocol::Discovery::scan() << interfaces;

    d->socket->write(msg);
}

}

#include "hyperdriveremotediscoveryserviceinterface.moc"
