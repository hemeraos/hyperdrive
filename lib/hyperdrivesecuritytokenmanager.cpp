/*
 *
 */

#include "hyperdrivesecuritytokenmanager.h"
#include "hyperdrivesecuritymanager.h"

#include "hyperdrivecore.h"

#include <HyperspaceCore/SecurityPass>
#include <HyperspaceCore/SecurityToken>

#include <QtCore/QDebug>
#include <QtCore/QLoggingCategory>
#include <QtCore/QDir>
#include <QtCore/QSettings>

#define SECURITYTOKENS_PATH "/var/lib/hyperdrive/securitytokens.d/"

Q_LOGGING_CATEGORY(hyperdriveSecurityTokenManagerDC, "hyperdrive.security.tokenmanager", DEBUG_MESSAGES_DEFAULT_LEVEL)

namespace Hyperdrive
{

SecurityTokenManager::SecurityTokenManager(SecurityManager* parent)
    : AsyncInitObject(parent)
{
}

SecurityTokenManager::~SecurityTokenManager()
{
}


void SecurityTokenManager::initImpl()
{
    loadTokens();
    setReady();
}

void SecurityTokenManager::loadTokens()
{
    m_tokens.clear();
    QDir tokensDir(QStringLiteral(SECURITYTOKENS_PATH));
    QStringList entries = tokensDir.entryList(QDir::Files);
    foreach (const QString &entry, entries){
        QSettings tokenSettings(tokensDir.absoluteFilePath(entry), QSettings::IniFormat);
        QByteArray token = tokenSettings.value(QStringLiteral("token")).toByteArray();
        m_tokens.insert(token);
    }
}


bool SecurityTokenManager::isAuthorized(const Hyperspace::Wave &wave, const QByteArray &interface, const QList<QByteArray> &apis)
{
    QByteArray token = wave.attributes().value("Authorization");
    qCDebug(hyperdriveSecurityTokenManagerDC) << "Auth token: " << token;
    if (m_tokens.contains(token)){
        qCDebug(hyperdriveSecurityTokenManagerDC) << "Token is authorized";
        return true;
    }else{
        qCDebug(hyperdriveSecurityTokenManagerDC) << "Token is not authorized";
        return false;
    }
}

void SecurityTokenManager::authorizeSecurityPass(Hyperspace::SecurityPass *pass)
{
    Hyperspace::SecurityToken *token = static_cast<Hyperspace::SecurityToken *>(pass);

    qCDebug(hyperdriveSecurityTokenManagerDC) << "Authorize: " << token->method() << " " << token->path() << " " << token->ttl() << " " << token->token();

    QSettings tokenSettings(QStringLiteral("/var/lib/hyperdrive/securitytokens.d/newToken.ini"), QSettings::IniFormat);
    tokenSettings.setValue(QStringLiteral("token"), token->token());
    tokenSettings.sync();

    m_tokens.insert(token->token());
}

void SecurityTokenManager::revokeSecurityPass(Hyperspace::SecurityPass *pass)
{
    qCDebug(hyperdriveSecurityTokenManagerDC) << "Revoke: " << pass->method() << " " << pass->path() << " " << pass->ttl();
}

}
