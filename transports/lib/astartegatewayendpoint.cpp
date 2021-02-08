#include "astartegatewayendpoint.h"
#include "astartegatewayendpoint_p.h"

#include "astartecrypto.h"
#include "hyperdrivemqttclientwrapper.h"

#include <QtCore/QDir>
#include <QtCore/QDebug>
#include <QtCore/QLoggingCategory>
#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QSettings>
#include <QtCore/QTimer>

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

#include <HemeraCore/CommonOperations>
#include <HemeraCore/Fingerprints>
#include <HemeraCore/Literals>

Q_LOGGING_CATEGORY(astarteGatewayEndpointDC, "astarte.gatewayendpoint", DEBUG_MESSAGES_DEFAULT_LEVEL)

namespace Astarte {

/* NOTE:
 *
 * The local gateway implementation is currently extremely simple and does not involve a compatibility
 * endpoint. As such, we just return the MQTT broker without further ado, and that's about it.
 */

GatewayEndpoint::GatewayEndpoint(const QUrl& endpoint, QObject* parent)
    : Endpoint(*new GatewayEndpointPrivate(this), endpoint, parent)
{
    Q_D(GatewayEndpoint);

    // Create the MQTT broker address out of the host. Port is always 1883 unless local (1885), no encryption.
    d->mqttBroker.setScheme(QStringLiteral("tcp"));
    d->mqttBroker.setHost(endpoint.host().isEmpty() ? endpoint.toString() : endpoint.host());
    // FIXME: We should determine whether our board is running the gateway and decide on the port there.
    d->mqttBroker.setPort(1885);

    qCInfo(astarteGatewayEndpointDC) << "Setting up a gateway connection to" << d->mqttBroker;
}

GatewayEndpoint::~GatewayEndpoint()
{
}

void GatewayEndpoint::initImpl()
{
    // Nothing to do.
    setReady();
}

QNetworkReply *GatewayEndpoint::sendRequest(const QString& relativeEndpoint, const QByteArray& payload,
                                            Crypto::AuthenticationDomain authenticationDomain)
{
    // return a null request
    return nullptr;
}

Hemera::Operation* GatewayEndpoint::pair(bool force)
{
    if (!force) {
        return new Hemera::FailureOperation(Hemera::Literals::literal(Hemera::Literals::Errors::badRequest()),
                                            tr("This device is already paired to this Astarte endpoint"));
    }

    // Always successful
    return new Hemera::SuccessOperation(this);
}

Hemera::Operation* GatewayEndpoint::verifyCertificate()
{
    // TODO: is this ok?
    return new Hemera::SuccessOperation(this);
}

Hyperdrive::MQTTClientWrapper *GatewayEndpoint::createMqttClientWrapper()
{
    if (!isReady() || mqttBrokerUrl().isEmpty()) {
        return nullptr;
    }

    Hyperdrive::MQTTClientWrapper *c = new Hyperdrive::MQTTClientWrapper(mqttBrokerUrl(), this);

    c->setCleanSession(true);

    // Local gateway connections have no bandwidth constraints, so let's just do this.
    c->setPublishQoS(Hyperdrive::MQTTClientWrapper::ExactlyOnceQoS);
    c->setSubscribeQoS(Hyperdrive::MQTTClientWrapper::ExactlyOnceQoS);
    c->setKeepAlive(300);

    return c;
}

QString GatewayEndpoint::endpointVersion() const
{
    return QStringLiteral("1.0.0");
}

QUrl GatewayEndpoint::mqttBrokerUrl() const
{
    Q_D(const GatewayEndpoint);
    return d->mqttBroker;
}

bool GatewayEndpoint::isPaired() const
{
    // Always true
    return true;
}

}

#include "moc_astartegatewayendpoint.cpp"
