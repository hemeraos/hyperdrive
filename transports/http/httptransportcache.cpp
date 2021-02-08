
#include "httptransportcache.h"

#include <QtCore/QJsonObject>

class HTTPTransportCache::Private
{
public:
    QMap< QByteArray, QByteArray > persistentEntries;
};

static HTTPTransportCache* s_instance;

HTTPTransportCache::HTTPTransportCache(QObject *parent)
    : Hemera::AsyncInitObject(parent)
    , d(new Private)
{
}

HTTPTransportCache *HTTPTransportCache::instance()
{
    if (Q_UNLIKELY(!s_instance)) {
        s_instance = new HTTPTransportCache();
    }

    return s_instance;
}

HTTPTransportCache::~HTTPTransportCache()
{
    delete d;
}

void HTTPTransportCache::initImpl()
{
    setReady();
}

void HTTPTransportCache::insertOrUpdatePersistentEntry(const QByteArray &target, const QByteArray &payload)
{
    d->persistentEntries.insert(target, payload);
}

void HTTPTransportCache::removePersistentEntry(const QByteArray &target)
{
    d->persistentEntries.remove(target);
}

bool HTTPTransportCache::isCached(const QByteArray &target) const
{
    return d->persistentEntries.contains(target);
}

bool HTTPTransportCache::hasTree(const QByteArray &target) const
{
    for (const QByteArray &key : d->persistentEntries.keys()) {
        if (key < target) {
            continue;
        }

        if (key.startsWith(target)) {
            // Make sure we match at path boundaries, not arbitrary prefixes
            QByteArray relativePath = key;
            relativePath.replace(target, "");
            if (target.endsWith("/") || relativePath.startsWith("/")) {
                return true;
            }
        }
        return false;
    }
    return false;
}

QByteArray HTTPTransportCache::persistentEntry(const QByteArray &target) const
{
    return d->persistentEntries.value(target);
}

QMap< QByteArray, QByteArray > HTTPTransportCache::allPersistentEntries() const
{
    return d->persistentEntries;
}

QJsonDocument HTTPTransportCache::tree(const QByteArray &target) const
{
    QList<QByteArray> pathStack;
    QList<QJsonObject> objectStack;
    // Root object
    objectStack.push_back(QJsonObject());
    for (const QByteArray &key : d->persistentEntries.keys()) {
        // We have not reached keys starting with target yet
        if (key < target) {
            continue;
        }

        if (key.startsWith(target)) {

            QByteArray relativePath = key;
            // Remove the target
            relativePath.replace(target, "");
            if (!target.endsWith("/")) {
                // Remove also the leading slash
                relativePath.remove(0, 1);
            }
            QList<QByteArray> subPaths = relativePath.split('/');
            subPaths.removeAll("");

            int toBePopped = pathStackPopSize(pathStack, subPaths);

            // Pop until until we reach the common part
            for (int i = 0; i < toBePopped; ++i) {
                QString objName = QLatin1String(pathStack.takeLast());
                QJsonObject obj = objectStack.takeLast();
                objectStack.last().insert(objName, obj);
            }

            // Push from the common part to the end - 1
            for (int i = pathStack.length(); i < subPaths.length() - 1; ++i) {
                pathStack.push_back(subPaths.at(i));
                objectStack.push_back(QJsonObject());
            }

            // The last subpath is the actual value
            objectStack.last().insert(QLatin1String(subPaths.last()), QLatin1String(d->persistentEntries.value(key)));

        // We reached the end of the keys starting with target, we're done here
        } else {
            break;
        }
    }

    while (!pathStack.isEmpty()) {
        QString objName = QLatin1String(pathStack.takeLast());
        QJsonObject obj = objectStack.takeLast();
        objectStack.last().insert(objName, obj);
    }

    QJsonDocument tree;
    tree.setObject(objectStack.first());

    return tree;
}

int HTTPTransportCache::pathStackPopSize(const QList<QByteArray> currentStack, const QList<QByteArray> &newPath) const
{
    if (currentStack.isEmpty()) {
        return 0;
    } else {
        int res = currentStack.length();
        for (int i=0; i < currentStack.length() && i < newPath.length(); ++i) {
            if (currentStack.at(i) == newPath.at(i)) {
                --res;
            } else {
                break;
            }
        }
        return res;
    }
}

void HTTPTransportCache::enqueueEntry(const QByteArray &target, const QByteArray &payload,
                         CacheReliability reliability, int cacheExpiry)
{
}

void HTTPTransportCache::removeEntry(const QByteArray &target)
{
}

QHash< QByteArray, QByteArray > HTTPTransportCache::allEnqueuedEntries() const
{
    return QHash< QByteArray, QByteArray >();
}

#include "httptransportcache.moc"
