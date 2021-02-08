#include "hyperdiscovery.h"

#include <HemeraCore/Literals>

#include <QtCore/QDebug>
#include <QtCore/QLoggingCategory>
#include <QtCore/QUrl>
#include <QtNetwork/QNetworkAddressEntry>
#include <QtNetwork/QNetworkInterface>

#include "service.h"

#define HYPERDISCOVERY_SCAN_PACKETTYPE 's'
#define HYPERDISCOVERY_ANSWER_PACKETTYPE 'a'

#define HYPERDISCOVERY_PORT 44389

#include "discoveryservices.h"


Q_LOGGING_CATEGORY(hyperdiscoveryDC, "hyperdrive.discoveryservice.hyperdiscovery", DEBUG_MESSAGES_DEFAULT_LEVEL)

HyperDiscovery::HyperDiscovery(QObject *parent)
    : RemoteDiscoveryService(parent)
{
}

HyperDiscovery::~HyperDiscovery()
{
}

void HyperDiscovery::initImpl()
{
    // TODO
    elapsedT.start();

    setName("HyperDiscovery");
    setTransportMedium("UDP");


    Hyperdrive::RemoteByteArrayListOperation *lCapsOp = localInterfaces();
    connect(lCapsOp, &Hyperdrive::RemoteByteArrayListOperation::finished, [this, lCapsOp] {
        udpSocket = new QUdpSocket(this);

        QList<QByteArray> caps = lCapsOp->result();
        qCDebug(hyperdiscoveryDC) << "Populating service interfaces cache: " << caps;
        for (int i = 0; i < caps.count(); i++) {
            m_interfaces.insert(caps.at(i));
        }

        // We must get the socket from systemd
        int fd;

        if (Q_UNLIKELY(sd_listen_fds(0) != 1)) {
            setInitError(Hemera::Literals::literal(Hemera::Literals::Errors::systemdError()),
                         QStringLiteral("No or too many file descriptors received."));
            return;
        }

        fd = SD_LISTEN_FDS_START + 0;

        if (Q_UNLIKELY(!udpSocket->setSocketDescriptor(fd))) {
            setInitError(Hemera::Literals::literal(Hemera::Literals::Errors::systemdError()),
                         QStringLiteral("Could not bind to systemd's socket."));
            return;
        }
        if (Q_UNLIKELY(!udpSocket->isOpen())) {
            setInitError(Hemera::Literals::literal(Hemera::Literals::Errors::systemdError()),
                         QStringLiteral("Systemd's socket is currently not open. This should not be happening."));
            return;
        }

        connect(udpSocket, &QUdpSocket::readyRead, this, &HyperDiscovery::packetReceived);

        setReady();
    });

    Hyperdrive::RemoteUrlFeaturesOperation *ufOp = transportsTemplateUrls();
    connect(ufOp, &Hyperdrive::RemoteUrlFeaturesOperation::finished, [this, ufOp] {
        if (Q_UNLIKELY(ufOp->isError())) {
            qCWarning(hyperdiscoveryDC) << "Could not retrieve transport template URLs! Introspection will fail.";
            return;
        }

        m_templateUrls = ufOp->result();
    });
}

void HyperDiscovery::scanInterfaces(const QList<QByteArray> &interfaces)
{
    /*
        Question packet structure: |'s' (quint8)|INTERFACES|0 (quint8)|
    */
    QByteArray discoveryPacket;
    discoveryPacket.append(HYPERDISCOVERY_SCAN_PACKETTYPE);

    for (const QByteArray &q : interfaces) {
        /*
            Interface: | interface length (quint8) | interface string |
        */
        quint8 len = q.length();
        discoveryPacket.append(len);
        discoveryPacket.append(q);
    }
    //End of interfaces list
    discoveryPacket.append((char) 0x00);

    QList<QNetworkInterface> ifaces = QNetworkInterface::allInterfaces();
    for (int i = 0; i < ifaces.count(); i++){
        QList<QNetworkAddressEntry> addrs = ifaces[i].addressEntries();
        for (int j = 0; j < addrs.size(); j++) {
            if ((addrs[j].ip().protocol() == QAbstractSocket::IPv4Protocol) && (!addrs[j].broadcast().isNull())) {
                udpSocket->writeDatagram(discoveryPacket, addrs[j].broadcast(), HYPERDISCOVERY_PORT);
            }
        }
    }
}

