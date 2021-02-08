#include "httptransportcallbackhost.h"

#include "httptransportcallbackmanager.h"

#include <cachemessage.h>

#include <HyperspaceCore/BSONDocument>

#include <QtCore/QLoggingCategory>

#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

Q_LOGGING_CATEGORY(httpTransportCallbackHostDC, "hyperdrive.transport.http.callbackhost", DEBUG_MESSAGES_DEFAULT_LEVEL)

HTTPTransportCallbackHost::HTTPTransportCallbackHost(QString host, QNetworkAccessManager *networkAccessManager, HTTPTransportCallbackManager *parent)
    : QObject(parent)
    , m_callbackManager(parent)
    , m_host(host)
    , m_networkAccessManager(networkAccessManager)
{
}

HTTPTransportCallbackHost::~HTTPTransportCallbackHost()
{
}

QString HTTPTransportCallbackHost::host() const
{
    return m_host;
}

void HTTPTransportCallbackHost::setHost(const QString &host)
{
    m_host = host;
}

bool HTTPTransportCallbackHost::hasCallbacks() const
{
    return !m_callbackIdToUrl.isEmpty();
}

void HTTPTransportCallbackHost::addCallback(int id, QUrl url)
{
    m_callbackIdToUrl.insert(id, url);
}

void HTTPTransportCallbackHost::removeCallback(int id)
{
    m_callbackIdToUrl.remove(id);
}

void HTTPTransportCallbackHost::triggerCallback(int id, const Hyperdrive::CacheMessage &cacheMessage)
{
    Hyperspace::Util::BSONDocument doc(cacheMessage.payload());
    // TODO: handle aggregates
    QByteArray deserializedPayload = doc.value("v").toByteArray();

    QString baseUrlString = m_callbackIdToUrl.value(id).toString(QUrl::StripTrailingSlash);
    // StripTrailingSlash sometimes leaves a trailing slash (see https://bugreports.qt.io/browse/QTBUG-35921), fix that
    if (baseUrlString.endsWith(QStringLiteral("/"))) {
        baseUrlString.remove(baseUrlString.length() - 1, 1);
    }
    QUrl fullUrl;
    fullUrl.setUrl(baseUrlString + QLatin1String(cacheMessage.target()));

    qCDebug(httpTransportCallbackHostDC) << "Sending POST to" << fullUrl;

    QNetworkRequest req(fullUrl);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("text/plain"));
    req.setRawHeader("X-Hardware-Id", m_callbackManager->hardwareId());
    req.setRawHeader("X-Subscription-Id", QByteArray::number(id));

    QNetworkReply *reply = m_networkAccessManager->post(req, deserializedPayload);
    connect(reply, &QNetworkReply::finished, [this, reply, fullUrl] {
        if (reply->error() == QNetworkReply::NoError) {
            qCDebug(httpTransportCallbackHostDC) << "POST to" << fullUrl << "delivered correctly";
        } else {
            qCWarning(httpTransportCallbackHostDC) << "POST to" << fullUrl << "error: " << reply->error();
        }
            reply->deleteLater();
    });
}
