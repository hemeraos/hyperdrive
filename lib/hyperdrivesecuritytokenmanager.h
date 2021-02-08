/*
 *
 */

#ifndef HYPERDRIVE_SECURITYTOKENMANAGER_H
#define HYPERDRIVE_SECURITYTOKENMANAGER_H

#include <QtCore/QDateTime>
#include <QtCore/QSet>

#include <HemeraCore/AsyncInitObject>

#include <HyperspaceCore/Global>
#include <HyperspaceCore/Wave>

namespace Hyperspace {
    class SecurityPass;
}

namespace Hyperdrive {

class SecurityManager;

class SecurityTokenManager : public Hemera::AsyncInitObject
{
    Q_OBJECT
    Q_DISABLE_COPY(SecurityTokenManager)

public:
    explicit SecurityTokenManager(SecurityManager *parent);
    virtual ~SecurityTokenManager();

    bool isAuthorized(const Hyperspace::Wave &wave, const QByteArray &interface, const QList<QByteArray> &apis);

    void authorizeSecurityPass(Hyperspace::SecurityPass *pass);
    void revokeSecurityPass(Hyperspace::SecurityPass *pass);

protected:
    virtual void initImpl() override final;

private:
    QSet<QByteArray> m_tokens;

    void loadTokens();
};
}

#endif // HYPERDRIVE_SECURITYTOKENMANAGER_H
