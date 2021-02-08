/*
 *
 */

#ifndef HYPERDRIVE_MQTTTRANSPORT_H
#define HYPERDRIVE_MQTTTRANSPORT_H

#include <hyperdriveremotetransport.h>

#include <QtCore/QSet>

namespace Hyperdrive {

class MQTTClientWrapper;

class MQTTTransport : public Hyperdrive::RemoteTransport
{
    Q_OBJECT

public:
    MQTTTransport(QObject *parent = Q_NULLPTR);
    virtual ~MQTTTransport();

    virtual void rebound(const Hyperspace::Rebound& rebound, int fd = -1) override final;
    virtual void fluctuation(const Hyperspace::Fluctuation& fluctuation) override final;
    virtual void cacheMessage(const CacheMessage& cacheMessage) override final;
    virtual void bigBang() override final;

protected:
    virtual void initImpl() override final;

private Q_SLOTS:
    void setupClientSubscriptions(MQTTClientWrapper *client);
    void publishIntrospection();
    void publishIntrospectionForClient(MQTTClientWrapper *c);

private:
    QList< MQTTClientWrapper* > m_clients;
};
}

#endif // HYPERDRIVE_MQTTTRANSPORT_H
