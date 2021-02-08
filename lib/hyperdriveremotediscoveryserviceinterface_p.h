#ifndef HYPERDRIVE_REMOTEDISCOVERYSERVICEINTERFACE_P_H
#define HYPERDRIVE_REMOTEDISCOVERYSERVICEINTERFACE_P_H

#include "hyperdriveremotediscoveryserviceinterface.h"

#include "hyperdrivediscoveryservice_p.h"

namespace Hyperdrive {

class Core;
class DiscoveryManager;

class RemoteDiscoveryServiceInterfacePrivate : public DiscoveryServicePrivate
{
public:
    RemoteDiscoveryServiceInterfacePrivate(RemoteDiscoveryServiceInterface* q) : DiscoveryServicePrivate(q) {}
    virtual ~RemoteDiscoveryServiceInterfacePrivate() {}

    Q_DECLARE_PUBLIC(RemoteDiscoveryServiceInterface)

    virtual void initHook() Q_DECL_OVERRIDE Q_DECL_FINAL;

    Hyperspace::Socket *socket;
    QString name;
    Core *core;
};
}

#endif // HYPERDRIVE_REMOTEDISCOVERYSERVICEINTERFACE_P_H
