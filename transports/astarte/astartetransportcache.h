#ifndef ASTARTE_TRANSPORT_CACHE_H
#define ASTARTE_TRANSPORT_CACHE_H

#include <HemeraCore/AsyncInitObject>

namespace Hyperdrive
{
class CacheMessage;
}

class AstarteTransportCache : public Hemera::AsyncInitObject
{
    Q_OBJECT
    Q_DISABLE_COPY(AstarteTransportCache)

public:
    static AstarteTransportCache *instance();

    virtual ~AstarteTransportCache();

public Q_SLOTS:
    void insertOrUpdatePersistentEntry(const QByteArray &target, const QByteArray &payload);
    void removePersistentEntry(const QByteArray &target);

    QByteArray persistentEntry(const QByteArray &target) const;
    QHash< QByteArray, QByteArray > allPersistentEntries() const;

    bool isCached(const QByteArray &target) const;

    void addInFlightEntry(int messageId, Hyperdrive::CacheMessage message);
    Hyperdrive::CacheMessage takeInFlightEntry(int messageId);
    void resetInFlightEntries();

    int addRetryEntry(Hyperdrive::CacheMessage message);
    void removeRetryEntry(int messageId);

    Hyperdrive::CacheMessage takeRetryEntry(int id);
    QList<int> allRetryIds() const;

    void removeFromDatabase(const Hyperdrive::CacheMessage &message);


protected:
    virtual void initImpl() override final;
    virtual void timerEvent(QTimerEvent *event) override final;

private:
    explicit AstarteTransportCache(QObject *parent = nullptr);

    void insertIntoDatabaseIfNotPresent(Hyperdrive::CacheMessage &message);

    bool ensureDatabase();

    bool m_dbOk;

    class Private;
    Private * const d;
};

#endif // ASTARTE_TRANSPORT_CACHE_H


