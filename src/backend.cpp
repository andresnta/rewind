#include "backend.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>

Backend::Backend(const QString &directory, const Tools &tools, QObject *parent)
    : QObject(parent),
      m_tools(tools),
      m_queue(&m_network, tools),
      m_archive(&m_network, [this](const Nts::Episode &e) { return status(e); }),
      m_persist(directory.isEmpty())
{
    QString saved = directory;
    if (saved.isEmpty())
        saved = QSettings().value("directory").toString();
    m_directory = saved.isEmpty() ? defaultDirectory() : saved;
    m_queue.setDirectory(m_directory);

    connect(&m_queue, &Queue::statusesChanged, &m_archive, &Archive::refreshStatuses);
}

QString Backend::defaultDirectory()
{
    QString music = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
    if (music.isEmpty() || music == QDir::homePath())
        music = QDir::homePath() + "/Music";
    return music + "/NTS";
}

void Backend::setDirectory(const QString &value)
{
    QString directory = value.trimmed();
    if (directory.startsWith("~/") || directory == "~")
        directory = QDir::homePath() + directory.mid(1);
    directory = QDir::cleanPath(directory);
    if (directory.isEmpty() || directory == m_directory)
        return;
    if (!QDir::isAbsolutePath(directory)) {
        setMessage(tr("The folder needs a full path, like ~/Music/NTS."));
        return;
    }
    m_directory = directory;
    m_queue.setDirectory(directory);
    if (m_persist)
        QSettings().setValue("directory", directory);
    emit directoryChanged();
    m_archive.refreshStatuses();
}

QString Backend::displayDirectory() const
{
    const QString home = QDir::homePath();
    return m_directory.startsWith(home + "/") ? "~" + m_directory.mid(home.size()) : m_directory;
}

QString Backend::status(const Nts::Episode &episode) const
{
    const QString queued = m_queue.statusFor(episode.show, episode.alias);
    if (!queued.isEmpty() && queued != "failed")
        return queued;
    if (QFileInfo::exists(Nts::targetPath(m_directory, episode)))
        return "done";
    return queued;
}

bool Backend::open(const QString &text)
{
    const Nts::Link link = Nts::parseLink(text);
    if (!link.isValid()) {
        setMessage(tr("That isn't an NTS show or episode link."));
        return false;
    }
    clearMessage();
    if (link.show != m_archive.showAlias() || !m_archive.error().isEmpty())
        m_archive.load(link.show);
    if (link.isEpisode()) {
        Nts::Episode episode;
        episode.show = link.show;
        episode.alias = link.episode;
        if (!m_queue.add(episode))
            setMessage(tr("That episode is already in the queue."));
    }
    return true;
}

void Backend::openFolder() const
{
    QDir().mkpath(m_directory);
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_directory));
}

void Backend::checkTools()
{
    const Tools found = Tools::find();
    if (found.missing() == m_tools.missing())
        return;
    m_tools = found;
    m_queue.setTools(found);
    emit toolsChanged();
}

void Backend::setMessage(const QString &message)
{
    if (message == m_message)
        return;
    m_message = message;
    emit messageChanged();
}

void Backend::clearMessage()
{
    setMessage({});
}
