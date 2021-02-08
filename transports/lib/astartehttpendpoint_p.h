#ifndef ASTARTE_HTTPENDPOINT_P_H
#define ASTARTE_HTTPENDPOINT_P_H

#include "astartehttpendpoint.h"

#include <HemeraCore/Operation>

#include "astarteendpoint_p.h"

class QNetworkAccessManager;

namespace Astarte {

class HTTPEndpointPrivate : public EndpointPrivate {

public:
    HTTPEndpointPrivate(HTTPEndpoint *q) : EndpointPrivate(q) {}

    Q_DECLARE_PUBLIC(HTTPEndpoint)

    QString endpointName;
    QString endpointVersion;
    QUrl mqttBroker;
    QNetworkAccessManager *nam;
    QByteArray hardwareId;

    QByteArray agentKey;
    QString brokerCa;
    bool ignoreSslErrors;

    QSslConfiguration sslConfiguration;

    void connectToEndpoint();
};

class PairOperation : public Hemera::Operation
{
    Q_OBJECT
    Q_DISABLE_COPY(PairOperation)

public:
    explicit PairOperation(HTTPEndpoint *parent);
    virtual ~PairOperation();

protected:
    virtual void startImpl() override final;

private Q_SLOTS:
    void initiatePairing();
    void performFakeAgentPairing();
    void performPairing();

private:
    HTTPEndpoint *m_endpoint;
};

}

#endif // ASTARTE_HTTPENDPOINT_P_H
