/*
 *
 */

#include "hyperdrivecore.h"

#include "hyperdrivecache.h"
#include "hyperdriveprotocol.h"
#include "hyperdrivetransport.h"
#include "hyperdrivetransportmanager.h"
#include "hyperdrivediscoverymanager.h"
#include "hyperdrivesecuritymanager.h"
#include "hyperdrivelocalserver.h"

#include <HyperspaceCore/BSONDocument>
#include <HyperspaceCore/BSONStreamReader>
#include <HyperspaceCore/Fluctuation>
#include <HyperspaceCore/Rebound>
#include <HyperspaceCore/Socket>
#include <HyperspaceCore/Wave>
#include <HyperspaceCore/Waveguide>

#include <HemeraCore/CommonOperations>
#include <HemeraCore/Fingerprints>
#include <HemeraCore/Literals>

#include <QtCore/QDebug>
#include <QtCore/QLoggingCategory>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileSystemWatcher>
#include <QtCore/QTimer>

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

#include <systemd/sd-daemon.h>

#include <signal.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <hyperdriveconfig.h>


Q_LOGGING_CATEGORY(hyperdriveCoreDC, "hyperdrive.core", DEBUG_MESSAGES_DEFAULT_LEVEL)

#define INTERFACE_RELOAD_TIMEOUT 2000

