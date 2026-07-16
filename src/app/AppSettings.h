#pragma once

#include <QFont>
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

    // Active build configuration file (per-project, absolute path to a .specconfig file)
    QString activeBuildConfig(const QString& projectRoot) const;
    void    setActiveBuildConfig(const QString& projectRoot, const QString& configPath);

    // Default project/solution location
    QString defaultProjectLocation() const;
    void    setDefaultProjectLocation(const QString& path);

    // Automatically reload files changed on disk without prompting
    bool autoReloadFiles() const;
    void setAutoReloadFiles(bool value);

    // Show every file in Solution Explorer, including types SpecStudio doesn't
    // recognize (FileType::Other). Default: hide them.
    bool showAllFiles() const;
    void setShowAllFiles(bool value);

    // Appearance
    bool darkTheme() const;
    void setDarkTheme(bool dark);

    // Fonts
    QFont editorFont() const;
    void  setEditorFont(const QFont& font);

    QFont outputFont() const;
    void  setOutputFont(const QFont& font);

    QFont uiFont() const;
    void  setUiFont(const QFont& font);

    // The full list of file extensions SpecStudio recognises (used in dialogs)
    static QStringList knownExtensions();

    // Returns the OS-registered default application path for an extension (e.g. "csv").
    // Returns empty string if not registered or on unsupported platforms.
    static QString osDefaultEditor(const QString& ext);

private:
    static QString projectKey(const QString& projectRoot);

    mutable QSettings m_settings;
};
