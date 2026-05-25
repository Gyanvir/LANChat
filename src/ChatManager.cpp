#include "ChatManager.h"
#include <QDebug>

ChatManager::ChatManager(QObject *parent)
    : QObject(parent),
      m_tcpServer(nullptr) {
    m_tcpServer = new QTcpServer(this);
}

ChatManager::~ChatManager() {
    // Clean up all active sockets
    for (auto socket : m_connections) {
        if (socket) {
            socket->disconnect(this);
            socket->close();
            socket->deleteLater();
        }
    }
    m_connections.clear();

    if (m_tcpServer) {
        m_tcpServer->close();
    }
}

quint16 ChatManager::start() {
    // Listen on Any IPv4 address, and port 0 to let the OS pick a random free port
    if (!m_tcpServer->listen(QHostAddress::AnyIPv4, 0)) {
        qWarning() << "ChatManager: Failed to start TCP server:" << m_tcpServer->errorString();
        return 0;
    }

    quint16 port = m_tcpServer->serverPort();
    qDebug() << "ChatManager: TCP Server listening on port" << port;

    connect(m_tcpServer, &QTcpServer::newConnection, this, &ChatManager::handleIncomingConnection);

    return port;
}

void ChatManager::sendMessage(const QString& peerUsername, const QHostAddress& peerIp, 
                             quint16 peerTcpPort, const QString& messageText) {
    if (peerUsername.isEmpty()) {
        return;
    }

    // Check if we already have a socket registered for this peer
    if (m_connections.contains(peerUsername)) {
        QTcpSocket *socket = m_connections[peerUsername];
        if (socket->state() == QAbstractSocket::ConnectedState) {
            writeMessageToSocket(socket, messageText, QDateTime::currentDateTime());
            return;
        } else if (socket->state() == QAbstractSocket::ConnectingState || 
                   socket->state() == QAbstractSocket::HostLookupState) {
            // Socket is in the process of connecting. Queue the message.
            m_pendingMessages[peerUsername].append(messageText);
            return;
        }
    }

    // Create a new outgoing socket
    QTcpSocket *socket = new QTcpSocket(this);
    socket->setProperty("packetSize", 0);
    socket->setProperty("peerUsername", peerUsername);

    // Register it immediately so that subsequent send calls queue their messages
    m_connections[peerUsername] = socket;
    m_pendingMessages[peerUsername].append(messageText);

    // Connect socket signals
    connect(socket, &QTcpSocket::connected, this, &ChatManager::onSocketConnected);
    connect(socket, &QTcpSocket::readyRead, this, &ChatManager::onSocketReadyRead);
    connect(socket, &QTcpSocket::disconnected, this, &ChatManager::onSocketDisconnected);
    connect(socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error),
            this, &ChatManager::onSocketError);

    qDebug() << "ChatManager: Attempting to connect to" << peerUsername << "at" << peerIp.toString() << ":" << peerTcpPort;
    socket->connectToHost(peerIp, peerTcpPort);
}

void ChatManager::handleIncomingConnection() {
    while (m_tcpServer->hasPendingConnections()) {
        QTcpSocket *socket = m_tcpServer->nextPendingConnection();
        socket->setProperty("packetSize", 0);
        socket->setProperty("peerUsername", ""); // Will be set upon handshake

        // Connect socket signals
        connect(socket, &QTcpSocket::readyRead, this, &ChatManager::onSocketReadyRead);
        connect(socket, &QTcpSocket::disconnected, this, &ChatManager::onSocketDisconnected);
        connect(socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error),
                this, &ChatManager::onSocketError);

        qDebug() << "ChatManager: Accepted incoming TCP connection from" 
                 << socket->peerAddress().toString() << ":" << socket->peerPort();
    }
}

void ChatManager::onSocketConnected() {
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    QString peerUsername = socket->property("peerUsername").toString();
    qDebug() << "ChatManager: Connected to peer" << peerUsername;

    // 1. Immediately send our handshake so the remote peer knows who is connecting
    sendHandshake(socket);

    // 2. Deliver any pending messages queued while the socket was connecting
    if (m_pendingMessages.contains(peerUsername)) {
        for (const QString& msg : m_pendingMessages[peerUsername]) {
            writeMessageToSocket(socket, msg, QDateTime::currentDateTime());
        }
        m_pendingMessages.remove(peerUsername);
    }

    emit peerConnected(peerUsername);
}

