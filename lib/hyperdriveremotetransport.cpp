/*
 *
 */

#include "hyperdriveremotetransport.h"

#include "hyperdrivetransport_p.h"
#include "hyperdriveprotocol.h"
#include "cachemessage.h"

#include <HemeraCore/Literals>

#include <HyperspaceCore/Socket>

#include <QtCore/QDebug>
#include <QtCore/QLoggingCategory>
#include <QtCore/QTimer>
#include <QtCore/QUuid>

Q_LOGGING_CATEGORY(hyperdriveRemoteTransportDC, "hyperdrive.remotetransport", DEBUG_MESSAGES_DEFAULT_LEVEL)

namespace Hyperdrive {

RemoteBoolOperation::RemoteBoolOperation(QObject* parent)
    : Operation(parent)
{
}

RemoteBoolOperation::~RemoteBoolOperation()
{
}

void RemoteBoolOperation::startImpl()
{
}

bool RemoteBoolOperation::result() const
{
    return m_result;
}


RemoteByteArrayListOperation::RemoteByteArrayListOperation(QObject* parent)
    : Operation(parent)
{
}

RemoteByteArrayListOperation::~RemoteByteArrayListOperation()
{
}

QList< QByteArray > RemoteByteArrayListOperation::result() const
{
    return m_result;
}

void RemoteByteArrayListOperation::startImpl()
{
}


class RemoteTransportPrivate : public TransportPrivate
{
    Q_DECLARE_PUBLIC(RemoteTransport)
public:
    RemoteTransportPrivate(RemoteTransport *q) : TransportPrivate(q) {}

    Hyperspace::Socket *socket;
    QByteArray dataBuffer;

    QHash< QByteArray, Interface > introspection;
    QHash< QUuid, RemoteByteArrayListOperation* > baListOperations;
    QHash< QUuid, RemoteBoolOperation* > boolOperations;

    virtual void initHook() Q_DECL_OVERRIDE {
        Q_Q(RemoteTransport);

        // Connect to socket
#ifdef ENABLE_TEST_CODEPATHS
        if (Q_UNLIKELY(qgetenv("RUNNING_AUTOTESTS").toInt() == 1)) {
            qCInfo(hyperdriveRemoteTransportDC) << "Connecting to gate for autotests";

            socket = new Hyperspace::Socket(QStringLiteral("/tmp/hyperdrive-transports-autotests"), q);
        } else {
#endif
            socket = new Hyperspace::Socket(QStringLiteral("/run/hyperdrive/transports"), q);
#ifdef ENABLE_TEST_CODEPATHS
        }
#endif

        // Handle the function pointer overload...
//         void (Hyperspace::Socket::*errorSignal)(Hyperspace::Socket::LocalSocketError) = &Hyperspace::Socket::error;
//         QObject::connect(socket, errorSignal, [this, q] (Hyperspace::Socket::LocalSocketError socketError) {
//             qWarning() << "Internal socket error: " << socketError;
//
//             if (!q->isReady()) {
//                 q->setInitError(Hemera::Literals::literal(Hemera::Literals::Errors::failedRequest()),
//                                 QStringLiteral("Could not connect to Hyperdrive."));
//             }
//         });

        QObject::connect(socket, &Hyperspace::Socket::readyRead, q, [this, q] (const QByteArray &d, int fd) {
            qCDebug(hyperdriveRemoteTransportDC) << "Data read." << d.size();
            QByteArray data = dataBuffer + d;
            dataBuffer.clear();
            QDataStream in(&data, QIODevice::ReadOnly);
            // Sockets may buffer several requests in case they're concurrent: unroll the whole DataStream.
            while (!in.atEnd()) {
                quint8 command;
                in >> command;

                if (command == Hyperdrive::Protocol::Control::rebound()) {
                    // Gate size is a 16 bit unsigned integer
                    QByteArray reboundData;
                    in >> reboundData;
                    Hyperspace::Rebound rebound = Hyperspace::Rebound::fromBinary(reboundData);

                    // We might be unrolling the datastream to a point.
                    if (in.atEnd()) {
                        // If there's no termination here, it means we have to wait for more data
                        qCDebug(hyperdriveRemoteTransportDC) << "Message was too long, caching and waiting for more data.";
                        dataBuffer = data;
                    } else {
                        quint8 verifyTerminator;
                        in >> verifyTerminator;
                        if (verifyTerminator != Hyperdrive::Protocol::Control::messageTerminator()) {
                            qCWarning(hyperdriveRemoteTransportDC) << "Got rebound with no message terminator! Discarding.";
                            break;
                        }

                        q->rebound(rebound, fd);
                    }
                } else if (command == Hyperdrive::Protocol::Control::fluctuation()) {
                    QByteArray fluctuationData;
                    in >> fluctuationData;
                    Hyperspace::Fluctuation fluctuation = Hyperspace::Fluctuation::fromBinary(fluctuationData);
                    q->fluctuation(fluctuation);
                } else if (command == Hyperdrive::Protocol::Control::cacheMessage()) {
                    QByteArray cacheMessageData;
                    in >> cacheMessageData;
                    CacheMessage cacheMessage = CacheMessage::fromBinary(cacheMessageData);
                    q->cacheMessage(cacheMessage);
                } else if (command == Hyperdrive::Protocol::Control::bigBang()) {
                    q->bigBang();
                } else if (command == Hyperdrive::Protocol::Control::listHyperdriveInterfaces()) {
                    QList< QByteArray > interfaces;
                    QUuid requestId;
                    in >> requestId >> interfaces;

                    RemoteByteArrayListOperation *op = baListOperations.take(requestId);
                    if (!op) {
                        qCWarning(hyperdriveRemoteTransportDC) << "Bad request on the remote transport!";
                        return;
                    }

                    op->m_result = interfaces;
                    op->setFinished();
                } else if (command == Hyperdrive::Protocol::Control::listGatesForHyperdriveInterfaces()) {
                    QList< QByteArray > interfaces;
                    QUuid requestId;
                    in >> requestId >> interfaces;

                    RemoteByteArrayListOperation *op = baListOperations.take(requestId);
                    if (!op) {
                        qCWarning(hyperdriveRemoteTransportDC) << "Bad request on the remote transport!";
                        return;
                    }

                    op->m_result = interfaces;
                    op->setFinished();
                } else if (command == Hyperdrive::Protocol::Control::hyperdriveHasInterface()) {
                    bool ret;
                    QUuid requestId;
                    in >> requestId >> ret;

                    RemoteBoolOperation *op = boolOperations.take(requestId);
                    if (!op) {
                        qCWarning(hyperdriveRemoteTransportDC) << "Bad request on the remote transport!";
                        return;
                    }

                    op->m_result = ret;
                    op->setFinished();
                } else if (command == Hyperdrive::Protocol::Control::introspection()) {
                    in >> introspection;

                    Q_EMIT q->introspectionChanged();
                } else {
                    qCWarning(hyperdriveRemoteTransportDC) << "Message malformed!" << data.size() << data.toHex();
                    return;
                }
            }
        });

        // Monitor socket
        QObject::connect(socket->init(), &Hemera::Operation::finished, q, [this, q] (Hemera::Operation *op) {
            if (!op->isError() && !q->isReady()) {
                // We are ready! Now, we should send the transport name to finalize the handshake.
                // To do that, we need the event loop to settle down before we can write to the socket.
                QTimer::singleShot(0, q, SLOT(sendName()));
            } else if (op->isError()) {
                // Manage errors...
            }
        });
    };

