#ifndef TRANSPORTS_H
#define TRANSPORTS_H

#include <QtCore/QDebug>
#include <QtCore/QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(transportDC)

#define TRANSPORT_MAIN(Class, Name, Version) static int sighupFd[2]; \
static int sigtermFd[2]; \
 \
static void hupSignalHandler(int) \
{ \
    char a = 1; \
    ::write(sighupFd[0], &a, sizeof(a)); \
} \
 \
static void termSignalHandler(int) \
{ \
    char a = 1; \
    ::write(sigtermFd[0], &a, sizeof(a)); \
} \
 \
static int setup_unix_signal_handlers() \
{ \
    struct sigaction hup, term; \
\
    hup.sa_handler = hupSignalHandler;\
    sigemptyset(&hup.sa_mask);\
    hup.sa_flags = 0;\
    hup.sa_flags |= SA_RESTART;\
\
    if (sigaction(SIGHUP, &hup, 0) > 0) {\
        return 1;\
    }\
\
    term.sa_handler = termSignalHandler;\
    sigemptyset(&term.sa_mask);\
    term.sa_flags |= SA_RESTART;\
\
    if (sigaction(SIGTERM, &term, 0) > 0) {\
        return 2;\
    }\
\
    return 0;\
}\
\
int main(int argc, char *argv[])\
{\
    QCoreApplication app(argc, argv);\
\
    app.setApplicationName(QStringLiteral("Name transport"));\
    app.setApplicationVersion(QStringLiteral("Version"));\
    app.setOrganizationDomain(QStringLiteral("com.ispirata.hemera"));\
    app.setOrganizationName(QStringLiteral("Ispirata"));\
\
    Class *transport;\
\
    auto startItUp = [&] () {\
        transport = new Class(Q_NULLPTR);\
\
        Hemera::Operation *op = transport->init();\
\
        QObject::connect(op, &Hemera::Operation::finished, [transport, op] {\
            if (op->isError()) {\
                sd_notifyf(0, "STATUS=Failed to start up: %s\n"\
                "BUSERROR=%s",\
                qstrdup(op->errorMessage().toLatin1().constData()),\
                           qstrdup(op->errorName().toLatin1().constData()));\
\
                qFatal("Initialization of the transport failed. Error reported: %s - %s", op->errorName().toLatin1().data(),\
                       op->errorMessage().toLatin1().data());\
            } else {\
                sd_notify(0, "READY=1");\
            }\
        });\
    };\
\
    auto shutItDown = [&] () {\
        transport->deleteLater();\
    };\
\
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sighupFd)) {\
        qFatal("Couldn't create HUP socketpair");\
    }\
\
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sigtermFd)) {\
        qFatal("Couldn't create TERM socketpair");\
    }\
\
    QSocketNotifier snHup(sighupFd[1], QSocketNotifier::Read);\
    QObject::connect(&snHup, &QSocketNotifier::activated, [&] () {\
        snHup.setEnabled(false);\
        char tmp;\
        ::read(sighupFd[1], &tmp, sizeof(tmp));\
\
        sd_notify(0, "STATUS=Name transport is reloading...");\
        qCDebug(transportDC) << "Reloading Name transport...";\
        QObject::connect(transport, &QObject::destroyed, [&] {\
            startItUp();\
            snHup.setEnabled(true);\
        });\
        shutItDown();\
    });\
    QSocketNotifier snTerm(sigtermFd[1], QSocketNotifier::Read);\
    QObject::connect(&snTerm, &QSocketNotifier::activated, [&] () {\
        snTerm.setEnabled(false);\
        char tmp;\
        ::read(sigtermFd[1], &tmp, sizeof(tmp));\
\
        sd_notify(0, "STATUS=Name transport is shutting down...");\
        qCDebug(transportDC) << "Shutting down...";\
\
        QObject::connect(transport, &QObject::destroyed, &QCoreApplication::quit);\
        shutItDown();\
    });\
\
    if (setup_unix_signal_handlers() != 0) {\
        qFatal("Couldn't register UNIX signal handler");\
        return -1;\
    }\
\
    startItUp();\
\
    return app.exec();\
}

#endif
