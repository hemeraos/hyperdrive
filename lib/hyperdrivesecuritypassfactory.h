/*
 *
 */

#ifndef HYPERDRIVE_SECURITYPASSFACTORY_H
#define HYPERDRIVE_SECURITYPASSFACTORY_H

#include <HyperspaceCore/SecurityPassAbstractFactory>

namespace Hyperspace {
    class SecurityPass;
}

namespace Hyperdrive {

class SecurityPassFactory : public Hyperspace::SecurityPassAbstractFactory
{
    Q_OBJECT
    Q_DISABLE_COPY(SecurityPassFactory)

public:
    SecurityPassFactory(QObject *parent = Q_NULLPTR);
    virtual ~SecurityPassFactory();

    virtual Hyperspace::SecurityPass *load(const QByteArray &securityPassType) override;
};
}

#endif // HYPERDRIVE_SECURITYPASSFACTORY_H
