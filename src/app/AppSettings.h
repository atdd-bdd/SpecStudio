#pragma once

#include <QSettings>
#include <QString>

enum class StepScope { File, Folder, Project };

class AppSettings
{
public:
    AppSettings();

    // Editor associations: empty string = built-in editor
    QString editorForExtension(const QString& ext) const;
    void    setEditorForExtension(const QString& ext, const QString& program);

    // Git settings (per-project, keyed by project root path)
    QString gitRemoteUrl(const QString& projectRoot) const;
    void    setGitRemoteUrl(const QString& projectRoot, const QString& url);

    QString gitBranch(const QString& projectRoot) const;
    void    setGitBranch(const QString& projectRoot, const QString& branch);

    QString gitUser(const QString& projectRoot) const;
    void    setGitUser(const QString& projectRoot, const QString& user);

    QString gitPassword(const QString& projectRoot) const;
    void    setGitPassword(const QString& projectRoot, const QString& password);

    // FeatureX options (per-project)
    bool       implicitFolderImport(const QString& projectRoot) const;
    void       setImplicitFolderImport(const QString& projectRoot, bool value);

    bool       uniqueScenarioNames(const QString& projectRoot) const;
    void       setUniqueScenarioNames(const QString& projectRoot, bool value);

    bool       uniqueStepNames(const QString& projectRoot) const;
    void       setUniqueStepNames(const QString& projectRoot, bool value);

    StepScope  stepSuggestionScope(const QString& projectRoot) const;
    void       setStepSuggestionScope(const QString& projectRoot, StepScope scope);

    // Window state
    QByteArray mainWindowGeometry() const;
    QByteArray mainWindowState()    const;
    void       saveMainWindowState(const QByteArray& geometry, const QByteArray& state);

    // Recent solutions (ordered list)
    QStringList recentSolutions() const;
    void        addRecentSolution(const QString& path);

    // The full list of file extensions SpecStudio recognises (used in dialogs)
    static QStringList knownExtensions();

private:
    static QString projectKey(const QString& projectRoot);

    mutable QSettings m_settings;
};
