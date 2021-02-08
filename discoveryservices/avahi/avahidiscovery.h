#ifndef _HYPERDISCOVERY_H_
#define _HYPERDISCOVERY_H_

#include "hyperdrivelocaldiscoveryservice.h"

#include <QtCore/QObject>
#include <QtCore/QByteArray>
#include <QtCore/QElapsedTimer>
#include <QtCore/QStringList>
#include <QtCore/QSet>

#include <QtNetwork/QUdpSocket>

class Service;
class DiscoveryCore;

class AvahiDiscovery : public Hyperdrive::LocalDiscoveryService
{
    Q_OBJECT
    Q_INTERFACES(Hyperdrive::LocalDiscoveryService)
    Q_PLUGIN_METADATA(IID "com.ispirata.Hemera.Hyperdrive.LocalDiscoveryService" FILE "avahi.json")

public:
    explicit AvahiDiscovery(QObject *parent = 0);
    virtual ~AvahiDiscovery();

protected:
    virtual void initImpl() Q_DECL_OVERRIDE Q_DECL_FINAL;
    virtual void scanInterfaces(const QList< QByteArray >& interfaces) Q_DECL_OVERRIDE Q_DECL_FINAL;

    virtual void introspectInterface(const QByteArray& interface) Q_DECL_OVERRIDE Q_DECL_FINAL;

    virtual void onLocalInterfacesRegistered(const QList<QByteArray> &interfaces) Q_DECL_OVERRIDE Q_DECL_FINAL;
    virtual void onLocalInterfacesUnregistered(const QList<QByteArray> &interfaces) Q_DECL_OVERRIDE Q_DECL_FINAL;

private:
};

#endif

