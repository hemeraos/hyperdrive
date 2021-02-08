/*
 *
 */

#ifndef HYPERDRIVE_ASTARTEGATEWAYPAIRINGHELPER_H
#define HYPERDRIVE_ASTARTEGATEWAYPAIRINGHELPER_H

#include <HemeraCore/AsyncInitObject>

#include <QtCore/QPointer>
#include <QtCore/QSet>

namespace Astarte {
class HTTPEndpoint;
}

namespace Hyperdrive {
class MQTTClientWrapper;
}

class AstarteGatewayPairingHelper : public Hemera::AsyncInitObject
{
    Q_OBJECT

public:
    AstarteGatewayPairingHelper(QObject *parent = Q_NULLPTR);
    virtual ~AstarteGatewayPairingHelper();

protected:
    virtual void initImpl() override final;

private Q_SLOTS:
    void writeConfigFile();
    void pairWithEndpoint();
    void testBroker();

private:
    Astarte::HTTPEndpoint *m_astarteEndpoint;
    QPointer<Hyperdrive::MQTTClientWrapper> m_mqttBroker;
    QByteArray m_hardwareId;
};

#endif // HYPERDRIVE_ASTARTEGATEWAYPAIRINGHELPER_H
