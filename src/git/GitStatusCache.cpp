#include "GitStatusCache.h"
#include "GitClient.h"

GitStatusCache::GitStatusCache(GitClient* client, QObject* parent)
    : QObject(parent)
    , m_client(client)
{
    connect(&m_timer, &QTimer::timeout, this, &GitStatusCache::refresh);
    m_timer.start(30000);
    refresh();
}

void GitStatusCache::refresh()
{
    QStringList lines = m_client->status();
    if (lines != m_cached) {
        m_cached = lines;
        emit statusChanged(m_cached);
    }
}
