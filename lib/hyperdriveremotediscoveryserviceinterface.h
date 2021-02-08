/*
 *
 */

#ifndef HYPERDRIVE_REMOTEDISCOVERYSERVICEINTERFACE_H
#define HYPERDRIVE_REMOTEDISCOVERYSERVICEINTERFACE_H

#include <hyperdrivediscoveryservice.h>

namespace Hyperspace {
class Socket;
}

namespace Hyperdrive {

class RemoteDiscoveryServiceInterfacePrivate;
class RemoteDiscoveryServiceInterface : public Hyperdrive::DiscoveryService
{
    Q_OBJECT
    Q_DECLARE_PRIVATE_D(d_h_ptr, RemoteDiscoveryServiceInterface)
    Q_DISABLE_COPY(RemoteDiscoveryServiceInterface)

public:
    virtual ~RemoteDiscoveryServiceInterface();

protected:
    virtual void initImpl() Q_DECL_OVERRIDE Q_DECL_FINAL;

    virtual void setInterfaceIntrospected(const QByteArray& interface, const QList< QByteArray >& servicesUrl) Q_DECL_OVERRIDE Q_DECL_FINAL;
    virtual void setInterfacesPurged(const QList< QByteArray >& interfaces) Q_DECL_OVERRIDE Q_DECL_FINAL;
    virtual void setInterfacesExpired(const QList< QByteArray >& interfaces) Q_DECL_OVERRIDE Q_DECL_FINAL;
    virtual void setInterfacesAnnounced(const QList< QByteArray >& interfaces) Q_DECL_OVERRIDE Q_DECL_FINAL;

    virtual void introspectInterface(const QByteArray& interface) Q_DECL_OVERRIDE Q_DECL_FINAL;

    virtual void onLocalInterfacesUnregistered(const QList< QByteArray >& interfaces) Q_DECL_OVERRIDE Q_DECL_FINAL;
    virtual void onLocalInterfacesRegistered(const QList< QByteArray >& interfaces) Q_DECL_OVERRIDE Q_DECL_FINAL;

    virtual void scanInterfaces(const QList< QByteArray >& interfaces) Q_DECL_OVERRIDE Q_DECL_FINAL;

private:
    explicit RemoteDiscoveryServiceInterface(Hyperspace::Socket *socket, const QString& name, QObject* parent = Q_NULLPTR);

    friend class DiscoveryManager;
};
}

#endif // HYPERDRIVE_REMOTEDISCOVERYSERVICEINTERFACE_H
