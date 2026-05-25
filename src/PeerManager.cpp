#include "PeerManager.h"
#include <QJsonObject>
#include <QJsonDocument>

PeerManager::PeerManager(QObject *parent)
    : QObject(parent),
      m_udpSocket(nullptr),
      m_heartbeatTimer(nullptr),
      m_cleanupTimer(nullptr),
      m_myTcpPort(0),
      m_isVisible(true) {
      
    m_udpSocket = new QUdpSocket(this);
    m_heartbeatTimer = new QTimer(this);
    m_cleanupTimer = new QTimer(this);

    connect(m_heartbeatTimer, &QTimer::timeout, this, &PeerManager::sendHeartbeat);
    connect(m_cleanupTimer, &QTimer::timeout, this, &PeerManager::checkPeerTimeouts);
}

void PeerManager::start(const QString& username, quint16 tcpPort) {
    m_myUsername = username;
    m_myTcpPort = tcpPort;

    // Bind to the UDP port. ShareAddress allows multiple instances on the same machine to bind.
    // ReuseAddressHint is useful to quickly bind to a port that was recently closed.
    bool bound = m_udpSocket->bind(QHostAddress::AnyIPv4, UDP_PORT, 
                                  QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
    if (!bound) {
        qWarning() << "PeerManager: Failed to bind UDP socket to port" << UDP_PORT;
    } else {
        qDebug() << "PeerManager: UDP discovery socket bound to port" << UDP_PORT;
        connect(m_udpSocket, &QUdpSocket::readyRead, this, &PeerManager::processPendingDatagrams);
    }

    // Start discovery timers
    m_heartbeatTimer->start(HEARTBEAT_INTERVAL_MS);
    m_cleanupTimer->start(CLEANUP_INTERVAL_MS);

    // Broadcast our presence immediately on start
    sendHeartbeat();
}

void PeerManager::setVisibility(bool visible) {
    if (m_isVisible != visible) {
        m_isVisible = visible;
        qDebug() << "PeerManager: Visibility changed to" << (m_isVisible ? "ON" : "OFF");
        if (m_isVisible) {
            sendHeartbeat(); // Broadcast immediately when turning visible
        }
    }
}

void PeerManager::setUsername(const QString& username) {
    m_myUsername = username;
    if (m_isVisible) {
        sendHeartbeat(); // Re-broadcast immediately with the new name
    }
}

void PeerManager::sendHeartbeat() {
    if (!m_isVisible || m_myUsername.isEmpty() || m_myTcpPort == 0) {
        return;
    }

    // Construct the discovery packet as JSON
    QJsonObject packet;
    packet["username"] = m_myUsername;
    packet["tcpPort"] = m_myTcpPort;

    QJsonDocument doc(packet);
    QByteArray data = doc.toJson(QJsonDocument::Compact);

    // Broadcast on all active interfaces
    QList<QHostAddress> broadcastAddrs = getBroadcastAddresses();
    for (const QHostAddress& addr : broadcastAddrs) {
        m_udpSocket->writeDatagram(data, addr, UDP_PORT);
    }

    // Fallback/standard broadcast to 255.255.255.255
    m_udpSocket->writeDatagram(data, QHostAddress::Broadcast, UDP_PORT);
}

void PeerManager::processPendingDatagrams() {
    while (m_udpSocket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(static_cast<int>(m_udpSocket->pendingDatagramSize()));
        QHostAddress senderIp;
        quint16 senderPort;

        m_udpSocket->readDatagram(datagram.data(), datagram.size(), &senderIp, &senderPort);

        // Parse JSON heartbeat
        QJsonDocument doc = QJsonDocument::fromJson(datagram);
        if (doc.isNull() || !doc.isObject()) {
            continue;
        }

        QJsonObject obj = doc.object();
        QString peerUsername = obj["username"].toString();
        int peerTcpPortVal = obj["tcpPort"].toInt();

        if (peerUsername.isEmpty() || peerTcpPortVal <= 0 || peerTcpPortVal > 65535) {
            continue;
        }

        quint16 peerTcpPort = static_cast<quint16>(peerTcpPortVal);

        // Ignore our own heartbeat broadcasts
        if (peerUsername == m_myUsername) {
            continue;
        }

        // Standardize localhost loopback IP to avoid confusion when testing locally
        QHostAddress peerIp = senderIp;
        if (peerIp.isLoopback()) {
            peerIp = QHostAddress::LocalHost;
        }

        bool isNew = !m_activePeers.contains(peerUsername);
        bool hasChanged = false;

        if (!isNew) {
            PeerInfo existing = m_activePeers[peerUsername];
            if (existing.ip != peerIp || existing.tcpPort != peerTcpPort) {
                hasChanged = true;
            }
        }

        // Update peer info
        PeerInfo info;
        info.username = peerUsername;
        info.ip = peerIp;
        info.tcpPort = peerTcpPort;
        info.lastSeen = QDateTime::currentDateTime();

        m_activePeers[peerUsername] = info;

        if (isNew || hasChanged) {
            qDebug() << "PeerManager: Discovered peer" << peerUsername << "at" << peerIp.toString() << ":" << peerTcpPort;
            emit peerDiscovered(peerUsername, peerIp, peerTcpPort);
            emit peerListUpdated();
        }
    }
}

void PeerManager::checkPeerTimeouts() {
    QDateTime now = QDateTime::currentDateTime();
    auto it = m_activePeers.begin();
    bool changed = false;

    while (it != m_activePeers.end()) {
        if (it.value().lastSeen.secsTo(now) > PEER_TIMEOUT_SECS) {
            QString expiredPeer = it.key();
            qDebug() << "PeerManager: Peer timed out:" << expiredPeer;
            it = m_activePeers.erase(it);
            emit peerRemoved(expiredPeer);
            changed = true;
        } else {
            ++it;
        }
    }

    if (changed) {
        emit peerListUpdated();
    }
}

QList<QHostAddress> PeerManager::getBroadcastAddresses() {
    QList<QHostAddress> addresses;
    for (const QNetworkInterface& interface : QNetworkInterface::allInterfaces()) {
        // Only active, non-loopback interfaces that can broadcast
        if (interface.flags().testFlag(QNetworkInterface::IsUp) &&
            !interface.flags().testFlag(QNetworkInterface::IsLoopBack) &&
            interface.flags().testFlag(QNetworkInterface::CanBroadcast)) {
            
            for (const QNetworkAddressEntry& entry : interface.addressEntries()) {
                QHostAddress broadcast = entry.broadcast();
                // Filter for IPv4 broadcast addresses to keep it clean
                if (!broadcast.isNull() && broadcast.protocol() == QAbstractSocket::IPv4Protocol) {
                    addresses.append(broadcast);
                }
            }
        }
    }
    return addresses;
}
