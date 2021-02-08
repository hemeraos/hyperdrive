#include "hyperdrivedatabasemanager.h"

#include <hyperdriveconfig.h>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QLoggingCategory>

#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>

#define VERSION_VALUE 0

#define INTERFACE_VALUE 0
#define PATH_VALUE 1
#define PAYLOAD_VALUE 2
#define WAVE_VALUE 2

Q_LOGGING_CATEGORY(transportDatabaseManagerDC, "hyperdrive.databasemanager", DEBUG_MESSAGES_DEFAULT_LEVEL)

namespace Hyperdrive {

namespace DatabaseManager {

DatabaseStatus ensureDatabase()
{
    if (QSqlDatabase::database().isValid()) {
        return DatabaseOk;
    }

    // Let's create our connection.
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"));

    QString dbPath = QStringLiteral("%1/persistence.db").arg(QDir::homePath());
    db.setDatabaseName(dbPath);

    if (!db.open()) {
        qCWarning(transportDatabaseManagerDC) << "Could not open database!" << dbPath << db.lastError().text();
        return DatabaseFailed;
    }

    QSqlQuery checkQuery(QStringLiteral("pragma quick_check"));
    if (!checkQuery.exec()) {
        qCWarning(transportDatabaseManagerDC) << "Database " << dbPath << " is corrupted, deleting it and starting from a new one " << checkQuery.lastError();
        db.close();
        if (!QFile::remove(dbPath)) {
            qCWarning(transportDatabaseManagerDC) << "Can't remove database " << dbPath << ", giving up ";
            return DatabaseFailed;
        }
        if (!db.open()) {
            qCWarning(transportDatabaseManagerDC) << "Can't reopen database " << dbPath << " after deleting it, giving up " << db.lastError().text();
            return DatabaseFailed;
        }
    }

    QSqlQuery migrationQuery;

    // Ok. Let's query our migrations.
    QDir migrationsDir(QStringLiteral("%1/db/migrations").arg(QLatin1String(StaticConfig::hyperdriveDataDir())));
    migrationsDir.setFilter(QDir::Files | QDir::NoSymLinks);
    migrationsDir.setSorting(QDir::Name);
    QHash< int, QString > migrations;
    int latestSchemaVersion = 0;
    for (const QFileInfo &migration : migrationsDir.entryInfoList()) {
        latestSchemaVersion = migration.baseName().split(QLatin1Char('_')).first().toInt();
        migrations.insert(latestSchemaVersion, migration.absoluteFilePath());
    }

    // Query our schema table
    QSqlQuery schemaQuery(QStringLiteral("SELECT version from schema_version"));
    int currentSchemaVersion = -1;
    while (schemaQuery.next()) {
        if (schemaQuery.value(VERSION_VALUE).toInt() == latestSchemaVersion) {
            // Yo.
            return DatabaseOk;
        } else if (schemaQuery.value(VERSION_VALUE).toInt() > latestSchemaVersion) {
            // Yo.
            qCWarning(transportDatabaseManagerDC) << "Something is weird!! Our schema version is higher than available migrations? Ignoring and hoping for the best...";
            return DatabaseOk;
        }

        // Let's point out which one we need.
        currentSchemaVersion = schemaQuery.value(VERSION_VALUE).toInt();
    }

    // We need to migrate.
    qCWarning(transportDatabaseManagerDC) << "Found these migrations:" << migrations;
    qCWarning(transportDatabaseManagerDC) << "Latest schema version is " << latestSchemaVersion;

    bool newDb = false;
    // If we got here, we might need to create the schema table first.
    if (currentSchemaVersion < 0) {
        newDb = true;
        qCDebug(transportDatabaseManagerDC) << "Creating schema_version table...";
        if (!migrationQuery.exec(QStringLiteral("CREATE TABLE schema_version(version integer)"))) {
            qCWarning(transportDatabaseManagerDC) << "Could not create schema_version!!" << migrationQuery.lastError();
            return DatabaseFailed;
        }
        if (!migrationQuery.exec(QStringLiteral("INSERT INTO schema_version (version) VALUES (0)"))) {
            qCWarning(transportDatabaseManagerDC) << "Could not create schema_version!!" << migrationQuery.lastError();
            return DatabaseFailed;
        }
    }

    bool updatedDb = false;
    qCDebug(transportDatabaseManagerDC) << "Now performing migrations" << currentSchemaVersion << latestSchemaVersion;
    // Perform migrations.
    for (++currentSchemaVersion; currentSchemaVersion <= latestSchemaVersion; ++currentSchemaVersion) {
        if (!migrations.contains(currentSchemaVersion)) {
            continue;
        }

        // Apply migration
        updatedDb = true;
        QFile migrationFile(migrations.value(currentSchemaVersion));
        if (migrationFile.open(QIODevice::ReadOnly)) {
            QString queryString = QTextStream(&migrationFile).readAll();
            if (!migrationQuery.exec(queryString)) {
                qCWarning(transportDatabaseManagerDC) << "Could not execute migration" << currentSchemaVersion << migrationQuery.lastError();
                return DatabaseFailed;
            }
        }

        // Update schema version
        if (!migrationQuery.exec(QStringLiteral("UPDATE schema_version SET version=%1").arg(currentSchemaVersion))) {
            qCWarning(transportDatabaseManagerDC) << "Could not update schema_version!! This error is critical, your database is compromised!!" << migrationQuery.lastError();
            return DatabaseFailed;
        }

        qCDebug(transportDatabaseManagerDC) << "Migration performed" << currentSchemaVersion;
    }

    // We're set!
    return newDb ? DatabaseCreated : (updatedDb ? DatabaseUpdated : DatabaseOk);
}

bool Transactions::insertProducerProperty(const QByteArray &interface, const QByteArray &path, const QByteArray &payload)
{
    if (!ensureDatabase()) {
        return false;
    }

    QSqlQuery query;
    query.prepare(QStringLiteral("INSERT INTO producer_properties (interface, path, payload) "
                                 "VALUES (:interface, :path, :payload)"));
    query.bindValue(QStringLiteral(":interface"), QLatin1String(interface));
    query.bindValue(QStringLiteral(":path"), QLatin1String(path));
    query.bindValue(QStringLiteral(":payload"), payload);

    if (!query.exec()) {
        qCWarning(transportDatabaseManagerDC) << "Insert producer property query failed!" << query.lastError();
        return false;
    }

    return true;
}

bool Transactions::updateProducerProperty(const QByteArray &interface, const QByteArray &path, const QByteArray &payload)
{
    if (!ensureDatabase()) {
        return false;
    }

    QSqlQuery query;
    query.prepare(QStringLiteral("UPDATE producer_properties SET payload=:payload "
                                 "WHERE interface=:interface AND path=:path"));
    query.bindValue(QStringLiteral(":interface"), QLatin1String(interface));
    query.bindValue(QStringLiteral(":path"), QLatin1String(path));
    query.bindValue(QStringLiteral(":payload"), payload);

    if (!query.exec()) {
        qCWarning(transportDatabaseManagerDC) << "Update producer property query failed!" << query.lastError();
        return false;
    }

    return true;
}

bool Transactions::deleteProducerProperty(const QByteArray &interface, const QByteArray &path)
{
    if (!ensureDatabase()) {
        return false;
    }

    QSqlQuery query;
    query.prepare(QStringLiteral("DELETE FROM producer_properties WHERE interface=:interface AND path=:path"));
    query.bindValue(QStringLiteral(":interface"), QLatin1String(interface));
    query.bindValue(QStringLiteral(":path"), QLatin1String(path));

    if (!query.exec()) {
        qCWarning(transportDatabaseManagerDC) << "Delete producer property " << interface << path << " query failed!" << query.lastError();
        return false;
    }

    return true;
}

QHash< QByteArray, QHash< QByteArray, QByteArray > > Transactions::allProducerProperties()
{
    QHash< QByteArray, QHash< QByteArray, QByteArray > > ret;

    if (!ensureDatabase()) {
        return ret;
    }

    QSqlQuery query;
    query.prepare(QStringLiteral("SELECT interface, path, payload FROM producer_properties"));

    if (!query.exec()) {
        qCWarning(transportDatabaseManagerDC) << "All producer properties query failed!" << query.lastError();
        return ret;
    }

    while (query.next()) {
        QByteArray interface = query.value(INTERFACE_VALUE).toByteArray();
        QByteArray path = query.value(PATH_VALUE).toByteArray();
        QByteArray payload = query.value(PAYLOAD_VALUE).toByteArray();
        if (ret.contains(interface)) {
            ret[interface].insert(path, payload);
        } else {
            ret.insert(interface, QHash< QByteArray, QByteArray >{{path, payload}});
        }
    }

    return ret;
}

bool Transactions::insertConsumerProperty(const QByteArray &interface, const QByteArray &path, const QByteArray &wave)
{
    if (!ensureDatabase()) {
        return false;
    }

    QSqlQuery query;
    query.prepare(QStringLiteral("INSERT INTO consumer_properties (interface, path, wave) "
                                 "VALUES (:interface, :path, :wave)"));
    query.bindValue(QStringLiteral(":interface"), QLatin1String(interface));
    query.bindValue(QStringLiteral(":path"), QLatin1String(path));
    query.bindValue(QStringLiteral(":wave"), wave);

    if (!query.exec()) {
        qCWarning(transportDatabaseManagerDC) << "Insert consumer property query failed!" << query.lastError();
        return false;
    }

    return true;
}

bool Transactions::updateConsumerProperty(const QByteArray &interface, const QByteArray &path, const QByteArray &wave)
{
    if (!ensureDatabase()) {
        return false;
    }

    QSqlQuery query;
    query.prepare(QStringLiteral("UPDATE consumer_properties SET wave=:wave "
                                 "WHERE interface=:interface AND path=:path"));
    query.bindValue(QStringLiteral(":interface"), QLatin1String(interface));
    query.bindValue(QStringLiteral(":path"), QLatin1String(path));
    query.bindValue(QStringLiteral(":wave"), wave);

    if (!query.exec()) {
        qCWarning(transportDatabaseManagerDC) << "Update consumer property query failed!" << query.lastError();
        return false;
    }

    return true;
}

bool Transactions::deleteConsumerProperty(const QByteArray &interface, const QByteArray &path)
{
    if (!ensureDatabase()) {
        return false;
    }

    QSqlQuery query;
    query.prepare(QStringLiteral("DELETE FROM consumer_properties WHERE interface=:interface AND path=:path"));
    query.bindValue(QStringLiteral(":interface"), QLatin1String(interface));
    query.bindValue(QStringLiteral(":path"), QLatin1String(path));

    if (!query.exec()) {
        qCWarning(transportDatabaseManagerDC) << "Delete consumer property " << interface << path << " query failed!" << query.lastError();
        return false;
    }

    return true;
}

QHash< QByteArray, QHash< QByteArray, QByteArray > > Transactions::allConsumerProperties()
{
    QHash< QByteArray, QHash< QByteArray, QByteArray > > ret;

    if (!ensureDatabase()) {
        return ret;
    }

    QSqlQuery query;
    query.prepare(QStringLiteral("SELECT interface, path, wave FROM consumer_properties"));

    if (!query.exec()) {
        qCWarning(transportDatabaseManagerDC) << "All consumer properties query failed!" << query.lastError();
        return ret;
    }

    while (query.next()) {
        QByteArray interface = query.value(INTERFACE_VALUE).toByteArray();
        QByteArray path = query.value(PATH_VALUE).toByteArray();
        QByteArray wave = query.value(WAVE_VALUE).toByteArray();
        if (ret.contains(interface)) {
            ret[interface].insert(path, wave);
        } else {
            ret.insert(interface, QHash< QByteArray, QByteArray >{{path, wave}});
        }
    }

    return ret;
}

}

}
