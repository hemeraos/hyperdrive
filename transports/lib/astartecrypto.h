#ifndef ASTARTE_CRYPTO_H
#define ASTARTE_CRYPTO_H

#include <HemeraCore/AsyncInitObject>
#include <QtCore/QFlags>

namespace Astarte {

// Define some convenience paths here.

class CryptoPrivate;
class Crypto : public Hemera::AsyncInitObject
{
    Q_OBJECT
    Q_DISABLE_COPY(Crypto)
    Q_DECLARE_PRIVATE_D(d_h_ptr, Crypto)

public:
    enum AuthenticationDomain {
        NoAuthenticationDomain = 0,
        DeviceAuthenticationDomain = 1 << 0,
        CustomerAuthenticationDomain = 1 << 1,
        AnyAuthenticationDomain = 255
    };
    Q_ENUMS(AuthenticationDomain)
    Q_DECLARE_FLAGS(AuthenticationDomains, AuthenticationDomain)

    static Crypto * instance();

    virtual ~Crypto();

    bool isKeyStoreAvailable() const;
    Hemera::Operation *generateAstarteKeyStore(bool forceGeneration = false);

    QByteArray sign(const QByteArray &payload, AuthenticationDomains = AnyAuthenticationDomain);

    static QString pathToCertificateRequest();
    static QString pathToPrivateKey();
    static QString pathToPublicKey();

protected:
    virtual void initImpl() override final;

Q_SIGNALS:
    void keyStoreAvailabilityChanged();

private:
    explicit Crypto(QObject *parent = 0);

    friend class ThreadedKeyOperation;
};
}

Q_DECLARE_OPERATORS_FOR_FLAGS(Astarte::Crypto::AuthenticationDomains)

#endif // ASTARTE_CRYPTO_H
