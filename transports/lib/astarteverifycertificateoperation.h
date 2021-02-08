#ifndef ASTARTEVERIFYCERTIFICATEOPERATION_H
#define ASTARTEVERIFYCERTIFICATEOPERATION_H

#include "astarteendpoint.h"
#include "astartecrypto.h"

#include <QtCore/QUrl>

#include <QtNetwork/QSslConfiguration>

#include <HemeraCore/Operation>

class QFile;

namespace Astarte {

class HTTPEndpoint;

class VerifyCertificateOperation : public Hemera::Operation
{
    Q_OBJECT
    Q_DISABLE_COPY(VerifyCertificateOperation)

public:
    explicit VerifyCertificateOperation(QFile &certFile, HTTPEndpoint *parent);
    explicit VerifyCertificateOperation(const QByteArray &certificate, HTTPEndpoint *parent);
    explicit VerifyCertificateOperation(const QSslCertificate &certificate, HTTPEndpoint *parent);
    virtual ~VerifyCertificateOperation();

protected:
    void startImpl();

private:
    void verify(const QByteArray &certificate);

    QByteArray m_certificate;
    HTTPEndpoint *m_endpoint;
};
}

#endif // ASTARTEVERIFYCERTIFICATEOPERATION_H
