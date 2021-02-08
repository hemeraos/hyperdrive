#ifndef HTTP_TRANSPORT_CACHE_H
#define HTTP_TRANSPORT_CACHE_H

#include <HemeraCore/AsyncInitObject>

#include <QtCore/QJsonDocument>

class HTTPTransportCache : public Hemera::AsyncInitObject
{
    Q_OBJECT
    Q_DISABLE_COPY(HTTPTransportCache)

public:
    enum class CacheReliability {
        Volatile = 1,
        Persistent = 2
    };

    static HTTPTransportCache *instance();

    virtual ~HTTPTransportCache();

public Q_SLOTS:
    void insertOrUpdatePersistentEntry(const QByteArray &target, const QByteArray &payload);
    void removePersistentEntry(const QByteArray &target);

    QByteArray persistentEntry(const QByteArray &target) const;
    QMap< QByteArray, QByteArray > allPersistentEntries() const;

    bool isCached(const QByteArray &target) const;
    bool hasTree(const QByteArray &target) const;

    QJsonDocument tree(const QByteArray &target) const;

    void enqueueEntry(const QByteArray &target, const QByteArray &payload,
                      CacheReliability reliability = CacheReliability::Volatile, int cacheExpiry = -1);
    void removeEntry(const QByteArray &target);

    QHash< QByteArray, QByteArray > allEnqueuedEntries() const;

protected:
    virtual void initImpl() override final;

private:
    explicit HTTPTransportCache(QObject *parent = nullptr);

    int pathStackPopSize(const QList<QByteArray> currentStack, const QList<QByteArray> &newPath) const;

    class Private;
    Private * const d;
};

#endif // HTTP_TRANSPORT_CACHE_H


