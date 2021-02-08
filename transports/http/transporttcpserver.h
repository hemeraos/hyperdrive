#ifndef TRANSPORTTCPSERVER_H
#define TRANSPORTTCPSERVER_H

#include <QtNetwork/QTcpServer>
#include <QtNetwork/QSslConfiguration>

class QSslConfiguration;
class QSslError;
class TransportTCPServer : public QTcpServer
{
    Q_OBJECT
    Q_DISABLE_COPY(TransportTCPServer)

public:
    explicit TransportTCPServer(QObject* parent = 0);
    virtual ~TransportTCPServer();

    void setSslConfiguration(const QSslConfiguration &sslConfiguration);

protected:
    virtual void incomingConnection(qintptr handle) override final;

Q_SIGNALS:
    void gotFd(qintptr fd);
    void sslErrors(const QList<QSslError> &errors);
    void peerVerifyError(const QSslError &error);
    void newEncryptedConnection();

private:
    QSslConfiguration m_sslConfiguration;
};

#endif // TRANSPORTTCPSERVER_H