void HyperDiscovery::introspectInterface(const QByteArray& interface)
{
    // TODO: Implement me
}

void HyperDiscovery::onLocalInterfacesRegistered(const QList<QByteArray> &interfaces)
{
    for (int i = 0; i < interfaces.count(); i++) {
        m_interfaces.insert(interfaces.at(i));
    }

    for (const QByteArray &interface : interfaces) {
        QByteArray announcePacket;
        announcePacket.append(HYPERDISCOVERY_ANSWER_PACKETTYPE);
        appendAnswer(announcePacket, interface, 3600, 8080, "p=https"); //FIXME
        announcePacket.append((char) 0);

        udpSocket->writeDatagram(announcePacket, QHostAddress::Broadcast, HYPERDISCOVERY_PORT);
    }
}

void HyperDiscovery::onLocalInterfacesUnregistered(const QList<QByteArray> &interfaces)
{
    for (int i = 0; i < interfaces.count(); i++) {
        m_interfaces.remove(interfaces.at(i));
    }

    for (const QByteArray &interface : interfaces) {
        QByteArray announcePacket;
        announcePacket.append(HYPERDISCOVERY_ANSWER_PACKETTYPE);
        appendAnswer(announcePacket, interface, 0, 8080, "p=https"); //FIXME
        announcePacket.append((char) 0);

        udpSocket->writeDatagram(announcePacket, QHostAddress::Broadcast, HYPERDISCOVERY_PORT);
    }
}

void HyperDiscovery::appendAnswer(QByteArray &a, const QByteArray &cap, quint16 ttl, quint16 port, const QByteArray &txt)
{
    /*
     Answer structure: |Interface length (quint8)|Extra data length (quint8)|TTL (quint16)|Port (quint16)|
                       |Interface|Extra data|
    */

    //interface.length() and extra data field length
    a.append((char) cap.length());
    a.append((char) txt.length());

    //TTL
    a.append((char) (ttl & 0xFF));
    a.append((char) ((ttl >> 8) & 0xFF));

    //Port
    a.append((char) (port & 0xFF));
    a.append((char) ((port >> 8) & 0xFF));

    a.append(cap);
    a.append(txt);
}

void HyperDiscovery::buildAnswer(QByteArray &answer, const QByteArray &interface)
{
    /*
        Answer packet: |'a' (quint8)| ANSWERS |
    */
    if (answer.size() == 0){
        answer.append(HYPERDISCOVERY_ANSWER_PACKETTYPE);
    }

    if (interface.endsWith('*')){
        qCDebug(hyperdiscoveryDC) << "* search";
        QByteArray searchPrefix = interface.left(interface.length() - 1);

        for (QSet<QByteArray>::const_iterator it = m_interfaces.constBegin(); it != m_interfaces.constEnd(); ++it) {
            if ((*it).left(searchPrefix.length()) == searchPrefix){
                appendAnswer(answer, (*it), 3600, 8080, "p=https"); //FIXME
            }
        }

    }else if (m_interfaces.contains(interface)) {
        qCDebug(hyperdiscoveryDC) << "We have: " << interface;
        appendAnswer(answer, interface, 3600, 8080, "p=https"); //FIXME
    }else{
        qCDebug(hyperdiscoveryDC) << "Missing interface: " << interface;
        return;
    }
}

