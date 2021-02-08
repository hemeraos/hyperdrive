/*
 *
 */

#ifndef HYPERDRIVE_REMOTEDISCOVERYSERVICE_H
#define HYPERDRIVE_REMOTEDISCOVERYSERVICE_H

#include "hyperdrivediscoveryservice.h"

#include "hyperdriveremotetransport.h"

namespace Hyperdrive {

class Core;

class RemoteUrlFeaturesOperation : public Hemera::Operation
{
    Q_OBJECT
    Q_DISABLE_COPY(RemoteUrlFeaturesOperation)

public:
    virtual ~RemoteUrlFeaturesOperation();

    QHash< QUrl, Transport::Features > result() const;

protected:
    virtual void startImpl() Q_DECL_OVERRIDE Q_DECL_FINAL;

private:
    explicit RemoteUrlFeaturesOperation(QObject* parent = Q_NULLPTR);

    QHash< QUrl, Transport::Features > m_result;

    friend class RemoteDiscoveryService;
    friend class RemoteDiscoveryServicePrivate;
};

class RemoteDiscoveryServicePrivate;
class RemoteDiscoveryService : public DiscoveryService
{
    Q_OBJECT
    Q_DECLARE_PRIVATE_D(d_h_ptr, RemoteDiscoveryService)
    Q_DISABLE_COPY(RemoteDiscoveryService)

    Q_PRIVATE_SLOT(d_func(), void sendName());

public:
    explicit RemoteDiscoveryService(QObject *parent = Q_NULLPTR);
    virtual ~RemoteDiscoveryService();

protected:
    RemoteByteArrayListOperation *localInterfaces();
    RemoteUrlFeaturesOperation *transportsTemplateUrls();

    virtual void setInterfacesAnnounced(const QList<QByteArray> &interfaces) Q_DECL_OVERRIDE Q_DECL_FINAL;
    virtual void setInterfacesExpired(const QList<QByteArray> &interfaces) Q_DECL_OVERRIDE Q_DECL_FINAL;
    virtual void setInterfacesPurged(const QList<QByteArray> &interfaces) Q_DECL_OVERRIDE Q_DECL_FINAL;

    virtual void setInterfaceIntrospected(const QByteArray &interface, const QList< QByteArray > &servicesUrl) Q_DECL_OVERRIDE Q_DECL_FINAL;

private:
    friend class DiscoveryManager;
};
}

#endif // HYPERDRIVE_REMOTEDISCOVERYSERVICE_H
