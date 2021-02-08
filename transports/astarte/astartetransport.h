/*
 *
 */

#ifndef HYPERDRIVE_ASTARTETRANSPORT_H
#define HYPERDRIVE_ASTARTETRANSPORT_H

#include <hyperdrivemqttclientwrapper.h>
#include <hyperdriveremotetransport.h>

#include <QtCore/QPointer>
#include <QtCore/QSet>

class QTimer;

namespace Astarte {
class Endpoint;
}

namespace Hyperdrive {
class MQTTClientWrapper;

class AstarteTransport : public Hyperdrive::RemoteTransport
{
    Q_OBJECT

public:
    AstarteTransport(QObject *parent = Q_NULLPTR);
    virtual ~AstarteTransport();

    virtual void rebound(const Hyperspace::Rebound& rebound, int fd = -1) override final;
    virtual void fluctuation(const Hyperspace::Fluctuation& fluctuation) override final;
    virtual void cacheMessage(const CacheMessage& cacheMessage) override final;
    virtual void bigBang() override final;

protected:
    virtual void initImpl() override final;

private Q_SLOTS:
    void startPairing(bool forcedPairing);
    void setupMqtt();
    void setupClientSubscriptions();
    void sendProperties();
    void resendFailedMessages();
    void publishIntrospection();
    void onStatusChanged(MQTTClientWrapper::Status status);
    void onMQTTMessageReceived(const QByteArray &topic, const QByteArray &payload);
    void onPublishConfirmed(int messageId);
    void handleFailedPublish(const CacheMessage &cacheMessage);
    void handleConnectionFailed();
    void handleConnackTimeout();
    void handleRebootTimerTimeout();
    void forceNewPairing();

private:
    QByteArray introspectionString() const;

    Astarte::Endpoint *m_astarteEndpoint;
    QPointer<MQTTClientWrapper> m_mqttBroker;
    QHash< quint64, Hyperspace::Wave > m_waveStorage;
    QHash< quint64, QByteArray > m_commandTree;
    QTimer *m_rebootTimer;
    QByteArray m_lastSentIntrospection;
    QByteArray m_inFlightIntrospection;
    bool m_synced;
    bool m_rebootWhenConnectionFails;
    int m_rebootDelayMinutes;
    int m_inFlightIntrospectionMessageId;
};
}

#endif // HYPERDRIVE_ASTARTETRANSPORT_H
