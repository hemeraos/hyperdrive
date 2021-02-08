/*
 *
 */

#include "httptransport.h"
#include "http_parser.h"
#include "transporttcpserver.h"
#include "httptransportcache.h"
#include "httptransportcallbackmanager.h"

#include <HemeraCore/CommonOperations>
#include <HemeraCore/Fingerprints>
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
#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QUrl>

#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QTcpServer>
#include <QtNetwork/QTcpSocket>
#include <QtNetwork/QSslKey>

#include <QtCore/QFuture>
#include <QtCore/QFutureWatcher>
#include <QFile>
#include <QtConcurrent/QtConcurrentRun>

#include <systemd/sd-daemon.h>

#include "transports.h"
#include <hyperdriveprotocol.h>
#include <hyperdriveconfig.h>

#include <HyperspaceCore/BSONDocument>
#include <HyperspaceCore/BSONSerializer>

#include <signal.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>

#define CONNECTION_TIMEOUT 15000
#define NEXT_HEADER "HYPERDRIVE_NEXT_HEADER"

#define METHOD_DELETE "DELETE"
#define METHOD_GET "GET"
#define METHOD_POST "POST"
#define METHOD_PUT "PUT"
#define METHOD_OPTIONS "OPTIONS"

Q_LOGGING_CATEGORY(httpTransportDC, "hyperdrive.transport.http", DEBUG_MESSAGES_DEFAULT_LEVEL)

