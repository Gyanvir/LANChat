#include "MainWindow.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QStyle>
#include <QInputDialog>
#include <QSettings>
#include <QScrollBar>
#include <QCoreApplication>
#include <QMessageBox>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_peerManager(nullptr),
      m_chatManager(nullptr),
      m_myTcpPort(0) {

    loadSettingsAndInitialize();
    setupUi();
    applyStyleSheet();

    // Instantiate Networking Managers
    m_peerManager = new PeerManager(this);
    m_chatManager = new ChatManager(this);

    // Set local username for outbound handshakes
    m_chatManager->setLocalUsername(m_myUsername);

    // Start the TCP server on a dynamic port
    m_myTcpPort = m_chatManager->start();
    if (m_myTcpPort == 0) {
        QMessageBox::critical(this, "Network Error", 
                              "Failed to start the TCP Chat Server. The app may not be able to receive messages.");
    }

    // Update profile display with port info
    m_myProfileLabel->setText(QString("%1\n[Port: %2]").arg(m_myUsername).arg(m_myTcpPort));

    // Connect signals and slots for discovery and messaging
    connect(m_peerManager, &PeerManager::peerDiscovered, this, &MainWindow::onPeerDiscovered);
    connect(m_peerManager, &PeerManager::peerRemoved, this, &MainWindow::onPeerRemoved);
    connect(m_peerManager, &PeerManager::peerListUpdated, this, &MainWindow::onPeerListUpdated);

    connect(m_chatManager, &ChatManager::messageReceived, this, &MainWindow::onMessageReceived);
    connect(m_chatManager, &ChatManager::peerConnected, this, &MainWindow::onPeerConnected);
    connect(m_chatManager, &ChatManager::peerDisconnected, this, &MainWindow::onPeerDisconnected);
    connect(m_chatManager, &ChatManager::connectionError, this, &MainWindow::onConnectionError);

    // Start discovery and broadcasting
    m_peerManager->start(m_myUsername, m_myTcpPort);
}

void MainWindow::loadSettingsAndInitialize() {
    QSettings settings("LANChat", "Preferences");
    m_myUsername = settings.value("username").toString().trimmed();

    if (m_myUsername.isEmpty()) {
        // Fallback to system environment username, otherwise generic name
        m_myUsername = qEnvironmentVariable("USERNAME").trimmed();
        if (m_myUsername.isEmpty()) {
            m_myUsername = "User_" + QString::number(QCoreApplication::applicationPid() % 1000);
        }
        settings.setValue("username", m_myUsername);
    }

    // Load history from local file log
    m_chatHistory = DatabaseManager::loadAllMessages(m_myUsername);
}

