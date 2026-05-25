#include "DatabaseManager.h"
#include <QRegularExpression>
#include <QFileInfo>

QString DatabaseManager::getFilePath(const QString& myUsername) {
    // Sanitize username to prevent invalid characters in file names on Windows
    QString sanitized = myUsername;
    sanitized.remove(QRegularExpression(R"([\\/:*?"<>|])"));
    if (sanitized.isEmpty()) {
        sanitized = "unknown";
    }
    return QString("chat_history_%1.txt").arg(sanitized);
}

void DatabaseManager::appendMessage(const QString& myUsername, const QString& sender, const QString& receiver, 
                                    const QString& message, const QDateTime& timestamp) {
    QString filePath = getFilePath(myUsername);
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        qWarning() << "DatabaseManager: Failed to open" << filePath << "for writing.";
        return;
    }

    QTextStream out(&file);
    out.setCodec("UTF-8");

    QString timeStr = timestamp.toString("yyyy-MM-dd hh:mm:ss");
    QString escapedMsg = escapeMessage(message);

    // Format: [Timestamp] [Sender] -> [Receiver]: MessageText
    out << "[" << timeStr << "] [" << sender << "] -> [" << receiver << "]: " << escapedMsg << "\n";
    file.close();
}

QList<MessageRecord> DatabaseManager::loadAllMessages(const QString& myUsername) {
    QList<MessageRecord> records;
    QString filePath = getFilePath(myUsername);
    QFile file(filePath);
    if (!file.exists()) {
        return records; // Return empty list if file doesn't exist yet
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "DatabaseManager: Failed to open" << filePath << "for reading.";
        return records;
    }

    QTextStream in(&file);
    in.setCodec("UTF-8");

    // Regular expression to parse: [Timestamp] [Sender] -> [Receiver]: Message
    QRegularExpression regex(R"(^\[(.*?)\] \[(.*?)\] -> \[(.*?)\]: (.*)$)");

    while (!in.atEnd()) {
        QString line = in.readLine();
        QRegularExpressionMatch match = regex.match(line);
        if (match.hasMatch()) {
            MessageRecord record;
            QString tsStr = match.captured(1);
            record.timestamp = QDateTime::fromString(tsStr, "yyyy-MM-dd hh:mm:ss");
            record.sender = match.captured(2);
            record.receiver = match.captured(3);
            record.message = unescapeMessage(match.captured(4));
            records.append(record);
        }
    }

    file.close();
    return records;
}

QString DatabaseManager::escapeMessage(const QString& msg) {
    QString escaped = msg;
    // Replace backslashes first, then escape newlines and carriage returns
    escaped.replace("\\", "\\\\");
    escaped.replace("\n", "\\n");
    escaped.replace("\r", "\\r");
    return escaped;
}

QString DatabaseManager::unescapeMessage(const QString& msg) {
    QString unescaped = msg;
    // Unescape newlines, carriage returns, and then restore backslashes
    unescaped.replace("\\n", "\n");
    unescaped.replace("\\r", "\r");
    unescaped.replace("\\\\", "\\");
    return unescaped;
}
