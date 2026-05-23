#pragma once

#include <QObject>

class Project;

class BuildController : public QObject
{
    Q_OBJECT

public:
    explicit BuildController(QObject* parent = nullptr);

    void buildFile(const QString& absolutePath, const QString& translatorProgram);
    void buildProject(Project* project,          const QString& translatorProgram);

signals:
    void outputReady(const QString& text);
    void errorOccurred(const QString& text);
    void buildFinished(bool success);
};