void MainWindow::setupUi() {
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    m_mainSplitter = new QSplitter(Qt::Horizontal, centralWidget);
    
    QHBoxLayout *mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(m_mainSplitter);

    // ------------------ LEFT SIDEBAR ------------------
    m_leftPanel = new QWidget(m_mainSplitter);
    m_leftPanel->setObjectName("sidebarWidget");
    QVBoxLayout *leftLayout = new QVBoxLayout(m_leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(0);

    // Profile and config section
    QWidget *profileWidget = new QWidget(m_leftPanel);
    profileWidget->setObjectName("profileWidget");
    QVBoxLayout *profileLayout = new QVBoxLayout(profileWidget);
    profileLayout->setContentsMargins(12, 12, 12, 12);
    profileLayout->setSpacing(8);

    m_myProfileLabel = new QLabel(m_myUsername, profileWidget);
    m_myProfileLabel->setObjectName("myProfileLabel");
    m_myProfileLabel->setWordWrap(true);
    profileLayout->addWidget(m_myProfileLabel);

    QHBoxLayout *profileBtnLayout = new QHBoxLayout();
    profileBtnLayout->setSpacing(8);

    m_changeNameBtn = new QPushButton("Set Name", profileWidget);
    m_changeNameBtn->setCursor(Qt::PointingHandCursor);
    connect(m_changeNameBtn, &QPushButton::clicked, this, &MainWindow::onChangeUsernameClicked);
    profileBtnLayout->addWidget(m_changeNameBtn);

    m_visibilityBtn = new QPushButton("Visible", profileWidget);
    m_visibilityBtn->setObjectName("visibilityBtn");
    m_visibilityBtn->setCheckable(true);
    m_visibilityBtn->setChecked(true);
    m_visibilityBtn->setProperty("activeState", "visible");
    m_visibilityBtn->setCursor(Qt::PointingHandCursor);
    connect(m_visibilityBtn, &QPushButton::clicked, this, &MainWindow::onVisibilityToggled);
    profileBtnLayout->addWidget(m_visibilityBtn);

    profileLayout->addLayout(profileBtnLayout);
    leftLayout->addWidget(profileWidget);

    // Online peer list
    m_userListWidget = new QListWidget(m_leftPanel);
    m_userListWidget->setObjectName("userListWidget");
    connect(m_userListWidget, &QListWidget::itemSelectionChanged, this, &MainWindow::onUserSelectionChanged);
    leftLayout->addWidget(m_userListWidget);

    // ------------------ RIGHT CHAT CONTENT ------------------
    m_rightPanel = new QWidget(m_mainSplitter);
    m_rightPanel->setObjectName("chatAreaWidget");
    QVBoxLayout *rightLayout = new QVBoxLayout(m_rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);

    // Header bar
    m_chatHeaderWidget = new QWidget(m_rightPanel);
    m_chatHeaderWidget->setObjectName("headerWidget");
    QVBoxLayout *headerLayout = new QVBoxLayout(m_chatHeaderWidget);
    headerLayout->setContentsMargins(16, 12, 16, 12);
    headerLayout->setSpacing(2);

    m_peerNameLabel = new QLabel("Select a peer to start chatting", m_chatHeaderWidget);
    m_peerNameLabel->setObjectName("peerNameLabel");
    headerLayout->addWidget(m_peerNameLabel);

    m_peerStatusLabel = new QLabel("", m_chatHeaderWidget);
    m_peerStatusLabel->setObjectName("peerStatusLabel");
    headerLayout->addWidget(m_peerStatusLabel);

    rightLayout->addWidget(m_chatHeaderWidget);

    // Message viewer
    m_chatBrowser = new QTextBrowser(m_rightPanel);
    m_chatBrowser->setObjectName("chatBrowser");
    m_chatBrowser->setOpenExternalLinks(true);
    rightLayout->addWidget(m_chatBrowser);

    // Bottom input panel
    QWidget *inputWidget = new QWidget(m_rightPanel);
    inputWidget->setObjectName("inputWidget");
    QHBoxLayout *inputLayout = new QHBoxLayout(inputWidget);
    inputLayout->setContentsMargins(12, 12, 12, 12);
    inputLayout->setSpacing(8);

    m_messageInput = new QLineEdit(inputWidget);
    m_messageInput->setObjectName("messageInput");
    m_messageInput->setPlaceholderText("Write a message...");
    m_messageInput->setEnabled(false);
    connect(m_messageInput, &QLineEdit::returnPressed, this, &MainWindow::onSendClicked);
    inputLayout->addWidget(m_messageInput);

    m_sendButton = new QPushButton("Send", inputWidget);
    m_sendButton->setObjectName("sendButton");
    m_sendButton->setEnabled(false);
    m_sendButton->setCursor(Qt::PointingHandCursor);
    connect(m_sendButton, &QPushButton::clicked, this, &MainWindow::onSendClicked);
    inputLayout->addWidget(m_sendButton);

    rightLayout->addWidget(inputWidget);

    // Assemble Splitter
    m_mainSplitter->addWidget(m_leftPanel);
    m_mainSplitter->addWidget(m_rightPanel);
    m_mainSplitter->setSizes({260, 540});

    setMinimumSize(800, 600);
    setWindowTitle("LANChat - P2P WiFi Chat");
}

void MainWindow::applyStyleSheet() {
    QString qss = R"(
        /* Global Stylesheet */
        QWidget {
            font-family: 'Segoe UI', Arial, sans-serif;
            font-size: 13px;
            color: #e4ecf2;
            background-color: #0e1621;
        }

        /* Sidebar Styling */
        #sidebarWidget {
            background-color: #17212b;
            border-right: 1px solid #101921;
        }

        #profileWidget {
            background-color: #17212b;
            border-bottom: 1px solid #101921;
        }

        #myProfileLabel {
            font-weight: bold;
            font-size: 14px;
            color: #ffffff;
        }

        /* User List Widget */
        QListWidget {
            background-color: #17212b;
            border: none;
            outline: none;
        }

        QListWidget::item {
            padding: 10px 14px;
            border-bottom: 1px solid #101921;
            color: #e4ecf2;
        }

        QListWidget::item:hover {
            background-color: #202b36;
        }

        QListWidget::item:selected {
            background-color: #2b5278;
            color: #ffffff;
        }

        /* Right Content Panel Styling */
        #chatAreaWidget {
            background-color: #0e1621;
        }

        #headerWidget {
            background-color: #17212b;
            border-bottom: 1px solid #101921;
        }

        #peerNameLabel {
            font-weight: bold;
            font-size: 15px;
            color: #ffffff;
        }

        #peerStatusLabel {
            font-size: 11px;
        }

        /* Chat Message Logs */
        QTextBrowser {
            background-color: #0e1621;
            border: none;
            outline: none;
        }

        /* Input Area Footer */
        #inputWidget {
            background-color: #17212b;
            border-top: 1px solid #101921;
        }

        QLineEdit#messageInput {
            background-color: #24303f;
            border: 1px solid #2b394a;
            border-radius: 6px;
            padding: 8px 12px;
            color: #ffffff;
        }

        QLineEdit#messageInput:focus {
            border: 1px solid #5288c1;
        }

        QLineEdit#messageInput:disabled {
            background-color: #1c2530;
            color: #5d6b79;
        }

        /* Buttons Styling */
        QPushButton {
            background-color: #2b5278;
            border: none;
            border-radius: 4px;
            padding: 6px 12px;
            color: #ffffff;
            font-weight: bold;
        }

        QPushButton:hover {
            background-color: #3b6a9b;
        }

        QPushButton:pressed {
            background-color: #1e3d5a;
        }

        QPushButton:disabled {
            background-color: #202b36;
            color: #5d6b79;
        }

        /* Dynamic toggle for visibility button */
        QPushButton#visibilityBtn[activeState="visible"] {
            background-color: #4bb85c;
        }
        QPushButton#visibilityBtn[activeState="visible"]:hover {
            background-color: #5cc96d;
        }
        QPushButton#visibilityBtn[activeState="invisible"] {
            background-color: #4f5d73;
        }
        QPushButton#visibilityBtn[activeState="invisible"]:hover {
            background-color: #5d6d85;
        }

        /* ScrollBar Styling */
        QScrollBar:vertical {
            border: none;
            background: #0e1621;
            width: 8px;
            margin: 0px;
        }
        QScrollBar::handle:vertical {
            background: #2b394a;
            min-height: 20px;
            border-radius: 4px;
        }
        QScrollBar::handle:vertical:hover {
            background: #3b4e64;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
    )";

    setStyleSheet(qss);
}