    void sendName() {
        Q_Q(RemoteTransport);

        QByteArray msg;
        QDataStream out(&msg, QIODevice::WriteOnly);

        out << Protocol::Control::nameExchange() << name;
        qint64 sw = socket->write(msg);

        if (Q_UNLIKELY(sw != msg.size())) {
            q->setInitError(Hemera::Literals::literal(Hemera::Literals::Errors::failedRequest()),
                            QStringLiteral("Could not exchange name over the transport."));
        } else {
            TransportPrivate::initHook();
        }
    }
};

RemoteTransport::RemoteTransport(const QString& name, QObject* parent)
    : Transport(*new RemoteTransportPrivate(this), name, parent)
{
}

RemoteTransport::~RemoteTransport()
{
}

QHash< QByteArray, Interface > RemoteTransport::introspection() const
{
    Q_D(const RemoteTransport);
    return d->introspection;
}

void RemoteTransport::routeWave(const Hyperspace::Wave &wave, int fd)
{
    Q_D(RemoteTransport);

    QByteArray msg;
    QDataStream out(&msg, QIODevice::WriteOnly);
    out << Hyperdrive::Protocol::Control::wave() << wave.serialize();
    d->socket->write(msg, fd);
}

RemoteByteArrayListOperation *RemoteTransport::listHyperdriveInterfaces()
{
    Q_D(RemoteTransport);

    RemoteByteArrayListOperation *ret = new RemoteByteArrayListOperation(this);
    QUuid requestId = QUuid::createUuid();
    d->baListOperations.insert(requestId, ret);

    QByteArray msg;
    QDataStream out(&msg, QIODevice::WriteOnly);
    out << Hyperdrive::Protocol::Control::listHyperdriveInterfaces() << requestId;
    d->socket->write(msg);

    return ret;
}

RemoteByteArrayListOperation *RemoteTransport::listGatesForHyperdriveInterface(const QByteArray& interface)
{
    Q_D(RemoteTransport);

    RemoteByteArrayListOperation *ret = new RemoteByteArrayListOperation(this);
    QUuid requestId = QUuid::createUuid();
    d->baListOperations.insert(requestId, ret);

    QByteArray msg;
    QDataStream out(&msg, QIODevice::WriteOnly);
    out << Hyperdrive::Protocol::Control::listGatesForHyperdriveInterfaces() << requestId << interface;
    d->socket->write(msg);

    return ret;
}

RemoteBoolOperation *RemoteTransport::hyperdriveHasInterface(const QByteArray& interface)
{
    Q_D(RemoteTransport);

    RemoteBoolOperation *ret = new RemoteBoolOperation(this);
    QUuid requestId = QUuid::createUuid();
    d->boolOperations.insert(requestId, ret);

    QByteArray msg;
    QDataStream out(&msg, QIODevice::WriteOnly);
    out << Hyperdrive::Protocol::Control::hyperdriveHasInterface() << requestId << interface;
    d->socket->write(msg);

    return ret;
}

}

#include "moc_hyperdriveremotetransport.cpp"
