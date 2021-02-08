#include "avahidiscovery.h"

AvahiDiscovery::AvahiDiscovery(QObject *parent)
    : LocalDiscoveryService(parent)
{
}

AvahiDiscovery::~AvahiDiscovery()
{
}

void AvahiDiscovery::initImpl()
{
    // Done.
    setReady();
}

void AvahiDiscovery::introspectInterface(const QByteArray& interface)
{
}

void AvahiDiscovery::scanInterfaces(const QList<QByteArray> &interfaces)
{
}

void AvahiDiscovery::onLocalInterfacesRegistered(const QList<QByteArray> &interfaces)
{
}

void AvahiDiscovery::onLocalInterfacesUnregistered(const QList<QByteArray> &interfaces)
{
}

