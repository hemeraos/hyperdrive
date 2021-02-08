#ifndef HYPERDRIVE_LOCALSERVER_H
#define HYPERDRIVE_LOCALSERVER_H

#include <QtNetwork/QLocalServer>

namespace Hyperdrive {

class LocalServer : public QLocalServer
{
    Q_OBJECT
    Q_DISABLE_COPY(LocalServer)

public:
    explicit LocalServer(QObject* parent = nullptr);
    virtual ~LocalServer();

protected:
    virtual void incomingConnection(quintptr socketDescriptor) override final;

Q_SIGNALS:
    void newNativeConnection(quintptr fd);

private:
};
}

#endif // HYPERDRIVE_LOCALSERVER_H