void HyperDiscovery::packetReceived()
{
    qCDebug(hyperdiscoveryDC) << Q_FUNC_INFO;
    QList<QByteArray> interfacesAnnounced;
    QList<QByteArray> interfacesPurged;

    while (udpSocket->hasPendingDatagrams()){
        int pendingSize = udpSocket->pendingDatagramSize();
        qCDebug(hyperdiscoveryDC) << "Datagram size: " << pendingSize;
        QByteArray data;
        data.resize(pendingSize);
        QHostAddress client;
        quint16 clientPort;
        udpSocket->readDatagram(data.data(), pendingSize, &client, &clientPort);

        int rPos = 0;
        if (data.at(rPos) == HYPERDISCOVERY_SCAN_PACKETTYPE) {
            QByteArray answer;

            rPos++;
            while (data.at(rPos) != (unsigned char) 0) {
                int cLen = (unsigned char) data[rPos];
                rPos++;
                if ((rPos + cLen) > data.size()) break;
                QByteArray cap(data.data() + rPos, cLen);
                qCDebug(hyperdiscoveryDC) << cap << cLen;
                buildAnswer(answer, cap);
                rPos += cLen;
            }
            answer.append((char) 0);

            udpSocket->writeDatagram(answer, client, clientPort);

        }else if (data.at(rPos) == HYPERDISCOVERY_ANSWER_PACKETTYPE) {
            rPos++;

            Q_FOREVER {
                if (rPos >= data.length()) break; //CHECK OUT OF BOUNDS
                quint8 capLen = data.at(rPos);
                qCDebug(hyperdiscoveryDC) << "Cap len: " << capLen;
                if (capLen == 0){
                    //NULL length interface found, we don't have more data
                    break;
                }
                rPos++;

                if (rPos >= data.length()) break; //CHECK OUT OF BOUNDS
                quint8 txtLen = data.at(rPos);
                rPos++;
                qCDebug(hyperdiscoveryDC) << "Txt len: " << txtLen;

                if (rPos + 1 >= data.length()) break; //CHECK OUT OF BOUNDS
                quint16 ttl = data.at(rPos) | data.at(rPos + 1) << 8;
                rPos += 2;

                if (rPos + 1>= data.length()) break; //CHECK OUT OF BOUNDS
                quint16 port = ((quint8) data.at(rPos)) | (((quint8) data.at(rPos + 1)) << 8);
                rPos += 2;

                if (rPos + capLen > data.length()) break; //CHECK OUT OF BOUNDS
                QByteArray cap(data.data() + rPos, capLen);
                rPos += capLen;

                if (rPos + txtLen> data.length()) break; //CHECK OUT OF BOUNDS
                QByteArray txt(data.data() + rPos, txtLen);
                rPos += txtLen;

                qCDebug(hyperdiscoveryDC) << "TTL: " << ttl << " PORT: " << port << " INTERFACE: " << cap << " TXT: " << txt;

                Service *service = 0;
                for (QHash<QByteArray, Service *>::const_iterator it = services.constFind(cap); it != services.constEnd(); ++it) {
                    if (it.value()->address == client) {
                        service = it.value();
                        break;
                    }
                }

                if (ttl != 0) {
                    if (service == 0) {
                        service = new Service;
                        service->interface = cap;
                        service->address = client;
                        services.insert(cap, service);
                    }

                    service->ttl = ttl;
                    service->port = port;
                    service->timestamp = elapsedT.msecsSinceReference() / 1000;
                    interfacesAnnounced.append(cap);
                }else{
                    delete service;
                    interfacesPurged.append(cap);
                }
            }
            if (interfacesAnnounced.count()) {
                setInterfacesAnnounced(interfacesAnnounced);
            }
            if (interfacesPurged.count()) {
                setInterfacesPurged(interfacesPurged);
            }
        }
    }
}

DISCOVERY_SERVICE_MAIN(HyperDiscovery, Hyperdiscovery, 1.0.0)
