#ifndef PEERMANAGER_H
#define PEERMANAGER_H

#include <QObject>
#include <QUdpSocket>
#include <QTimer>
#include <QDateTime>
#include <QHostAddress>
#include <QMap>
#include <QNetworkInterface>
#include <QNetworkAddressEntry>

struct PeerInfo {
    QString username;
    QHostAddress ip;
    quint16 tcpPort;
    QDateTime lastSeen;
};

class PeerManager : public QObject {
    Q_OBJECT
public:
    explicit PeerManager(QObject *parent = nullptr);
    ~PeerManager() override = default;

    // Start discovery with the specified username and TCP chat port
    void start(const QString& username, quint16 tcpPort);

    // Toggle local user visibility (enable/disable broadcasting)
    void setVisibility(bool visible);
    bool isVisible() const { return m_isVisible; }

    // Set/update local username
    void setUsername(const QString& username);

    // Get list of active online peers
    QMap<QString, PeerInfo> activePeers() const { return m_activePeers; }

signals:
    // Emitted when a new peer is discovered or an existing one is updated
    void peerDiscovered(const QString& username, const QHostAddress& ip, quint16 tcpPort);
    
    // Emitted when a peer times out (no heartbeat for > 8s)
    void peerRemoved(const QString& username);
    
    // Emitted whenever the list of active peers changes
    void peerListUpdated();

private slots:
    void sendHeartbeat();
    void processPendingDatagrams();
    void checkPeerTimeouts();

private:
    QUdpSocket *m_udpSocket;
    QTimer *m_heartbeatTimer;
    QTimer *m_cleanupTimer;

    QString m_myUsername;
    quint16 m_myTcpPort;
    bool m_isVisible;

    QMap<QString, PeerInfo> m_activePeers; // Key: Username

    static const quint16 UDP_PORT = 45454;
    static const int HEARTBEAT_INTERVAL_MS = 2500;
    static const int CLEANUP_INTERVAL_MS = 2000;
    static const int PEER_TIMEOUT_SECS = 8;

    // Helper to retrieve broadcast addresses of all active interfaces
    QList<QHostAddress> getBroadcastAddresses();
};

#endif // PEERMANAGER_H
