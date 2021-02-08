#ifndef HYPERDRIVE_LOCALDISCOVERYSERVICE_P_H
#define HYPERDRIVE_LOCALDISCOVERYSERVICE_P_H

#include "hyperdrivelocaldiscoveryservice.h"

#include "hyperdrivediscoveryservice_p.h"

namespace Hyperdrive {

class Core;
class DiscoveryManager;

class LocalDiscoveryServicePrivate : public DiscoveryServicePrivate
{
public:
    LocalDiscoveryServicePrivate(LocalDiscoveryService* q) : DiscoveryServicePrivate(q) {}
    virtual ~LocalDiscoveryServicePrivate() {}

    Q_DECLARE_PUBLIC(LocalDiscoveryService)

    virtual void initHook() Q_DECL_OVERRIDE Q_DECL_FINAL;

    Core *core;
    DiscoveryManager *manager;
};
}

#endif // HYPERDRIVE_LOCALDISCOVERYSERVICE_P_H
