#include "BuildController.h"

#include <QProcess>

BuildController::BuildController(QObject* parent)
    : QObject(parent)
{}

void BuildController::run(const QString& program, const QStringList& args)
{
    if (program.isEmpty()) {
        emit errorOccurred(tr("No converter configured for this file type."));
        emit buildFinished(false);
        return;
    }

    auto* proc = new QProcess(this);
    proc->setProgram(program);
    proc->setArguments(args);

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