void MainWindow::onSendClicked() {
    QString text = m_messageInput->text().trimmed();
    if (text.isEmpty() || m_selectedPeer.isEmpty()) {
        return;
    }

    m_messageInput->clear();

    // Check if peer is still online to send the message
    QMap<QString, PeerInfo> peers = m_peerManager->activePeers();
    if (!peers.contains(m_selectedPeer)) {
        appendSystemMessage(QString("Cannot send message: %1 is offline.").arg(m_selectedPeer));
        return;
    }

    PeerInfo info = peers[m_selectedPeer];

    // Trigger TCP send
    m_chatManager->sendMessage(m_selectedPeer, info.ip, info.tcpPort, text);

    // Save locally
    QDateTime now = QDateTime::currentDateTime();
    DatabaseManager::appendMessage(m_myUsername, m_myUsername, m_selectedPeer, text, now);

    // Cache locally
    MessageRecord rec;
    rec.timestamp = now;
    rec.sender = m_myUsername;
    rec.receiver = m_selectedPeer;
    rec.message = text;
    m_chatHistory.append(rec);

    // Render directly in chat window
    appendMessageToView(m_myUsername, text, now, true);
}

void MainWindow::onUserSelectionChanged() {
    QListWidgetItem *item = m_userListWidget->currentItem();
    if (!item) {
        return;
    }

    QString peerUsername = item->data(Qt::UserRole).toString();
    m_selectedPeer = peerUsername;

    // Clear unread badge
    m_unreadMessages.remove(peerUsername);
    updatePeerList(); // Redraw UI list

    // Display loaded logs
    displayChatHistory(peerUsername);
}

void MainWindow::onVisibilityToggled() {
    bool isVisible = m_visibilityBtn->isChecked();
    m_peerManager->setVisibility(isVisible);

    m_visibilityBtn->setText(isVisible ? "Visible" : "Invisible");
    m_visibilityBtn->setProperty("activeState", isVisible ? "visible" : "invisible");
    
    // Refresh QSS styling rules on the button
    m_visibilityBtn->style()->unpolish(m_visibilityBtn);
    m_visibilityBtn->style()->polish(m_visibilityBtn);
}

