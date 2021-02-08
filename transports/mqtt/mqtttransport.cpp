/*
 *
 */

#include "mqtttransport.h"

#include <HemeraCore/Literals>
#include <HemeraCore/Operation>

#include <QtCore/QByteArray>
#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QTimer>
#include <QtCore/QStringList>
#include <QtCore/QSocketNotifier>
#include <QtCore/QDebug>
#include <QtCore/QLoggingCategory>
#include <QtCore/QSettings>
#include <QtCore/QVariantMap>

#include <QtCore/QFuture>
#include <QtCore/QFutureWatcher>
#include <QtCore/QFile>
#include <QtConcurrent/QtConcurrentRun>

#include <systemd/sd-daemon.h>

#include "transports.h"
#include "hyperdrivemqttclientwrapper.h"
#include <hyperdriveprotocol.h>
#include <hyperdriveconfig.h>

#include <signal.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>

#define CONNECTION_TIMEOUT 15000

Q_LOGGING_CATEGORY(mqttTransportDC, "hyperdrive.transport.mqtt", DEBUG_MESSAGES_DEFAULT_LEVEL)

namespace Hyperdrive
{

MQTTTransport::MQTTTransport(QObject* parent)
    : RemoteTransport(QStringLiteral("MQTT"), parent)
{
    connect(this, &MQTTTransport::introspectionChanged, this, &MQTTTransport::publishIntrospection);
}

MQTTTransport::~MQTTTransport()
{
}

void MQTTTransport::initImpl()
{
    // Load from configuration
    QDir confDir(QLatin1String(Hyperdrive::StaticConfig::hyperspaceConfigurationDir()));
    for (const QString &conf : confDir.entryList(QStringList() << QStringLiteral("transport-mqtt-*.conf"))) {
        QSettings settings(confDir.absoluteFilePath(conf), QSettings::IniFormat);
        settings.beginGroup(QStringLiteral("MQTTTransport")); {
            MQTTClientWrapper *c = new MQTTClientWrapper(settings.value(QStringLiteral("hostUrl")).toUrl(), this);
            c->setCleanSession(settings.value(QStringLiteral("cleanSession")).toBool());
            c->setPublishQoS(static_cast<MQTTClientWrapper::MQTTQoS>(settings.value(QStringLiteral("publishQoS")).toInt()));
            c->setSubscribeQoS(static_cast<MQTTClientWrapper::MQTTQoS>(settings.value(QStringLiteral("subscribeQoS")).toInt()));
            c->setKeepAlive(settings.value(QStringLiteral("keepAlive")).toInt());

            connect(c->init(), &Hemera::Operation::finished, c, &MQTTClientWrapper::connectToBroker);
            connect(c, &MQTTClientWrapper::statusChanged, [this, c]  {
                if (c->status() == MQTTClientWrapper::ConnectedStatus) {
                    // We need to setup again the subscriptions, unless we have a persistent session on the other end.
                    setupClientSubscriptions(c);
                    // And publish the introspection.
                    publishIntrospectionForClient(c);
                }
            });

            m_clients.append(c);
        } settings.endGroup();
    }

    setReady();
}

void MQTTTransport::setupClientSubscriptions(MQTTClientWrapper* client)
{
    // Setup subscriptions. This way we can listen to "waves" (as in, publish)
    client->subscribe(client->rootClientTopic() + "/#");
}

void MQTTTransport::rebound(const Hyperspace::Rebound& r, int fd)
{
    Hyperspace::Rebound rebound = r;

    // TODO: What to do here?
}

void MQTTTransport::fluctuation(const Hyperspace::Fluctuation &fluctuation)
{
    qCDebug(mqttTransportDC) << "Received fluctuation from: " << fluctuation.target() << fluctuation.payload();
}

void MQTTTransport::cacheMessage(const CacheMessage &cacheMessage)
{
    qCDebug(mqttTransportDC) << "Received cache message from: " << cacheMessage.target() << cacheMessage.payload();
    // Publish it!
    for (MQTTClientWrapper *c : m_clients) {
        // Convert to string representation regardless
        c->publish(c->rootClientTopic() + cacheMessage.target(), cacheMessage.payload());
    }}

void MQTTTransport::bigBang()
{
    qCDebug(mqttTransportDC) << "Received bigBang";
}

void MQTTTransport::publishIntrospection()
{
    publishIntrospectionForClient(nullptr);
}

void MQTTTransport::publishIntrospectionForClient(MQTTClientWrapper *c)
{
    qCDebug(mqttTransportDC) << "Publishing introspection!";

    // Create a string representation
    QByteArray payload;
    for (QHash< QByteArray, Hyperdrive::Interface >::const_iterator i = introspection().constBegin(); i != introspection().constEnd(); ++i) {
        payload.append(i.key());
        payload.append(':');
        payload.append(i.value().versionMajor());
        payload.append(':');
        payload.append(i.value().versionMinor());
        payload.append('\n');
    }

    if (c) {
        c->publish(c->rootClientTopic(), payload, Hyperdrive::MQTTClientWrapper::DefaultQoS, true);
    } else {
        for (MQTTClientWrapper *c : m_clients) {
            c->publish(c->rootClientTopic(), payload, Hyperdrive::MQTTClientWrapper::DefaultQoS, true);
        }
    }
}

}

TRANSPORT_MAIN(Hyperdrive::MQTTTransport, MQTT, 1.0.0)
