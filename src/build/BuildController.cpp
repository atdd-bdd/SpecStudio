#include "BuildController.h"

#include <QProcess>

BuildController::BuildController(QObject* parent)
    : QObject(parent)
{}

void BuildController::run(const QString& program, const QStringList& args)
{
    if (program.isEmpty()) {
        emit outputReady(tr("ERROR: No converter found. Set the converter path in the .specconfig file,\n"
                            "or place SpecTableConverter.exe next to AlignThree.exe.\n"));
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
    connect(proc, &QProcess::errorOccurred, this, [this, proc](QProcess::ProcessError err) {
        QString msg;
        switch (err) {
        case QProcess::FailedToStart:
            msg = tr("ERROR: Failed to start converter: %1\n"
                     "Check that the file exists and is executable.").arg(proc->program());
            break;
        case QProcess::Crashed:  msg = tr("ERROR: Converter process crashed.");   break;
        case QProcess::Timedout: msg = tr("ERROR: Converter process timed out."); break;
        default:                 msg = tr("ERROR: Process error (%1).").arg(static_cast<int>(err)); break;
        }
        emit outputReady(msg + "\n");
        // FailedToStart never emits finished(), so we must signal completion here
        if (err == QProcess::FailedToStart) {
            proc->deleteLater();
            emit buildFinished(false);
        }
    });
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc](int exitCode, QProcess::ExitStatus) {
                // Drain any output buffered after the last readyRead signal
                const QByteArray outRem = proc->readAllStandardOutput();
                const QByteArray errRem = proc->readAllStandardError();
                if (!outRem.isEmpty()) emit outputReady(QString::fromUtf8(outRem));
                if (!errRem.isEmpty()) emit outputReady(QString::fromUtf8(errRem));
                emit buildFinished(exitCode == 0);
                proc->deleteLater();
            });

    proc->start();
}
