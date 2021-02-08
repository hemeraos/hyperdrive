/*
 *
 */

#include "hyperdrivesecuritymanager.h"

#include "hyperdrivecore.h"
#if 0
#include "hyperdrivesecuritypassfactory.h"
#include "hyperdrivesecuritytokenmanager.h"
#endif
#include "../hyperdriveconfig.h"

#include <HemeraCore/CommonOperations>
#if 0
#include <HyperspaceCore/SecurityPass>
#include <HyperspaceCore/SecurityToken>
#endif

#include <QtCore/QDataStream>
#include <QtCore/QDebug>
#include <QtCore/QLoggingCategory>
#include <QtCore/QDir>
#include <QtCore/QSettings>

Q_LOGGING_CATEGORY(hyperdriveSecurityManagerDC, "hyperdrive.security.manager", DEBUG_MESSAGES_DEFAULT_LEVEL)

namespace Hyperdrive
{

class SecurityManager::AuthAPIConf
{
public:
    QByteArray authAPIPath;
    QList<QByteArray> interfaces;
};

SecurityManager::SecurityManager(Hyperdrive::Core* parent)
    : AsyncInitObject(parent)
#if 0
    , m_securityPassFactory(new SecurityPassFactory(this))
#endif
    , m_core(parent)
{
}

SecurityManager::~SecurityManager()
{
}


void SecurityManager::initImpl()
{
#if 0
    setParts(2);

    m_securityTokenManager = new SecurityTokenManager(this);
    connect(m_securityTokenManager->init(), &Hemera::Operation::finished, this, [this] (Hemera::Operation *op) {
        if (op->isError()) {
            setInitError(op->errorName(), op->errorMessage());
        } else {
            setOnePartIsReady();
        }
    });

    loadRules(false);
    loadRules(true);
    loadAuthAPIConfs();

    setOnePartIsReady();
#endif
    setReady();
}

void SecurityManager::loadRules(bool areExceptions)
{
#if 0
    QDir rulesDir(areExceptions ? QLatin1String(Hyperdrive::StaticConfig::hyperdriveSecurityExceptionsPath()) : QLatin1String(Hyperdrive::StaticConfig::hyperdriveSecurityRestrictionsPath()));
    QStringList entries = rulesDir.entryList(QDir::Files);
    foreach (const QString &entry, entries){
        QSettings ruleSettings(rulesDir.absoluteFilePath(entry), QSettings::IniFormat);
        QByteArray ruleType = ruleSettings.value(QStringLiteral("type")).toByteArray();
        QByteArray path = ruleSettings.value(QStringLiteral("path")).toByteArray();

        if (path.isEmpty()){
            qCDebug(hyperdriveSecurityManagerDC) << "Invalid path";
            continue;
        }

        if (!areExceptions && (ruleType == "SecurityRestriction")){
            addSecurityRestriction(path);
        } else if (areExceptions && (ruleType == "SecurityException")) {
            addSecurityException(path);
        } else {
            qCDebug(hyperdriveSecurityManagerDC) << "Invalid rule type: " << ruleType << " (" << rulesDir.absoluteFilePath(entry) << ") ";
        }
    }
#endif
}

void SecurityManager::loadAuthAPIConfs()
{
#if 0
    QDir apiConfsDir(QLatin1String(Hyperdrive::StaticConfig::hyperdriveAuthAPIsConfsPath()));
    QStringList entries = apiConfsDir.entryList(QDir::Files);
    foreach (const QString &entry, entries){
        QSettings authAPISettings(apiConfsDir.absoluteFilePath(entry), QSettings::IniFormat);
        QByteArray authAPIPath = authAPISettings.value(QStringLiteral("authAPIPath")).toByteArray();
        QByteArray tmpCap = authAPISettings.value(QStringLiteral("interface")).toByteArray();
        AuthAPIConf *aConf = apiConf(authAPIPath);
        if (!aConf) {
            aConf = new AuthAPIConf();
            aConf->authAPIPath = authAPIPath;
            addAPIConf(aConf);
        }
        if (tmpCap.endsWith('*')) {
            tmpCap = tmpCap.left(tmpCap.count() - 1);
        }
        aConf->interfaces.append(tmpCap);
    }
#endif
}

bool SecurityManager::isAuthorized(const Hyperspace::Wave &wave, const QByteArray &interface, QPair<QByteArray, QByteArray> &errorAttribute)
{
#if 0
    bool needed = isAuthorizationNeeded(wave, interface);

    QList<QByteArray> apis = needed ? authAPIsFor(interface) : QList<QByteArray>();

    if (needed && !apis.isEmpty() && wave.attributes().contains("Authorization")) {
        bool authorized = m_securityTokenManager->isAuthorized(wave, interface, apis);
        if (!authorized) {
            errorAttribute.first = QByteArray("WWW-Authenticate");
            //TODO: QByteArrayList.join(' ')
            QByteArray allApis;
            foreach (QByteArray tApi, apis){
                allApis.append(tApi + ' ');
            }
            allApis = allApis.left(allApis.count() - 1);
            errorAttribute.second = QByteArray("APIBasedAuthentication " + allApis);
        }
        return authorized;
    }

    if (needed && !apis.isEmpty()) {
        errorAttribute.first = QByteArray("WWW-Authenticate");
        //TODO: QByteArrayList.join(' ')
        QByteArray allApis;
        foreach (QByteArray tApi, apis){
            allApis.append(tApi + ' ');
        }
        allApis = allApis.left(allApis.count() - 1);
        errorAttribute.second = QByteArray("APIBasedAuthentication " + allApis);
    }

    if (Q_UNLIKELY(apis.isEmpty())) {
        errorAttribute.first = QByteArray("WWW-Authenticate");
        errorAttribute.second = QByteArray("none");
    }

    return !needed;
#endif
    return true;
}

bool SecurityManager::isAuthorizationNeeded(const Hyperspace::Wave &wave, const QByteArray &interface)
{
#if 0
    bool needed = false;

    {
        int parentCapLen = INT_MAX;
        int parentCapIndex = -1;

        for (int i = 0; i < m_interfaceRestrictions.count(); i++) {
            if (interface.startsWith(m_interfaceRestrictions.at(i))) {
                int tmpLen = m_interfaceRestrictions.at(i).length();
                if (tmpLen < parentCapLen){
                    parentCapLen = tmpLen;
                    parentCapIndex = i;
                    needed = true;
                }
            }
        }

        if (parentCapIndex >= 0) {
            qCDebug(hyperdriveSecurityManagerDC) << "We have restrictions enforced by: " << m_interfaceRestrictions.at(parentCapIndex);
        }
    }

    {
        int parentCapLen = INT_MAX;
        int parentCapIndex = -1;

        for (int i = 0; i < m_interfaceExceptions.count(); i++) {
            if (interface.startsWith(m_interfaceExceptions.at(i))) {
                int tmpLen = m_interfaceExceptions.at(i).length();
                if (tmpLen < parentCapLen){
                    parentCapLen = tmpLen;
                    parentCapIndex = i;
                    needed = false;
                }
            }
        }

        if (parentCapIndex >= 0) {
            qCDebug(hyperdriveSecurityManagerDC) << "We have exceptions enforced by: " << m_interfaceExceptions.at(parentCapIndex);
        }
    }

    return needed;
#endif
    return false;
}

QList<QByteArray> SecurityManager::authAPIsFor(const QByteArray &interface)
{
#if 0
    QList<QByteArray> apis;
    foreach (AuthAPIConf *apiConf, m_securityAPIsConfs) {
        qCDebug(hyperdriveSecurityManagerDC) << "We have " << apiConf->authAPIPath << " with " << apiConf->interfaces.count();
        foreach (QByteArray c, apiConf->interfaces) {
            if (interface.startsWith(c)){
                apis.append(apiConf->authAPIPath);
                break;
            }
        }
    }
    return apis;
#endif
    return QList<QByteArray>();
}

void SecurityManager::addSecurityRestriction(const QByteArray &path)
{
#if 0
    if (path.endsWith('.')){
        qCDebug(hyperdriveSecurityManagerDC) << "Invalid path";
        return;
    }

    qCDebug(hyperdriveSecurityManagerDC) << "Added security restriction to " << path;

    if (path.endsWith('*')) {
        m_interfaceRestrictions.append(path.left(path.count() - 1));

    } else {
        m_interfaceRestrictions.append(path);
    }
#endif
}

void SecurityManager::addSecurityException(const QByteArray &path)
{
#if 0
    if (path.endsWith('.')){
        qCDebug(hyperdriveSecurityManagerDC) << "Invalid path";
        return;
    }

    qCDebug(hyperdriveSecurityManagerDC) << "Added security exception to " << path;

    if (path.endsWith('*')) {
        m_interfaceExceptions.append(path.left(path.count() - 1));

    } else {
        m_interfaceExceptions.append(path);
    }
#endif
}

void SecurityManager::addAPIConf(AuthAPIConf *conf)
{
#if 0
    m_securityAPIsConfs.append(conf);
#endif
}


SecurityManager::AuthAPIConf *SecurityManager::apiConf(const QByteArray &authAPIPath)
{
#if 0
    foreach (AuthAPIConf *conf, m_securityAPIsConfs) {
        if (conf->authAPIPath == authAPIPath){
            return conf;
        }
    }
#endif
    return nullptr;
}

void SecurityManager::authorizeSecurityPass(QDataStream &stream, Hyperspace::Socket *socket)
{
#if 0
    Hyperspace::SecurityPass *sp = Hyperspace::SecurityPass::load(stream, m_securityPassFactory);

    bool isLegit = false;
    QList<QByteArray> apis = SecurityManager::authAPIsFor(sp->path());
    foreach (QByteArray api, apis) {
        if (!api.startsWith('/')) {
            api = '/' + api;
        }
        if (m_core->socketOwnsPath(socket, api)) {
            isLegit = true;
            break;
        }
    }

    if (isLegit) {
        if (sp->securityType() == Hyperspace::SecurityToken::securityTypeName()){
            m_securityTokenManager->authorizeSecurityPass(sp);
        } else {
            qCDebug(hyperdriveSecurityManagerDC) << "Unknown security pass type: " << sp->securityType();
        }
    } else {
        qCInfo(hyperdriveSecurityManagerDC) << "You are not authorized for this  " << sp->path();
        qCInfo(hyperdriveSecurityManagerDC) << "Owners: " << apis;
    }
    delete sp;
#endif
}

void SecurityManager::revokeSecurityPass(QDataStream &stream, Hyperspace::Socket *socket)
{
#if 0
    Hyperspace::SecurityPass *sp = Hyperspace::SecurityPass::load(stream, m_securityPassFactory);

    bool isLegit = false;
    QList<QByteArray> apis = SecurityManager::authAPIsFor(sp->path());
    foreach (QByteArray api, apis)  {
        if (!api.startsWith('/')) {
            api = '/' + api;
        }
        if (m_core->socketOwnsPath(socket, api)) {
            isLegit = true;
            break;
        }
    }

    if (isLegit) {
        if (sp->securityType() == Hyperspace::SecurityToken::securityTypeName()){
            m_securityTokenManager->revokeSecurityPass(sp);
        } else {
            qCDebug(hyperdriveSecurityManagerDC) << "Unknown security pass type: " << sp->securityType();
        }
    } else {
        qCDebug(hyperdriveSecurityManagerDC) << "You are not authorized for this  " << sp->path();
        qCDebug(hyperdriveSecurityManagerDC) << "Owners: " << apis;
    }
    delete sp;
#endif
}

}
