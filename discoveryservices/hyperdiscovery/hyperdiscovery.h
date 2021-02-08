#ifndef _HYPERDISCOVERY_H_
#define _HYPERDISCOVERY_H_

#include "hyperdriveremotediscoveryservice.h"

#include <QtCore/QObject>
#include <QtCore/QByteArray>
#include <QtCore/QElapsedTimer>
#include <QtCore/QStringList>
#include <QtCore/QSet>

#include <QtNetwork/QUdpSocket>

class Service;
class DiscoveryCore;

class HyperDiscovery : public Hyperdrive::RemoteDiscoveryService
{
    Q_OBJECT

public:
    explicit HyperDiscovery(QObject *parent = 0);
    virtual ~HyperDiscovery();
    void scanInterfaces(const QList<QByteArray> &interfaces);
    void appendAnswer(QByteArray &a, const QByteArray &cap, quint16 ttl, quint16 port, const QByteArray &txt);

public slots:
    void packetReceived();

protected:
    virtual void initImpl() Q_DECL_OVERRIDE Q_DECL_FINAL;

    virtual void introspectInterface(const QByteArray& interface) Q_DECL_OVERRIDE Q_DECL_FINAL;

    virtual void onLocalInterfacesRegistered(const QList<QByteArray> &interfaces) Q_DECL_OVERRIDE Q_DECL_FINAL;
    virtual void onLocalInterfacesUnregistered(const QList<QByteArray> &interfaces) Q_DECL_OVERRIDE Q_DECL_FINAL;

private:
    QUdpSocket *udpSocket;
    void buildAnswer(QByteArray &answer, const QByteArray &interface);

    QMultiHash<QByteArray, Service *> services;
    QElapsedTimer elapsedT;
    QSet<QByteArray> m_interfaces;
    QHash< QUrl, Hyperdrive::Transport::Features > m_templateUrls;
};

#endif

