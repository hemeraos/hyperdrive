#ifndef ASTARTE_CRYPTO_P_H
#define ASTARTE_CRYPTO_P_H

#include "astartecrypto.h"

#include <HemeraCore/Operation>

namespace Astarte {
class Connector;

class ThreadedKeyOperation : public Hemera::Operation
{
    Q_OBJECT
    Q_DISABLE_COPY(ThreadedKeyOperation)

public:
    explicit ThreadedKeyOperation(const QString &privateKeyFile, const QString &publicKeyFile, QObject* parent = nullptr);
    explicit ThreadedKeyOperation(const QString &cn, const QString &privateKeyFile, const QString &publicKeyFile, const QString &csrFile, QObject* parent = nullptr);
    virtual ~ThreadedKeyOperation();

protected:
    virtual void startImpl() override final;

private:
    QString m_cn;
    QString m_privateKeyFile;
    QString m_publicKeyFile;
    QString m_csrFile;
};
}

#endif // ASTARTE_CRYPTO_P_H
