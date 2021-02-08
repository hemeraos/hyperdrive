/*
 *
 */

#ifndef HYPERDRIVE_TRANSPORTMANAGER_H
#define HYPERDRIVE_TRANSPORTMANAGER_H

#include <HemeraCore/AsyncInitObject>

#include "hyperdrivetransport.h"

namespace Hyperspace {
class Socket;
}

namespace Hyperdrive {

class Core;

class TransportManager : public Hemera::AsyncInitObject
{
    Q_OBJECT
    Q_DISABLE_COPY(TransportManager)

public:
    TransportManager(Core *parent);
    virtual ~TransportManager();

    Transport *transportFor(const QString &name);
    Hemera::Operation *loadLocalTransport(const QString &name);
    QHash< QUrl, Transport::Features > templateUrls() const;
    QList< Transport* > loadedTransports() const;

signals:
    void remoteTransportLoaded(Transport *t);

protected:
    virtual void initImpl() Q_DECL_OVERRIDE Q_DECL_FINAL;

    void addRemoteTransport(Hyperspace::Socket *socket);

private:
    class Private;
    Private * const d;

    friend class Core;
};
}

#endif // HYPERDRIVE_TRANSPORTMANAGER_H
