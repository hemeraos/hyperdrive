/*
 *
 */

#include "hyperdrivetransportmanager.h"

#include "hyperdrivecore.h"
#include "hyperdriveremotetransport.h"
#include "hyperdrivetransport.h"
#include "hyperdrivelocaltransport_p.h"
#include "hyperdriveprotocol.h"
#include "hyperdriveremotetransportinterface.h"
#include <hyperdriveconfig.h>

#include <HyperspaceCore/Socket>

#include <HemeraCore/CommonOperations>
#include <HemeraCore/Literals>

#include <QtCore/QDebug>
#include <QtCore/QLoggingCategory>
#include <QtCore/QDir>
#include <QtCore/QSettings>
#include <QtCore/QUrl>
#include <QtCore/QUuid>

#include <QtCore/QDebug>

#include "../transports/transports.h"

Q_LOGGING_CATEGORY(hyperdriveTransportManagerDC, "hyperdrive.transport.manager", DEBUG_MESSAGES_DEFAULT_LEVEL)
Q_LOGGING_CATEGORY(transportDC, "hyperdrive.transport", DEBUG_MESSAGES_DEFAULT_LEVEL)

namespace Hyperdrive
{

class LoadLocalTransportOperation : public Hemera::Operation
{
    Q_OBJECT
public:
    explicit LoadLocalTransportOperation(const QString &name, Core *core, QObject* parent = Q_NULLPTR) : Hemera::Operation(parent)
                                                                                                       , m_transport(Q_NULLPTR)
                                                                                                       , m_name(name)
                                                                                                       , m_core(core) {}
    virtual ~LoadLocalTransportOperation() {}

