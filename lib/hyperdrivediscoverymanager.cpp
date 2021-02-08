/*
 *
 */

#include "hyperdrivediscoverymanager.h"

#include "hyperdrivecore.h"
#include "hyperdrivediscoveryservice_p.h"
#include "hyperdrivelocaldiscoveryservice_p.h"
#include "hyperdrivelocalserver.h"
#include "hyperdriveremotediscoveryserviceinterface_p.h"

#include <HyperspaceCore/Socket>

#include "hyperdriveprotocol.h"
#include "hyperdrivetransport.h"
#include "hyperdrivetransportmanager.h"

#include <hyperdriveconfig.h>

#include <HemeraCore/Literals>
#include <HemeraCore/CommonOperations>
#include <HemeraCore/Fingerprints>

#include <QtCore/QDebug>
#include <QtCore/QLoggingCategory>
#include <QtCore/QDir>
#include <QtCore/QPluginLoader>
#include <QtCore/QUrl>
#include <QtCore/QUuid>

#include "../discoveryservices/discoveryservices.h"

Q_LOGGING_CATEGORY(hyperdriveDiscoveryManagerDC, "hyperdrive.discovery.manager", DEBUG_MESSAGES_DEFAULT_LEVEL)
Q_LOGGING_CATEGORY(discoveryServiceDC, "hyperdrive.discoveryservice", DEBUG_MESSAGES_DEFAULT_LEVEL)

namespace Hyperdrive
{

class IntrospectInterfaceOperation : public Hemera::Operation
{
    Q_OBJECT
    Q_DISABLE_COPY(IntrospectInterfaceOperation)
public:
    explicit IntrospectInterfaceOperation(uint pluginsCount, const QByteArray &interface, QObject* parent = Q_NULLPTR)
        : interface(interface), left(pluginsCount) {}
    virtual ~IntrospectInterfaceOperation() {}

    QList< QByteArray > result() const { return m_servicesUrl.toList(); }

