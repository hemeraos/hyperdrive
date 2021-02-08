#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <QtCore/QLoggingCategory>
#include <QtCore/QSocketNotifier>
#include <QtCore/QTimer>

#include <HemeraCore/Operation>

#include <systemd/sd-daemon.h>

#include <signal.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>

#include "hyperdrivecore.h"
#include <hyperdrivetransportmanager.h>

Q_LOGGING_CATEGORY(hyperdriveDC, "hyperdrive.daemon", DEBUG_MESSAGES_DEFAULT_LEVEL)

static int sighupFd[2];
static int sigtermFd[2];

static void hupSignalHandler(int)
{
    char a = 1;
    ::write(sighupFd[0], &a, sizeof(a));
}

static void termSignalHandler(int)
{
    char a = 1;
    ::write(sigtermFd[0], &a, sizeof(a));
}

static int setup_unix_signal_handlers()
{
    struct sigaction hup, term;

    hup.sa_handler = hupSignalHandler;
    sigemptyset(&hup.sa_mask);
    hup.sa_flags = 0;
    hup.sa_flags |= SA_RESTART;

    if (sigaction(SIGHUP, &hup, 0) > 0) {
        return 1;
    }

    term.sa_handler = termSignalHandler;
    sigemptyset(&term.sa_mask);
    term.sa_flags |= SA_RESTART;

    if (sigaction(SIGTERM, &term, 0) > 0) {
        return 2;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    app.setApplicationName(QStringLiteral("Hyperdrive"));
    app.setApplicationVersion(QStringLiteral(HYPERDRIVE_VERSION));
    app.setOrganizationDomain(QStringLiteral("com.ispirata.hemera"));
    app.setOrganizationName(QStringLiteral("Ispirata"));

    Hyperdrive::Core *core;

    auto startItUp = [&] () {
            core = new Hyperdrive::Core(Q_NULLPTR);

            Hemera::Operation *op = core->init();

            QObject::connect(op, &Hemera::Operation::finished, [core, op] {
                if (op->isError()) {
                    // Notify failure to systemd
                    sd_notifyf(0, "STATUS=Failed to start up: %s\n"
                               "BUSERROR=%s",
                               qstrdup(op->errorMessage().toLatin1().constData()),
                               qstrdup(op->errorName().toLatin1().constData()));

                    qFatal("Initialization of the core failed. Error reported: %s - %s", op->errorName().toLatin1().data(),
                                                                                         op->errorMessage().toLatin1().data());
                } else {
                    // Load static plugins here

                    // Notify startup to systemd
                    sd_notify(0, "READY=1");
                }
            });
    };

    auto shutItDown = [&] () {
            // Trigger a chain deletion
            QObject::connect(core->transportManager(), &QObject::destroyed,
                             core, &QObject::deleteLater);
            core->transportManager()->deleteLater();
    };

    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sighupFd)) {
        qFatal("Couldn't create HUP socketpair");
    }

    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sigtermFd)) {
        qFatal("Couldn't create TERM socketpair");
    }

    // Signal handling
    QSocketNotifier snHup(sighupFd[1], QSocketNotifier::Read);
    QObject::connect(&snHup, &QSocketNotifier::activated, [&] () {
            // Handle SIGHUP here
            snHup.setEnabled(false);
            char tmp;
            ::read(sighupFd[1], &tmp, sizeof(tmp));

            sd_notify(0, "STATUS=Hyperdrive is reloading...");
            qCDebug(hyperdriveDC) << "Reloading Hyperdrive...";
            // Destroy & create
            QObject::connect(core, &QObject::destroyed, [&] {
                startItUp();
                snHup.setEnabled(true);
            });
            shutItDown();
    });
    QSocketNotifier snTerm(sigtermFd[1], QSocketNotifier::Read);
    QObject::connect(&snTerm, &QSocketNotifier::activated, [&] () {
            // Handle SIGTERM here
            snTerm.setEnabled(false);
            char tmp;
            ::read(sigtermFd[1], &tmp, sizeof(tmp));

            sd_notify(0, "STATUS=Hyperdrive is shutting down...");
            qCDebug(hyperdriveDC) << "Shutting down...";

            QObject::connect(core, &QObject::destroyed, &QCoreApplication::quit);
            shutItDown();
    });

    if (setup_unix_signal_handlers() != 0) {
        qFatal("Couldn't register UNIX signal handler");
        return -1;
    }

    // Start the application
    startItUp();

    return app.exec();
}
