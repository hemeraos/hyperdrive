#ifndef TRANSPORT_DATABASE_MANAGER_H
#define TRANSPORT_DATABASE_MANAGER_H

#include <cachemessage.h>

#include <QtCore/QDateTime>
#include <QtCore/QHash>
#include <QtCore/QString>

namespace Hyperdrive
{

namespace TransportDatabaseManager
{
    bool ensureDatabase(const QString &dbPath = QString(), const QString &migrationsDirPath = QString());

namespace Transactions
{
    bool insertPersistentEntry(const QByteArray &target, const QByteArray &payload);
    bool updatePersistentEntry(const QByteArray &target, const QByteArray &payload);
    bool deletePersistentEntry(const QByteArray &target);
    QHash<QByteArray, QByteArray> allPersistentEntries();

    int insertCacheMessage(const CacheMessage &cacheMessage, const QDateTime &expiry = QDateTime());
    bool deleteCacheMessage(int id);
    QList<CacheMessage> allCacheMessages();
}

}

}

#endif
