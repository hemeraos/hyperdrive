/*
 *
 */

#ifndef HYPERDRIVE_LOCALTRANSPORT_H
#define HYPERDRIVE_LOCALTRANSPORT_H

#include "hyperdrivetransport.h"

#include "hyperdriveinterface.h"

namespace Hyperdrive {

class LocalTransportPrivate;
class LocalTransport : public Hyperdrive::Transport
{
    Q_OBJECT
    Q_DISABLE_COPY(LocalTransport)
    Q_DECLARE_PRIVATE_D(d_h_ptr, LocalTransport)

public:
    virtual ~LocalTransport();

protected:
    explicit LocalTransport(const QString &name, QObject *parent = Q_NULLPTR);

    virtual void routeWave(const Hyperspace::Wave &wave, int fd) Q_DECL_OVERRIDE Q_DECL_FINAL;

    QHash< QByteArray, Interface > introspection() const;

    QList<QByteArray> listHyperdriveInterfaces() const;
    QList<QByteArray> listGatesForHyperdriveInterface(const QByteArray &interface) const;
    bool hyperdriveHasInterface(const QByteArray &interface) const;

Q_SIGNALS:
    void introspectionChanged();

private:
    friend class LoadLocalTransportOperation;
};
}

#endif // HYPERDRIVE_LOCALTRANSPORT_H
