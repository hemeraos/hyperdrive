/*
 *
 */

#ifndef HYPERDRIVE_REMOTETRANSPORTINTERFACE_H
#define HYPERDRIVE_REMOTETRANSPORTINTERFACE_H

#include "hyperdrivetransport.h"

namespace Hyperspace {
class Socket;
}

namespace Hyperdrive {

class RemoteTransportInterfacePrivate;
class RemoteTransportInterface : public Hyperdrive::Transport
{
    Q_OBJECT
    Q_DECLARE_PRIVATE_D(d_h_ptr, RemoteTransportInterface)
    Q_DISABLE_COPY(RemoteTransportInterface)

public:
    virtual ~RemoteTransportInterface();

    virtual void rebound(const Hyperspace::Rebound& rebound, int fd) Q_DECL_OVERRIDE Q_DECL_FINAL;
    virtual void fluctuation(const Hyperspace::Fluctuation& fluctuation) override final;
    virtual void cacheMessage(const CacheMessage& cacheMessage) override final;
    virtual void bigBang() override final;

    virtual void initImpl();

protected:
    virtual void routeWave(const Hyperspace::Wave& wave, int fd);

private:
    explicit RemoteTransportInterface(Hyperspace::Socket *socket, const QString& name, QObject* parent = nullptr);

    friend class TransportManager;
};
}

#endif // HYPERDRIVE_REMOTETRANSPORTINTERFACE_H
