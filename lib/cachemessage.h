#ifndef HYPERDRIVE_CACHE_MESSAGE_H
#define HYPERDRIVE_CACHE_MESSAGE_H

#include "hyperdriveinterface.h"

#include <QtCore/QByteArray>
#include <QtCore/QHash>
#include <QtCore/QSharedDataPointer>
#include <QtCore/QDataStream>

namespace Hyperdrive {

class CacheMessageData;

class CacheMessage {
public:
    CacheMessage();
    CacheMessage(const CacheMessage &other);
    ~CacheMessage();

    CacheMessage &operator=(const CacheMessage &rhs);
    bool operator==(const CacheMessage &other) const;
    inline bool operator!=(const CacheMessage &other) const { return !operator==(other); }

    QByteArray target() const;
    void setTarget(const QByteArray &t);

    Hyperdrive::Interface::Type interfaceType() const;
    void setInterfaceType(Hyperdrive::Interface::Type interfaceType);

    QByteArray payload() const;
    void setPayload(const QByteArray &p);

    QHash<QByteArray, QByteArray> attributes() const;
    QByteArray attribute(const QByteArray &attribute) const;
    bool hasAttribute(const QByteArray &attribute) const;
    void setAttributes(const QHash<QByteArray, QByteArray> &a);
    void addAttribute(const QByteArray &attribute, const QByteArray &value);
    bool removeAttribute(const QByteArray &attribute);
    QByteArray takeAttribute(const QByteArray &attribute);

    QByteArray serialize() const;
    static CacheMessage fromBinary(const QByteArray &data);

private:
    QSharedDataPointer<CacheMessageData> d;
};

}

#endif // HYPERDRIVE_CACHE_MESSAGE_H
