#include "BuildController.h"
#include "../model/Project.h"

#include <QProcess>

BuildController::BuildController(QObject* parent)
    : QObject(parent)
{}

void BuildController::buildFile(const QString& absolutePath,
                                 const QString& translatorProgram)
{
    if (translatorProgram.isEmpty()) {
        emit errorOccurred(tr("No translator configured. Set it in Build settings."));
        emit buildFinished(false);
        return;
    }

    auto* proc = new QProcess(this);
    proc->setProgram(translatorProgram);
    proc->setArguments({absolutePath});

    connect(proc, &QProcess::readyReadStandardOutput, this, [this, proc] {
        emit outputReady(QString::fromUtf8(proc->readAllStandardOutput()));
    });
    connect(proc, &QProcess::readyReadStandardError, this, [this, proc] {
        emit outputReady(QString::fromUtf8(proc->readAllStandardError()));
    });
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc](int exitCode, QProcess::ExitStatus) {
                emit buildFinished(exitCode == 0);
                proc->deleteLater();
            });

    proc->start();
}

void BuildController::buildProject(Project* project,
                                    const QString& translatorProgram)
{
    if (translatorProgram.isEmpty()) {
        emit errorOccurred(tr("No translator configured. Set it in Build settings."));
        emit buildFinished(false);
        return;
    }

    auto* proc = new QProcess(this);
    proc->setProgram(translatorProgram);
    proc->setArguments({project->rootPath()});

    connect(proc, &QProcess::readyReadStandardOutput, this, [this, proc] {
        emit outputReady(QString::fromUtf8(proc->readAllStandardOutput()));
    });
    connect(proc, &QProcess::readyReadStandardError, this, [this, proc] {
        emit outputReady(QString::fromUtf8(proc->readAllStandardError()));
    });
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc](int exitCode, QProcess::ExitStatus) {
                emit buildFinished(exitCode == 0);
                proc->deleteLater();
            });

    proc->start();
}
