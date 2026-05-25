#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidget>
#include <QTextBrowser>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QSplitter>
#include "PeerManager.h"
#include "ChatManager.h"
#include "DatabaseManager.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

private slots:
    // UI Event Slots
    void onSendClicked();
    void onUserSelectionChanged();
    void onVisibilityToggled();
    void onChangeUsernameClicked();

    // Network Event Slots
    void onPeerDiscovered(const QString& username, const QHostAddress& ip, quint16 tcpPort);
    void onPeerRemoved(const QString& username);
    void onPeerListUpdated();
    
    void onMessageReceived(const QString& senderUsername, const QString& messageText, const QDateTime& timestamp);
    void onPeerConnected(const QString& peerUsername);
    void onPeerDisconnected(const QString& peerUsername);
    void onConnectionError(const QString& peerUsername, const QString& errorString);

private:
    // Setup and Styling
    void setupUi();
    void applyStyleSheet();
    void loadSettingsAndInitialize();

    // UI Updates
    void updatePeerList();
    void displayChatHistory(const QString& peerUsername);
    void appendMessageToView(const QString& sender, const QString& message, 
                             const QDateTime& timestamp, bool isOutgoing);
    void appendSystemMessage(const QString& text);

    // Business Logic State
    PeerManager *m_peerManager;
    ChatManager *m_chatManager;

    QString m_myUsername;
    quint16 m_myTcpPort;

    QString m_selectedPeer; // Currently active chat peer
    QList<MessageRecord> m_chatHistory;

    // Track unread message indicators. Key: sender username, Value: unread count
    QMap<QString, int> m_unreadMessages;

    // UI Widgets
    QWidget *m_leftPanel;
    QWidget *m_rightPanel;
    QSplitter *m_mainSplitter;

    // Left Panel Widgets
    QLabel *m_myProfileLabel;
    QPushButton *m_visibilityBtn;
    QPushButton *m_changeNameBtn;
    QListWidget *m_userListWidget;

    // Right Panel Widgets
    QWidget *m_chatHeaderWidget;
    QLabel *m_peerNameLabel;
    QLabel *m_peerStatusLabel;
    QTextBrowser *m_chatBrowser;
    QLineEdit *m_messageInput;
    QPushButton *m_sendButton;
};

#endif // MAINWINDOW_H
