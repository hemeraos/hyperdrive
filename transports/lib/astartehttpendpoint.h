#ifndef ASTARTE_HTTPENDPOINT_H
#define ASTARTE_HTTPENDPOINT_H

#include "astarteendpoint.h"
#include "astartecrypto.h"

#include <QtCore/QUrl>

#include <QtNetwork/QSslConfiguration>

class QNetworkReply;

namespace Hyperdrive {
class MQTTClientWrapper;
}

namespace Astarte {

class HTTPEndpointPrivate;
class HTTPEndpoint : public Endpoint
{
    Q_OBJECT
    Q_DISABLE_COPY(HTTPEndpoint)
    Q_DECLARE_PRIVATE_D(d_h_ptr, HTTPEndpoint)

    Q_PRIVATE_SLOT(d_func(), void connectToEndpoint())

    friend class PairOperation;

public:
    explicit HTTPEndpoint(const QUrl &endpoint, const QSslConfiguration &sslConfiguration = QSslConfiguration(), QObject *parent = nullptr);
    virtual ~HTTPEndpoint();

    QUrl endpoint() const;
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

#endif // ASTARTE_HTTPENDPOINT_H
