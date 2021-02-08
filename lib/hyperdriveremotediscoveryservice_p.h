#ifndef HYPERDRIVE_REMOTEDISCOVERYSERVICE_P_H
#define HYPERDRIVE_REMOTEDISCOVERYSERVICE_P_H

#include "hyperdriveremotediscoveryservice.h"

#include "hyperdrivediscoveryservice_p.h"

namespace Hyperspace {
class Socket;
}

namespace Hyperdrive {

class Core;
class DiscoveryManager;

class RemoteDiscoveryServicePrivate : public DiscoveryServicePrivate
{
public:
    RemoteDiscoveryServicePrivate(RemoteDiscoveryService* q) : DiscoveryServicePrivate(q) {}
    virtual ~RemoteDiscoveryServicePrivate() {}

    Q_DECLARE_PUBLIC(RemoteDiscoveryService)

    virtual void initHook() Q_DECL_OVERRIDE Q_DECL_FINAL;
    void sendName();

    Hyperspace::Socket *socket;

    QHash< QUuid, RemoteByteArrayListOperation* > baListOperations;
    QHash< QUuid, RemoteUrlFeaturesOperation* > ufOperations;
};
}

#endif // HYPERDRIVE_REMOTEDISCOVERYSERVICE_P_H
