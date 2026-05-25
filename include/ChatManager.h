#ifndef CHATMANAGER_H
#define CHATMANAGER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QMap>
#include <QList>
#include <QDateTime>
#include <QDataStream>

class ChatManager : public QObject {
    Q_OBJECT
public:
    explicit ChatManager(QObject *parent = nullptr);
    ~ChatManager() override;

    // Start the TCP server on an OS-assigned port. Returns the port number.
    quint16 start();

    // Send a message to a peer. Establishes connection if not already active.
    void sendMessage(const QString& peerUsername, const QHostAddress& peerIp, 
                     quint16 peerTcpPort, const QString& messageText);

    // Set local username (needed for handshakes)
    void setLocalUsername(const QString& username) { m_localUsername = username; }

signals:
    // Emitted when a connection with a peer is established and handshaked
    void peerConnected(const QString& peerUsername);
    
    // Emitted when a peer disconnects
    void peerDisconnected(const QString& peerUsername);
    
    // Emitted when a new chat message is received
    void messageReceived(const QString& senderUsername, const QString& messageText, const QDateTime& timestamp);

    // Emitted when a connection error occurs (useful for logging)
    void connectionError(const QString& peerUsername, const QString& errorString);

private slots:
    void handleIncomingConnection();
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketReadyRead();
    void onSocketError(QAbstractSocket::SocketError socketError);

private:
    QTcpServer *m_tcpServer;
    QString m_localUsername;

    // Map of active peer sockets. Key: peer username
    QMap<QString, QTcpSocket*> m_connections;

    // Queue of pending messages to send when connection completes. Key: peer username
    QMap<QString, QList<QString>> m_pendingMessages;

    // Protocol Constants
    enum PacketType : quint8 {
        PacketHandshake = 1,
        PacketMessage = 2
    };

    // Helper to send the initial handshake packet
    void sendHandshake(QTcpSocket* socket);

    // Helper to serialize and write a message over a socket
    void writeMessageToSocket(QTcpSocket* socket, const QString& messageText, const QDateTime& timestamp);

    // Clean up connections map and delete socket safely
    void removeConnection(const QString& peerUsername);
};

#endif // CHATMANAGER_H
