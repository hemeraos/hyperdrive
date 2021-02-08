#ifndef ASTARTE_GATEWAYENDPOINT_H
#define ASTARTE_GATEWAYENDPOINT_H

#include "astarteendpoint.h"
#include "astartecrypto.h"

#include <QtCore/QUrl>

#include <QtNetwork/QSslConfiguration>

class QNetworkReply;

namespace Hyperdrive {
class MQTTClientWrapper;
}

namespace Astarte {

class GatewayEndpointPrivate;
class GatewayEndpoint : public Endpoint
{
    Q_OBJECT
    Q_DISABLE_COPY(GatewayEndpoint)
    Q_DECLARE_PRIVATE_D(d_h_ptr, GatewayEndpoint)

public:
    explicit GatewayEndpoint(const QUrl &endpoint, QObject *parent = nullptr);
    virtual ~GatewayEndpoint();

    virtual QString endpointVersion() const override final;
    virtual QUrl mqttBrokerUrl() const override final;

    virtual Hyperdrive::MQTTClientWrapper *createMqttClientWrapper() override final;

    virtual bool isPaired() const override final;

    virtual Hemera::Operation *pair(bool force = true) override final;

    virtual Hemera::Operation *verifyCertificate() override final;

    virtual QNetworkReply *sendRequest(const QString &relativeEndpoint, const QByteArray &payload,
                                       Crypto::AuthenticationDomain authenticationDomain = Crypto::DeviceAuthenticationDomain) override final;

protected:
    virtual void initImpl() override final;
};
}

#endif // ASTARTE_GATEWAYENDPOINT_H
