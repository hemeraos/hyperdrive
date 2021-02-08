/*
 *
 */

#ifndef HYPERDRIVE_DISCOVERYSERVICE_H
#define HYPERDRIVE_DISCOVERYSERVICE_H

#include <HemeraCore/AsyncInitObject>
#include <HemeraCore/Operation>

#include <QtCore/QStringList>

namespace Hyperdrive {

class Core;
class DiscoveryManager;

class DiscoveryServicePrivate;
class DiscoveryService : public Hemera::AsyncInitObject
{
    Q_OBJECT
    Q_DECLARE_PRIVATE_D(d_h_ptr, DiscoveryService)
    Q_DISABLE_COPY(DiscoveryService)

public:
    virtual ~DiscoveryService();

    QByteArray transportMedium() const;

protected:
    explicit DiscoveryService(DiscoveryServicePrivate &dd, QObject *parent = Q_NULLPTR);

    DiscoveryManager *discoveryManager();

    virtual void scanInterfaces(const QList<QByteArray> &interfaces) = 0;

    virtual void onLocalInterfacesRegistered(const QList<QByteArray> &interfaces) = 0;
    virtual void onLocalInterfacesUnregistered(const QList<QByteArray> &interfaces) = 0;

    virtual void introspectInterface(const QByteArray &interface) = 0;

    void setName(const QByteArray &name);
    void setTransportMedium(const QByteArray &name);

    virtual void setInterfacesAnnounced(const QList<QByteArray> &interfaces) = 0;
    virtual void setInterfacesExpired(const QList<QByteArray> &interfaces) = 0;
    virtual void setInterfacesPurged(const QList<QByteArray> &interfaces) = 0;

    virtual void setInterfaceIntrospected(const QByteArray &interface, const QList< QByteArray > &servicesUrl) = 0;

private:
    friend class DiscoveryManager;
};
}

Q_DECLARE_INTERFACE(Hyperdrive::DiscoveryService, "com.ispirata.Hemera.Hyperdrive.DiscoveryService")

#endif // HYPERDRIVE_DISCOVERYSERVICE_H
