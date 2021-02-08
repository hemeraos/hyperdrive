/*
 *
 */

#include "hyperdriveremotediscoveryservice_p.h"

#include "hyperdrivecore.h"
#include "hyperdrivediscoverymanager.h"

#include "hyperdriveprotocol.h"

#include <hyperdriveconfig.h>

#include <HemeraCore/Literals>

#include <HyperspaceCore/Socket>

#include <QtCore/QDebug>
#include <QtCore/QLoggingCategory>
#include <QtCore/QTimer>
#include <QtCore/QUrl>
#include <QtCore/QUuid>

Q_LOGGING_CATEGORY(hyperdriveRemoteDiscoveryServiceDC, "hyperdrive.remotediscoveryservice", DEBUG_MESSAGES_DEFAULT_LEVEL)

namespace Hyperdrive {

RemoteUrlFeaturesOperation::RemoteUrlFeaturesOperation(QObject* parent)
    : Operation(parent)
{
}

RemoteUrlFeaturesOperation::~RemoteUrlFeaturesOperation()
{
}

void RemoteUrlFeaturesOperation::startImpl()
{
}

QHash< QUrl, Transport::Features > RemoteUrlFeaturesOperation::result() const
{
    return m_result;
}

void RemoteDiscoveryServicePrivate::initHook()
{
    Q_Q(RemoteDiscoveryService);

    // Connect to socket
#ifdef ENABLE_TEST_CODEPATHS
    if (Q_UNLIKELY(qgetenv("RUNNING_AUTOTESTS").toInt() == 1)) {
        qCDebug(hyperdriveRemoteDiscoveryServiceDC) << "Connecting to socket for autotests";
        socket = new Hyperspace::Socket(QStringLiteral("/tmp/hyperdrive/discovery/services-autotests"), q);
    } else {
#endif
        socket = new Hyperspace::Socket(QLatin1String(Hyperdrive::StaticConfig::hyperdriveDiscoveryServicesSocketPath()), q);
#ifdef ENABLE_TEST_CODEPATHS
    }
#endif

    // Handle the function pointer overload...
//     void (Hyperspace::Socket::*errorSignal)(Hyperspace::Socket::LocalSocketError) = &Hyperspace::Socket::error;
//     QObject::connect(socket, errorSignal, [this, q] (Hyperspace::Socket::LocalSocketError socketError) {
//         qCWarning(hyperdriveRemoteDiscoveryServiceDC) << "Internal socket error: " << socketError;
//
//         if (!q->isReady()) {
//             q->setInitError(Hemera::Literals::literal(Hemera::Literals::Errors::failedRequest()),
//                             QStringLiteral("Could not connect to Hyperdrive."));
//         }
//     });

    QObject::connect(socket, &Hyperspace::Socket::readyRead, q, [this, q] (QByteArray data, int fd) {
        QDataStream in(&data, QIODevice::ReadOnly);
        // Sockets may buffer several requests in case they're concurrent: unroll the whole DataStream.
        while (!in.atEnd()) {
            quint8 command;
            in >> command;

            if (command == Hyperspace::Protocol::Discovery::introspectRequest()) {
                QByteArray interface;
                in >> interface;
                q->introspectInterface(interface);
            } else if (command == Hyperspace::Protocol::Discovery::scan()) {
                QList< QByteArray > interfaces;
                in >> interfaces;
                q->scanInterfaces(interfaces);
            } else if (command == Hyperdrive::Protocol::Discovery::interfacesRegistered()) {
                QList< QByteArray > interfaces;
                in >> interfaces;
                q->onLocalInterfacesRegistered(interfaces);
            } else if (command == Hyperdrive::Protocol::Discovery::interfacesUnregistered()) {
                QList< QByteArray > interfaces;
                in >> interfaces;
                q->onLocalInterfacesUnregistered(interfaces);
            } else if (command == Hyperdrive::Protocol::Control::listHyperdriveInterfaces()) {
                QList< QByteArray > interfaces;
                QUuid requestId;
                in >> requestId >> interfaces;

                RemoteByteArrayListOperation *op = baListOperations.take(requestId);
                if (!op) {
                    qCWarning(hyperdriveRemoteDiscoveryServiceDC) << "Bad request on the remote discovery service!";
                    return;
                }

                op->m_result = interfaces;
                op->setFinished();
            } else if (command == Hyperdrive::Protocol::Control::transportsTemplateUrls()) {
                QHash< QUrl, uint > templateUrls;
                QUuid requestId;
                in >> requestId >> templateUrls;

                RemoteUrlFeaturesOperation *op = ufOperations.take(requestId);
                if (!op) {
                    qCWarning(hyperdriveRemoteDiscoveryServiceDC) << "Bad request on the remote discovery service!";
                    return;
                }

                op->m_result.clear();
                for (QHash< QUrl, uint >::const_iterator i = templateUrls.constBegin(); i != templateUrls.constEnd(); ++i) {
                    op->m_result.insert(i.key(), static_cast< Transport::Features >(i.value()));
                }
                op->setFinished();
            } else {
                qCWarning(hyperdriveRemoteDiscoveryServiceDC) << "Discovery message malformed!";
            }
        }
    });

    // Monitor socket
    QObject::connect(socket->init(), &Hemera::Operation::finished, q, [this, q] (Hemera::Operation *op) {
        if (!op->isError() && !q->isReady()) {
            // We are ready! Now, we should send the transport name to finalize the handshake.
            // To do that, we need the event loop to settle down before we can write to the socket.
            QTimer::singleShot(0, q, SLOT(sendName()));
        }
    });
};

void RemoteDiscoveryServicePrivate::sendName() {
    Q_Q(RemoteDiscoveryService);

    QByteArray msg;
    QDataStream out(&msg, QIODevice::WriteOnly);

    out << Protocol::Control::nameExchange() << name;
    qint64 sw = socket->write(msg);

    if (Q_UNLIKELY(sw != msg.size())) {
        q->setInitError(Hemera::Literals::literal(Hemera::Literals::Errors::failedRequest()),
                        QStringLiteral("Could not exchange name over the transport."));
    } else {
        DiscoveryServicePrivate::initHook();
    }
}

RemoteDiscoveryService::RemoteDiscoveryService(QObject* parent)
    : DiscoveryService(*new RemoteDiscoveryServicePrivate(this), parent)
{
}

RemoteDiscoveryService::~RemoteDiscoveryService()
{
}

RemoteByteArrayListOperation *RemoteDiscoveryService::localInterfaces()
{
    Q_D(RemoteDiscoveryService);

    RemoteByteArrayListOperation *ret = new RemoteByteArrayListOperation(this);
    QUuid requestId = QUuid::createUuid();
    d->baListOperations.insert(requestId, ret);

    QByteArray msg;
    QDataStream out(&msg, QIODevice::WriteOnly);
    out << Hyperdrive::Protocol::Control::listHyperdriveInterfaces() << requestId;
    d->socket->write(msg);

    return ret;
}

RemoteUrlFeaturesOperation *RemoteDiscoveryService::transportsTemplateUrls()
{
    Q_D(RemoteDiscoveryService);

    RemoteUrlFeaturesOperation *ret = new RemoteUrlFeaturesOperation(this);
    QUuid requestId = QUuid::createUuid();
    d->ufOperations.insert(requestId, ret);

    QByteArray msg;
    QDataStream out(&msg, QIODevice::WriteOnly);
    out << Hyperdrive::Protocol::Control::transportsTemplateUrls() << requestId;
    d->socket->write(msg);

    return ret;
}

void RemoteDiscoveryService::setInterfacesAnnounced(const QList< QByteArray >& interfaces)
{
    Q_D(RemoteDiscoveryService);

    QByteArray msg;
    QDataStream out(&msg, QIODevice::WriteOnly);

    out << Hyperspace::Protocol::Discovery::announced() << interfaces;

    d->socket->write(msg);
}

void RemoteDiscoveryService::setInterfacesExpired(const QList< QByteArray >& interfaces)
{
    Q_D(RemoteDiscoveryService);

    QByteArray msg;
    QDataStream out(&msg, QIODevice::WriteOnly);

    out << Hyperspace::Protocol::Discovery::expired() << interfaces;

    d->socket->write(msg);
}

void RemoteDiscoveryService::setInterfacesPurged(const QList< QByteArray >& interfaces)
{
    Q_D(RemoteDiscoveryService);

    QByteArray msg;
    QDataStream out(&msg, QIODevice::WriteOnly);

    out << Hyperspace::Protocol::Discovery::purged() << interfaces;

    d->socket->write(msg);
}

void RemoteDiscoveryService::setInterfaceIntrospected(const QByteArray& interface, const QList< QByteArray >& servicesUrl)
{
    Q_D(RemoteDiscoveryService);

    QByteArray msg;
    QDataStream out(&msg, QIODevice::WriteOnly);

    out << Hyperspace::Protocol::Discovery::introspectReply() << interface << servicesUrl;

    d->socket->write(msg);
}

}

#include "moc_hyperdriveremotediscoveryservice.cpp"
