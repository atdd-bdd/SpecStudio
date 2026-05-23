#include "ProjectFile.h"
#include <QFileInfo>

ProjectFile::ProjectFile(const QString& absolutePath, const QString& relativePath)
    : m_absolutePath(absolutePath)
    , m_relativePath(relativePath)
    , m_type(fileTypeFromPath(absolutePath))
{}

QString ProjectFile::fileName() const
{
    return QFileInfo(m_absolutePath).fileName();
}
