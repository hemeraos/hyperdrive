#include <QtCore/QCoreApplication>

#include <HemeraCore/Operation>

#include "astartegatewaypairinghelper.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    app.setApplicationName(QStringLiteral("Astarte Gateway Pairing Helper"));
    app.setApplicationVersion(QStringLiteral(HYPERDRIVE_VERSION));
    app.setOrganizationDomain(QStringLiteral("com.ispirata.hemera"));
    app.setOrganizationName(QStringLiteral("Ispirata"));

    AstarteGatewayPairingHelper *agph = new AstarteGatewayPairingHelper(nullptr);
    Hemera::Operation *op = agph->init();

    QObject::connect(op, &Hemera::Operation::finished, [op] {
        if (op->isError()) {
            qFatal("Initialization of the pairing helper failed. Error reported: %s - %s", op->errorName().toLatin1().data(),
                    op->errorMessage().toLatin1().data());
        }
    });

    return app.exec();
}
