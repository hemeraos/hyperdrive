/*
 *
 */

#ifndef HYPERDRIVE_TRANSPORT_P_H
#define HYPERDRIVE_TRANSPORT_P_H

#include "hyperdrivetransport.h"

#include <private/HemeraCore/hemeraasyncinitobject_p.h>

namespace Hyperdrive {

class Core;

class TransportPrivate : public Hemera::AsyncInitObjectPrivate
{
    Q_DECLARE_PUBLIC(Transport)
public:
    TransportPrivate(Transport *q) : AsyncInitObjectPrivate(q) {}
    virtual ~TransportPrivate() {}

    virtual void initHook() Q_DECL_OVERRIDE;

    QString name;
};

}

#endif // HYPERDRIVE_TRANSPORT_H