namespace Hyperdrive
{

Core::Core(QObject* parent)
    : AsyncInitDBusObject(parent)
    , m_needsBigBang(false)
    , m_discoveryManager(nullptr)
{
}

Core::~Core()
{
}

TransportManager *Core::transportManager()
{
    return m_transportManager;
}

void Core::initImpl()
{
    // Check our socket activation, first of all
    if (Q_UNLIKELY(sd_listen_fds(0) != 4)) {
        setInitError(Hemera::Literals::literal(Hemera::Literals::Errors::systemdError()),
                     QStringLiteral("No or too many file descriptors received."));
        return;
    }

    setParts(5);

    m_transportManager = new TransportManager(this);
    connect(m_transportManager->init(), &Hemera::Operation::finished, this, [this] (Hemera::Operation *op) {
            if (op->isError()) {
                setInitError(op->errorName(), op->errorMessage());
            } else {
                setOnePartIsReady();
            }
    });

    m_discoveryManager = new DiscoveryManager(SD_LISTEN_FDS_START + 2, SD_LISTEN_FDS_START + 3, this);
    connect(m_discoveryManager->init(), &Hemera::Operation::finished, this, [this] (Hemera::Operation *op) {
        if (op->isError()) {
            if (op->errorName() == Hemera::Literals::literal(Hemera::Literals::Errors::notFound())) {
                // Simply no discovery plugins found.
                qCInfo(hyperdriveCoreDC) << "No discovery plugins found. Discovery won't be available.";
                setOnePartIsReady();
            } else {
                setInitError(op->errorName(), op->errorMessage());
            }
        } else {
            setOnePartIsReady();
        }
    });

    m_securityManager = new SecurityManager(this);
    connect(m_securityManager->init(), &Hemera::Operation::finished, this, [this] (Hemera::Operation *op) {
            if (op->isError()) {
                setInitError(op->errorName(), op->errorMessage());
            } else {
                setOnePartIsReady();
            }
    });

    // If the Cache creates/updates the DB, we need to send a bigBang
    connect(Cache::instance(), &Cache::invalidCache, this, [this] {
            m_needsBigBang = true;
    });
    connect(Cache::instance()->init(), &Hemera::Operation::finished, this, [this] (Hemera::Operation *op) {
            if (op->isError()) {
                setInitError(op->errorName(), op->errorMessage());
            } else {
                setOnePartIsReady();
            }
    });

    // Populate the interfaces
    loadInterfaces();

    m_interfaceDirWatcher = new QFileSystemWatcher({StaticConfig::hyperspaceInterfacesDir()});
    m_interfaceDirWatcher->addPath(QStringLiteral("%1/interfaces").arg(QDir::homePath()));
    m_interfaceDirTimer = new QTimer(this);
    m_interfaceDirTimer->setSingleShot(true);
    m_interfaceDirTimer->setInterval(INTERFACE_RELOAD_TIMEOUT);

    connect(m_interfaceDirWatcher, &QFileSystemWatcher::directoryChanged, this, [this] {
        m_interfaceDirTimer->start();
    });

    connect(m_interfaceDirTimer, &QTimer::timeout, this, &Core::loadInterfaces);

    // Create servers
    m_gatesServer = new LocalServer(this);
    m_transportsServer = new LocalServer(this);

    connect(m_gatesServer, &LocalServer::newNativeConnection, this, [this] (quintptr fd) {
        qCInfo(hyperdriveCoreDC) << "Got a new Gate connection";
        Hyperspace::Socket *socket = new Hyperspace::Socket(fd, this);

        m_gateToBSONStreamReader.insert(socket, new Hyperspace::Util::BSONStreamReader());

        // Connect everything so we don't lose data
        connect(socket, &Hyperspace::Socket::readyRead, this, [this, socket] (const QByteArray &d, int fd) {
            m_gateToBSONStreamReader[socket]->enqueueData(d);
            // Sockets may buffer several requests in case they're concurrent: unroll the whole stream
            while (m_gateToBSONStreamReader.value(socket)->canReadDocument()) {
                QByteArray streamData = m_gateToBSONStreamReader[socket]->dequeueDocumentData();
                Hyperspace::Util::BSONDocument doc(streamData);
                Hyperspace::Protocol::MessageType messageType = static_cast<Hyperspace::Protocol::MessageType>(doc.int32Value("y"));

                switch (messageType) {
                    case Hyperspace::Protocol::MessageType::Waveguide: {
                        Hyperspace::Waveguide waveguide = Hyperspace::Waveguide::fromBinary(streamData);

                        qCDebug(hyperdriveCoreDC) << "Got a Waveguide for " << waveguide.interface();

                        // If what we received is in the possible interfaces and is not already implemented by someone else
                        if (m_introspection.contains(waveguide.interface()) && !m_interfaceToGate.contains(waveguide.interface())) {
                            m_interfaceToGate.insert(waveguide.interface(), socket);
                            m_socketToInterface.insert(socket, waveguide.interface());
                            m_discoveryManager->localInterfacesRegistered(QList<QByteArray>() << waveguide.interface());
                            if (m_introspection.value(waveguide.interface()).interfaceQuality() == Interface::Quality::Consumer) {
                                sendConsumerCache(waveguide.interface());
                            }
                        } else {
                            qCDebug(hyperdriveCoreDC) << "Can't add interface " << waveguide.interface();
                        }
                        break;
                    }

                    case Hyperspace::Protocol::MessageType::Rebound: {
                        Hyperspace::Rebound rebound = Hyperspace::Rebound::fromBinary(streamData);

                        if (Q_LIKELY(m_waveIdToTransport.contains(rebound.id()))) {
                            m_waveIdToTransport[rebound.id()]->rebound(rebound, fd);
                            m_waveIdToTransport.remove(rebound.id());

                            // If the fd is valid, we have to close it to prevent leakage.
                            if (fd > 0) {
                                ::close(fd);
                            }
                        } else if (m_dropReboundWaveSet.contains(rebound.id())) {
                            // Just drop the rebound
                            m_dropReboundWaveSet.remove(rebound.id());
                        } else {
                            qCWarning(hyperdriveCoreDC) << "Got rebound for id " << rebound.id() << ", but no transport is associated to it!!";
                        }
                        break;
                    }

                    case Hyperspace::Protocol::MessageType::Fluctuation: {
                        Hyperspace::Fluctuation fluctuation = Hyperspace::Fluctuation::fromBinary(streamData);

                        qCDebug(hyperdriveCoreDC) << "Got a Fluctuation!" << fluctuation.interface() << fluctuation.target();

                        if (!m_introspection.contains(fluctuation.interface()) || !m_socketToInterface.values(socket).contains(fluctuation.interface())) {
                            qCWarning(hyperdriveCoreDC) << "Fluctuation is not authorized for " << fluctuation.interface() << " ignoring it.";
                        } else {

                            if (m_introspection.value(fluctuation.interface()).interfaceType() == Interface::Type::Properties &&
                                m_introspection.value(fluctuation.interface()).interfaceQuality() == Interface::Quality::Producer) {
                                if (fluctuation.payload().isEmpty()) {
                                    Cache::instance()->removeProducerProperty(fluctuation.interface(), fluctuation.target());
                                } else {
                                    Cache::instance()->insertOrUpdateProducerProperty(fluctuation.interface(), fluctuation.target(), fluctuation.payload());
                                }
                            }

                            CacheMessage cacheMessage;
                            cacheMessage.setPayload(fluctuation.payload());
                            cacheMessage.setAttributes(fluctuation.attributes());

                            // Build the fully-qualified path(s) for the fluctuation
                            QByteArray fullyQualifiedPath = QByteArray("/%1%2").replace("%1", fluctuation.interface()).replace("%2", fluctuation.target());

                            cacheMessage.setTarget(fullyQualifiedPath);

                            Hyperdrive::Interface::Type interfaceType = m_introspection.value(fluctuation.interface()).interfaceType();
                            cacheMessage.setInterfaceType(interfaceType);

                            // Multicast to every available transport
                            for (Transport *transport : m_transportManager->loadedTransports()) {
                                transport->cacheMessage(cacheMessage);
                            }
                        }

                        break;
                    }

                    default: {
                        qCWarning(hyperdriveCoreDC) << "Control message malformed!";
                        return;
                    }
                }
            }
        });

        connect(socket, &Hyperspace::Socket::disconnected, this, [this, socket] {
            // Cleanup
            qCInfo(hyperdriveCoreDC) << "Gate removed";
            QList<QByteArray> interfaces;
            QMultiHash< Hyperspace::Socket*, QByteArray >::const_iterator i = m_socketToInterface.find(socket);
            while (i != m_socketToInterface.constEnd() && i.key() == socket) {
                QByteArray interface = i.value();
                qCDebug(hyperdriveCoreDC) << "Unloading interface " << interface;
                m_interfaceToGate.remove(interface);
                interfaces << interface;
                ++i;
            }
            delete m_gateToBSONStreamReader.take(socket);
            m_socketToInterface.remove(socket);
            socket->deleteLater();

            if (!interfaces.isEmpty() && m_discoveryManager) {
                m_discoveryManager->localInterfacesUnregistered(interfaces);
            }
        });

        // Finally, init the socket
        connect(socket->init(), &Hemera::Operation::finished, this, [this, socket] {
            qCInfo(hyperdriveCoreDC) << "Socket initialized";
        });
    });

    connect(m_transportsServer, &LocalServer::newNativeConnection, this, [this] (quintptr fd) {
        qCInfo(hyperdriveCoreDC) << "Got a new Transport connection";

        Hyperspace::Socket *socket = new Hyperspace::Socket(fd, this);
        transportManager()->addRemoteTransport(socket);
    });

    connect(m_transportManager, &TransportManager::remoteTransportLoaded, this, [this] (Transport *transport) {
        qCInfo(hyperdriveCoreDC) << "Remote transport loaded, sending producer cache";

        QList<CacheMessage> cacheMessagesToSend;

        for (QHash< QByteArray, QHash< QByteArray, QByteArray > >::const_iterator iter = Cache::instance()->allProducerProperties().constBegin();
             iter != Cache::instance()->allProducerProperties().constEnd(); ++iter) {

            if (!m_introspection.contains(iter.key())) {
                qCWarning(hyperdriveCoreDC) << "Interface " << iter.key() << " has producer cache but isn't in the introspection, not sending its cache";
                continue;
            }

            CacheMessage cacheMessage;

            QByteArray interfacePrefix = "/";
            interfacePrefix.append(iter.key());

            cacheMessage.setInterfaceType(m_introspection.value(iter.key()).interfaceType());

            for (QHash< QByteArray, QByteArray >::const_iterator innerIter = iter.value().constBegin(); innerIter != iter.value().constEnd(); ++innerIter) {
                cacheMessage.setTarget(interfacePrefix + innerIter.key());
                cacheMessage.setPayload(innerIter.value());
                cacheMessagesToSend.append(cacheMessage);
            }
        }

        for (const CacheMessage &c : cacheMessagesToSend) {
            transport->cacheMessage(c);
        }

        // We wiped the device, we need Consumer Properties again
        if (m_needsBigBang) {
            transport->bigBang();
        }
    });

    // Gates first
    if (Q_UNLIKELY(!m_gatesServer->listen(SD_LISTEN_FDS_START + 0))) {
        setInitError(Hemera::Literals::literal(Hemera::Literals::Errors::systemdError()),
                     QStringLiteral("Could not bind to systemd's socket for gates."));
        return;
    }
    if (Q_UNLIKELY(!m_gatesServer->isListening())) {
        setInitError(Hemera::Literals::literal(Hemera::Literals::Errors::systemdError()),
                     QStringLiteral("Systemd's socket for gates is currently not listening. This should not be happening."));
        return;
    }

    // Then transports
    if (Q_UNLIKELY(!m_transportsServer->listen(SD_LISTEN_FDS_START + 1))) {
        setInitError(Hemera::Literals::literal(Hemera::Literals::Errors::systemdError()),
                     QStringLiteral("Could not bind to systemd's socket for transports."));
        return;
    }
    if (Q_UNLIKELY(!m_transportsServer->isListening())) {
        setInitError(Hemera::Literals::literal(Hemera::Literals::Errors::systemdError()),
                     QStringLiteral("Systemd's socket for transports is currently not listening. This should not be happening."));
        return;
    }

    setOnePartIsReady();
}

QHash< QByteArray, Interface > Core::introspection() const
{
    return m_introspection;
}

bool Core::hasInterface(const QByteArray& interface) const
{
    return m_interfaceToGate.contains(interface);
}

QList< QByteArray > Core::listInterfaces() const
{
    return m_interfaceToGate.keys();
}

QList< QByteArray > Core::listGatesForInterface(const QByteArray& interface) const
{
    // Right now, there's no Gate component in the URL. Return the Interface.
    return QList<QByteArray>() << interface;
}

void Core::sendConsumerCache(const QByteArray &interface)
{
    QHash< QByteArray, QByteArray > consumerProperties = Cache::instance()->consumerProperties(interface);
    for (QHash< QByteArray, QByteArray>::const_iterator it = consumerProperties.constBegin(); it != consumerProperties.constEnd(); ++it) {
        Hyperspace::Wave wave = Hyperspace::Wave::fromBinary(it.value());
        if (wave.payload().isEmpty()) {
            Cache::instance()->removeConsumerProperty(interface, it.key());
        }
        m_dropReboundWaveSet.insert(wave.id());
        sendWave(interface, wave);
    }
}

qint64 Core::routeWave(Transport *transport, const Hyperspace::Wave &wave, int fd)
{
    // Start from the interface
    QByteArray waveTarget = wave.target();
    int targetIndex = waveTarget.indexOf('/', 1);
    QByteArray interface = waveTarget.mid(1, targetIndex - 1);
    QByteArray relativeTarget = waveTarget.right(waveTarget.length() - targetIndex);

    if (interface == "control") {
        // Special case, control wave
        Hyperspace::Wave controlWave = wave;
        controlWave.setTarget(relativeTarget);
        return handleControlWave(transport, controlWave, fd);
    }

    qCDebug(hyperdriveCoreDC) << "Routing wave: " << wave.id() << wave.method() << " " << waveTarget << " We have those gates:" << m_interfaceToGate;

    bool isProperty;
    // Does the interface exist for us?
    if (!m_introspection.contains(interface)) {
        // Nope
        qCDebug(hyperdriveCoreDC) << "Interface not found" << interface << m_interfaceToGate;
        transport->rebound(Hyperspace::Rebound(wave, Hyperspace::ResponseCode::NotFound));
        return -1;
    } else {
        isProperty = m_introspection.value(interface).interfaceType() == Interface::Type::Properties;
    }

    Hyperspace::Wave ws = wave;
    ws.setInterface(interface);
    ws.setTarget(relativeTarget);

    QByteArray serializedWave = ws.serialize();

    if (isProperty) {
        // We save it also if it's an empty payload, we have to make sure that it's forwarded to the Consumer
        Cache::instance()->insertOrUpdateConsumerProperty(interface, relativeTarget, serializedWave);
    }

    // Is the Gate for this interface already up?
    if (!m_interfaceToGate.contains(interface)) {
        // Nope, we will send the wave when it comes up
        // TODO: we know that the interface exists, what about the relative target? For now, assume it exists and return a 200
        transport->rebound(Hyperspace::Rebound(ws, Hyperspace::ResponseCode::OK));
        return serializedWave.length();
    }

    QPair<QByteArray, QByteArray> errorAttribute;
    if (Q_UNLIKELY(!m_securityManager->isAuthorized(ws, interface, errorAttribute))) {
        Hyperspace::Rebound rebound(wave, Hyperspace::ResponseCode::Unauthorized);
        rebound.addAttribute(errorAttribute.first, errorAttribute.second);
        transport->rebound(rebound);

        // If the fd is valid, we have to close it to prevent leakage.
        if (fd > 0) {
            ::close(fd);
        }
        return -1;
    }

    // Ready to listen to the rebound
    m_waveIdToTransport.insert(ws.id(), transport);

    // We are sending the wave now, so we can delete it from the Cache if it has an empty payload
    if (isProperty && ws.payload().isEmpty()) {
        Cache::instance()->removeConsumerProperty(interface, relativeTarget);
    }

    return sendWave(interface, ws, fd);
}

qint64 Core::sendWave(const QByteArray &interface, const Hyperspace::Wave &wave, int fd) const
{
    Hyperspace::Socket *gate = m_interfaceToGate.value(interface);

    int written = gate->write(wave.serialize(), fd);

    // If the fd is valid, we have to close it to prevent leakage.
    if (fd > 0) {
        ::close(fd);
    }
    return written;
}

qint64 Core::handleControlWave(Transport *transport, const Hyperspace::Wave &wave, int fd)
{
    if (wave.target() == "/consumer/properties") {
        // Send an ok so the transport is happy
        transport->rebound(Hyperspace::Rebound(wave, Hyperspace::ResponseCode::OK));

        QSet< QByteArray > toBeUnset = Cache::instance()->allConsumerPropertiesFullPaths();
        QByteArray uncompressedPayload = qUncompress(wave.payload());
        QByteArrayList consumerPaths = uncompressedPayload.split(';');

        qCDebug(hyperdriveCoreDC) << "Remote consumer property paths are: " << consumerPaths;
        qCDebug(hyperdriveCoreDC) << "Local consumer property paths are: " << toBeUnset;

        for (const QByteArray &consumerPath : consumerPaths) {
            toBeUnset.remove(consumerPath);
        }

        qCDebug(hyperdriveCoreDC) << "Consumer property paths to be unset: " << toBeUnset;

        for (const QByteArray &path : toBeUnset) {
            int targetIndex = path.indexOf('/');
            QByteArray interface = path.left(targetIndex);
            QByteArray relativeTarget = path.right(path.length() - targetIndex);

            Hyperspace::Wave unsetWave;
            unsetWave.setInterface(interface);
            unsetWave.setTarget(relativeTarget);
            unsetWave.setPayload(QByteArray());

            // Is the Gate for this interface already up?
            if (!m_interfaceToGate.contains(interface)) {
                // We save it so that it's forwarded to the Consumer when it comes up
                Cache::instance()->insertOrUpdateConsumerProperty(interface, relativeTarget, unsetWave.serialize());
            } else {
                // Ready to listen to the rebound
                m_waveIdToTransport.insert(unsetWave.id(), transport);
                Cache::instance()->removeConsumerProperty(interface, relativeTarget);
                sendWave(interface, unsetWave, fd);
            }
        }

        // Return something meaningful
        return wave.serialize().length();
    }

    transport->rebound(Hyperspace::Rebound(wave, Hyperspace::ResponseCode::NotImplemented));
    return -1;
}

bool Core::socketOwnsPath(const Hyperspace::Socket *socket, const QByteArray &path) const
{
    // Start from the interface
    int targetIndex = path.indexOf('/', 1);
    QByteArray interface = path.mid(1, targetIndex - 1);

    // Find the right gate
    return (m_interfaceToGate.contains(interface) && m_interfaceToGate.value(interface) == socket);
}

void Core::loadInterfaces()
{
    QHash< QByteArray, Interface > newIntrospection;

    QDir interfacesDirectory(StaticConfig::hyperspaceInterfacesDir());
    interfacesDirectory.setFilter(QDir::Files | QDir::NoDotAndDotDot | QDir::NoSymLinks);
    interfacesDirectory.setNameFilters(QStringList { QStringLiteral("*.json") } );

    QDir interfacesLocalDirectory(QStringLiteral("%1/interfaces").arg(QDir::homePath()));
    interfacesLocalDirectory.setFilter(QDir::Files | QDir::NoDotAndDotDot | QDir::NoSymLinks);
    interfacesLocalDirectory.setNameFilters(QStringList { QStringLiteral("*.json") } );

    QList<QFileInfo> interfaceFiles = interfacesDirectory.entryInfoList();
    interfaceFiles.append(interfacesLocalDirectory.entryInfoList());

    for (const QFileInfo &fileInfo : interfaceFiles) {
        QString filePath = fileInfo.absoluteFilePath();
        qCDebug(hyperdriveCoreDC) << "Loading interface " << filePath;

        QFile interfaceFile(filePath);
        if (!interfaceFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qCWarning(hyperdriveCoreDC) << "Error opening interface file " << filePath;
            continue;
        }
        QJsonDocument interfaceJson = QJsonDocument::fromJson(interfaceFile.readAll());
        if (!interfaceJson.isObject()) {
            qCWarning(hyperdriveCoreDC) << "interface file " << filePath << " doesn't contain a JSON object";
            continue;
        }

        Interface interface = Interface::fromJson(interfaceJson.object());
        if (interface.isValid()) {
            qCDebug(hyperdriveCoreDC) << "Interface loaded " << interface.interface();
            newIntrospection.insert(interface.interface(), interface);
        } else {
            qCWarning(hyperdriveCoreDC) << "Error loading interface " << filePath;
        }
    }

    QByteArrayList oldInterfaces = m_introspection.keys();
    QByteArrayList newInterfaces = newIntrospection.keys();

    // Cleanup cache
    for (QByteArray interface : oldInterfaces) {
        Interface oldInterface = m_introspection.value(interface);
        if (!newInterfaces.contains(interface) ||
            oldInterface.versionMajor() != newIntrospection.value(interface).versionMajor()) {
            qCDebug(hyperdriveCoreDC) << "Interface " << interface << " was removed or had a major update";

            // Clear interface-socket association
            Hyperspace::Socket* socket = m_interfaceToGate.take(interface);
            m_socketToInterface.remove(socket, interface);

            if (oldInterface.interfaceType() == Interface::Type::DataStream) {
                // No cache to cleanup
                continue;
            }

            if (oldInterface.interfaceQuality() == Interface::Quality::Producer) {
                // We need to clear cache here and in the transports
                clearProducerProperties(interface);
            } else {
                // Consumer are cached only here
                Cache::instance()->removeAllConsumerProperties(interface);
            }
        }
    }

    // Finally, replace the introspection with the new one
    m_introspection = newIntrospection;

    Q_EMIT introspectionChanged();
}

void Core::clearProducerProperties(const QByteArray &interface)
{
    QList<CacheMessage> cacheMessagesToSend;

    for (QHash< QByteArray, QByteArray>::const_iterator iter = Cache::instance()->producerProperties(interface).constBegin();
            iter != Cache::instance()->producerProperties(interface).constEnd(); ++iter) {

        CacheMessage cacheMessage;

        QByteArray target = QByteArray("/%1%2").replace("%1", interface).replace("%2", iter.key());
        cacheMessage.setInterfaceType(Interface::Type::Properties);
        cacheMessage.setTarget(target);
        cacheMessage.setPayload(QByteArray());
        cacheMessagesToSend.append(cacheMessage);
    }

    // Multicast to every available transport
    for (Transport *transport : m_transportManager->loadedTransports()) {
        for (const CacheMessage &c : cacheMessagesToSend) {
            transport->cacheMessage(c);
        }
    }

    Cache::instance()->removeAllProducerProperties(interface);
}

}
