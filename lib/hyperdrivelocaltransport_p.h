/*
 *
 */

#ifndef HYPERDRIVE_LOCALTRANSPORT_P_H
#define HYPERDRIVE_LOCALTRANSPORT_P_H

#include "hyperdrivelocaltransport.h"

#include "hyperdrivetransport_p.h"

namespace Hyperdrive {

class Core;

class LocalTransportPrivate : public TransportPrivate
{
    Q_DECLARE_PUBLIC(LocalTransport)
public:
    LocalTransportPrivate(LocalTransport *q) : TransportPrivate(q), core(Q_NULLPTR) {}

    Core *core;
};

}

#endif // HYPERDRIVE_LOCALTRANSPORT_H
