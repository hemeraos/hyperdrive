#include "hyperdrivelocalserver.h"

namespace Hyperdrive {

LocalServer::LocalServer(QObject* parent)
    : QLocalServer(parent)
{
}

LocalServer::~LocalServer()
{
}

void LocalServer::incomingConnection(quintptr socketDescriptor)
{
    Q_EMIT newNativeConnection(socketDescriptor);
}

}
