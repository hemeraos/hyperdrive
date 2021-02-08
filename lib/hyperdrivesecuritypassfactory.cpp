/*
 *
 */

#include "hyperdrivesecuritypassfactory.h"

#include <HyperspaceCore/SecurityPass>
#include <HyperspaceCore/SecurityToken>

#include <QtCore/QDebug>
#include <QtCore/QLoggingCategory>

Q_LOGGING_CATEGORY(hyperdriveSecurityPassFactoryDC, "hyperdrive.security.pass.factory", DEBUG_MESSAGES_DEFAULT_LEVEL)

namespace Hyperdrive
{

SecurityPassFactory::SecurityPassFactory(QObject *parent)
    : SecurityPassAbstractFactory(parent)
{
}

SecurityPassFactory::~SecurityPassFactory()
{
}


Hyperspace::SecurityPass *SecurityPassFactory::load(const QByteArray &securityPassType)
{
    if (securityPassType == Hyperspace::SecurityToken::securityTypeName()) {
       return new Hyperspace::SecurityToken();
    } else {
       return nullptr;
    }
}

}
