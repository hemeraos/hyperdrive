#include "httptransportcallbackmanager.h"

#include "httptransportcallback.h"
#include "httptransportcallbackhost.h"

#include <cachemessage.h>

#include <QtCore/QLoggingCategory>

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>

#include <HemeraCore/CommonOperations>
#include <HemeraCore/Fingerprints>
#include <HemeraCore/Literals>

Q_LOGGING_CATEGORY(callbackManagerDC, "hyperdrive.transport.http.callbackmanager", DEBUG_MESSAGES_DEFAULT_LEVEL)

HTTPTransportCallbackManager::HTTPTransportCallbackManager(QObject *parent)
    : Hemera::AsyncInitObject(parent)
    , m_callbackIdCounter(0)
    , m_networkAccessManager(new QNetworkAccessManager())
{
    // TODO: make this configurable
    connect(m_networkAccessManager, &QNetworkAccessManager::sslErrors, [] (QNetworkReply *reply, const QList<QSslError> &errors) {
        qCDebug(callbackManagerDC) << "Ignoring this SSL errors: " << errors;
        reply->ignoreSslErrors();
    });
}

HTTPTransportCallbackManager::~HTTPTransportCallbackManager()
{
}

void HTTPTransportCallbackManager::initImpl()
{
    Hemera::ByteArrayOperation *op = Hemera::Fingerprints::globalHardwareId();
    connect(op, &Hemera::Operation::finished, this, [this, op] {
        if (op->isError()) {
            setInitError(Hemera::Literals::literal(Hemera::Literals::Errors::failedRequest()), tr("Could not retrieve global hardware ID!"));
            return;
        }

        m_hardwareId = op->result();

        setReady();
    });
}

QByteArray HTTPTransportCallbackManager::hardwareId() const
{
    return m_hardwareId;
}

int HTTPTransportCallbackManager::subscribe(QByteArray prefix, QUrl url, QDateTime expiry, bool persistent)
{
    int id = m_callbackIdCounter;

    QString host = url.host();
    if (host.isEmpty()) {
        qCWarning(callbackManagerDC) << "Invalid host in subscribe request for prefix " << prefix;
        return -1;
    }

    m_callbackIdCounter++;

    m_prefixToIds.insert(prefix, id);
    HTTPTransportCallback *callback = new HTTPTransportCallback(id, prefix, url, host, expiry);
    m_idToCallback.insert(id, callback);

    if (persistent) {
        // TODO: save the callback
    }

    if (!m_callbackHosts.contains(host)) {
        m_callbackHosts.insert(host, new HTTPTransportCallbackHost(host, m_networkAccessManager, this));
    }
    m_callbackHosts.value(host)->addCallback(id, url);

    return id;
}

bool HTTPTransportCallbackManager::unsubscribe(int id)
{
    if (!m_idToCallback.contains(id)) {
        return false;
    }
    QString host = m_idToCallback.value(id)->url().host();
    HTTPTransportCallbackHost *callbackHost = m_callbackHosts.value(host);
    callbackHost->removeCallback(id);
    if (!callbackHost->hasCallbacks()) {
        callbackHost->deleteLater();
        m_callbackHosts.remove(host);
    }

    QByteArray prefix = m_idToCallback.value(id)->prefix();
    m_prefixToIds.remove(prefix, id);

    delete m_idToCallback.value(id);
    m_idToCallback.remove(id);
    return true;
}

void HTTPTransportCallbackManager::checkCallbacks(const Hyperdrive::CacheMessage &cacheMessage)
{
    for (const QByteArray &prefix : m_prefixToIds.uniqueKeys()) {
        // We passed all the possible prefixes, it's an ordered map
        if (prefix > cacheMessage.target()) {
            break;
        }

        if (cacheMessage.target().startsWith(prefix)) {
            for (int callbackId : m_prefixToIds.values(prefix)) {
                QString host = m_idToCallback.value(callbackId)->url().host();
                m_callbackHosts.value(host)->triggerCallback(callbackId, cacheMessage);
            }
        }
    }
}