namespace Hyperdrive
{

int HTTPTransport::onUrl (http_parser *parser, const char *url, size_t urlLen)
{
    (*HTTPTransport::instance()->m_waveStorage.find(HTTPTransport::instance()->m_parserToWave.value(parser))).setTarget(QByteArray(url, urlLen));
    return 0;
}

int HTTPTransport::onStatus (http_parser *parser, const char *status, size_t statusLen)
{
    return 0;
}

int HTTPTransport::onHeaderField (http_parser *parser, const char *headerField, size_t headerLen)
{
    (*HTTPTransport::instance()->m_waveStorage.find(HTTPTransport::instance()->m_parserToWave.value(parser))).addAttribute(NEXT_HEADER, QByteArray(headerField, headerLen));
    return 0;
}

int HTTPTransport::onHeaderValue (http_parser *parser, const char *headerValue, size_t valueLen)
{
    QByteArray headerField = (*HTTPTransport::instance()->m_waveStorage.find(HTTPTransport::instance()->m_parserToWave.value(parser))).takeAttribute(NEXT_HEADER);
    (*HTTPTransport::instance()->m_waveStorage.find(HTTPTransport::instance()->m_parserToWave.value(parser))).addAttribute(headerField, QByteArray(headerValue, valueLen));
    return 0;

}

int HTTPTransport::onBody (http_parser *parser, const char *body, size_t bodyLen)
{
    (*HTTPTransport::instance()->m_waveStorage.find(HTTPTransport::instance()->m_parserToWave.value(parser))).setPayload(QByteArray(body, bodyLen));
    return 0;
}

int HTTPTransport::onMessageBegin (http_parser *parser)
{
    return 0;
}

int HTTPTransport::onHeadersComplete (http_parser *parser)
{
    Hyperspace::Wave wave = HTTPTransport::instance()->m_waveStorage.value(HTTPTransport::instance()->m_parserToWave.value(parser));
    QTcpSocket *socket = HTTPTransport::instance()->m_waveToSocket.value(wave.id());

    QByteArray connectionValue = wave.attributes().value("Connection");

    if (connectionValue == "keep-alive" && !HTTPTransport::instance()->m_keepAliveSockets.contains(socket))  {
        HTTPTransport::instance()->m_keepAliveSockets.insert(socket);
    }

    return 0;
}

int HTTPTransport::onMessageComplete (http_parser *parser)
{
    QTcpSocket *socket = HTTPTransport::instance()->m_waveToSocket.value(HTTPTransport::instance()->m_waveStorage.value(HTTPTransport::instance()->m_parserToWave.value(parser)).id());
    (*HTTPTransport::instance()->m_waveStorage.find(HTTPTransport::instance()->m_parserToWave.value(parser))).setMethod(http_method_str(static_cast<http_method>(parser->method)));
    HTTPTransport::instance()->triggerWave(HTTPTransport::instance()->m_waveStorage.value(HTTPTransport::instance()->m_parserToWave.value(parser)), socket);
    return 0;
}



static HTTPTransport *s_instance = 0;


HTTPTransport::HTTPTransport(QObject* parent)
    : RemoteTransport(QStringLiteral("HTTP"), parent)
    , m_parserSettings(Q_NULLPTR)
{
    if (s_instance) {
        Q_ASSERT("Trying to create an additional instance!");
    }
    s_instance = this;
}

HTTPTransport::~HTTPTransport()
{
    delete m_parserSettings;
}

HTTPTransport *HTTPTransport::instance()
{
    return s_instance;
}

QSslConfiguration HTTPTransport::sslConfiguration()
{
    if (!m_sslConfiguration.isNull()) {
        return m_sslConfiguration;
    }

    // Construct SSL configuration
    // TODO: create the keys here if not found instead of using systemd
    QSslConfiguration sslConfiguration = QSslConfiguration::defaultConfiguration();
    QList< QSslCertificate > certificates = QSslCertificate::fromPath(QStringLiteral("%1/ssl.pem").arg(QDir::homePath()));
    if (certificates.isEmpty()) {
        setInitError(Hemera::Literals::literal(Hemera::Literals::Errors::notFound()),
                     QStringLiteral("Could not find SSL certificate! Your system is not installed properly."));
        return QSslConfiguration();
    }

    m_sslConfiguration.setLocalCertificate(certificates.first());
    QFile *key = new QFile(QStringLiteral("%1/ssl.key").arg(QDir::homePath()), this);
    if (!key->open(QIODevice::ReadOnly)) {
        setInitError(Hemera::Literals::literal(Hemera::Literals::Errors::notFound()),
                     QStringLiteral("Could not open SSL private key! Your system is not installed properly."));
        return QSslConfiguration();
    }
    m_sslConfiguration.setPrivateKey(QSslKey(key, QSsl::Rsa));

    return m_sslConfiguration;
}

void HTTPTransport::initImpl()
{
    setParts(3);

    connect(HTTPTransportCache::instance()->init(), &Hemera::Operation::finished, this, [this] (Hemera::Operation *op) {
            if (op->isError()) {
                setInitError(op->errorName(), op->errorMessage());
            } else {
                setOnePartIsReady();
            }
    });

    m_callbackManager = new HTTPTransportCallbackManager(this);
    connect(m_callbackManager->init(), &Hemera::Operation::finished, this, [this] (Hemera::Operation *op) {
        if (op->isError()) {
            setInitError(Hemera::Literals::literal(Hemera::Literals::Errors::failedRequest()), tr("Could not init callback manager"));
            return;
        }

        setOnePartIsReady();
    });

    // We must get the sockets from systemd
    int fd;

    int sockets = sd_listen_fds(0);
    QSettings settings(QStringLiteral("%1/transport-http.conf").arg(QLatin1String(Hyperdrive::StaticConfig::hyperspaceConfigurationDir())), QSettings::IniFormat);

    for (int i = 0; i != sockets; ++i) {
        TransportTCPServer *server = new TransportTCPServer(this);

        fd = SD_LISTEN_FDS_START + i;

        if (Q_UNLIKELY(!server->setSocketDescriptor(fd))) {
            setInitError(Hemera::Literals::literal(Hemera::Literals::Errors::systemdError()),
                         QStringLiteral("Could not bind to system socket."));
            return;
        }
        if (Q_UNLIKELY(!server->isListening())) {
            setInitError(Hemera::Literals::literal(Hemera::Literals::Errors::systemdError()),
                         QStringLiteral("System socket is currently not listening. This should not be happening."));
            return;
        }

        // Check settings
        bool found = false;
        for (const QString &group : settings.childGroups()) {
            settings.beginGroup(group); {
                if (server->serverAddress().toString() == settings.value(QStringLiteral("address")).toString() &&
                    server->serverPort() == settings.value(QStringLiteral("port")).toInt()) {
                    found = true;
                    if (settings.value(QStringLiteral("secure")).toBool()) {
                        QSslConfiguration configuration = sslConfiguration();
                        if (configuration.isNull()) {
                            return;
                        }

                        server->setSslConfiguration(configuration);
                        connect(server, &TransportTCPServer::newEncryptedConnection, this, &HTTPTransport::clientConnected);
                        connect(server, &TransportTCPServer::sslErrors, this, [this] (const QList<QSslError> &errors) { qCDebug(httpTransportDC) << errors; });
                    } else {
                        connect(server, &QTcpServer::newConnection, this, &HTTPTransport::clientConnected);
                    }
                }
            } settings.endGroup();
        }

        if (!found) {
            setInitError(Hemera::Literals::literal(Hemera::Literals::Errors::notFound()),
                         QStringLiteral("Server not found in configuration! Your installation is corrupted."));
            return;
        }
    }

    // Set up our global parser settings
    m_parserSettings = new http_parser_settings;
    m_parserSettings->on_url = onUrl;
    m_parserSettings->on_header_field = onHeaderField;
    m_parserSettings->on_body = onBody;
    m_parserSettings->on_header_value = onHeaderValue;
    m_parserSettings->on_headers_complete = onHeadersComplete;
    m_parserSettings->on_message_begin = onMessageBegin;
    m_parserSettings->on_message_complete = onMessageComplete;
    m_parserSettings->on_status = onStatus;

    setOnePartIsReady();
}


void HTTPTransport::clientConnected()
{
    QTcpServer *server = qobject_cast< QTcpServer* >(sender());
    if (Q_UNLIKELY(!server)) {
        qCWarning(httpTransportDC) << "Could not retrieve sender server! This is a serious bug!";
        return;
    }
    QTcpSocket *socket = server->nextPendingConnection();

    m_socketToState.insert(socket, HTTPInternalServerState::WaitingForRequest);

    // Parenting to the socket. This way, when the socket dies, the timer does too.
    QTimer *timeoutTimer = new QTimer(socket);
    timeoutTimer->setSingleShot(true);
    timeoutTimer->setInterval(CONNECTION_TIMEOUT);
    timeoutTimer->start();

    http_parser *parser = new http_parser();
    http_parser_init(parser, HTTP_BOTH);

    auto onReadyRead = [this, socket, timeoutTimer, parser] {
        if (m_socketToState.value(socket) == HTTPInternalServerState::WaitingForRequest) {
            Hyperspace::Wave wave;
            m_waveToSocket.insert(wave.id(), socket);
            m_waveStorage.insert(wave.id(), wave);

            m_parserToWave.insert(parser, wave.id());
            m_socketToState.insert(socket, HTTPInternalServerState::ParsingRequest);
        }
        QByteArray data = socket->readAll();
        int read = data.count();
        qCDebug(httpTransportDC) << "Read data" << read;
        int parsed = http_parser_execute(parser, m_parserSettings, data.constData(), read);
        if (read != parsed) {
            qCWarning(httpTransportDC) << "data lost" << read << parsed;
        }
    };

    // Socket read
    connect(socket, &QTcpSocket::readyRead, this, onReadyRead);
    if (socket->bytesAvailable() > 0) {
        onReadyRead();
    }

    // Socket disconnect
    connect(socket, &QTcpSocket::disconnected, this,
            [this, socket, parser] {
                m_socketToState.remove(socket);
                quint64 id = m_waveToSocket.key(socket);
                m_waveToSocket.remove(m_waveToSocket.key(socket));
                m_waveStorage.remove(id);
                m_keepAliveSockets.remove(socket);
                m_parserToWave.remove(parser);
                socket->deleteLater();
                delete parser;
            });

    // Timeout
    connect(timeoutTimer, &QTimer::timeout, socket, &QTcpSocket::close);
}

void HTTPTransport::triggerWave(Hyperspace::Wave wave, QTcpSocket* socket)
{
    qCDebug(httpTransportDC) << "Trigger wave!";

    auto sendPayloadRebound = [this, wave] (const QByteArray &payload) {
        Hyperspace::Rebound r(wave, Hyperspace::ResponseCode::OK);
        r.setPayload(payload);
        rebound(r);
    };

    // Wait! Are we on a special case?
    if (wave.target().startsWith("/control/")) {
        handleControlWave(wave, socket);
        return;
    } else if (wave.method() == METHOD_OPTIONS) {
        auto joinByteArrayList = [this] (const QList<QByteArray> &list) -> QByteArray {
            QByteArray ret;
            for (const QByteArray &ba : list) {
                ret.append(ba);
                ret.append(',');
            }
            ret.chop(1);
            return ret;
        };

        if (wave.target() == "/") {
            Hyperdrive::RemoteByteArrayListOperation *op = listHyperdriveInterfaces();
            connect(op, &Hemera::Operation::finished, this, [this, op, wave, joinByteArrayList, sendPayloadRebound] {
                if (op->isError()) {
                    rebound(Hyperspace::Rebound(wave, Hyperspace::ResponseCode::InternalError));
                    return;
                }
                sendPayloadRebound(joinByteArrayList(op->result()));
            });
            return;
        } else if (wave.target().count('/') == 1 || (wave.target().count('/') == 2 && wave.target().endsWith('/'))) {
            QByteArray path = wave.target();
            while (path.indexOf('/') >= 0) {
                path.remove(path.indexOf('/'), 1);
            }

            Hyperdrive::RemoteByteArrayListOperation *op = listGatesForHyperdriveInterface(path);
            connect(op, &Hemera::Operation::finished, this, [this, op, wave, joinByteArrayList, sendPayloadRebound] {
                if (op->isError()) {
                    rebound(Hyperspace::Rebound(wave, Hyperspace::ResponseCode::InternalError));
                    return;
                }

                QList<QByteArray> gates = op->result();
                if (gates.isEmpty()) {
                    rebound(Hyperspace::Rebound(wave, Hyperspace::ResponseCode::NotFound));
                } else {
                    sendPayloadRebound(joinByteArrayList(gates));
                }
            });
            return;
        }
    } else if (wave.method() == METHOD_GET) {
        // Special case
        if (wave.target() == "/") {
            QJsonArray interfacesArray;
            for (QByteArray interface : introspection().keys()) {
                interfacesArray.append(QLatin1String(interface));
            }
            sendPayloadRebound(QJsonDocument(interfacesArray).toJson(QJsonDocument::Compact));

        } else if (HTTPTransportCache::instance()->isCached(wave.target())) {
            qCDebug(httpTransportDC) << "GETting cached target " << wave.target();
            QByteArray ret = HTTPTransportCache::instance()->persistentEntry(wave.target());
            sendPayloadRebound(ret);
        } else if (HTTPTransportCache::instance()->hasTree(wave.target())) {
            qCDebug(httpTransportDC) << "GETting " << wave.target() << "as a tree";
            QByteArray ret = HTTPTransportCache::instance()->tree(wave.target()).toJson(QJsonDocument::Compact);
            sendPayloadRebound(ret);
        } else {
            qCDebug(httpTransportDC) << "Target " << wave.target() << "was not cached";
            rebound(Hyperspace::Rebound(wave, Hyperspace::ResponseCode::NotFound));
        }
        return;
    } else if (wave.method() == METHOD_POST || wave.method() == METHOD_PUT) {
        // Convert the payload to BSON
        QByteArray bsonPayload = waveToBSONPayload(wave);
        wave.setPayload(bsonPayload);
    }

    wave.addAttribute("X-Hyperspace-ClientAddress", socket->peerAddress().toString().toLatin1());

    routeWave(wave, -1);
}

QByteArray HTTPTransport::waveToBSONPayload(const Hyperspace::Wave &wave)
{
    if (wave.payload().isEmpty()) {
        // Handle unset case
        return QByteArray();
    }

    Hyperspace::Util::BSONSerializer serializer;
    QByteArray mimeType = wave.attributes().value("Content-Type");

    if (mimeType.startsWith("application/octet-stream")) {
        // Raw binary
        serializer.appendBinaryValue("v", wave.payload());
        qCDebug(httpTransportDC) << "Converting wave payload to BSON Binary";

    } else if (mimeType.startsWith("application/json")) {
        QJsonDocument doc = QJsonDocument::fromJson(wave.payload());
        QJsonObject obj = doc.object();
        QJsonValue value;
        if (obj.contains(QStringLiteral("v"))) {
            value = obj.value(QStringLiteral("v"));
        } else if (obj.contains(QStringLiteral("value"))) {
            value = obj.value(QStringLiteral("value"));
        }
        switch (value.type()) {
            case QJsonValue::Bool: {
                serializer.appendBooleanValue("v", value.toBool());
                qCDebug(httpTransportDC) << "Converting wave JSON payload to BSON Bool";
                break;
            }

            case QJsonValue::Double: {
                // Check if it's an int or a double, thanks Javascript for not having ints
                int intValue = value.toInt();
                double doubleValue = value.toDouble();
                qint64 longLongValue = (qint64) doubleValue;

                if ((double) intValue == doubleValue) {
                    serializer.appendInt32Value("v", intValue);
                    qCDebug(httpTransportDC) << "Converting wave JSON payload to BSON Int32";
                } else if ((double) longLongValue == doubleValue) {
                    serializer.appendInt64Value("v", longLongValue);
                    qCDebug(httpTransportDC) << "Converting wave JSON payload to BSON Int64";
                } else {
                    serializer.appendDoubleValue("v", doubleValue);
                    qCDebug(httpTransportDC) << "Converting wave JSON payload to BSON Double";
                }

                break;
            }

            case QJsonValue::String: {
                QString stringValue = value.toString();
                // Try to see if it's a datetime, using ISO 8601
                QDateTime dateValue = QDateTime::fromString(stringValue, Qt::ISODate);
                if (dateValue.isValid()) {
                    serializer.appendDateTime("v", dateValue);
                    qCDebug(httpTransportDC) << "Converting wave JSON payload to BSON DateTime";
                } else {
                    serializer.appendString("v", stringValue);
                    qCDebug(httpTransportDC) << "Converting wave JSON payload to BSON String";
                }
                break;
            }

            default:
                qCWarning(httpTransportDC) << "Can't convert wave JSON payload to BSON";
        }

    } else {
        // Raw value and no mimetype, heuristic conversion
        QVariant value = byteArrayToVariantHeuristic(wave.payload());

        switch (value.type()) {
            case QVariant::Int:
                serializer.appendInt32Value("v", value.toInt());
                qCDebug(httpTransportDC) << "Converting wave payload to BSON Int32";
                break;
            case QVariant::LongLong:
                serializer.appendInt64Value("v", value.toLongLong());
                qCDebug(httpTransportDC) << "Converting wave payload to BSON Int64";
                break;
            case QVariant::Double:
                serializer.appendDoubleValue("v", value.toDouble());
                qCDebug(httpTransportDC) << "Converting wave payload to BSON Double";
                break;
            case QVariant::Bool:
                serializer.appendBooleanValue("v", value.toBool());
                qCDebug(httpTransportDC) << "Converting wave payload to BSON Bool";
                break;
            case QVariant::DateTime:
                serializer.appendDateTime("v", value.toDateTime());
                qCDebug(httpTransportDC) << "Converting wave payload to BSON DateTime";
                break;
            case QVariant::String:
                serializer.appendString("v", value.toString());
                qCDebug(httpTransportDC) << "Converting wave payload to BSON String";
                break;

            default:
                qCWarning(httpTransportDC) << "Can't convert wave payload to BSON";
        }
    }

    serializer.appendEndOfDocument();

    return serializer.document();
}

QVariant HTTPTransport::byteArrayToVariantHeuristic(const QByteArray &value) {
    bool ok;

    int intValue = value.toInt(&ok, 10);
    if (ok) {
        return QVariant(intValue);
    }

    qint64 longLongValue = value.toLongLong(&ok, 10);
    if (ok) {
        return QVariant(longLongValue);
    }

    double doubleValue = value.toDouble(&ok);
    if (ok) {
        return QVariant(doubleValue);
    }

    if (value == "true" || value == "false") {
        bool boolValue = value == "true";
        return QVariant(boolValue);
    }

    QString stringValue = QString::fromLatin1(value);
    QDateTime dateValue = QDateTime::fromString(stringValue, Qt::ISODate);
    if (dateValue.isValid()) {
        return QVariant(dateValue);
    } else {
        return QVariant(stringValue);
    }
}

void HTTPTransport::handleControlWave(const Hyperspace::Wave &wave, QTcpSocket *socket)
{
    qCDebug(httpTransportDC) << "Received control message " << wave.method() << wave.target() << "with payload" << wave.payload();

    QByteArray controlTarget = wave.target();
    // Remove /control
    controlTarget.remove(0, 8);

    if (wave.method() == METHOD_POST) {
        if (controlTarget == "/callbacks") {
            QJsonDocument doc = QJsonDocument::fromJson(wave.payload());
            if (Q_UNLIKELY(!doc.isObject())) {
                qCWarning(httpTransportDC) << "Subscribe request doesn't contain a valid JSON payload";
                rebound(Hyperspace::Rebound(wave, Hyperspace::ResponseCode::BadRequest));
                return;
            }

            QJsonObject subscribeParams = doc.object();
            QByteArray prefix = subscribeParams.value(QStringLiteral("prefix")).toString().toLatin1();
            if (Q_UNLIKELY(prefix.isEmpty())) {
                qCWarning(httpTransportDC) << "Subscribe JSON doesn't contain a valid prefix: " << prefix;
                rebound(Hyperspace::Rebound(wave, Hyperspace::ResponseCode::BadRequest));
                return;
            }

            QUrl callbackUrl = subscribeParams.value(QStringLiteral("callback_url")).toString();
            if (!callbackUrl.isValid()) {
                callbackUrl.clear();

                // callback_path MUST be present if callback_url is not present, the other fields are optional
                QString callbackPath = subscribeParams.value(QStringLiteral("callback_path")).toString();
                if (Q_UNLIKELY(callbackPath.isEmpty())) {
                    qCWarning(httpTransportDC) << "Subscribe JSON for prefix" << prefix << "doesn't contain a valid callback_url or callback_path";
                    rebound(Hyperspace::Rebound(wave, Hyperspace::ResponseCode::BadRequest));
                    return;
                }
                callbackUrl.setPath(callbackPath);

                QString callbackProtocol = subscribeParams.value(QStringLiteral("callback_protocol")).toString();
                if (!callbackProtocol.isEmpty()) {
                    callbackUrl.setScheme(callbackProtocol);
                } else {
                    // Default HTTP
                    callbackUrl.setScheme(QStringLiteral("http"));
                }

                QString callbackHost = subscribeParams.value(QStringLiteral("callback_host")).toString();
                if (!callbackHost.isEmpty()) {
                    callbackUrl.setHost(callbackHost);
                } else {
                    // Default sender
                    callbackUrl.setHost(socket->peerAddress().toString());
                }

                int callbackPort = subscribeParams.value(QStringLiteral("callback_port")).toInt(-1);
                if (callbackPort >= 0 && callbackPort <= 65535) {
                    callbackUrl.setPort(callbackPort);
                } else {
                    // Default 80
                    callbackUrl.setPort(80);
                }

                if (Q_UNLIKELY(!callbackUrl.isValid())) {
                    qCWarning(httpTransportDC) << "Subscribe JSON for prefix" << prefix << "didn't construct a valid url:" << callbackUrl;
                    rebound(Hyperspace::Rebound(wave, Hyperspace::ResponseCode::BadRequest));
                    return;
                }
            }

            qCInfo(httpTransportDC) << "Subscribe request for prefix " << prefix << "with callback Url" << callbackUrl;

            QJsonObject response{{QStringLiteral("callback_url"), callbackUrl.toString()}, {QStringLiteral("prefix"), QLatin1String(prefix)}};

            QDateTime callbackExpiry;
            bool persistent = subscribeParams.value(QStringLiteral("persistent")).toBool();
            if (persistent) {
                int expiry = subscribeParams.value(QStringLiteral("expiry")).toInt(-1);
                if (expiry > 0) {
                    callbackExpiry = QDateTime::currentDateTime().addSecs(expiry);
                    response.insert(QStringLiteral("expiry"), callbackExpiry.toString());
                } else {
                    qCWarning(httpTransportDC) << "Subscribe request is persistent but doesn't have an expiry";
                    rebound(Hyperspace::Rebound(wave, Hyperspace::ResponseCode::BadRequest));
                    return;
                }
            }

            int id = m_callbackManager->subscribe(prefix, callbackUrl, callbackExpiry, persistent);
            if (id < 0) {
                rebound(Hyperspace::Rebound(wave, Hyperspace::ResponseCode::BadRequest));
                return;
            }

            response.insert(QStringLiteral("id"), id);

            Hyperspace::Rebound r(wave, Hyperspace::ResponseCode::Created);
            r.setPayload(QJsonDocument(response).toJson(QJsonDocument::Compact));
            rebound(r);
            return;
        }
    } else if (wave.method() == METHOD_DELETE) {
        if (controlTarget.startsWith("/callbacks")) {
            QByteArray idPath = controlTarget;
            idPath.replace("/callbacks/", "");
            bool ok;
            int id = idPath.toInt(&ok, 10);
            if (ok) {
                qCInfo(httpTransportDC) << "Unsubscribe request for id " << id;
                if (!m_callbackManager->unsubscribe(id)) {
                    rebound(Hyperspace::Rebound(wave, Hyperspace::ResponseCode::NotFound));
                } else {
                    rebound(Hyperspace::Rebound(wave, Hyperspace::ResponseCode::NoResponse));
                }
            } else {
                qCWarning(httpTransportDC) << "Invalid id in unsubscribe request: " << idPath;
                rebound(Hyperspace::Rebound(wave, Hyperspace::ResponseCode::BadRequest));
            }
            return;
        }
    }

    // Default, let hyperdrive handle it
    routeWave(wave, -1);
}

QByteArray HTTPTransport::statusString(int statusCode)
{
    QByteArray ret = "HTTP/1.1 %i %s\r\n";
    ret.replace("%i", QByteArray::number(statusCode));

    switch (statusCode) {
        case 200:
            ret.replace("%s", "OK");
            break;
        case 202:
            ret.replace("%s", "Accepted");
            break;
        case 304:
            ret.replace("%s", "Not Modified");
            break;
        case 400:
            ret.replace("%s", "Bad Request");
            break;

        case 401:
            ret.replace("%s", "Unauthorized");
            break;

        case 500:
            ret.replace("%s", "Internal Server Error");
            break;

        case 403:
            ret.replace("%s", "Forbidden");
            break;

        case 404:
            ret.replace("%s", "Not Found");
            break;

        case 405:
            ret.replace("%s", "Method Not Allowed");
            break;

        case 422:
            ret.replace("%s", "Unprocessable Entity");
            break;

        default:
            ret.replace("%s", "Unknown");
            break;
    }

    return ret;
}

void HTTPTransport::rebound(const Hyperspace::Rebound& r, int fd)
{
    Hyperspace::Rebound rebound = r;
    QTcpSocket *socket = m_waveToSocket.value(rebound.id(), Q_NULLPTR);

    if (Q_UNLIKELY(socket == Q_NULLPTR)) {
        qCWarning(httpTransportDC) << "wave id " << rebound.id() << " not found!!" << m_waveToSocket;
        return;
    }

    QByteArray data;

    data.append(HTTPTransport::statusString(static_cast<quint16>(rebound.response())));

    for (Hyperspace::ByteArrayHash::const_iterator i = rebound.attributes().constBegin(); i != rebound.attributes().constEnd(); ++i) {
        data.append(i.key());
        data.append(": ");
        data.append(i.value());
        data.append("\r\n");
    }

    // Handle the payload!
    QByteArray actualPayload = rebound.payload();

    QByteArray target = m_waveStorage.value(rebound.id()).target();
    target.remove(0, 1);

    data.append(QByteArray("Content-Length: %1\r\n\r\n").replace("%1", QByteArray::number(actualPayload.size())));

    if (!actualPayload.isEmpty()) {
        data.append(actualPayload);
    }

    qint64 written = socket->write(data);
    qCDebug(httpTransportDC) << "Writing data to socket" << written;

    // Remove the wave from the storage
    m_waveToSocket.remove(rebound.id());
    m_waveStorage.remove(rebound.id());
    m_socketToState.insert(socket, HTTPInternalServerState::WaitingForRequest);

    if (!m_keepAliveSockets.contains(socket) || rebound.response() == Hyperspace::ResponseCode::BadRequest ||
        rebound.response() == Hyperspace::ResponseCode::InternalError) {
        // Allow some time for encryption, in case the socket is SSL.
        socket->waitForBytesWritten(5000);
        socket->close();
    }
}

void HTTPTransport::fluctuation(const Hyperspace::Fluctuation &fluctuation)
{
    qCDebug(httpTransportDC) << "Got a fluctuation from" << fluctuation.target() << fluctuation.payload();
}

void HTTPTransport::cacheMessage(const CacheMessage &cacheMessage)
{
    Hyperspace::Util::BSONDocument doc(cacheMessage.payload());
    // TODO: handle aggregates
    QByteArray deserializedPayload = doc.value("v").toByteArray();
    qCDebug(httpTransportDC) << "Received cache message from: " << cacheMessage.target() << deserializedPayload;
    switch (cacheMessage.interfaceType()) {
        case Hyperdrive::Interface::Type::Properties: {
            if (HTTPTransportCache::instance()->isCached(cacheMessage.target())
                && HTTPTransportCache::instance()->persistentEntry(cacheMessage.target()) == deserializedPayload) {

                qCDebug(httpTransportDC) << cacheMessage.target() << "is not changed, not sending it again";
                // TODO: remove it from the database, consider it delivered
                return;
            }

            // Here we check the original payload, empty payload != empty BSON document
            if (cacheMessage.payload().isEmpty()) {
                HTTPTransportCache::instance()->removePersistentEntry(cacheMessage.target());
            } else {
                HTTPTransportCache::instance()->insertOrUpdatePersistentEntry(cacheMessage.target(), deserializedPayload);
            }
            break;
        }

        case Hyperdrive::Interface::Type::DataStream: {
            break;
        }

        default: {
            qCDebug(httpTransportDC) << "Invalid interface type in cacheMessage";
            return;
        }
    }

    // Check if we have callbacks
    m_callbackManager->checkCallbacks(cacheMessage);
}

void HTTPTransport::bigBang()
{
    qCDebug(httpTransportDC) << "Received bigBang";
}

}

TRANSPORT_MAIN(Hyperdrive::HTTPTransport, HTTP, 1.0.0)
