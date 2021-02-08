/*
 *
 */

#ifndef HYPERDRIVE_CORE_H
#define HYPERDRIVE_CORE_H

#include <HemeraCore/AsyncInitDBusObject>
#include <HyperspaceCore/Global>
#include <HyperspaceCore/Wave>
#include <HyperspaceCore/Fluctuation>
#include <HyperspaceCore/Rebound>

#include "hyperdriveinterface.h"

class QLocalServer;
class QFileSystemWatcher;
class QTimer;

namespace Hyperspace {
class Socket;

namespace Util {
class BSONStreamReader;
}
}

namespace Hyperdrive {

class Cache;
class DiscoveryManager;
class LocalServer;
class SecurityManager;
class Transport;
class TransportManager;

class Core : public Hemera::AsyncInitDBusObject
{
    Q_OBJECT
    Q_DISABLE_COPY(Core)

public:
    explicit Core(QObject* parent);
    virtual ~Core();

    TransportManager *transportManager();

public Q_SLOTS:
    QHash< QByteArray, Interface > introspection() const;
    QList<QByteArray> listInterfaces() const;
    QList<QByteArray> listGatesForInterface(const QByteArray &interface) const;
    bool hasInterface(const QByteArray &interface) const;
    bool socketOwnsPath(const Hyperspace::Socket *socket, const QByteArray &path) const;

protected:
    virtual void initImpl() Q_DECL_OVERRIDE Q_DECL_FINAL;

Q_SIGNALS:
    void introspectionChanged();

private Q_SLOTS:
    void loadInterfaces();

private:
    qint64 routeWave(Transport *transport, const Hyperspace::Wave &wave, int fd);
    qint64 sendWave(const QByteArray &interface, const Hyperspace::Wave &wave, int fd = -1) const;
    qint64 handleControlWave(Transport *transport, const Hyperspace::Wave &wave, int fd);

    void sendConsumerCache(const QByteArray &interface);

    void clearProducerProperties(const QByteArray &interface);

    bool m_needsBigBang;

    QHash< QByteArray, Hyperspace::Socket* > m_interfaceToGate;
    QHash< quint64, Transport* > m_waveIdToTransport;
    QSet< quint64 > m_dropReboundWaveSet;
    QMultiHash< Hyperspace::Socket*, QByteArray > m_socketToInterface;
    QHash< Hyperspace::Socket*, Hyperspace::Util::BSONStreamReader* > m_gateToBSONStreamReader;
    QHash< QByteArray, Interface > m_introspection;
    QFileSystemWatcher *m_interfaceDirWatcher;
    QTimer *m_interfaceDirTimer;
    LocalServer *m_gatesServer;
    LocalServer *m_transportsServer;
    DiscoveryManager *m_discoveryManager;
    SecurityManager *m_securityManager;
    TransportManager *m_transportManager;

    friend class LocalTransport;
    friend class TransportManager;
};
}

#endif // HYPERDRIVE_CORE_H