    inline LocalTransport *transport() { return m_transport; }

protected:
    virtual void startImpl() Q_DECL_OVERRIDE Q_DECL_FINAL {
        // Load selectively, then...
        if (m_transport == Q_NULLPTR) {
            setFinishedWithError(Hemera::Literals::literal(Hemera::Literals::Errors::badRequest()),
                                 QStringLiteral("No such transport"));
            return;
        }

        // Inject core into the transport
        m_transport->d_func()->core = m_core;
        // Connect to metadata changes
        QObject::connect(m_core, &Core::introspectionChanged, m_transport, &LocalTransport::introspectionChanged);

        connect(m_transport->init(), &Hemera::Operation::finished, this, [this] (Hemera::Operation *op) {
                if (op->isError()) {
                    setFinishedWithError(op->errorName(), op->errorMessage());
                } else {
                    setFinished();
                }
        });
    }

private:
    LocalTransport *m_transport;
    QString m_name;
    Core *m_core;
};

class TransportManager::Private
{
public:
    QHash< QString, Transport* > transportCache;
    QHash< Hyperspace::Socket*, QString > remoteTransportToName;
    QHash< QUrl, Transport::Features > templateUrls;
    Core *core;
};

TransportManager::TransportManager(Core *parent)
    : AsyncInitObject(parent)
    , d(new Private)
{
    d->core = parent;

    // Connect to introspection changes
    connect(d->core, &Core::introspectionChanged, this, [this] {
        for (QHash< Hyperspace::Socket*, QString >::const_iterator i = d->remoteTransportToName.constBegin(); i != d->remoteTransportToName.constEnd(); ++i) {
            // Send updated introspection
            QByteArray msg;
            QDataStream out(&msg, QIODevice::WriteOnly);

            out << Hyperdrive::Protocol::Control::introspection() << d->core->introspection();
            i.key()->write(msg);
        }
    });
}

TransportManager::~TransportManager()
{
    delete d;
}

void TransportManager::initImpl()
{
    // Upon creation, we should read configuration and cache URL templates for all known transports.
    QDir urlTemplatesDir(QStringLiteral("%1/transports.d").arg(QLatin1String(Hyperdrive::StaticConfig::hyperspaceConfigurationDir())));
    for (const QFileInfo &file : urlTemplatesDir.entryInfoList(QStringList() << QStringLiteral("*.conf"))) {
        QSettings hyperdriveSettings(file.absoluteFilePath(), QSettings::IniFormat);
        hyperdriveSettings.beginGroup(file.baseName()); {
            for (int i = hyperdriveSettings.beginReadArray(QStringLiteral("templateUrls")); i > 0;) {
                --i;
                hyperdriveSettings.setArrayIndex(i);
                d->templateUrls.insert(hyperdriveSettings.value(QStringLiteral("url")).toUrl(),
                                       static_cast< Transport::Features >(hyperdriveSettings.value(QStringLiteral("features")).toUInt()));
            }
            hyperdriveSettings.endArray();
        } hyperdriveSettings.endGroup();
    }

    setReady();
}

Transport *TransportManager::transportFor(const QString &name)
{
    return d->transportCache.value(name, Q_NULLPTR);
}

QList< Transport* > TransportManager::loadedTransports() const
{
    return d->transportCache.values();
}

Hemera::Operation* TransportManager::loadLocalTransport(const QString& name)
{
    return new LoadLocalTransportOperation(name, d->core, this);
}

QHash< QUrl, Transport::Features > TransportManager::templateUrls() const
{
    return d->templateUrls;
}

void TransportManager::addRemoteTransport(Hyperspace::Socket *socket)
{
    connect(socket, &Hyperspace::Socket::disconnected, this, [this, socket] {
        // Cleanup
        qCInfo(hyperdriveTransportManagerDC) << "Transport removed";
        d->transportCache.take(d->remoteTransportToName.value(socket))->deleteLater();

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

                d->remoteTransportToName.insert(socket, name);
                d->transportCache.insert(name, new RemoteTransportInterface(socket, name, this));

                qCDebug(hyperdriveTransportManagerDC) << "Authenticated remote transport successfully: " << name;

                // Let's send the introspection as an additional greeting.
                QByteArray msg;
                QDataStream out(&msg, QIODevice::WriteOnly);

                out << Hyperdrive::Protocol::Control::introspection() << d->core->introspection();
                socket->write(msg);

                Q_EMIT remoteTransportLoaded(d->transportCache.value(name));
            } else if (command == Hyperdrive::Protocol::Control::wave()) {
                QByteArray waveData;
                in >> waveData;
                Hyperspace::Wave wave = Hyperspace::Wave::fromBinary(waveData);

                Transport *transport = d->transportCache.value(d->remoteTransportToName.value(socket));

                if (d->core->routeWave(transport, wave, fd) < 0) {
                    qCWarning(hyperdriveTransportManagerDC) << "Could not route wave!!";
                    transport->rebound(Hyperspace::Rebound(wave, Hyperspace::ResponseCode::InternalError));
                }
            } else if (command == Hyperdrive::Protocol::Control::listHyperdriveInterfaces()) {
                QUuid requestId;
                in >> requestId;

                QByteArray msg;
                QDataStream out(&msg, QIODevice::WriteOnly);

                out << Hyperdrive::Protocol::Control::listHyperdriveInterfaces()
                    << requestId << d->core->listInterfaces();

                socket->write(msg);
            } else if (command == Hyperdrive::Protocol::Control::listGatesForHyperdriveInterfaces()) {
                QUuid requestId;
                QByteArray interface;
                in >> requestId >> interface;

                QByteArray msg;
                QDataStream out(&msg, QIODevice::WriteOnly);

                out << Hyperdrive::Protocol::Control::listGatesForHyperdriveInterfaces()
                    << requestId << d->core->listGatesForInterface(interface);

                socket->write(msg);
            } else if (command == Hyperdrive::Protocol::Control::hyperdriveHasInterface()) {
                QUuid requestId;
                QByteArray interface;
                in >> requestId >> interface;

                QByteArray msg;
                QDataStream out(&msg, QIODevice::WriteOnly);

                out << Hyperdrive::Protocol::Control::hyperdriveHasInterface() << requestId << d->core->hasInterface(interface);
                socket->write(msg);
            } else {
                qCWarning(hyperdriveTransportManagerDC) << "Message malformed!";
                return;
            }
        }
    });

    // Fast init
    connect(socket->init(), &Hemera::Operation::finished, this, [this] (Hemera::Operation *op) {
        if (op->isError()) {
            // TODO: What to do?
            qCWarning(hyperdriveTransportManagerDC) << "Could not initialize socket!";
        }
    });
}

}

#include "hyperdrivetransportmanager.moc"
