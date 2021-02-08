/*
 *
 */

#ifndef HYPERDRIVE_DISCOVERYMANAGER_H
#define HYPERDRIVE_DISCOVERYMANAGER_H

#include <HemeraCore/AsyncInitObject>

namespace Hyperspace {
class Socket;
}

class QPluginLoader;
namespace Hyperdrive {

class Core;
class DiscoveryService;
class IntrospectInterfaceOperation;
class LocalServer;

class DiscoveryManager : public Hemera::AsyncInitObject
{
    Q_OBJECT
    Q_DISABLE_COPY(DiscoveryManager)

    enum class InterfaceStatus {
        Announced = 1,
        Expired = 2,
        Purged = 3
    };

public:
    explicit DiscoveryManager(qintptr clientDescriptor, qintptr serviceDescriptor, Core *parent);
    virtual ~DiscoveryManager();

    QList< QByteArray > registeredLocalInterfaces() const;

protected:
    virtual void initImpl() Q_DECL_OVERRIDE Q_DECL_FINAL;

Q_SIGNALS:
    void localInterfacesRegistered(const QList< QByteArray > &interfaces);
    void localInterfacesUnregistered(const QList< QByteArray > &interfaces);

private:
    void setInterfacesAnnounced(DiscoveryService *plugin, const QList<QByteArray> &interfaces);
    void setInterfacesExpired(DiscoveryService *plugin, const QList<QByteArray> &interfaces);
    void setInterfacesPurged(DiscoveryService *plugin, const QList<QByteArray> &interfaces);

    void setInterfaceIntrospected(DiscoveryService *plugin, const QByteArray &interface, const QList< QByteArray > &servicesUrl);

    void sendInterfaceMessageToSubscribers(const QByteArray &interface, quint8 command);
    void sendToSubscribers(const QByteArray &interface, const QByteArray &message);

    void registerLocalInterfaces(const QList< QByteArray > &interfaces);
    void unregisterLocalInterfaces(const QList< QByteArray > &interfaces);

    Core *m_core;
    QList< DiscoveryService* > m_discoveryServices;
    QList< QPluginLoader* > m_discoveryServiceLoaders;
    QHash< QByteArray, QHash< DiscoveryService*, InterfaceStatus > > m_interfaces;
    QMultiHash< QByteArray, Hyperspace::Socket* > m_subscribers;
    QHash< QByteArray, IntrospectInterfaceOperation* > m_ongoingIntrospect;
    QHash< Hyperspace::Socket*, DiscoveryService* > m_remoteServices;
    QList< QByteArray > m_registeredLocalInterfaces;

    qintptr m_clientDescriptor;
    qintptr m_serviceDescriptor;
    LocalServer *m_clientServer;
    LocalServer *m_serviceServer;

    friend class LocalDiscoveryService;
    friend class Core;
};
}

#endif // HYPERDRIVE_DISCOVERYMANAGER_H
