/*
 *
 */

#ifndef HYPERDRIVE_REMOTETRANSPORT_H
#define HYPERDRIVE_REMOTETRANSPORT_H

#include <hyperdrivetransport.h>

#include "hyperdriveinterface.h"

#include <HemeraCore/Operation>

namespace Hyperdrive {

class RemoteBoolOperation : public Hemera::Operation
{
    Q_OBJECT
    Q_DISABLE_COPY(RemoteBoolOperation)

public:
    virtual ~RemoteBoolOperation();

    bool result() const;

protected:
    virtual void startImpl() Q_DECL_OVERRIDE Q_DECL_FINAL;

private:
    explicit RemoteBoolOperation(QObject* parent = Q_NULLPTR);

    bool m_result;

    friend class RemoteTransport;
    friend class RemoteTransportPrivate;
};

class RemoteByteArrayListOperation : public Hemera::Operation
{
    Q_OBJECT
    Q_DISABLE_COPY(RemoteByteArrayListOperation)

public:
    virtual ~RemoteByteArrayListOperation();

    QList< QByteArray > result() const;

protected:
    virtual void startImpl() Q_DECL_OVERRIDE Q_DECL_FINAL;

private:
    explicit RemoteByteArrayListOperation(QObject* parent = Q_NULLPTR);

    QList< QByteArray > m_result;

    friend class RemoteDiscoveryService;
    friend class RemoteDiscoveryServicePrivate;
    friend class RemoteTransport;
    friend class RemoteTransportPrivate;
};

class RemoteTransportPrivate;
class RemoteTransport : public Hyperdrive::Transport
{
    Q_OBJECT
    Q_DECLARE_PRIVATE_D(d_h_ptr, RemoteTransport)
    Q_DISABLE_COPY(RemoteTransport)

    Q_PRIVATE_SLOT(d_func(), void sendName());

public:
    explicit RemoteTransport(const QString& name, QObject* parent = Q_NULLPTR);
    virtual ~RemoteTransport();

    QHash< QByteArray, Interface > introspection() const;

protected:
    virtual void routeWave(const Hyperspace::Wave &wave, int fd);

    RemoteByteArrayListOperation *listHyperdriveInterfaces();
    RemoteByteArrayListOperation *listGatesForHyperdriveInterface(const QByteArray &interface);
    RemoteBoolOperation *hyperdriveHasInterface(const QByteArray &interface);

Q_SIGNALS:
    void introspectionChanged();
};

}

#endif // HYPERDRIVE_REMOTETRANSPORT_H
