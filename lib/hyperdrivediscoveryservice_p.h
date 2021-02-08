#ifndef HYPERDRIVE_DISCOVERYSERVICE_P_H
#define HYPERDRIVE_DISCOVERYSERVICE_P_H

#include "hyperdrivediscoveryservice.h"

#include <private/HemeraCore/hemeraasyncinitobject_p.h>

namespace Hyperdrive {

class Core;
class DiscoveryManager;

class DiscoveryServicePrivate : public Hemera::AsyncInitObjectPrivate
{
public:
    DiscoveryServicePrivate(DiscoveryService* q) : AsyncInitObjectPrivate(q) {}
    virtual ~DiscoveryServicePrivate() {}

    Q_DECLARE_PUBLIC(DiscoveryService)

    virtual void initHook() Q_DECL_OVERRIDE;

    QByteArray name;
    QByteArray transportMedium;

    DiscoveryManager *discoveryManager;
};
}

#endif // HYPERDRIVE_DISCOVERYSERVICE_P_H