void ChatManager::onSocketDisconnected() {
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    QString peerUsername = socket->property("peerUsername").toString();
    qDebug() << "ChatManager: Socket disconnected for peer:" << (peerUsername.isEmpty() ? "Unknown" : peerUsername);

    // Clean up connections map if this is the active socket for the peer
    if (!peerUsername.isEmpty() && m_connections.value(peerUsername) == socket) {
        m_connections.remove(peerUsername);
        emit peerDisconnected(peerUsername);
    }

    socket->deleteLater();
}

void ChatManager::onSocketReadyRead() {
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    QDataStream in(socket);
    in.setVersion(QDataStream::Qt_5_15);

    while (true) {
        quint32 size = socket->property("packetSize").toUInt();
        
        // Read the packet size header if we don't have it yet
        if (size == 0) {
            if (socket->bytesAvailable() < sizeof(quint32)) {
                break; // Not enough bytes to read the size header
            }
            in >> size;
            socket->setProperty("packetSize", size);
        }

        // Verify if the complete packet data has arrived
        if (socket->bytesAvailable() < size) {
            break; // Wait for the remaining bytes to arrive
        }

        // Reset for the next packet
        socket->setProperty("packetSize", 0);

        // Read the Packet Type
        quint8 typeVal;
        in >> typeVal;
        PacketType type = static_cast<PacketType>(typeVal);

        if (type == PacketHandshake) {
            QString peerName;
            in >> peerName;

            qDebug() << "ChatManager: Handshake received. Remote peer is:" << peerName;
            socket->setProperty("peerUsername", peerName);

            // Handle potential socket conflicts/reconnections
            if (m_connections.contains(peerName)) {
                QTcpSocket *existingSocket = m_connections[peerName];
                if (existingSocket != socket) {
                    qDebug() << "ChatManager: Replacing existing socket for" << peerName;
                    existingSocket->disconnect(this);
                    existingSocket->close();
                    existingSocket->deleteLater();
                }
            }

            m_connections[peerName] = socket;
            emit peerConnected(peerName);

        } else if (type == PacketMessage) {
            QString senderUsername;
            QString messageText;
            qint64 epochTime;
            in >> senderUsername >> messageText >> epochTime;

            QDateTime timestamp = QDateTime::fromMSecsSinceEpoch(epochTime);
            qDebug() << "ChatManager: Message received from" << senderUsername << ":" << messageText;

            emit messageReceived(senderUsername, messageText, timestamp);
        }
    }
}

void ChatManager::onSocketError(QAbstractSocket::SocketError socketError) {
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    QString peerUsername = socket->property("peerUsername").toString();
    QString errorMsg = socket->errorString();
    
    qWarning() << "ChatManager: Socket error for" << (peerUsername.isEmpty() ? "Unknown" : peerUsername) 
               << "Error code:" << socketError << "Msg:" << errorMsg;

    emit connectionError(peerUsername, errorMsg);
}

void ChatManager::sendHandshake(QTcpSocket* socket) {
    if (!socket || socket->state() != QAbstractSocket::ConnectedState) {
        return;
    }

    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_5_15);

    // Prepend 0 for sizing block
    out << static_cast<quint32>(0);
    out << static_cast<quint8>(PacketHandshake);
    out << m_localUsername;

    // Calculate exact payload size and overwrite size header
    out.device()->seek(0);
    out << static_cast<quint32>(block.size() - sizeof(quint32));

    socket->write(block);
    socket->flush();
}

void ChatManager::writeMessageToSocket(QTcpSocket* socket, const QString& messageText, const QDateTime& timestamp) {
    if (!socket || socket->state() != QAbstractSocket::ConnectedState) {
        return;
    }

    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_5_15);

    // Prepend 0 for sizing block
    out << static_cast<quint32>(0);
    out << static_cast<quint8>(PacketMessage);
    out << m_localUsername;
    out << messageText;
    out << static_cast<qint64>(timestamp.toMSecsSinceEpoch());

    // Calculate exact payload size and overwrite size header
    out.device()->seek(0);
    out << static_cast<quint32>(block.size() - sizeof(quint32));

    socket->write(block);
    socket->flush();
}
