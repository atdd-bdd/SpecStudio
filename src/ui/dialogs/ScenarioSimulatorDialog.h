#pragma once

#include <QDialog>
#include <QSet>
#include <QStringList>
#include <QVector>

class QFileSystemWatcher;
class QPushButton;
class QTextBrowser;

class ScenarioSimulatorDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ScenarioSimulatorDialog(const QString& filePath, int cursorLine,
                                     QWidget* parent = nullptr);

public slots:
    void refresh();

private:
    struct DisplayStep {
        QString keyword, text, attrSetName, defineRef;
        QVector<QStringList> tableRows;
        bool vertical = false;
        bool hasHeader  = false;
    };
    struct DisplayDefine {
        QString name, scalarValue;
        QVector<QStringList> tableRows;
        bool isTable    = false;
        bool vertical = false;
    };
    struct ParsedScenario {
        QString keyword;   // "Scenario" or "ScenarioGroup"
        QString name;
        int     startLine = 0;
        QVector<DisplayStep> steps;
    };
    struct ParsedFile {
        QVector<DisplayStep>    backgroundSteps;
        QVector<DisplayStep>    cleanupSteps;
        QVector<ParsedScenario> scenarios;
        QVector<DisplayDefine>  defines;
    };

    static ParsedFile parseFile(const QString& filePath);
    static DisplayDefine* findDefine(const QString& name, QVector<DisplayDefine>& defines);

    QString buildHtml(const ParsedFile& pf, int cursorLine) const;
    QString renderSection(const QString& title, const QColor& titleColor,
                          const QVector<DisplayStep>& steps,
                          QVector<DisplayDefine>& defines) const;
    QString renderSteps(const QVector<DisplayStep>& steps,
                        QVector<DisplayDefine>& defines) const;
    // `resolving` guards against a Define whose table cells (directly or
    // transitively) reference back to itself.
    QString renderTable(const QVector<QStringList>& rows,
                        bool vertical, bool hasHeader,
                        QVector<DisplayDefine>& defines,
                        const QSet<QString>& resolving = {}) const;
    QString resolveDefine(const QString& defName, QVector<DisplayDefine>& defines,
                         const QSet<QString>& resolving = {}) const;

    QString m_filePath;
    int     m_cursorLine;

    QTextBrowser*        m_browser = nullptr;
    QPushButton*         m_refresh = nullptr;
    QFileSystemWatcher*  m_watcher = nullptr;
};
