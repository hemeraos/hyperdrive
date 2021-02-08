/*
 *
 */

#include "astartegatewaypairinghelper.h"

#include <HemeraCore/CommonOperations>
#include <HemeraCore/Literals>
#include <HemeraCore/Operation>
#include <HemeraCore/Fingerprints>

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

#include <hyperdriveconfig.h>

#include <hyperdrivemqttclientwrapper.h>
#include <astartehttpendpoint.h>

#define CONNECTION_TIMEOUT 15000

Q_LOGGING_CATEGORY(astarteGPHDC, "astarte.gateway.pairinghelper", DEBUG_MESSAGES_DEFAULT_LEVEL)

AstarteGatewayPairingHelper::AstarteGatewayPairingHelper(QObject* parent)
    : Hemera::AsyncInitObject(parent)
{
}

AstarteGatewayPairingHelper::~AstarteGatewayPairingHelper()
{
}

void AstarteGatewayPairingHelper::initImpl()
{
    // Make sure our dir exists
    QDir confDir;
    if (!confDir.exists(QLatin1String("/var/lib/astarte/gateway/"))) {
        confDir.mkpath(QLatin1String("/var/lib/astarte/gateway/"));
    }

    // Connect to the right thing
    if (QFile::exists(QStringLiteral("/var/lib/astarte/gateway/mosquitto.conf"))) {
        qCInfo(astarteGPHDC) << "Broker configuration already exists. Will just attempt pairing.";
        connect(this, &AstarteGatewayPairingHelper::ready, this, &AstarteGatewayPairingHelper::pairWithEndpoint);
    } else {
        // We have to write configuration
        connect(this, &AstarteGatewayPairingHelper::ready, this, &AstarteGatewayPairingHelper::writeConfigFile);
    }

    // Grab the hw id
    Hemera::ByteArrayOperation *hwIdOperation = Hemera::Fingerprints::globalHardwareId();
    connect(hwIdOperation, &Hemera::Operation::finished, [this, hwIdOperation] {
        if (hwIdOperation->isError()) {
            setInitError(Hemera::Literals::literal(Hemera::Literals::Errors::unhandledRequest()), QStringLiteral("Could not retrieve hardware id!"));
            return;
        } else {
            // Done.
            m_hardwareId = hwIdOperation->result();
            setReady();
        }
    });
}

void AstarteGatewayPairingHelper::writeConfigFile()
{
    // Get the payload
    QFile fileTemplate(QStringLiteral("%1/mosquitto.conf.default").arg(QLatin1String(Hyperdrive::StaticConfig::hyperspaceConfigurationDir())));

    if (!fileTemplate.open(QIODevice::ReadOnly)) {
        qCWarning(astarteGPHDC) << "Could not open default broker configuration for reading! Please check your installation.";
        // Quit badly. Shit just won't work.
        QCoreApplication::exit(1);
        return;
    }

    QByteArray result = fileTemplate.readAll();
    fileTemplate.close();

    // Replace client id
    result.replace("@GATEWAY_CLIENT_ID@", m_hardwareId);

    // Write it down to our actual config file
    QFile destination(QStringLiteral("/var/lib/astarte/gateway/mosquitto.conf"));

    if (!destination.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qCWarning(astarteGPHDC) << "Could not open broker configuration for writing! Please check your installation.";
        // Quit badly. Shit just won't work.
        QCoreApplication::exit(1);
        return;
    }

    bool opOk = true;
    opOk &= destination.write(result) != -1;
    opOk &= destination.flush();

    destination.close();

    if (!opOk){
        qCWarning(astarteGPHDC) << "Could not write broker configuration! Please check your installation.";
        // Quit badly. Shit just won't work.
        QCoreApplication::exit(1);
        return;
    }

    // Good! Let's move on.
    qCInfo(astarteGPHDC) << "Broker configuration written. Now pairing with Astarte...";
    pairWithEndpoint();
}

void AstarteGatewayPairingHelper::pairWithEndpoint()
{
    // Load from configuration
    QDir confDir(QLatin1String(Hyperdrive::StaticConfig::hyperspaceConfigurationDir()));
    for (const QString &conf : confDir.entryList(QStringList() << QStringLiteral("transport-astarte.conf"))) {
        QSettings settings(confDir.absoluteFilePath(conf), QSettings::IniFormat);
        settings.beginGroup(QStringLiteral("AstarteGateway")); {
            // It has an HTTP endpoint
            m_astarteEndpoint = new Astarte::HTTPEndpoint(settings.value(QStringLiteral("endpoint")).toUrl(), QSslConfiguration::defaultConfiguration(), this);

            connect(m_astarteEndpoint->init(), &Hemera::Operation::finished, [this] (Hemera::Operation *op) {
                if (op->isError()) {
                    qCWarning(astarteGPHDC) << "Could not connect to the remote endpoint!! Bridging will not work!";
                    // Quit gracefully anyway.
                    QCoreApplication::quit();
                    return;
                }

                // Pair
                connect(m_astarteEndpoint->pair(), &Hemera::Operation::finished, [this] (Hemera::Operation *pOp) {
                    if (pOp->isError()) {
                        qCWarning(astarteGPHDC) << "Pairing failed!!" << pOp->errorMessage();
                        qCWarning(astarteGPHDC) << "Bridging will not work!";
                        // Quit gracefully anyway.
                        QCoreApplication::quit();
                        return;
                    }

                    // Cool. Let's set up some symlinks, shall we?
                    QString endpointHost = m_astarteEndpoint->endpoint().host();
                    QFile::link(QStringLiteral("/var/lib/astarte/endpoint/%1/mqtt_broker.ca").arg(endpointHost),
                                QStringLiteral("/var/lib/astarte/gateway/mqtt_broker.ca"));
                    QFile::link(QStringLiteral("/var/lib/astarte/endpoint/%1/mqtt_broker.crt").arg(endpointHost),
                                QStringLiteral("/var/lib/astarte/gateway/mqtt_broker.crt"));

                    qCInfo(astarteGPHDC) << "Pairing successful. Now testing broker connection...";

                    // Time to test our broker!
                    testBroker();
                });
            });
        } settings.endGroup();
    }
}

void AstarteGatewayPairingHelper::testBroker()
{
    // Good. Let's set up our MQTT broker.
    m_mqttBroker = m_astarteEndpoint->createMqttClientWrapper();

    if (m_mqttBroker.isNull()) {
        qCWarning(astarteGPHDC) << "Could not create the MQTT client!! Broker connection was not verified!";
        // Quit gracefully anyway.
        QCoreApplication::quit();
        return;
    }

    connect(m_mqttBroker->init(), &Hemera::Operation::finished, [this] {
        // Test connection
        m_mqttBroker->connectToBroker();
    });
    connect(m_mqttBroker, &Hyperdrive::MQTTClientWrapper::statusChanged, [this]  {
        if (m_mqttBroker->status() == Hyperdrive::MQTTClientWrapper::ConnectedStatus) {
            // We're in!
            qCInfo(astarteGPHDC) << "Connection to broker was successful!";
            qCInfo(astarteGPHDC) << "Gateway bridging setup successfully!";
            m_mqttBroker->disconnectFromBroker();

            // Quit gracefully.
            QCoreApplication::quit();
        }
    });
}

#include "moc_astartegatewaypairinghelper.cpp"
