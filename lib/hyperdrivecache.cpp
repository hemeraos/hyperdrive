
#include "hyperdrivecache.h"

#include <hyperdrivedatabasemanager.h>

#include <QtCore/QDebug>

namespace Hyperdrive {

class Cache::Private
{
public:
    QString identifier;
    QHash< QByteArray, QHash< QByteArray, QByteArray > > producerProperties;
    QHash< QByteArray, QHash< QByteArray, QByteArray > > consumerProperties;
};

static Cache *s_instance;

Cache::Cache(QObject *parent)
    : Hemera::AsyncInitObject(parent)
    , d(new Private)
{
}

Hyperdrive::Cache * Cache::instance()
{
    if (Q_UNLIKELY(!s_instance)) {
        s_instance = new Cache();
    }

    return s_instance;
}

Cache::~Cache()
{
    delete d;
}

void Cache::initImpl()
{
    DatabaseManager::DatabaseStatus status = DatabaseManager::ensureDatabase();
    switch (status) {
        case DatabaseManager::DatabaseCreated:
        case DatabaseManager::DatabaseUpdated:
            Q_EMIT invalidCache();
            break;
        // Avoid compiler warnings
        default:
            break;
    }
    d->producerProperties = DatabaseManager::Transactions::allProducerProperties();
    d->consumerProperties = DatabaseManager::Transactions::allConsumerProperties();
    setReady();
}

void Cache::insertOrUpdateProducerProperty(const QByteArray &interface, const QByteArray &path, const QByteArray &payload)
{
    if (d->producerProperties.value(interface).value(path) == payload) {
        return;
    }

    if (d->producerProperties.contains(interface)) {
        if (d->producerProperties.value(interface).contains(path)) {
            DatabaseManager::Transactions::updateProducerProperty(interface, path, payload);
        } else {
            DatabaseManager::Transactions::insertProducerProperty(interface, path, payload);
        }
        d->producerProperties[interface].insert(path, payload);
    } else {
        DatabaseManager::Transactions::insertProducerProperty(interface, path, payload);
        d->producerProperties.insert(interface, QHash< QByteArray, QByteArray >{{path, payload}});
    }
}

void Cache::removeProducerProperty(const QByteArray &interface, const QByteArray &path)
{
    DatabaseManager::Transactions::deleteProducerProperty(interface, path);
    d->producerProperties[interface].remove(path);
}

void Cache::removeAllProducerProperties(const QByteArray &interface)
{
    QByteArrayList paths = d->producerProperties.value(interface).keys();
    for (const QByteArray path : paths) {
        removeProducerProperty(interface, path);
    }
    d->producerProperties.remove(interface);
}

QByteArray Cache::producerProperty(const QByteArray &interface, const QByteArray &path) const
{
    return d->producerProperties.value(interface).value(path);
}

QHash< QByteArray, QByteArray > Cache::producerProperties(const QByteArray &interface) const
{
    return d->producerProperties.value(interface);
}

QHash< QByteArray, QHash < QByteArray, QByteArray > > Cache::allProducerProperties() const
{
    return d->producerProperties;
}

void Cache::insertOrUpdateConsumerProperty(const QByteArray &interface, const QByteArray &path, const QByteArray &wave)
{
    if (d->consumerProperties.value(interface).value(path) == wave) {
        return;
    }

    if (d->consumerProperties.contains(interface)) {
        if (d->consumerProperties.value(interface).contains(path)) {
            DatabaseManager::Transactions::updateConsumerProperty(interface, path, wave);
        } else {
            DatabaseManager::Transactions::insertConsumerProperty(interface, path, wave);
        }
        d->consumerProperties[interface].insert(path, wave);
    } else {
        DatabaseManager::Transactions::insertConsumerProperty(interface, path, wave);
        d->consumerProperties.insert(interface, QHash< QByteArray, QByteArray >{{path, wave}});
    }
}

void Cache::removeConsumerProperty(const QByteArray &interface, const QByteArray &path)
{
    DatabaseManager::Transactions::deleteConsumerProperty(interface, path);
    d->consumerProperties[interface].remove(path);
}

void Cache::removeAllConsumerProperties(const QByteArray &interface)
{
    QByteArrayList paths = d->consumerProperties.value(interface).keys();
    for (const QByteArray path : paths) {
        removeConsumerProperty(interface, path);
    }
    d->consumerProperties.remove(interface);
}

QHash< QByteArray, QByteArray > Cache::consumerProperties(const QByteArray &interface) const
{
    return d->consumerProperties.value(interface);
}

QSet< QByteArray > Cache::allConsumerPropertiesFullPaths() const
{
    QSet<QByteArray> result;
    for (const QByteArray &interface : d->consumerProperties.keys()) {
        for (const QByteArray &path : d->consumerProperties.value(interface).keys()) {
            result.insert(QByteArray("%1%2").replace("%1", interface).replace("%2", path));
        }
    }
    return result;
}

}

#include "hyperdrivecache.moc"
