/*
 *
 */

#ifndef HYPERDRIVE_SECURITYMANAGER_H
#define HYPERDRIVE_SECURITYMANAGER_H

#include <QtCore/QSet>

#include <HemeraCore/AsyncInitObject>

#include <HyperspaceCore/Global>
#include <HyperspaceCore/Wave>

namespace Hyperspace {
class Socket;
}

namespace Hyperdrive {

class Core;
class SecurityPassFactory;
class SecurityTokenManager;

class SecurityManager : public Hemera::AsyncInitObject
{
    Q_OBJECT
    Q_DISABLE_COPY(SecurityManager)

public:
    explicit SecurityManager(Core *parent);
    virtual ~SecurityManager();

    bool isAuthorized(const Hyperspace::Wave &wave, const QByteArray &interface, QPair<QByteArray, QByteArray> &errorAttribute);

    /*
      - Everything is authorized by default
      - Security restrictions allow to restrict a certain subset of the universe
      - Security exceptions allow to have some security exceptions to the rules applied just before
    */
    bool isAuthorizationNeeded(const Hyperspace::Wave &wave, const QByteArray &interface);
    QList<QByteArray> authAPIsFor(const QByteArray &interface);

    void authorizeSecurityPass(QDataStream &stream, Hyperspace::Socket *socket);
    void revokeSecurityPass(QDataStream &stream, Hyperspace::Socket *socket);

protected:
    virtual void initImpl() Q_DECL_OVERRIDE Q_DECL_FINAL;

private:
    class AuthAPIConf;

    void loadRules(bool areExceptions);
    void loadAuthAPIConfs();
    void addSecurityRestriction(const QByteArray &path);
    void addSecurityException(const QByteArray &path);
    void addAPIConf(AuthAPIConf *conf);
    AuthAPIConf *apiConf(const QByteArray &authAPIPath);

    QList<AuthAPIConf *> m_securityAPIsConfs;
    QList<QByteArray> m_interfaceRestrictions;
    QList<QByteArray> m_interfaceExceptions;
    SecurityTokenManager *m_securityTokenManager;
    SecurityPassFactory *m_securityPassFactory;
    Core *m_core;
};
}

#endif // HYPERDRIVE_SECURITYMANAGER_H
