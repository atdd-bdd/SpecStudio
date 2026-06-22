#pragma once

#include <QObject>
#include <QStringList>

class Project;

class BuildController : public QObject
{
    Q_OBJECT

public:
    explicit BuildController(QObject* parent = nullptr);

    // Run an external tool with the given arguments.
    // Emits outputReady for each chunk of stdout/stderr,
    // then buildFinished(true/false) when the process exits.
    void run(const QString& program, const QStringList& args);

signals:
    void outputReady(const QString& text);
    void errorOccurred(const QString& text);
    void buildFinished(bool success);
};
