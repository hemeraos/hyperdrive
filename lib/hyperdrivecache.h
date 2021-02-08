#ifndef HYPERDRIVE_CACHE_H
#define HYPERDRIVE_CACHE_H

#include <HemeraCore/AsyncInitObject>

#include <QtCore/QSet>

namespace Hyperdrive {

class Cache : public Hemera::AsyncInitObject
{
    Q_OBJECT
    Q_DISABLE_COPY(Cache)

public:
    enum class CacheReliability {
        Unknown = 0,
        Volatile = 1,
        Persistent = 2
    };

    static Cache *instance();

    virtual ~Cache();

public Q_SLOTS:
    void insertOrUpdateProducerProperty(const QByteArray &interface, const QByteArray &path, const QByteArray &payload);
    void removeProducerProperty(const QByteArray &interface, const QByteArray &path);
    void removeAllProducerProperties(const QByteArray &interface);

    QByteArray producerProperty(const QByteArray &interface, const QByteArray &path) const;
    QHash< QByteArray, QByteArray > producerProperties(const QByteArray &interface) const;
    QHash< QByteArray, QHash< QByteArray, QByteArray > > allProducerProperties() const;

    void insertOrUpdateConsumerProperty(const QByteArray &interface, const QByteArray &path, const QByteArray &wave);
    void removeConsumerProperty(const QByteArray &interface, const QByteArray &path);
    void removeAllConsumerProperties(const QByteArray &interface);

    QHash< QByteArray, QByteArray > consumerProperties(const QByteArray &interface) const;
    QSet< QByteArray > allConsumerPropertiesFullPaths() const;

Q_SIGNALS:
    void invalidCache();

protected:
    virtual void initImpl() override final;

private:
    explicit Cache(QObject *parent = nullptr);

    class Private;
    Private * const d;
};
}

#endif // HYPERDRIVE_CACHE_H


