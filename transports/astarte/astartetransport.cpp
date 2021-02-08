/*
 *
 */

#include "astartetransport.h"

#include "astartetransportcache.h"

#include <HemeraCore/DeviceManagement>
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

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

#include <QtCore/QFuture>
#include <QtCore/QFutureWatcher>
#include <QtCore/QFile>
#include <QtConcurrent/QtConcurrentRun>

#include <systemd/sd-daemon.h>

#include "transports.h"
#include <hyperdriveprotocol.h>
#include <hyperdriveconfig.h>
#include <hyperdriveutils.h>

#include <astartegatewayendpoint.h>
#include <astartehttpendpoint.h>

#include <HyperspaceProducerConsumer/ProducerAbstractInterface>

#include <signal.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>

#define CONNECTION_RETRY_INTERVAL 15000
#define PAIRING_RETRY_INTERVAL (5 * 60 * 1000)

#define METHOD_WRITE "WRITE"
#define METHOD_ERROR "ERROR"

#define CERTIFICATE_RENEWAL_DAYS 8

Q_LOGGING_CATEGORY(astarteTransportDC, "hyperdrive.transport.astarte", DEBUG_MESSAGES_DEFAULT_LEVEL)

namespace Hyperdrive
{

AstarteTransport::AstarteTransport(QObject* parent)
    : RemoteTransport(QStringLiteral("Astarte"), parent)
    , m_rebootTimer(new QTimer(this))
    , m_rebootWhenConnectionFails(false)
    , m_rebootDelayMinutes(600)
    , m_inFlightIntrospectionMessageId(-1)
{
    qRegisterMetaType<MQTTClientWrapper::Status>();
    connect(this, &AstarteTransport::introspectionChanged, this, [this] {
            publishIntrospection();
            setupClientSubscriptions();
    });
}

AstarteTransport::~AstarteTransport()
{
}

void AstarteTransport::initImpl()
{
    setParts(2);

    Hyperdrive::Utils::seedRNG();

    // When we are ready to go, we setup MQTT
    connect(this, &Hemera::AsyncInitObject::ready, this, &AstarteTransport::setupMqtt);

    connect(AstarteTransportCache::instance()->init(), &Hemera::Operation::finished, this, [this] (Hemera::Operation *op) {
            if (op->isError()) {
                setInitError(op->errorName(), op->errorMessage());
            } else {
                setOnePartIsReady();
            }
    });

    QSettings syncSettings(QStringLiteral("%1/transportStatus.conf").arg(QDir::homePath()), QSettings::IniFormat);
    m_synced = syncSettings.value(QStringLiteral("isSynced"), false).toBool();
    m_lastSentIntrospection = syncSettings.value(QStringLiteral("lastSentIntrospection"), QByteArray()).toByteArray();

    // Load from configuration
    QDir confDir(QLatin1String(Hyperdrive::StaticConfig::hyperspaceConfigurationDir()));
    for (const QString &conf : confDir.entryList(QStringList() << QStringLiteral("transport-astarte.conf"))) {
        QSettings settings(confDir.absoluteFilePath(conf), QSettings::IniFormat);
        settings.beginGroup(QStringLiteral("AstarteTransport")); {
            // Gateway or real endpoint?
            if (settings.childKeys().contains(QStringLiteral("endpoint"))) {
                m_astarteEndpoint = new Astarte::HTTPEndpoint(settings.value(QStringLiteral("endpoint")).toUrl(), QSslConfiguration::defaultConfiguration(), this);
            } else {
                m_astarteEndpoint = new Astarte::GatewayEndpoint(settings.value(QStringLiteral("gateway")).toUrl(), this);
            }

            m_rebootWhenConnectionFails = settings.value(QStringLiteral("rebootWhenConnectionFails"), false).toBool();
            m_rebootDelayMinutes = settings.value(QStringLiteral("rebootDelayMinutes"), 600).toInt();
            m_rebootTimer->setTimerType(Qt::VeryCoarseTimer);
            int randomizedRebootDelayms = Hyperdrive::Utils::randomizedInterval(m_rebootDelayMinutes * 60 * 1000, 0.1);
            m_rebootTimer->setInterval(randomizedRebootDelayms);

            if (m_rebootWhenConnectionFails) {
                qCDebug(astarteTransportDC) << "Activating the reboot timer with delay " << (randomizedRebootDelayms / (60 * 1000)) << " minutes";
                m_rebootTimer->start();
            }
            connect(m_rebootTimer, &QTimer::timeout, this, &AstarteTransport::handleRebootTimerTimeout);

            connect(m_astarteEndpoint->init(), &Hemera::Operation::finished, this, [this] (Hemera::Operation *op) {
                if (op->isError()) {
                    // TODO
                    return;
                }

                if (!m_mqttBroker.isNull()) {
                    m_mqttBroker->disconnectFromBroker();
                    m_mqttBroker->deleteLater();
                }

                // Are we paired?
                if (!m_astarteEndpoint->isPaired()) {
                    startPairing(false);
                } else {
                    setOnePartIsReady();
                }
            });
        } settings.endGroup();
    }
}

void AstarteTransport::startPairing(bool forcedPairing) {
    connect(m_astarteEndpoint->pair(forcedPairing), &Hemera::Operation::finished, this, [this, forcedPairing] (Hemera::Operation *pOp) {
        if (pOp->isError()) {
            int retryInterval = Hyperdrive::Utils::randomizedInterval(PAIRING_RETRY_INTERVAL, 1.0);
            qCWarning(astarteTransportDC) << "Pairing failed!!" << pOp->errorMessage() << ", retrying in " << (retryInterval / 1000) << " seconds";
            QTimer::singleShot(retryInterval, this, [this, forcedPairing] {
                startPairing(forcedPairing);
            });
            return;
        } else {
            if (forcedPairing) {
                setupMqtt();
            } else {
                setOnePartIsReady();
            }
        }
    });
}

void AstarteTransport::setupMqtt()
{
    // Good. Let's set up our MQTT broker.
    m_mqttBroker = m_astarteEndpoint->createMqttClientWrapper();

    if (m_mqttBroker.isNull()) {
        qCWarning(astarteTransportDC) << "Could not create the MQTT client!!";
        return;
    }

    if (m_mqttBroker->clientCertificateExpiry().isValid() &&
        QDateTime::currentDateTime().daysTo(m_mqttBroker->clientCertificateExpiry()) <= CERTIFICATE_RENEWAL_DAYS) {
        forceNewPairing();
        return;
    }

    connect(m_mqttBroker->init(), &Hemera::Operation::finished, this, [this] {
        m_mqttBroker->setKeepAlive(60);
        m_mqttBroker->connectToBroker();
    });
    connect(m_mqttBroker, &MQTTClientWrapper::statusChanged, this, &AstarteTransport::onStatusChanged);
    connect(m_mqttBroker, &MQTTClientWrapper::messageReceived, this, &AstarteTransport::onMQTTMessageReceived);
    connect(m_mqttBroker, &MQTTClientWrapper::publishConfirmed, this, &AstarteTransport::onPublishConfirmed);
    connect(m_mqttBroker, &MQTTClientWrapper::connackTimeout, this, &AstarteTransport::handleConnackTimeout);
    connect(m_mqttBroker, &MQTTClientWrapper::connectionFailed, this, &AstarteTransport::handleConnectionFailed);
}

void AstarteTransport::onMQTTMessageReceived(const QByteArray& topic, const QByteArray& payload)
{
    // Normalize the message
    if (!topic.startsWith(m_mqttBroker->rootClientTopic())) {
        qCWarning(astarteTransportDC) << "Received MQTT message on topic" << topic << ", which does not match the device base hierarchy!";
        return;
    }

    QByteArray relativeTopic = topic;
    relativeTopic.remove(0, m_mqttBroker->rootClientTopic().length());

    // Prepare a write wave
    Hyperspace::Wave w;
    w.setMethod(METHOD_WRITE);
    w.setTarget(relativeTopic);
    w.setPayload(payload);
    m_waveStorage.insert(w.id(), w);
    qCDebug(astarteTransportDC) << "Sending wave" << w.method() << w.target();
    routeWave(w, -1);
}

void AstarteTransport::setupClientSubscriptions()
{
    if (m_mqttBroker.isNull()) {
        qCWarning(astarteTransportDC) << "Can't publish subscriptions, broker is null";
        return;
    }
    // Setup subscriptions to control interface
    m_mqttBroker->subscribe(m_mqttBroker->rootClientTopic() + "/control/#", MQTTClientWrapper::ExactlyOnceQoS);
    // Setup subscriptions to interfaces where we can receive data
    for (QHash< QByteArray, Hyperdrive::Interface >::const_iterator i = introspection().constBegin(); i != introspection().constEnd(); ++i) {
        if (i.value().interfaceQuality() == Interface::Quality::Consumer) {
            // Subscribe to the interface itself
            m_mqttBroker->subscribe(m_mqttBroker->rootClientTopic() + "/" + i.value().interface(), MQTTClientWrapper::ExactlyOnceQoS);
            // Subscribe to the interface properties
            m_mqttBroker->subscribe(m_mqttBroker->rootClientTopic() + "/" + i.value().interface() + "/#", MQTTClientWrapper::ExactlyOnceQoS);
        }
    }
}

void AstarteTransport::sendProperties()
{
    for (QHash< QByteArray, QByteArray >::const_iterator i = AstarteTransportCache::instance()->allPersistentEntries().constBegin();
         i != AstarteTransportCache::instance()->allPersistentEntries().constEnd();
         ++i) {

        // Recreate the cacheMessage
        CacheMessage c;
        c.setTarget(i.key());
        c.setPayload(i.value());
        c.setInterfaceType(Hyperdrive::Interface::Type::Properties);

        if (m_mqttBroker.isNull()) {
            handleFailedPublish(c);
            continue;
        }

        int rc = m_mqttBroker->publish(m_mqttBroker->rootClientTopic() + i.key(), i.value(), MQTTClientWrapper::ExactlyOnceQoS);
        if (rc < 0) {
            // If it's < 0, it's an error
            handleFailedPublish(c);
        } else {
            // Otherwise, it's the messageId
            qCInfo(astarteTransportDC) << "Inserting in-flight message id " << rc;
            AstarteTransportCache::instance()->addInFlightEntry(rc, c);
        }
    }
}

void AstarteTransport::resendFailedMessages()
{
    QList<int> ids = AstarteTransportCache::instance()->allRetryIds();
    for (int id: ids) {
        CacheMessage failedMessage = AstarteTransportCache::instance()->takeRetryEntry(id);
        // Call cache message function with the failed message
        cacheMessage(failedMessage);
    }
}

void AstarteTransport::rebound(const Hyperspace::Rebound& r, int fd)
{
    Hyperspace::Rebound rebound = r;

    // FIXME: We should just trigger errors here.
    return;

    if (!m_waveStorage.contains(r.id())) {
        qCWarning(astarteTransportDC) << "Got a rebound with id" << r.id() << "which does not match any routing rules!";
        return;
    }

    Hyperspace::Wave w = m_waveStorage.take(r.id());
    QByteArray waveTarget = w.target();

    if (m_commandTree.contains(r.id())) {
        QByteArray commandId = m_commandTree.take(r.id());
        m_mqttBroker->publish(m_mqttBroker->rootClientTopic() + "/response" + waveTarget,
                              commandId + "," + QByteArray::number(static_cast<quint16>(r.response())));
    }

    if (r.response() != Hyperspace::ResponseCode::OK) {
        // TODO: How do we handle failed requests?
        qCWarning(astarteTransportDC) << "Wave failed!" << static_cast<quint16>(r.response());
        return;
    }

    if (!r.payload().isEmpty()) {
        m_mqttBroker->publish(m_mqttBroker->rootClientTopic() + waveTarget, r.payload());
    }
}

void AstarteTransport::fluctuation(const Hyperspace::Fluctuation &fluctuation)
{
    qCDebug(astarteTransportDC) << "Received fluctuation from: " << fluctuation.target() << fluctuation.payload();
}

void AstarteTransport::cacheMessage(const CacheMessage &cacheMessage)
{
    qCDebug(astarteTransportDC) << "Received cacheMessage from: " << cacheMessage.target() << cacheMessage.payload();
    if (m_mqttBroker.isNull()) {
        handleFailedPublish(cacheMessage);
        return;
    }

    int rc;

    switch (cacheMessage.interfaceType()) {
        case Hyperdrive::Interface::Type::Properties: {
            if (AstarteTransportCache::instance()->isCached(cacheMessage.target())
                    && AstarteTransportCache::instance()->persistentEntry(cacheMessage.target()) == cacheMessage.payload()) {

                qCDebug(astarteTransportDC) << cacheMessage.target() << "is not changed, not publishing it again";
                // We consider it delivered, so remove it from the DB
                AstarteTransportCache::instance()->removeFromDatabase(cacheMessage);
                return;
            }

            rc = m_mqttBroker->publish(m_mqttBroker->rootClientTopic() + cacheMessage.target(), cacheMessage.payload(), MQTTClientWrapper::ExactlyOnceQoS);
            break;
        }

        case Hyperdrive::Interface::Type::DataStream: {
            Hyperspace::Reliability reliability = static_cast<Hyperspace::Reliability>(cacheMessage.attributes().value("reliability").toInt());
            switch (reliability) {
                case (Hyperspace::Reliability::Guaranteed):
                    rc = m_mqttBroker->publish(m_mqttBroker->rootClientTopic() + cacheMessage.target(), cacheMessage.payload(), MQTTClientWrapper::AtLeastOnceQoS);
                    break;
                case (Hyperspace::Reliability::Unique):
                    rc = m_mqttBroker->publish(m_mqttBroker->rootClientTopic() + cacheMessage.target(), cacheMessage.payload(), MQTTClientWrapper::ExactlyOnceQoS);
                    break;
                default:
                    // Default Unreliable
                    rc = m_mqttBroker->publish(m_mqttBroker->rootClientTopic() + cacheMessage.target(), cacheMessage.payload(), MQTTClientWrapper::AtMostOnceQoS);
                    break;
            }
            break;
        }

        default: {
            qCDebug(astarteTransportDC) << "Unsupported interfaceType";
            break;
        }
    }

    if (rc < 0) {
        // If it's < 0, it's an error
        handleFailedPublish(cacheMessage);
    } else {
        // Otherwise, it's the messageId
        qCInfo(astarteTransportDC) << "Inserting in-flight message id " << rc;
        AstarteTransportCache::instance()->addInFlightEntry(rc, cacheMessage);
    }
}

void AstarteTransport::forceNewPairing()
{
    // Operation is error, certificate is invalid
    qCWarning(astarteTransportDC) << "Forcing new pairing";

    if (!m_mqttBroker.isNull()) {
        m_mqttBroker->disconnectFromBroker();
        m_mqttBroker->deleteLater();
    }
    // Reset the cache
    AstarteTransportCache::instance()->resetInFlightEntries();

    startPairing(true);
}

void AstarteTransport::onStatusChanged(MQTTClientWrapper::Status status)
{
    if (status == MQTTClientWrapper::ConnectedStatus) {
        // We're connected, stop the reboot timer
        qCDebug(astarteTransportDC) << "Connected, stopping the reboot timer";
        m_rebootTimer->stop();
        if (!m_mqttBroker->sessionPresent() || !m_synced) {
            // We're desynced
            bigBang();
        } else if (m_lastSentIntrospection != introspectionString()) {
            qCDebug(astarteTransportDC) << "Introspection changed while offline, was:" << m_lastSentIntrospection;
            publishIntrospection();
            setupClientSubscriptions();
        }

        // Resend the messages that failed to be published
        resendFailedMessages();
    } else {
        // If we are in every other state, we start the reboot timer (if needed)
        if (m_rebootWhenConnectionFails && !m_rebootTimer->isActive()) {
            qCDebug(astarteTransportDC) << "Not connected state, restarting the reboot timer";
            m_rebootTimer->start();
        }
    }
}

void AstarteTransport::handleConnectionFailed()
{
    int retryInterval = Hyperdrive::Utils::randomizedInterval(CONNECTION_RETRY_INTERVAL, 1.0);
    qCInfo(astarteTransportDC) << "Connection failed, trying to reconnect to the broker in " << (retryInterval / 1000) << " seconds";
    QTimer::singleShot(retryInterval, m_mqttBroker, &MQTTClientWrapper::connectToBroker);
}

void AstarteTransport::handleConnackTimeout()
{
    qCWarning(astarteTransportDC) << "CONNACK timeout, verifying certificate";

    connect(m_astarteEndpoint->verifyCertificate(), &Hemera::Operation::finished, this, [this] (Hemera::Operation *vOp) {
        if (vOp->isError()) {
            qCWarning(astarteTransportDC) << vOp->errorMessage();
            forceNewPairing();
        }
    });
}

void AstarteTransport::handleRebootTimerTimeout()
{
    if (m_rebootWhenConnectionFails) {
        qCWarning(astarteTransportDC) << "Connection with Astarte was lost for too long, rebooting!";
        QTimer::singleShot(2100, this, [this] {
            // Don't be too rude
            ::sync();
            Hemera::DeviceManagement::reboot();
        });
    }
}

void AstarteTransport::handleFailedPublish(const CacheMessage &cacheMessage)
{
    qCWarning(astarteTransportDC) << "Can't publish for target" << cacheMessage.target();
    if (cacheMessage.attributes().value("retention").toInt() == static_cast<int>(Hyperspace::Retention::Discard)) {
        // Prepare an error wave
        Hyperspace::Wave w;
        w.setMethod(METHOD_ERROR);
        w.setTarget(cacheMessage.target());
        w.setPayload(cacheMessage.payload());
        qCDebug(astarteTransportDC) << "Sending error wave for Discard target " << w.target();
        routeWave(w, -1);
    } else {
        int id = AstarteTransportCache::instance()->addRetryEntry(cacheMessage);
        Q_UNUSED(id);
    }
}

void AstarteTransport::bigBang()
{
    qCWarning(astarteTransportDC) << "Received bigBang";

    QSettings syncSettings(QStringLiteral("%1/transportStatus.conf").arg(QDir::homePath()), QSettings::IniFormat);
    if (m_synced) {
        m_synced = false;
        syncSettings.setValue(QStringLiteral("isSynced"), false);
    }

    if (m_mqttBroker.isNull()) {
        qCDebug(astarteTransportDC) << "Can't send emptyCache request, broker is null";
        return;
    }

    // We need to setup again the subscriptions, unless we have a persistent session on the other end.
    setupClientSubscriptions();
    // And publish the introspection.
    publishIntrospection();

    int rc = m_mqttBroker->publish(m_mqttBroker->rootClientTopic() + "/control/emptyCache", "1", MQTTClientWrapper::ExactlyOnceQoS);
    if (rc < 0) {
        // We leave m_synced to false and we retry when we're back online
        qCWarning(astarteTransportDC) << "Can't send emptyCache request, error " << rc;
        return;
    }

    QByteArray payload;
    for (const QByteArray &path : AstarteTransportCache::instance()->allPersistentEntries().keys()) {
        // Remove leading slash
        payload.append(path.mid(1));
        payload.append(';');
    }
    // Remove trailing semicolon
    payload.chop(1);

    qCDebug(astarteTransportDC) << "Producer property paths are: " << payload;

    rc = m_mqttBroker->publish(m_mqttBroker->rootClientTopic() + "/control/producer/properties", qCompress(payload), MQTTClientWrapper::ExactlyOnceQoS);
    if (rc < 0) {
        // We leave m_synced to false and we retry when we're back online
        qCWarning(astarteTransportDC) << "Can't send producer properties list, error " << rc;
        return;
    }

    // Send the cached properties
    sendProperties();

    // If we are here, the messages are in the hands of mosquitto
    m_synced = true;
    syncSettings.setValue(QStringLiteral("isSynced"), true);
}

void AstarteTransport::onPublishConfirmed(int messageId)
{
    qCInfo(astarteTransportDC) << "Message with id" << messageId << ": publish confirmed";
    CacheMessage cacheMessage = AstarteTransportCache::instance()->takeInFlightEntry(messageId);

    if (cacheMessage.interfaceType() == Hyperdrive::Interface::Type::Properties) {
        if (cacheMessage.payload().isEmpty()) {
            AstarteTransportCache::instance()->removePersistentEntry(cacheMessage.target());
        } else {
            AstarteTransportCache::instance()->insertOrUpdatePersistentEntry(cacheMessage.target(), cacheMessage.payload());
        }
    } else if (messageId == m_inFlightIntrospectionMessageId) {
        m_inFlightIntrospectionMessageId = -1;
        m_lastSentIntrospection = m_inFlightIntrospection;
        m_inFlightIntrospection = QByteArray();

        QSettings syncSettings(QStringLiteral("%1/transportStatus.conf").arg(QDir::homePath()), QSettings::IniFormat);
        syncSettings.setValue(QStringLiteral("lastSentIntrospection"), m_lastSentIntrospection);
    }
}

QByteArray AstarteTransport::introspectionString() const
{
    QByteArray ret;
    QMap<QByteArray, Hyperdrive::Interface> sortedIntrospection;

    // Put the introspection in a temporary map to guarantee ordering
    for (QHash< QByteArray, Hyperdrive::Interface >::const_iterator i = introspection().constBegin(); i != introspection().constEnd(); ++i) {
        sortedIntrospection.insert(i.key(), i.value());
    }

    for (QMap< QByteArray, Hyperdrive::Interface >::const_iterator i = sortedIntrospection.constBegin(); i != sortedIntrospection.constEnd(); ++i) {
        ret.append(i.key());
        ret.append(':');
        ret.append(QByteArray::number(i.value().versionMajor()));
        ret.append(':');
        ret.append(QByteArray::number(i.value().versionMinor()));
        ret.append(';');
    }

    // Remove last ;
    ret.chop(1);

    return ret;
}

void AstarteTransport::publishIntrospection()
{
    if (m_mqttBroker.isNull()) {
        return;
    }

    qCInfo(astarteTransportDC) << "Publishing introspection!";

    QByteArray introspectionPayload = introspectionString();
    qCDebug(astarteTransportDC) << "Introspection is " << introspectionPayload;

    int rc = m_mqttBroker->publish(m_mqttBroker->rootClientTopic(), introspectionPayload, Hyperdrive::MQTTClientWrapper::ExactlyOnceQoS);
    if (rc < 0) {
        qCWarning(astarteTransportDC) << "Can't send introspection, error " << rc;
    } else {
        qCInfo(astarteTransportDC) << "Published introspection with message id " << rc;
        m_inFlightIntrospectionMessageId = rc;
        m_inFlightIntrospection = introspectionPayload;
    }
}

}

TRANSPORT_MAIN(Hyperdrive::AstarteTransport, Astarte, 1.0.0)
