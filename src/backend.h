#pragma once

#include <QNetworkAccessManager>
#include <QObject>

#include "archive.h"
#include "queue.h"

// What the window talks to: the show being browsed, the download queue,
// and where MP3s are saved.
class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(Archive *archive READ archive CONSTANT)
    Q_PROPERTY(Queue *queue READ queue CONSTANT)
    Q_PROPERTY(QString directory READ directory WRITE setDirectory NOTIFY directoryChanged)
    Q_PROPERTY(QString displayDirectory READ displayDirectory NOTIFY directoryChanged)
    Q_PROPERTY(QStringList missingTools READ missingTools NOTIFY toolsChanged)
    Q_PROPERTY(QString message READ message NOTIFY messageChanged)

public:
    // An empty directory means the saved setting, or ~/Music/NTS.
    explicit Backend(const QString &directory = {}, const Tools &tools = Tools::find(), QObject *parent = nullptr);

    Archive *archive() { return &m_archive; }
    Queue *queue() { return &m_queue; }
    QString directory() const { return m_directory; }
    void setDirectory(const QString &directory);
    QString displayDirectory() const;
    QStringList missingTools() const { return m_tools.missing(); }
    QString message() const { return m_message; }

    static QString defaultDirectory();

    // Opens a show's archive, and queues the episode too when the link is
    // one. Returns false, with a message, when the text isn't an NTS link.
    Q_INVOKABLE bool open(const QString &text);
    Q_INVOKABLE void openFolder() const;
    Q_INVOKABLE void checkTools();
    Q_INVOKABLE void clearMessage();

signals:
    void directoryChanged();
    void toolsChanged();
    void messageChanged();

private:
    void setMessage(const QString &message);
    QString status(const Nts::Episode &episode) const;

    QNetworkAccessManager m_network;
    Tools m_tools;
    QString m_directory;
    Queue m_queue;
    Archive m_archive;
    QString m_message;
    bool m_persist;
};
