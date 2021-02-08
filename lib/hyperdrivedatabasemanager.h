#ifndef HYPERDRIVE_DATABASE_MANAGER_H
#define HYPERDRIVE_DATABASE_MANAGER_H

#include <cachemessage.h>

#include <QtCore/QDateTime>
#include <QtCore/QHash>
#include <QtCore/QString>

namespace Hyperdrive
{

namespace DatabaseManager
{
    enum DatabaseStatus {
        DatabaseFailed = 0,
        DatabaseOk = 1,
        DatabaseCreated = 2,
        DatabaseUpdated = 3
    };

    DatabaseStatus ensureDatabase();

namespace Transactions
{
    bool insertProducerProperty(const QByteArray &interface, const QByteArray &path, const QByteArray &payload);
    bool updateProducerProperty(const QByteArray &interface, const QByteArray &path, const QByteArray &payload);
    bool deleteProducerProperty(const QByteArray &interface, const QByteArray &path);

    QHash< QByteArray, QHash< QByteArray, QByteArray > > allProducerProperties();

    bool insertConsumerProperty(const QByteArray &interface, const QByteArray &path, const QByteArray &wave);
    bool updateConsumerProperty(const QByteArray &interface, const QByteArray &path, const QByteArray &wave);
    bool deleteConsumerProperty(const QByteArray &interface, const QByteArray &path);

    QHash< QByteArray, QHash< QByteArray, QByteArray > > allConsumerProperties();
}

}

}

#endif
