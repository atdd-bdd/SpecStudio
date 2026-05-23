#pragma once

#include "FileType.h"
#include <QString>

class ProjectFile
{
public:
    ProjectFile(const QString& absolutePath, const QString& relativePath);

    QString  absolutePath() const { return m_absolutePath; }
    QString  relativePath() const { return m_relativePath; }
    FileType type()         const { return m_type; }
    QString  fileName()     const;

private:
    QString  m_absolutePath;
    QString  m_relativePath;
    FileType m_type;
};
