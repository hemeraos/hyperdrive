#ifndef HTTP_TRANSPORT_CALLBACK
#define HTTP_TRANSPORT_CALLBACK

#include <QtCore/QByteArray>
#include <QtCore/QDateTime>
#include <QtCore/QString>
#include <QtCore/QUrl>

class HTTPTransportCallback
{
public:
    HTTPTransportCallback();
    HTTPTransportCallback(int id, QByteArray prefix, QUrl url, QString host, QDateTime expiry);

    ~HTTPTransportCallback();

    int id() const;
    void setId(int id);

    QByteArray prefix() const;
    void setPrefix(const QByteArray &prefix);

    QUrl url() const;
    void setUrl(const QUrl &url);

    QString host() const;
    void setHost(const QString &host);

    QDateTime expiry() const;
    void setExpiry(const QDateTime &expiry);

private:
    int m_id;
    QByteArray m_prefix;
    QUrl m_url;
    QString m_host;
    QDateTime m_expiry;
};

#endif
