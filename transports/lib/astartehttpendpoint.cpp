#include "astartehttpendpoint.h"
#include "astartehttpendpoint_p.h"

#include "astartecrypto.h"
#include "astarteverifycertificateoperation.h"
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
#include <HemeraCore/FetchSystemConfigOperation>
#include <HemeraCore/SetSystemConfigOperation>

#include <private/HemeraCore/hemeraasyncinitobject_p.h>

#include <hyperdriveconfig.h>
#include <hyperdriveutils.h>

#define RETRY_INTERVAL 15000

Q_LOGGING_CATEGORY(astarteHttpEndpointDC, "astarte.httpendpoint", DEBUG_MESSAGES_DEFAULT_LEVEL)

namespace Astarte {

constexpr const char *pathToAstarteEndpointConfiguration() { return "/var/lib/astarte/endpoint/"; }
QString pathToAstarteEndpointConfiguration(const QString &endpointName)
{ return QStringLiteral("%1%2").arg(QLatin1String(pathToAstarteEndpointConfiguration()), endpointName); }

PairOperation::PairOperation(HTTPEndpoint *parent)
    : Hemera::Operation(parent)
    , m_endpoint(parent)
{
}

PairOperation::~PairOperation()
{
}

void PairOperation::startImpl()
{
    // Before anything else, we need to check if we have an available keystore.
    if (!Crypto::instance()->isKeyStoreAvailable()) {
        // Let's build one.
        Hemera::Operation *op = Crypto::instance()->generateAstarteKeyStore();
        connect(op, &Hemera::Operation::finished, this, [this, op] {
            if (op->isError()) {
                // That's ugly.
                setFinishedWithError(op->errorName(), op->errorMessage());
                return;
            }

            initiatePairing();
        });
    } else {
        // Let's just go
        initiatePairing();
    }
}

void PairOperation::initiatePairing()
{
    // FIXME: This should be done using Global configuration!!
    QSettings settings(QStringLiteral("%1/endpoint_crypto.conf").arg(pathToAstarteEndpointConfiguration(m_endpoint->d_func()->endpointName)),
                       QSettings::IniFormat);
    if (settings.value(QStringLiteral("apiKey")).toString().isEmpty()) {
        Hemera::FetchSystemConfigOperation *apiKeyOp = new Hemera::FetchSystemConfigOperation(QStringLiteral("hemera_astarte_apikey"));
        connect(apiKeyOp, &Hemera::Operation::finished, this, [this, apiKeyOp] {
            QString apiKey = apiKeyOp->value();
            if (apiKeyOp->isError() || apiKey.isEmpty()) {
                performFakeAgentPairing();
            } else {
                {
                    QSettings settings(QStringLiteral("%1/endpoint_crypto.conf").arg(pathToAstarteEndpointConfiguration(m_endpoint->d_func()->endpointName)),
                                   QSettings::IniFormat);
                    settings.setValue(QStringLiteral("apiKey"), apiKey);
                }
                performPairing();
            }
        });
    } else {
        performPairing();
    }
}

void PairOperation::performFakeAgentPairing()
{
    qWarning() << "Fake agent pairing!";
    QJsonObject o;
    o.insert(QStringLiteral("hwId"), QLatin1String(m_endpoint->d_func()->hardwareId));

    QByteArray deviceIDPayload = QJsonDocument(o).toJson(QJsonDocument::Compact);
    QNetworkReply *r = m_endpoint->sendRequest(QStringLiteral("/devices/apikeysFromDevice"), deviceIDPayload, Crypto::CustomerAuthenticationDomain);

    qCDebug(astarteHttpEndpointDC) << "I'm sending: " << deviceIDPayload.constData();

    connect(r, &QNetworkReply::finished, this, [this, r, deviceIDPayload] {
        if (r->error() != QNetworkReply::NoError) {
            qCWarning(astarteHttpEndpointDC) << "Pairing error! Error: " << r->error();
            setFinishedWithError(Hemera::Literals::literal(Hemera::Literals::Errors::failedRequest()), r->errorString());
            r->deleteLater();
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(r->readAll());
        r->deleteLater();
        qCDebug(astarteHttpEndpointDC) << "Got the ok!";
        if (!doc.isObject()) {
            qCWarning(astarteHttpEndpointDC) << "Parsing pairing result error!";
            setFinishedWithError(Hemera::Literals::literal(Hemera::Literals::Errors::badRequest()), QStringLiteral("Parsing pairing result error!"));
            return;
        }

        qCDebug(astarteHttpEndpointDC) << "Payload is " << doc.toJson().constData();

        QJsonObject pairData = doc.object();
        if (!pairData.contains(QStringLiteral("apiKey"))) {
            qCWarning(astarteHttpEndpointDC) << "Missing apiKey in the pairing routine!";
            setFinishedWithError(Hemera::Literals::literal(Hemera::Literals::Errors::badRequest()),
                                 QStringLiteral("Missing apiKey in the pairing routine!"));
            return;
        }

        // Ok, we need to write the files now.
        {
            QSettings settings(QStringLiteral("%1/endpoint_crypto.conf").arg(pathToAstarteEndpointConfiguration(m_endpoint->d_func()->endpointName)),
                               QSettings::IniFormat);
            settings.setValue(QStringLiteral("apiKey"), pairData.value(QStringLiteral("apiKey")).toString());

            new Hemera::SetSystemConfigOperation(QStringLiteral("hemera_astarte_apikey"), pairData.value(QStringLiteral("apiKey")).toString());
        }

        // That's all, folks!
        performPairing();
    });
}

void PairOperation::performPairing()
{
    QFile csr(Crypto::instance()->pathToCertificateRequest());
    if (!csr.open(QIODevice::ReadOnly)) {
        qCWarning(astarteHttpEndpointDC) << "Could not open CSR for reading! Aborting.";
        setFinishedWithError(Hemera::Literals::literal(Hemera::Literals::Errors::notFound()), QStringLiteral("Could not open CSR for reading! Aborting."));
    }

    QByteArray deviceIDPayload = csr.readAll();
    QNetworkReply *r = m_endpoint->sendRequest(QStringLiteral("/pairing"), deviceIDPayload, Crypto::DeviceAuthenticationDomain);

    qCDebug(astarteHttpEndpointDC) << "I'm sending: " << deviceIDPayload.constData();

    connect(r, &QNetworkReply::finished, this, [this, r, deviceIDPayload] {
        if (r->error() != QNetworkReply::NoError) {
            qCWarning(astarteHttpEndpointDC) << "Pairing error!";
            setFinishedWithError(Hemera::Literals::literal(Hemera::Literals::Errors::failedRequest()), r->errorString());
            r->deleteLater();
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(r->readAll());
        r->deleteLater();
        qCDebug(astarteHttpEndpointDC) << "Got the ok!";
        if (!doc.isObject()) {
            qCWarning(astarteHttpEndpointDC) << "Parsing pairing result error!";
            setFinishedWithError(Hemera::Literals::literal(Hemera::Literals::Errors::badRequest()), QStringLiteral("Parsing pairing result error!"));
            return;
        }

        qCDebug(astarteHttpEndpointDC) << "Payload is " << doc.toJson().constData();

        QJsonObject pairData = doc.object();
        if (!pairData.contains(QStringLiteral("clientCrt"))) {
            qCWarning(astarteHttpEndpointDC) << "Missing certificate in the pairing routine!";
            setFinishedWithError(Hemera::Literals::literal(Hemera::Literals::Errors::badRequest()), QStringLiteral("Missing certificate in the pairing routine!"));
            return;
        }

        // Ok, we need to write the files now.
        {
            QFile generatedCertificate(QStringLiteral("%1/mqtt_broker.crt").arg(pathToAstarteEndpointConfiguration(m_endpoint->d_func()->endpointName)));
            if (!generatedCertificate.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                qCWarning(astarteHttpEndpointDC) << "Could not write certificate!";
                setFinishedWithError(Hemera::Literals::literal(Hemera::Literals::Errors::badRequest()), generatedCertificate.errorString());
                return;
            }
            generatedCertificate.write(pairData.value(QStringLiteral("clientCrt")).toVariant().toByteArray());
            generatedCertificate.flush();
            generatedCertificate.close();
        }
        {
            QSettings settings(QStringLiteral("%1/mqtt_broker.conf").arg(pathToAstarteEndpointConfiguration(m_endpoint->d_func()->endpointName)),
                               QSettings::IniFormat);
        }

        // That's all, folks!
        setFinished();
    });
}

void HTTPEndpointPrivate::connectToEndpoint()
{
    QUrl infoEndpoint = endpoint;
    infoEndpoint.setPath(endpoint.path() + QStringLiteral("/info"));
    QNetworkRequest req(infoEndpoint);
    req.setSslConfiguration(sslConfiguration);
    req.setRawHeader("Authorization", agentKey);
    req.setRawHeader("X-Astarte-Transport-Provider", "Hemera");
    req.setRawHeader("X-Astarte-Transport-Version", QStringLiteral("%1.%2.%3")
                                                     .arg(Hyperdrive::StaticConfig::hyperdriveMajorVersion())
                                                     .arg(Hyperdrive::StaticConfig::hyperdriveMinorVersion())
                                                     .arg(Hyperdrive::StaticConfig::hyperdriveReleaseVersion())
                                                     .toLatin1());
    QNetworkReply *reply = nam->get(req);
    qCDebug(astarteHttpEndpointDC) << "Connecting to our endpoint! " << infoEndpoint;

    Q_Q(HTTPEndpoint);
    QObject::connect(reply, &QNetworkReply::finished, q, [this, q, reply] {
        if (reply->error() != QNetworkReply::NoError) {
            int retryInterval = Hyperdrive::Utils::randomizedInterval(RETRY_INTERVAL, 1.0);
            qCWarning(astarteHttpEndpointDC) << "Error while connecting! Retrying in " << (retryInterval / 1000) << " seconds. error: " << reply->error();

            // We never give up. If we couldn't connect, we reschedule this in 15 seconds.
            QTimer::singleShot(retryInterval, q, SLOT(connectToEndpoint()));
            reply->deleteLater();
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        reply->deleteLater();
        if (!doc.isObject()) {
            return;
        }

        qCDebug(astarteHttpEndpointDC) << "Connected! " << doc.toJson(QJsonDocument::Indented);

        QJsonObject rootReplyObj = doc.object();
        endpointVersion = rootReplyObj.value(QStringLiteral("version")).toString();

        // Get configuration
        QSettings settings(QStringLiteral("%1/mqtt_broker.conf").arg(pathToAstarteEndpointConfiguration(endpointName)),
                           QSettings::IniFormat);
        mqttBroker = QUrl::fromUserInput(rootReplyObj.value(QStringLiteral("url")).toString());

        // Initialize cryptography
        auto processCryptoStatus = [this, q] (bool ready) {
            if (!ready) {
                qCWarning(astarteHttpEndpointDC) << "Could not initialize signature system!!";
                if (!q->isReady()) {
                    q->setInitError(Hemera::Literals::literal(Hemera::Literals::Errors::failedRequest()),
                                    HTTPEndpoint::tr("Could not initialize signature system!"));
                }
                return;
            }

            qCInfo(astarteHttpEndpointDC) << "Signature system initialized correctly!";

            if (!q->isReady()) {
                q->setReady();
            }
        };

        if (Crypto::instance()->isReady()) {
            processCryptoStatus(true);
        } else if (Crypto::instance()->hasInitError()) {
            processCryptoStatus(false);
        } else {
            QObject::connect(Crypto::instance()->init(), &Hemera::Operation::finished, q, [this, q, processCryptoStatus] (Hemera::Operation *op) {
                processCryptoStatus(!op->isError());
            });
        }
    });
}


HTTPEndpoint::HTTPEndpoint(const QUrl& endpoint, const QSslConfiguration &sslConfiguration, QObject* parent)
    : Endpoint(*new HTTPEndpointPrivate(this), endpoint, parent)
{
    Q_D(HTTPEndpoint);
    d->nam = new QNetworkAccessManager(this);
    d->sslConfiguration = sslConfiguration;

    d->endpointName = endpoint.host();
}

HTTPEndpoint::~HTTPEndpoint()
{
}

QUrl HTTPEndpoint::endpoint() const
{
    Q_D(const HTTPEndpoint);
    return d->endpoint;
}

void HTTPEndpoint::initImpl()
{
    Q_D(HTTPEndpoint);

    // FIXME: This should be done using Global configuration!!
    QDir confDir(QLatin1String(Hyperdrive::StaticConfig::hyperspaceConfigurationDir()));
    for (const QString &conf : confDir.entryList(QStringList() << QStringLiteral("transport-astarte.conf"))) {
        QSettings settings(confDir.absoluteFilePath(conf), QSettings::IniFormat);
        settings.beginGroup(QStringLiteral("AstarteTransport")); {
            d->agentKey = settings.value(QStringLiteral("agentKey")).toString().toLatin1();
            d->brokerCa = settings.value(QStringLiteral("brokerCa"), QStringLiteral("/etc/ssl/certs/ca-bundle.trust.crt")).toString();
            d->ignoreSslErrors = settings.value(QStringLiteral("ignoreSslErrors"), false).toBool();
            if (settings.contains(QStringLiteral("pairingCa"))) {
               d->sslConfiguration.setCaCertificates(QSslCertificate::fromPath(settings.value(QStringLiteral("pairingCa")).toString()));
            }
        } settings.endGroup();
    }

    // Grab the hw id
    Hemera::ByteArrayOperation *hwIdOperation = Hemera::Fingerprints::globalHardwareId();
    connect(hwIdOperation, &Hemera::Operation::finished, this, [this, hwIdOperation] {
        if (hwIdOperation->isError()) {
            setInitError(Hemera::Literals::literal(Hemera::Literals::Errors::unhandledRequest()), QStringLiteral("Could not retrieve hardware id!"));
        } else {
            Q_D(HTTPEndpoint);
            d->hardwareId = hwIdOperation->result();

            // Verify the configuration directory is up.
            if (!QFileInfo::exists(pathToAstarteEndpointConfiguration(d->endpointName))) {
                // Create
                qCDebug(astarteHttpEndpointDC) << "Creating Astarte endpoint configuration directory for " << d->endpointName;
                QDir dir;
                if (!dir.mkpath(pathToAstarteEndpointConfiguration(d->endpointName))) {
                    setInitError(Hemera::Literals::literal(Hemera::Literals::Errors::failedRequest()), QStringLiteral("Could not create configuration directory!"));
                }
            }

            // Let's connect to our endpoint, shall we?
            d->connectToEndpoint();
        }
    });

    connect(d->nam, &QNetworkAccessManager::sslErrors, this, [this] (QNetworkReply *reply, const QList<QSslError> &errors) {
        Q_D(const HTTPEndpoint);
        qCWarning(astarteHttpEndpointDC) << "SslErrors: " << errors;
        if (d->ignoreSslErrors) {
            reply->ignoreSslErrors(errors);
        }
    });
}

QNetworkReply *HTTPEndpoint::sendRequest(const QString& relativeEndpoint, const QByteArray& payload, Crypto::AuthenticationDomain authenticationDomain)
{
    Q_D(const HTTPEndpoint);
    // Build the endpoint
    QUrl target = d->endpoint;
    target.setPath(d->endpoint.path() + relativeEndpoint);

    QNetworkRequest req(target);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setSslConfiguration(d->sslConfiguration);

    qWarning() << "La richiesta lo zio" << relativeEndpoint << payload << authenticationDomain;
    // Authentication?
    if (authenticationDomain == Crypto::DeviceAuthenticationDomain) {
        // FIXME: This should be done using Global configuration!!
        QSettings settings(QStringLiteral("%1/endpoint_crypto.conf").arg(pathToAstarteEndpointConfiguration(d->endpointName)),
                           QSettings::IniFormat);
        req.setRawHeader("X-API-Key", settings.value(QStringLiteral("apiKey")).toString().toLatin1());
        req.setRawHeader("X-Hardware-ID", d->hardwareId);
        req.setRawHeader("X-Astarte-Transport-Provider", "Hemera");
        req.setRawHeader("X-Astarte-Transport-Version", QStringLiteral("%1.%2.%3")
                                                     .arg(Hyperdrive::StaticConfig::hyperdriveMajorVersion())
                                                     .arg(Hyperdrive::StaticConfig::hyperdriveMinorVersion())
                                                     .arg(Hyperdrive::StaticConfig::hyperdriveReleaseVersion())
                                                     .toLatin1());
        qWarning() << "Lozio";
        qWarning() << "Setting X-API-Key:" << settings.value(QStringLiteral("apiKey")).toString();
    } else if (authenticationDomain == Crypto::CustomerAuthenticationDomain) {
        qWarning() << "Authorization lo zio";
        req.setRawHeader("Authorization", d->agentKey);
        req.setRawHeader("X-Astarte-Transport-Provider", "Hemera");
        req.setRawHeader("X-Astarte-Transport-Version", QStringLiteral("%1.%2.%3")
                                                     .arg(Hyperdrive::StaticConfig::hyperdriveMajorVersion())
                                                     .arg(Hyperdrive::StaticConfig::hyperdriveMinorVersion())
                                                     .arg(Hyperdrive::StaticConfig::hyperdriveReleaseVersion())
                                                     .toLatin1());
    }

    return d->nam->post(req, payload);
}

Hemera::Operation* HTTPEndpoint::pair(bool force)
{
    if (!force && isPaired()) {
        return new Hemera::FailureOperation(Hemera::Literals::literal(Hemera::Literals::Errors::badRequest()),
                                            tr("This device is already paired to this Astarte endpoint"));
    }

    return new PairOperation(this);
}

Hemera::Operation* HTTPEndpoint::verifyCertificate()
{
    Q_D(const HTTPEndpoint);
    QFile cert(QStringLiteral("%1/mqtt_broker.crt").arg(pathToAstarteEndpointConfiguration(d->endpointName)));
    return new VerifyCertificateOperation(cert, this);
}

Hyperdrive::MQTTClientWrapper *HTTPEndpoint::createMqttClientWrapper()
{
    if (!isReady() || mqttBrokerUrl().isEmpty()) {
        return nullptr;
    }

    Q_D(const HTTPEndpoint);

    // Create the client ID
    QList < QSslCertificate > certificates = QSslCertificate::fromPath(QStringLiteral("%1/mqtt_broker.crt")
                                                    .arg(pathToAstarteEndpointConfiguration(d->endpointName)), QSsl::Pem);
    if (certificates.size() != 1) {
        qCWarning(astarteHttpEndpointDC) << "Could not retrieve device certificate!";
        return nullptr;
    }

    QByteArray customerId = certificates.first().subjectInfo(QSslCertificate::CommonName).first().toLatin1();

    Hyperdrive::MQTTClientWrapper *c = new Hyperdrive::MQTTClientWrapper(mqttBrokerUrl(), customerId, this);

    c->setCleanSession(false);
    c->setPublishQoS(Hyperdrive::MQTTClientWrapper::AtMostOnceQoS);
    c->setSubscribeQoS(Hyperdrive::MQTTClientWrapper::AtMostOnceQoS);
    c->setKeepAlive(300);
    c->setIgnoreSslErrors(d->ignoreSslErrors);

    // SSL
    c->setMutualSSLAuthentication(d->brokerCa, Crypto::pathToPrivateKey(),
                                  QStringLiteral("%1/mqtt_broker.crt").arg(pathToAstarteEndpointConfiguration(d->endpointName)));

    QSettings settings(QStringLiteral("%1/mqtt_broker.conf").arg(pathToAstarteEndpointConfiguration(d->endpointName)),
                       QSettings::IniFormat);

    return c;
}

QString HTTPEndpoint::endpointVersion() const
{
    Q_D(const HTTPEndpoint);
    return d->endpointVersion;
}

QUrl HTTPEndpoint::mqttBrokerUrl() const
{
    Q_D(const HTTPEndpoint);
    return d->mqttBroker;
}

bool HTTPEndpoint::isPaired() const
{
    Q_D(const HTTPEndpoint);
    return QFile::exists(QStringLiteral("%1/mqtt_broker.crt").arg(pathToAstarteEndpointConfiguration(d->endpointName)));
}

}

#include "moc_astartehttpendpoint.cpp"
