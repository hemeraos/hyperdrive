#ifndef HTTP_TRANSPORT_CALLBACK_MANAGER
#define HTTP_TRANSPORT_CALLBACK_MANAGER

#include <HemeraCore/AsyncInitObject>

#include <QtCore/QHash>
#include <QtCore/QMultiMap>
#include <QtCore/QTimer>

namespace Hyperdrive {
class CacheMessage;
}

class HTTPTransportCallback;
class HTTPTransportCallbackHost;
class QNetworkAccessManager;

class HTTPTransportCallbackManager : public Hemera::AsyncInitObject
{
    Q_OBJECT
    Q_DISABLE_COPY(HTTPTransportCallbackManager)

public:
    HTTPTransportCallbackManager(QObject *parent = nullptr);

    ~HTTPTransportCallbackManager();

    int subscribe(QByteArray prefix, QUrl url, QDateTime expiry, bool persistent);
    bool unsubscribe(int id);
    void checkCallbacks(const Hyperdrive::CacheMessage &cacheMessage);

    QByteArray hardwareId() const;

protected:
    virtual void initImpl() override final;

private:
    int m_callbackIdCounter;
    QByteArray m_hardwareId;

    QMultiMap< QByteArray, int > m_prefixToIds;
    QHash< int, HTTPTransportCallback *> m_idToCallback;

    QHash< QString, HTTPTransportCallbackHost *> m_callbackHosts;

    QNetworkAccessManager *m_networkAccessManager;
};

#endif