void MainWindow::onChangeUsernameClicked() {
    bool ok;
    QString newName = QInputDialog::getText(this, "Set Username", "Enter your username:", 
                                            QLineEdit::Normal, m_myUsername, &ok);
    newName = newName.trimmed();
    if (ok && !newName.isEmpty() && newName != m_myUsername) {
        m_myUsername = newName;
        m_myProfileLabel->setText(QString("%1\n[Port: %2]").arg(m_myUsername).arg(m_myTcpPort));

        // Save preference
        QSettings settings("LANChat", "Preferences");
        settings.setValue("username", m_myUsername);

        // Update core modules
        m_peerManager->setUsername(m_myUsername);
        m_chatManager->setLocalUsername(m_myUsername);

        // Reload history for the new username
        m_chatHistory = DatabaseManager::loadAllMessages(m_myUsername);

        // Refresh current chat view
        if (!m_selectedPeer.isEmpty()) {
            displayChatHistory(m_selectedPeer);
        }

        appendSystemMessage(QString("You changed your name to: %1").arg(m_myUsername));
    }
}

void MainWindow::onPeerDiscovered(const QString& username, const QHostAddress& ip, quint16 tcpPort) {
    Q_UNUSED(ip);
    Q_UNUSED(tcpPort);
    
    if (username == m_selectedPeer) {
        appendSystemMessage(QString("%1 is back online.").arg(username));
    }
}

void MainWindow::onPeerRemoved(const QString& username) {
    if (username == m_selectedPeer) {
        appendSystemMessage(QString("%1 went offline.").arg(username));
    }
}

void MainWindow::onPeerListUpdated() {
    updatePeerList();
}

void MainWindow::onMessageReceived(const QString& senderUsername, const QString& messageText, const QDateTime& timestamp) {
    // Append to local storage logs
    DatabaseManager::appendMessage(m_myUsername, senderUsername, m_myUsername, messageText, timestamp);

    // Cache locally
    MessageRecord rec;
    rec.timestamp = timestamp;
    rec.sender = senderUsername;
    rec.receiver = m_myUsername;
    rec.message = messageText;
    m_chatHistory.append(rec);

    if (senderUsername == m_selectedPeer) {
        // Display immediately in active chat
        appendMessageToView(senderUsername, messageText, timestamp, false);
    } else {
        // Increment unread and reload sidebar
        m_unreadMessages[senderUsername]++;
        updatePeerList();
    }
}

void MainWindow::onPeerConnected(const QString& peerUsername) {
    if (peerUsername == m_selectedPeer) {
        appendSystemMessage(QString("TCP connection established with %1.").arg(peerUsername));
    }
}

void MainWindow::onPeerDisconnected(const QString& peerUsername) {
    if (peerUsername == m_selectedPeer) {
        appendSystemMessage(QString("TCP connection with %1 disconnected.").arg(peerUsername));
    }
}

void MainWindow::onConnectionError(const QString& peerUsername, const QString& errorString) {
    if (peerUsername == m_selectedPeer) {
        appendSystemMessage(QString("Connection error with %1: %2").arg(peerUsername).arg(errorString));
    }
}

void MainWindow::updatePeerList() {
    m_userListWidget->blockSignals(true);
    m_userListWidget->clear();
    QMap<QString, PeerInfo> peers = m_peerManager->activePeers();

    for (auto it = peers.constBegin(); it != peers.constEnd(); ++it) {
        QString peerName = it.key();
        PeerInfo info = it.value();

        QListWidgetItem *item = new QListWidgetItem();
        item->setData(Qt::UserRole, peerName);

        int unreads = m_unreadMessages.value(peerName, 0);
        QString displayLabel = peerName;
        if (unreads > 0) {
            displayLabel += QString("  [%1]").arg(unreads);
        }

        // Green dot indicator showing online status
        item->setText(QString("● %1\n  %2:%3").arg(displayLabel).arg(info.ip.toString()).arg(info.tcpPort));

        if (unreads > 0) {
            QFont f = item->font();
            f.setBold(true);
            item->setFont(f);
            item->setForeground(QBrush(QColor("#5288c1"))); // Light Blue highlight for unread
        } else {
            item->setForeground(QBrush(QColor("#4bb85c"))); // Green online text representation
        }

        m_userListWidget->addItem(item);

        if (peerName == m_selectedPeer) {
            m_userListWidget->setCurrentItem(item);
        }
    }

    // Refresh selected peer header status
    if (!m_selectedPeer.isEmpty()) {
        if (peers.contains(m_selectedPeer)) {
            m_peerNameLabel->setText(m_selectedPeer);
            m_peerStatusLabel->setText(QString("Online - %1:%2").arg(peers[m_selectedPeer].ip.toString()).arg(peers[m_selectedPeer].tcpPort));
            m_peerStatusLabel->setStyleSheet("font-size: 11px; color: #4bb85c;"); // Green status
            
            m_messageInput->setEnabled(true);
            m_sendButton->setEnabled(true);
        } else {
            m_peerStatusLabel->setText("Offline");
            m_peerStatusLabel->setStyleSheet("font-size: 11px; color: #7f91a4;"); // Gray status
            
            m_messageInput->setEnabled(false);
            m_sendButton->setEnabled(false);
        }
    } else {
        m_peerNameLabel->setText("Select a peer to start chatting");
        m_peerStatusLabel->setText("");
        m_messageInput->setEnabled(false);
        m_sendButton->setEnabled(false);
    }
    m_userListWidget->blockSignals(false);
}

