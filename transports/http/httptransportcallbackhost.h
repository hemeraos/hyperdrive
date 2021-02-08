#ifndef HTTP_TRANSPORT_CALLBACK_HOST
#define HTTP_TRANSPORT_CALLBACK_HOST

#include <QtCore/QObject>
#include <QtCore/QQueue>
#include <QtCore/QSet>
#include <QtCore/QTimer>
#include <QtCore/QUrl>

#include <QtNetwork/QNetworkAccessManager>

namespace Hyperdrive {
class CacheMessage;
}

class HTTPTransportCallbackManager;

class HTTPTransportCallbackHost : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(HTTPTransportCallbackHost)

public:
    HTTPTransportCallbackHost(QString host, QNetworkAccessManager *networkAccessManager, HTTPTransportCallbackManager *parent);

    ~HTTPTransportCallbackHost();

    QString host() const;
    void setHost(const QString &host);

    bool hasCallbacks() const;

    void addCallback(int id, QUrl url);
    void removeCallback(int id);
    void triggerCallback(int id, const Hyperdrive::CacheMessage &cacheMessage);

private:
    HTTPTransportCallbackManager *m_callbackManager;

    bool m_busy;
    QString m_host;
    QTimer *m_retryTimer;
    QHash<int, QUrl> m_callbackIdToUrl;
    QNetworkAccessManager *m_networkAccessManager;
    QQueue<Hyperdrive::CacheMessage> m_messageQueue;
};

#endif
