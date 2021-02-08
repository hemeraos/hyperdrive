#include "astarteendpoint.h"
#include "astarteendpoint_p.h"

#include "astartecrypto.h"

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

#include <private/HemeraCore/hemeraasyncinitobject_p.h>

Q_LOGGING_CATEGORY(astarteEndpointDC, "astarte.endpoint", DEBUG_MESSAGES_DEFAULT_LEVEL)

namespace Astarte {

Endpoint::Endpoint(EndpointPrivate &dd, const QUrl &endpoint, QObject* parent)
    : AsyncInitObject(dd, parent)
{
    Q_D(Endpoint);
    d->endpoint = endpoint;
}

Endpoint::~Endpoint()
{
}

QUrl Endpoint::endpoint() const
{
    Q_D(const Endpoint);
    return d->endpoint;
}

QString Endpoint::endpointVersion() const
{
    return QString();
}

}

#include "moc_astarteendpoint.cpp"
