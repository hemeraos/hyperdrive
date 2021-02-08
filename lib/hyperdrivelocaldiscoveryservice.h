/*
 *
 */

#ifndef HYPERDRIVE_LOCALDISCOVERYSERVICE_H
#define HYPERDRIVE_LOCALDISCOVERYSERVICE_H

#include "hyperdrivediscoveryservice.h"

class InterfaceWatcher;
namespace Hyperdrive {

class Core;

class LocalDiscoveryServicePrivate;
class LocalDiscoveryService : public DiscoveryService
{
    Q_OBJECT
    Q_DECLARE_PRIVATE_D(d_h_ptr, LocalDiscoveryService)
    Q_DISABLE_COPY(LocalDiscoveryService)

public:
    explicit LocalDiscoveryService(QObject *parent = Q_NULLPTR);
    virtual ~LocalDiscoveryService();

protected:
    virtual void setInterfacesAnnounced(const QList<QByteArray> &interfaces) Q_DECL_OVERRIDE Q_DECL_FINAL;
    virtual void setInterfacesExpired(const QList<QByteArray> &interfaces) Q_DECL_OVERRIDE Q_DECL_FINAL;
    virtual void setInterfacesPurged(const QList<QByteArray> &interfaces) Q_DECL_OVERRIDE Q_DECL_FINAL;

    virtual void setInterfaceIntrospected(const QByteArray &interface, const QList< QByteArray > &servicesUrl) Q_DECL_OVERRIDE Q_DECL_FINAL;

private:
    friend class DiscoveryManager;
};
}

Q_DECLARE_INTERFACE(Hyperdrive::LocalDiscoveryService, "com.ispirata.Hemera.Hyperdrive.LocalDiscoveryService")

#endif // HYPERDRIVE_LOCALDISCOVERYSERVICE_H
