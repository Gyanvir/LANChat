#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QString>
#include <QDateTime>
#include <QList>
#include <QFile>
#include <QTextStream>
#include <QDebug>

struct MessageRecord {
    QDateTime timestamp;
    QString sender;
    QString receiver;
    QString message;
};

class DatabaseManager {
public:
    // Append a new message to the local user-specific chat history file
    static void appendMessage(const QString& myUsername, const QString& sender, const QString& receiver, 
                              const QString& message, const QDateTime& timestamp = QDateTime::currentDateTime());

    // Load all historical messages from the user-specific chat history file
    static QList<MessageRecord> loadAllMessages(const QString& myUsername);

private:
    static QString getFilePath(const QString& myUsername);

    // Helper functions to escape/unescape multi-line messages to/from a single line
    static QString escapeMessage(const QString& msg);
    static QString unescapeMessage(const QString& msg);
};

#endif // DATABASEMANAGER_H
