/*
 *
 */

#ifndef HYPERDRIVE_HTTPTRANSPORT_H
#define HYPERDRIVE_HTTPTRANSPORT_H

#include "http_parser.h"

#include <hyperdriveremotetransport.h>

#include <QtCore/QSet>

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QSslConfiguration>

#include <HyperspaceCore/Wave>

class QSslConfiguration;
class TransportTCPServer;

class HTTPTransportCallbackManager;
class QTcpSocket;
class QTcpServer;
namespace Hyperdrive {

enum class HTTPInternalServerState
{
    WaitingForRequest,
    ParsingRequest
};

class HTTPTransport : public Hyperdrive::RemoteTransport
{
    Q_OBJECT

public:
    HTTPTransport(QObject *parent = Q_NULLPTR);
    virtual ~HTTPTransport();

    virtual void rebound(const Hyperspace::Rebound& rebound, int fd = -1) override final;
    virtual void fluctuation(const Hyperspace::Fluctuation& fluctuation) override final;
    virtual void cacheMessage(const CacheMessage& cacheMessage) override final;
    virtual void bigBang() override final;

    static HTTPTransport *instance();

    static int onUrl (http_parser *parser, const char *url, size_t urlLen);
    static int onStatus (http_parser *parser, const char *status, size_t statusLen);
    static int onHeaderField (http_parser *parser, const char *headerField, size_t headerLen);
    static int onHeaderValue (http_parser *parser, const char *headerValue, size_t valueLen);
    static int onBody (http_parser *parser, const char *body, size_t bodyLen);
    static int onMessageBegin (http_parser *parser);
    static int onHeadersComplete (http_parser *parser);
    static int onMessageComplete (http_parser *parser);



private Q_SLOTS:
    void clientConnected();

protected:
    virtual void initImpl();

private:
    HTTPTransportCallbackManager *m_callbackManager;
    QSslConfiguration sslConfiguration();
    void triggerWave(Hyperspace::Wave wave, QTcpSocket *socket);
    void handleControlWave(const Hyperspace::Wave &wave, QTcpSocket *socket);
    QByteArray statusString(int statusCode);
    QByteArray waveToBSONPayload(const Hyperspace::Wave &wave);
    QVariant byteArrayToVariantHeuristic(const QByteArray &value);

    QHash< quint64, QTcpSocket* > m_waveToSocket;
    QHash< http_parser*, quint64 > m_parserToWave;
    QHash< QTcpSocket*, HTTPInternalServerState > m_socketToState;
    QHash< quint64, Hyperspace::Wave > m_waveStorage;
    QSet< QTcpSocket* > m_keepAliveSockets;
    QSslConfiguration m_sslConfiguration;

    http_parser_settings *m_parserSettings;
};
}

#endif // HYPERDRIVE_HTTPTRANSPORT_H