void MainWindow::displayChatHistory(const QString& peerUsername) {
    m_chatBrowser->clear();

    for (const MessageRecord& record : m_chatHistory) {
        if (record.sender == m_myUsername && record.receiver == peerUsername) {
            appendMessageToView(m_myUsername, record.message, record.timestamp, true);
        } else if (record.sender == peerUsername && record.receiver == m_myUsername) {
            appendMessageToView(peerUsername, record.message, record.timestamp, false);
        }
    }
}

void MainWindow::appendMessageToView(const QString& sender, const QString& message, 
                                     const QDateTime& timestamp, bool isOutgoing) {
                                     
    QString escapedMsg = message.toHtmlEscaped();
    escapedMsg.replace("\n", "<br/>");
    QString timeStr = timestamp.toString("hh:mm AP");

    QString html;
    if (isOutgoing) {
        html = QString(
            "<table width='100%' border='0' cellpadding='0' cellspacing='0' style='margin: 4px 0;'>"
            "  <tr>"
            "    <td align='right'>"
            "      <div style='background-color: #2b5278; color: #ffffff; padding: 8px 12px; border-radius: 12px 12px 0px 12px; max-width: 70%; text-align: left;'>"
            "        <span style='font-size: 13px; font-family: sans-serif;'>%1</span>"
            "        <br/>"
            "        <span style='font-size: 9px; color: #a5c3e0; float: right; margin-top: 4px; margin-left: 10px;'>%2</span>"
            "      </div>"
            "    </td>"
            "  </tr>"
            "</table>"
        ).arg(escapedMsg).arg(timeStr);
    } else {
        html = QString(
            "<table width='100%' border='0' cellpadding='0' cellspacing='0' style='margin: 4px 0;'>"
            "  <tr>"
            "    <td align='left'>"
            "      <div style='background-color: #182533; color: #ffffff; padding: 8px 12px; border-radius: 12px 12px 12px 0px; max-width: 70%; text-align: left;'>"
            "        <b style='color: #5288c1; font-size: 11px;'>%1</b><br/>"
            "        <span style='font-size: 13px; font-family: sans-serif;'>%2</span>"
            "        <br/>"
            "        <span style='font-size: 9px; color: #7f91a4; float: right; margin-top: 4px; margin-left: 10px;'>%3</span>"
            "      </div>"
            "    </td>"
            "  </tr>"
            "</table>"
        ).arg(sender.toHtmlEscaped()).arg(escapedMsg).arg(timeStr);
    }

    m_chatBrowser->append(html);
    
    // Automatically scroll to the latest message
    m_chatBrowser->verticalScrollBar()->setValue(m_chatBrowser->verticalScrollBar()->maximum());
}

void MainWindow::appendSystemMessage(const QString& text) {
    QString html = QString(
        "<div style='margin: 8px; text-align: center;'>"
        "  <div style='display: inline-block; background-color: #1c2836; color: #7f91a4; padding: 4px 10px; border-radius: 10px; font-size: 11px;'>"
        "    <i>%1</i>"
        "  </div>"
        "</div>"
    ).arg(text.toHtmlEscaped());

    m_chatBrowser->append(html);
    m_chatBrowser->verticalScrollBar()->setValue(m_chatBrowser->verticalScrollBar()->maximum());
}
