#pragma once

#include <QDialog>
#include <QVector>
#include <QStringList>

class QTextBrowser;
class QPushButton;
class QFileSystemWatcher;

class BackgroundCleanupDialog : public QDialog
{
    Q_OBJECT
public:
    enum class Mode { Background, Cleanup };

    explicit BackgroundCleanupDialog(const QString& filePath, Mode mode, QWidget* parent = nullptr);

public slots:
    void refresh();

private:
    // Lightweight parsed step for display
    struct DisplayStep {
        QString              keyword;
        QString              text;
        QString              attrSetName;
        QString              defineRef;
        QVector<QStringList> tableRows;
        bool                 transposed  = false;
        bool                 hasHeader   = false;
    };

    struct DisplayDefine {
        QString              name;
        QString              scalarValue;
        QVector<QStringList> tableRows;
        bool                 isTable    = false;
        bool                 transposed = false;
    };

    struct ParsedFile {
        QVector<DisplayStep>   backgroundSteps;
        QVector<DisplayStep>   cleanupSteps;
        QVector<DisplayDefine> defines;
    };

    static ParsedFile parseFile(const QString& filePath);
    static DisplayDefine* findDefine(const QString& name, QVector<DisplayDefine>& defines);

    QString buildHtml(const QVector<DisplayStep>& steps,
                      QVector<DisplayDefine>& defines) const;

    QString renderTable(const QVector<QStringList>& rows, bool transposed,
                        bool hasHeader) const;
    QString resolveDefine(const QString& defName,
                          QVector<DisplayDefine>& defines) const;

    QString  m_filePath;
    Mode     m_mode;
    QTextBrowser*      m_browser  = nullptr;
    QPushButton*       m_refresh  = nullptr;
    QFileSystemWatcher* m_watcher = nullptr;
};
