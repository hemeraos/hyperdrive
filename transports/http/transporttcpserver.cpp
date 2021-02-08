#include "transporttcpserver.h"

#include <QtNetwork/QSslSocket>
#include <QtNetwork/QTcpSocket>
#include <QtNetwork/QSslConfiguration>

#define MIN_PORT 1024
#define MAX_PORT 1048

TransportTCPServer::TransportTCPServer(QObject* parent)
    : QTcpServer(parent)
{
}

TransportTCPServer::~TransportTCPServer()
{

}

void TransportTCPServer::setSslConfiguration(const QSslConfiguration& sslConfiguration)
{
    m_sslConfiguration = sslConfiguration;
}

void TransportTCPServer::incomingConnection(qintptr handle)
{
    // Check if we need SSL
    if (!m_sslConfiguration.isNull()) {
        QSslSocket *pSslSocket = new QSslSocket();

        if (Q_LIKELY(pSslSocket)) {
            pSslSocket->setSslConfiguration(m_sslConfiguration);

            if (Q_LIKELY(pSslSocket->setSocketDescriptor(handle))) {
                connect(pSslSocket, &QSslSocket::peerVerifyError, this, &TransportTCPServer::peerVerifyError);

                typedef void (QSslSocket::* sslErrorsSignal)(const QList<QSslError> &);
                connect(pSslSocket, static_cast<sslErrorsSignal>(&QSslSocket::sslErrors),
                        this, &TransportTCPServer::sslErrors);
                connect(pSslSocket, &QSslSocket::encrypted, [this, pSslSocket] {
                        addPendingConnection(pSslSocket);
                        Q_EMIT newEncryptedConnection();
                        });

                pSslSocket->startServerEncryption();
            } else {
                pSslSocket->deleteLater();
            }
        }
    } else {
        QTcpSocket *socket = new QTcpSocket();
        if (Q_LIKELY(socket)) {
            if (Q_LIKELY(socket->setSocketDescriptor(handle))) {
                addPendingConnection(socket);
            } else {
                socket->deleteLater();
            }
        }
    }
}
