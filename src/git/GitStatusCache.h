#pragma once

#include <QObject>
#include <QMap>
#include <QTimer>

class GitClient;

class GitStatusCache : public QObject
{
    Q_OBJECT

public:
    explicit GitStatusCache(GitClient* client, QObject* parent = nullptr);

    QStringList cachedStatus() const { return m_cached; }
    bool hasChanges() const { return !m_cached.isEmpty(); }

signals:
    void statusChanged(const QStringList& lines);

private:
    void refresh();

    GitClient*  m_client = nullptr;
    QTimer      m_timer;
    QStringList m_cached;
};
