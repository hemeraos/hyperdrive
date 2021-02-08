#include "httptransportcallback.h"

HTTPTransportCallback::HTTPTransportCallback()
{
}

HTTPTransportCallback::HTTPTransportCallback(int id, QByteArray prefix, QUrl url, QString host, QDateTime expiry)
    : m_id(id)
    , m_prefix(prefix)
    , m_url(url)
    , m_host(host)
    , m_expiry(expiry)
{
}

HTTPTransportCallback::~HTTPTransportCallback()
{
}

int HTTPTransportCallback::id() const
{
    return m_id;
}

QByteArray HTTPTransportCallback::prefix() const
{
    return m_prefix;
}

QUrl HTTPTransportCallback::url() const
{
    return m_url;
}

QDateTime HTTPTransportCallback::expiry() const
{
    return m_expiry;
}

void HTTPTransportCallback::setId(int id)
{
    m_id = id;
}

void HTTPTransportCallback::setPrefix(const QByteArray &prefix)
{
    m_prefix = prefix;
}

void HTTPTransportCallback::setUrl(const QUrl &url)
{
    m_url = url;
}

void HTTPTransportCallback::setExpiry(const QDateTime &expiry)
{
    m_expiry = expiry;
}
