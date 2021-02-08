/*
 *
 */

#ifndef HYPERDRIVE_TRANSPORT_H
#define HYPERDRIVE_TRANSPORT_H

#include "cachemessage.h"

#include <HemeraCore/AsyncInitObject>
#include <HyperspaceCore/Global>

#include <HyperspaceCore/Wave>
#include <HyperspaceCore/Fluctuation>
#include <HyperspaceCore/Rebound>

namespace Hyperdrive {

class Core;

class TransportPrivate;
class Transport : public Hemera::AsyncInitObject
{
    Q_OBJECT
    Q_DISABLE_COPY(Transport)
    Q_DECLARE_PRIVATE_D(d_h_ptr, Transport)

    Q_PROPERTY(QString name READ name)

public:
    enum class Feature {
        None = 0,
        Reliable = 1 << 0,
        Unreliable = 1 << 1,
        Secure = 1 << 2,
        Insecure = 1 << 3,
    };
    Q_ENUMS(Feature)
    Q_DECLARE_FLAGS(Features, Feature)

    virtual ~Transport();

    QString name() const;

    virtual void rebound(const Hyperspace::Rebound &rebound, int fd = -1) = 0;
    virtual void fluctuation(const Hyperspace::Fluctuation &fluctuation) = 0;
    virtual void cacheMessage(const CacheMessage &cacheMessage) = 0;
    virtual void bigBang() = 0;

protected:
    explicit Transport(TransportPrivate &dd, const QString &name, QObject *parent = Q_NULLPTR);

    virtual void routeWave(const Hyperspace::Wave &wave, int fd) = 0;
};
}

Q_DECLARE_OPERATORS_FOR_FLAGS(Hyperdrive::Transport::Features)

#endif // HYPERDRIVE_TRANSPORT_H