    void setPluginFinished(DiscoveryService *plugin, const QSet< QByteArray > &services) {
        if (!m_finishedPlugins.contains(plugin)) {
            m_servicesUrl.unite(services);
            m_finishedPlugins.append(plugin);
            --left;
            if (left == 0) {
                setFinished();
            }
        }
    }

protected:
    virtual void startImpl() { /* Stuff happens in the main handler */ }

private:
    QByteArray interface;
    QList< DiscoveryService* > m_finishedPlugins;
    QSet< QByteArray > m_servicesUrl;
    uint left;
};

DiscoveryManager::DiscoveryManager(qintptr clientDescriptor, qintptr serviceDescriptor, Hyperdrive::Core* parent)
    : AsyncInitObject(parent)
    , m_core(parent)
    , m_clientDescriptor(clientDescriptor)
    , m_serviceDescriptor(serviceDescriptor)
{
}

DiscoveryManager::~DiscoveryManager()
{
    // Do not delete plugins!! They will be automatically cleansed upon unload. Loaders will die by parenting.
    for (QPluginLoader *loader : m_discoveryServiceLoaders) {
        loader->unload();
    }
}

void DiscoveryManager::initImpl()
{
    // Seek and load!!
    QDir pluginsDir(QLatin1String(StaticConfig::hyperdriveDiscoveryServicesPath()));
    QStringList plugins = pluginsDir.entryList(QStringList() << QStringLiteral("*.so"), QDir::Files);

    // Set init parts
    setParts(plugins.count() + 1);

    for (const QString &fileName : plugins) {
        // Try and load this.
        // Create plugin loader
        QPluginLoader* pluginLoader = new QPluginLoader(pluginsDir.absoluteFilePath(fileName), this);
        // Load plugin
        if (!pluginLoader->load()) {
            // File was wrong. We assume it's a misplaced file and just continue.
            pluginLoader->deleteLater();
            setOnePartIsReady();
            continue;
        }

        // Create plugin instance
        QObject *plugin = pluginLoader->instance();
        if (plugin) {
            // Plugin instance created
            LocalDiscoveryService *discoveryService  = qobject_cast<LocalDiscoveryService*>(plugin);
            if (!discoveryService) {
                // Interface was wrong. We assume it's a misplaced file and just continue.
                pluginLoader->deleteLater();
                setOnePartIsReady();
                continue;
            }

            discoveryService->d_func()->core = m_core;
            discoveryService->d_func()->discoveryManager = this;
            connect(discoveryService->init(), &Hemera::Operation::finished,
                    this, [this, fileName, plugin, pluginLoader, discoveryService] (Hemera::Operation *op) {
                    if (op->isError()) {
                        qCWarning(hyperdriveDiscoveryManagerDC) << "Warning!! Plugin " << fileName << " could not be loaded. "
                                                                << "This particular discovery mechanism won't be available. Reported error: "
                                                                << op->errorName() << op->errorMessage();

                        // Kill it.
                        pluginLoader->unload();
                        pluginLoader->deleteLater();
                    } else {
                        // Cool. Add our plugin to the list.
                        m_discoveryServices.append(discoveryService);
                        m_discoveryServiceLoaders.append(pluginLoader);
                    }

                    setOnePartIsReady();
            });
        }
    }

    // Kewl. Now it's time to enjoy some client/server logic.
    m_clientServer = new LocalServer(this);
    connect(m_clientServer, &LocalServer::newNativeConnection, this, [this] (quintptr fd) {
        qCInfo(hyperdriveDiscoveryManagerDC) << "Got a new discovery client connection";
        Hyperspace::Socket *socket = new Hyperspace::Socket(fd, this);

        connect(socket, &Hyperspace::Socket::disconnected, this, [this, socket] {
            // Cleanup
            qCInfo(hyperdriveDiscoveryManagerDC) << "Client discovery removed";
            // Expensive, but we gotta do this.
            QList<QByteArray> interfaces = m_subscribers.keys(socket);
            for (const QByteArray &interface : interfaces) {
                m_subscribers.remove(interface, socket);
            }

            socket->deleteLater();
        });

        connect(socket, &Hyperspace::Socket::readyRead, this, [this, socket] (QByteArray data, int fd) {
            QDataStream in(&data, QIODevice::ReadOnly);
            // Sockets may buffer several requests in case they're concurrent: unroll the whole DataStream.
            while (!in.atEnd()) {
                quint8 command;
                in >> command;

                if (command == Hyperspace::Protocol::Discovery::introspectRequest()) {
                    QByteArray interface;
                    in >> interface;

                    IntrospectInterfaceOperation *op = m_ongoingIntrospect.value(interface, Q_NULLPTR);
                    if (!op) {
                        op = new IntrospectInterfaceOperation(m_discoveryServices.count(), interface, this);
                        m_ongoingIntrospect.insert(interface, op);
                    }

                    connect(op, &Hemera::Operation::finished, this, [this, interface, op, socket] {
                        m_ongoingIntrospect.remove(interface);
                        QByteArray msg;
                        QDataStream out(&msg, QIODevice::WriteOnly);

                        out << Hyperspace::Protocol::Discovery::introspectReply() << interface << op->result();

                        socket->write(msg);
                    });

                    for (DiscoveryService *p : m_discoveryServices) {
                        p->introspectInterface(interface);
                    }
                } else if (command == Hyperspace::Protocol::Discovery::scan()) {
                    for (DiscoveryService *p : m_discoveryServices) {
                        QList< QByteArray > interfaces;
                        in >> interfaces;
                        p->scanInterfaces(interfaces);
                    }
                } else if (command == Hyperspace::Protocol::Discovery::subscribe()) {
                    QList< QByteArray > interfaces;
                    in >> interfaces;

                    for (const QByteArray &interface : interfaces) {
                        bool scanAfterwards = !m_subscribers.contains(interface);

                        m_subscribers.insert(interface, socket);

                        if (scanAfterwards) {
                            for (QHash< DiscoveryService*, InterfaceStatus >::const_iterator i = m_interfaces[interface].constBegin();
                                i != m_interfaces[interface].constEnd(); ++i) {
                                i.key()->scanInterfaces(interfaces);
                            }
                        }
                    }
                } else if (command == Hyperspace::Protocol::Discovery::unsubscribe()) {
                    QList< QByteArray > interfaces;
                    in >> interfaces;

                    for (const QByteArray &interface : interfaces) {
                        m_subscribers.remove(interface, socket);
                    }
                } else {
                    qCWarning(hyperdriveDiscoveryManagerDC) << "Discovery message malformed!";
                }
            }
        });

        // Fast init
        connect(socket->init(), &Hemera::Operation::finished, this, [this] (Hemera::Operation *op) {
            if (op->isError()) {
                // TODO: What to do?
                qCWarning(hyperdriveDiscoveryManagerDC) << "Could not initialize socket!";
            }
        });
    });

    m_serviceServer = new LocalServer(this);
    connect(m_serviceServer, &LocalServer::newNativeConnection, this, [this] (quintptr fd) {
        qCInfo(hyperdriveDiscoveryManagerDC) << "Got a new discovery service connection";
        Hyperspace::Socket *socket = new Hyperspace::Socket(fd, this);

        connect(socket, &Hyperspace::Socket::disconnected, this, [this, socket] {
            // Cleanup
            qCInfo(hyperdriveDiscoveryManagerDC) << "Discovery service removed";
            DiscoveryService *service = m_remoteServices.take(socket);
            m_discoveryServices.removeOne(service);

            service->deleteLater();
            socket->deleteLater();
        });

        connect(socket, &Hyperspace::Socket::readyRead, this, [this, socket] (QByteArray data, int fd) {
            QDataStream in(&data, QIODevice::ReadOnly);
            // Sockets may buffer several requests in case they're concurrent: unroll the whole DataStream.
            while (!in.atEnd()) {
                quint8 command;
                in >> command;

                if (command == Hyperdrive::Protocol::Control::nameExchange()) {
                    // Register the transport
                    QString name;

                    in >> name;

                    RemoteDiscoveryServiceInterface *service = new RemoteDiscoveryServiceInterface(socket, name, this);
                    service->d_func()->core = m_core;
                    service->d_func()->discoveryManager = this;

                    connect(service->init(), &Hemera::Operation::finished, this, [this, name, socket, service] (Hemera::Operation *op) {
                        if (op->isError()) {
                            qCWarning(hyperdriveDiscoveryManagerDC) << "Failed to instantiate remote interface for discovery service "
                                    << name << op->errorName() << op->errorMessage();
                            return;
                        }

                        m_remoteServices.insert(socket, service);
                        m_discoveryServices.append(service);

                        qCDebug(hyperdriveDiscoveryManagerDC) << "Authenticated remote discovery service successfully: " << name;
                    });
                } else if (command == Hyperdrive::Protocol::Control::listHyperdriveInterfaces()) {
                    QUuid requestId;
                    in >> requestId;

                    QByteArray msg;
                    QDataStream out(&msg, QIODevice::WriteOnly);

                    out << Hyperdrive::Protocol::Control::listHyperdriveInterfaces()
                        << requestId << registeredLocalInterfaces();

                    socket->write(msg);
                } else if (command == Hyperdrive::Protocol::Control::transportsTemplateUrls()) {
                    QUuid requestId;
                    in >> requestId;

                    QByteArray msg;
                    QDataStream out(&msg, QIODevice::WriteOnly);

                    // Convert to serializable format...
                    QHash< QUrl, Transport::Features > urlToFeatures = m_core->transportManager()->templateUrls();
                    QHash< QUrl, uint > urlToFeaturesUInt;
                    for (QHash< QUrl, Transport::Features >::const_iterator i = urlToFeatures.constBegin(); i != urlToFeatures.constEnd(); ++i) {
                        urlToFeaturesUInt.insert(i.key(), static_cast< uint >(i.value()));
                    }

                    out << Hyperdrive::Protocol::Control::transportsTemplateUrls()
                        << requestId << urlToFeaturesUInt;

                    socket->write(msg);
                } else if (command == Hyperspace::Protocol::Discovery::announced()) {
                    QList< QByteArray > interfaces;
                    in >> interfaces;
                    setInterfacesAnnounced(m_remoteServices.value(socket), interfaces);
                } else if (command == Hyperspace::Protocol::Discovery::expired()) {
                    QList< QByteArray > interfaces;
                    in >> interfaces;
                    setInterfacesExpired(m_remoteServices.value(socket), interfaces);
                } else if (command == Hyperspace::Protocol::Discovery::purged()) {
                    QList< QByteArray > interfaces;
                    in >> interfaces;
                    setInterfacesPurged(m_remoteServices.value(socket), interfaces);
                } else if (command == Hyperspace::Protocol::Discovery::introspectReply()) {
                    QByteArray interface;
                    QList< QByteArray > servicesUrl;
                    in >> interface >> servicesUrl;
                    setInterfaceIntrospected(m_remoteServices.value(socket), interface, servicesUrl);
                } else {
                    qCWarning(hyperdriveDiscoveryManagerDC) << "Discovery message malformed!";
                }
            }
        });

        // Fast init
        connect(socket->init(), &Hemera::Operation::finished, this, [this] (Hemera::Operation *op) {
            if (op->isError()) {
                // TODO: What to do?
                qCWarning(hyperdriveDiscoveryManagerDC) << "Could not initialize socket!";
            }
        });
    });

    // Start listening
    if (Q_UNLIKELY(!m_clientServer->listen(m_clientDescriptor))) {
        setInitError(Hemera::Literals::literal(Hemera::Literals::Errors::systemdError()),
                     QStringLiteral("Could not bind to systemd's socket for client discovery."));
        return;
    }
    if (Q_UNLIKELY(!m_clientServer->isListening())) {
        setInitError(Hemera::Literals::literal(Hemera::Literals::Errors::systemdError()),
                     QStringLiteral("Systemd's socket for client discovery is currently not listening. This should not be happening."));
        return;
    }

    if (Q_UNLIKELY(!m_serviceServer->listen(m_serviceDescriptor))) {
        setInitError(Hemera::Literals::literal(Hemera::Literals::Errors::systemdError()),
                     QStringLiteral("Could not bind to systemd's socket for discovery services."));
        return;
    }
    if (Q_UNLIKELY(!m_serviceServer->isListening())) {
        setInitError(Hemera::Literals::literal(Hemera::Literals::Errors::systemdError()),
                     QStringLiteral("Systemd's socket for discovery services is currently not listening. This should not be happening."));
        return;
    }

    // We also want to start advertising our native interfaces.
    Hemera::ByteArrayOperation *hwOp = Hemera::Fingerprints::globalHardwareId();
    connect(hwOp, &Hemera::Operation::finished, this, [this, hwOp] {
        if (hwOp->isError()) {
            qCWarning(hyperdriveDiscoveryManagerDC) << "Could not retrieve hardware ID! This device might not be fully discoverable.";
            return;
        }

        QByteArray interface = "hwid." + hwOp->result();

        registerLocalInterfaces(QList< QByteArray >() << interface);
    });

    Hemera::ByteArrayOperation *sysOp = Hemera::Fingerprints::globalSystemId();
    connect(sysOp, &Hemera::Operation::finished, this, [this, sysOp] {
        if (sysOp->isError()) {
            qCWarning(hyperdriveDiscoveryManagerDC) << "Could not retrieve system ID! This device might not be fully discoverable.";
            return;
        }

        QByteArray interface = "sysid." + sysOp->result();

        registerLocalInterfaces(QList< QByteArray >() << interface);
    });

    setOnePartIsReady();
}

void DiscoveryManager::sendInterfaceMessageToSubscribers(const QByteArray& interface, quint8 command)
{
    QByteArray msg;
    QDataStream out(&msg, QIODevice::WriteOnly);

    out << command << interface;

    sendToSubscribers(interface, msg);
}

void DiscoveryManager::sendToSubscribers(const QByteArray& interface, const QByteArray& message)
{
    // Send out to subscribers
    QMultiHash< QByteArray, Hyperspace::Socket* >::const_iterator i = m_subscribers.constFind(interface);
    while (i != m_subscribers.constEnd() && i.key() == interface) {
        i.value()->write(message);
        ++i;
    }
}

void DiscoveryManager::setInterfacesAnnounced(DiscoveryService* plugin, const QList< QByteArray >& interfaces)
{
    // Let's find out one by one.
    for (const QByteArray &interface : interfaces) {
        // First things first: let's see if we need to globally announce that.
        if (!m_interfaces[interface].values().contains(InterfaceStatus::Announced)) {
            sendInterfaceMessageToSubscribers(interface, Hyperspace::Protocol::Discovery::announced());
        } else {
            // Then, just notify the introspection changed.
            sendInterfaceMessageToSubscribers(interface, Hyperspace::Protocol::Discovery::changed());
        }

        // Ok, let's just add it now
        m_interfaces[interface][plugin] = InterfaceStatus::Announced;
    }
}

void DiscoveryManager::setInterfacesExpired(DiscoveryService* plugin, const QList< QByteArray >& interfaces)
{
    // Let's find out one by one.
    for (const QByteArray &interface : interfaces) {
        // First things first: let's see if we *might* need to globally announce that.
        bool wasAnnounced = m_interfaces[interface].values().contains(InterfaceStatus::Announced);
        bool wasExpired = m_interfaces[interface].values().contains(InterfaceStatus::Expired);

        // Ok, let's just add it now
        m_interfaces[interface][plugin] = InterfaceStatus::Expired;

        // Now. Did the situation change, and do we need to announce in case?
        if ((wasAnnounced && !m_interfaces[interface].values().contains(InterfaceStatus::Announced)) ||
            (!wasAnnounced && !wasExpired)) {
            // Uh. Looks like the interface really expired. Notify subscribers.
            sendInterfaceMessageToSubscribers(interface, Hyperspace::Protocol::Discovery::expired());
        } else {
            // Then, just notify the introspection changed.
            sendInterfaceMessageToSubscribers(interface, Hyperspace::Protocol::Discovery::changed());
        }
    }
}

void DiscoveryManager::setInterfacesPurged(DiscoveryService* plugin, const QList< QByteArray >& interfaces)
{
    // Let's find out one by one.
    for (const QByteArray &interface : interfaces) {
        // First things first: let's see if we *might* need to globally announce that.
        bool wasAnnounced = m_interfaces[interface].values().contains(InterfaceStatus::Announced);
        bool wasExpired = m_interfaces[interface].values().contains(InterfaceStatus::Expired);

        // Ok, let's just add it now
        m_interfaces[interface][plugin] = InterfaceStatus::Purged;

        // Now. Did the situation change, and do we need to announce in case?
        // Hardcore trouble in here. This unreadable block means: act if the interface was either announced or expired (not both),
        // and if that's the case, verify this is not the case anymore
        if (!(wasAnnounced && wasExpired) && (wasAnnounced || wasExpired) && (
            (wasAnnounced && !m_interfaces[interface].values().contains(InterfaceStatus::Announced)) ||
            (wasExpired && !m_interfaces[interface].values().contains(InterfaceStatus::Expired)))) {
            // Uh. Looks like the interface really got purged. Notify subscribers.
            sendInterfaceMessageToSubscribers(interface, Hyperspace::Protocol::Discovery::purged());
        } else {
            // Then, just notify the introspection changed.
            sendInterfaceMessageToSubscribers(interface, Hyperspace::Protocol::Discovery::changed());
        }
    }
}

void DiscoveryManager::setInterfaceIntrospected(DiscoveryService* plugin,
                                                 const QByteArray& interface, const QList< QByteArray >& servicesUrl)
{
    IntrospectInterfaceOperation *op = m_ongoingIntrospect.value(interface);
    if (!op) {
        qCWarning(hyperdriveDiscoveryManagerDC) << "Sent introspect with no pending requests! A plugin is acting weird...";
        return;
    }

    op->setPluginFinished(plugin, servicesUrl.toSet());
}

QList< QByteArray > DiscoveryManager::registeredLocalInterfaces() const
{
    return m_registeredLocalInterfaces;
}

void DiscoveryManager::registerLocalInterfaces(const QList< QByteArray >& interfaces)
{
    m_registeredLocalInterfaces.append(interfaces);
    Q_EMIT localInterfacesRegistered(interfaces);
}

void DiscoveryManager::unregisterLocalInterfaces(const QList< QByteArray >& interfaces)
{
    for (const QByteArray &interface : interfaces) {
        m_interfaces.remove(interface);
    }
    Q_EMIT localInterfacesUnregistered(interfaces);
}

}

#include "hyperdrivediscoverymanager.moc"
