#include "AppSettings.h"
#include "../git/SecretStore.h"

#include <QApplication>
#include <QCryptographicHash>
#include <QStandardPaths>

namespace {
// A fixed account name, deliberately not the git username. SecretStore is keyed
// by (service, account); using the username would orphan the stored secret the
// moment someone edited it. The username is not a secret and stays in the INI.
const char* const kCredentialAccount = "credential";
} // namespace

AppSettings::AppSettings()
    : m_settings(QSettings::IniFormat,
                 QSettings::UserScope,
                 "SpecStudio", "SpecStudio")
{
    migrateSecretsOutOfIni();
}

// Older builds wrote git passwords straight into SpecStudio.ini, in clear, where
// anything running as the user could read them. Move any that are still there
// into the OS credential store and delete the plain-text copy. Runs on every
// start; after the first it finds nothing and costs one pass over two groups.
void AppSettings::migrateSecretsOutOfIni()
{
    int moved = 0, failed = 0;
    for (const char* group : { "Projects", "Solutions" }) {
        m_settings.beginGroup(QString::fromLatin1(group));
        const QStringList entries = m_settings.childGroups();
        for (const QString& hash : entries) {
            const QString key = hash + "/gitPassword";
            const QString legacy = m_settings.value(key).toString();
            if (legacy.isEmpty()) {
                // Remove an empty leftover too, so the key stops reappearing.
                if (m_settings.contains(key)) m_settings.remove(key);
                continue;
            }
            const QString service = QString::fromLatin1(group) + QLatin1Char('/') + hash;
            if (SecretStore::store(service, QString::fromLatin1(kCredentialAccount), legacy)) {
                m_settings.remove(key);
                ++moved;
            } else {
                // Leave it alone rather than lose it. Better a plain-text secret
                // the user still has than a deleted one they cannot recover.
                ++failed;
            }
        }
        m_settings.endGroup();
    }
    if (moved || failed)
        qInfo("AppSettings: moved %d git secret(s) into %s; %d could not be moved",
              moved, qPrintable(SecretStore::backendName()), failed);
}

QString AppSettings::projectKey(const QString& projectRoot)
{
    return "Projects/" +
           QString::fromLatin1(
               QCryptographicHash::hash(projectRoot.toUtf8(), QCryptographicHash::Md5).toHex());
}

QString AppSettings::solutionKey(const QString& solutionRoot)
{
    return "Solutions/" +
           QString::fromLatin1(
               QCryptographicHash::hash(solutionRoot.toUtf8(), QCryptographicHash::Md5).toHex());
}

// ---- Editor associations ----

QString AppSettings::editorForExtension(const QString& ext) const
{
    return m_settings.value(QStringLiteral("Editors/extension/") + ext.toLower()).toString();
}

void AppSettings::setEditorForExtension(const QString& ext, const QString& program)
{
    m_settings.setValue(QStringLiteral("Editors/extension/") + ext.toLower(), program);
}

// ---- Git settings ----

QString AppSettings::gitRemoteUrl(const QString& projectRoot) const
{
    return m_settings.value(projectKey(projectRoot) + "/gitRemoteUrl").toString();
}

void AppSettings::setGitRemoteUrl(const QString& projectRoot, const QString& url)
{
    m_settings.setValue(projectKey(projectRoot) + "/gitRemoteUrl", url);
}

QString AppSettings::gitBranch(const QString& projectRoot) const
{
    return m_settings.value(projectKey(projectRoot) + "/gitBranch", "main").toString();
}

void AppSettings::setGitBranch(const QString& projectRoot, const QString& branch)
{
    m_settings.setValue(projectKey(projectRoot) + "/gitBranch", branch);
}

QString AppSettings::gitUser(const QString& projectRoot) const
{
    return m_settings.value(projectKey(projectRoot) + "/gitUser").toString();
}

void AppSettings::setGitUser(const QString& projectRoot, const QString& user)
{
    m_settings.setValue(projectKey(projectRoot) + "/gitUser", user);
}

QString AppSettings::gitPassword(const QString& projectRoot) const
{
    return SecretStore::retrieve(projectKey(projectRoot), kCredentialAccount);
}

void AppSettings::setGitPassword(const QString& projectRoot, const QString& password)
{
    if (password.isEmpty())
        SecretStore::remove(projectKey(projectRoot), kCredentialAccount);
    else
        SecretStore::store(projectKey(projectRoot), kCredentialAccount, password);
}

// ---- Git settings (per-solution — every project in a solution shares one repo) ----

QString AppSettings::solutionGitRemoteUrl(const QString& solutionRoot) const
{
    return m_settings.value(solutionKey(solutionRoot) + "/gitRemoteUrl").toString();
}

void AppSettings::setSolutionGitRemoteUrl(const QString& solutionRoot, const QString& url)
{
    m_settings.setValue(solutionKey(solutionRoot) + "/gitRemoteUrl", url);
}

QString AppSettings::solutionGitBranch(const QString& solutionRoot) const
{
    return m_settings.value(solutionKey(solutionRoot) + "/gitBranch", "main").toString();
}

void AppSettings::setSolutionGitBranch(const QString& solutionRoot, const QString& branch)
{
    m_settings.setValue(solutionKey(solutionRoot) + "/gitBranch", branch);
}

QString AppSettings::solutionGitUser(const QString& solutionRoot) const
{
    return m_settings.value(solutionKey(solutionRoot) + "/gitUser").toString();
}

void AppSettings::setSolutionGitUser(const QString& solutionRoot, const QString& user)
{
    m_settings.setValue(solutionKey(solutionRoot) + "/gitUser", user);
}

QString AppSettings::solutionGitPassword(const QString& solutionRoot) const
{
    return SecretStore::retrieve(solutionKey(solutionRoot), kCredentialAccount);
}

void AppSettings::setSolutionGitPassword(const QString& solutionRoot, const QString& password)
{
    if (password.isEmpty())
        SecretStore::remove(solutionKey(solutionRoot), kCredentialAccount);
    else
        SecretStore::store(solutionKey(solutionRoot), kCredentialAccount, password);
}

QString AppSettings::lastGitHubHost() const
{
    return m_settings.value(QStringLiteral("GitHub/lastHost"), QStringLiteral("github.com")).toString();
}

void AppSettings::setLastGitHubHost(const QString& host)
{
    m_settings.setValue(QStringLiteral("GitHub/lastHost"), host);
}

// ---- FeatureX options ----

bool AppSettings::implicitFolderImport(const QString& projectRoot) const
{
    return m_settings.value(projectKey(projectRoot) + "/Featurex/implicitFolderImport", true).toBool();
}

void AppSettings::setImplicitFolderImport(const QString& projectRoot, bool value)
{
    m_settings.setValue(projectKey(projectRoot) + "/Featurex/implicitFolderImport", value);
}

bool AppSettings::uniqueScenarioNames(const QString& projectRoot) const
{
    return m_settings.value(projectKey(projectRoot) + "/Featurex/uniqueScenarioNames", false).toBool();
}

void AppSettings::setUniqueScenarioNames(const QString& projectRoot, bool value)
{
    m_settings.setValue(projectKey(projectRoot) + "/Featurex/uniqueScenarioNames", value);
}

bool AppSettings::uniqueStepNames(const QString& projectRoot) const
{
    return m_settings.value(projectKey(projectRoot) + "/Featurex/uniqueStepNames", false).toBool();
}

void AppSettings::setUniqueStepNames(const QString& projectRoot, bool value)
{
    m_settings.setValue(projectKey(projectRoot) + "/Featurex/uniqueStepNames", value);
}

StepScope AppSettings::stepSuggestionScope(const QString& projectRoot) const
{
    QString val = m_settings.value(projectKey(projectRoot) + "/Featurex/stepSuggestionScope",
                                   "folder").toString();
    if (val == "file")    return StepScope::File;
    if (val == "project") return StepScope::Project;
    return StepScope::Folder;
}

void AppSettings::setStepSuggestionScope(const QString& projectRoot, StepScope scope)
{
    QString val;
    switch (scope) {
    case StepScope::File:    val = "file";    break;
    case StepScope::Project: val = "project"; break;
    default:                 val = "folder";  break;
    }
    m_settings.setValue(projectKey(projectRoot) + "/Featurex/stepSuggestionScope", val);
}

// ---- Window state ----

QByteArray AppSettings::mainWindowGeometry() const
{
    return m_settings.value("Window/geometry").toByteArray();
}

QByteArray AppSettings::mainWindowState() const
{
    return m_settings.value("Window/state").toByteArray();
}

void AppSettings::saveMainWindowState(const QByteArray& geometry, const QByteArray& state)
{
    m_settings.setValue("Window/geometry", geometry);
    m_settings.setValue("Window/state",    state);
}

// ---- Recent solutions ----

QStringList AppSettings::recentSolutions() const
{
    int n = m_settings.beginReadArray("RecentSolutions");
    QStringList list;
    for (int i = 0; i < n; ++i) {
        m_settings.setArrayIndex(i);
        list << m_settings.value("path").toString();
    }
    m_settings.endArray();
    return list;
}

QStringList AppSettings::knownExtensions()
{
    return { ".spectable", ".specconfig", ".featurex", ".feature", ".txt", ".md", ".csv", ".xls", ".xlsx" };
}

QString AppSettings::activeBuildConfig(const QString& projectRoot) const
{
    return m_settings.value(projectKey(projectRoot) + "/activeBuildConfig").toString();
}

void AppSettings::setActiveBuildConfig(const QString& projectRoot, const QString& configPath)
{
    m_settings.setValue(projectKey(projectRoot) + "/activeBuildConfig", configPath);
}

void AppSettings::addRecentSolution(const QString& path)
{
    QStringList list = recentSolutions();
    list.removeAll(path);
    list.prepend(path);
    while (list.size() > 10) list.removeLast();

    m_settings.beginWriteArray("RecentSolutions");
    for (int i = 0; i < list.size(); ++i) {
        m_settings.setArrayIndex(i);
        m_settings.setValue("path", list[i]);
    }
    m_settings.endArray();
}

// ---- OS default editor ----

QString AppSettings::osDefaultEditor(const QString& ext)
{
#ifdef Q_OS_WIN
    const QString dotExt = "." + ext.toLower();

    // Prefer the user's explicit file-type association (HKCU UserChoice)
    QString progId;
    {
        QSettings uc(
            "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion"
            "\\Explorer\\FileExts\\" + dotExt + "\\UserChoice",
            QSettings::NativeFormat);
        progId = uc.value("ProgId").toString();
    }

    // Fall back to the machine-wide association in HKCR
    if (progId.isEmpty()) {
        QSettings cr("HKEY_CLASSES_ROOT\\" + dotExt, QSettings::NativeFormat);
        progId = cr.value(".").toString();
    }

    if (progId.isEmpty()) return {};

    // Resolve the shell\open\command for this ProgID
    QSettings cmd(
        "HKEY_CLASSES_ROOT\\" + progId + "\\shell\\open\\command",
        QSettings::NativeFormat);
    QString command = cmd.value(".").toString().trimmed();
    if (command.isEmpty()) return {};

    // Extract just the executable path (strip args and surrounding quotes)
    if (command.startsWith('"')) {
        int end = command.indexOf('"', 1);
        if (end > 1) return command.mid(1, end - 1);
    } else {
        int sp = command.indexOf(' ');
        return (sp > 0) ? command.left(sp) : command;
    }
#else
    Q_UNUSED(ext)
#endif
    return {};
}

// ---- Default project location ----

QString AppSettings::defaultProjectLocation() const
{
    const QString docs = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    return m_settings.value("General/defaultProjectLocation", docs).toString();
}

void AppSettings::setDefaultProjectLocation(const QString& path)
{
    m_settings.setValue("General/defaultProjectLocation", path);
}

// ---- Auto-reload ----

bool AppSettings::autoReloadFiles() const
{
    return m_settings.value("General/autoReloadFiles", false).toBool();
}

void AppSettings::setAutoReloadFiles(bool value)
{
    m_settings.setValue("General/autoReloadFiles", value);
}

bool AppSettings::showAllFiles() const
{
    return m_settings.value("General/showAllFiles", false).toBool();
}

void AppSettings::setShowAllFiles(bool value)
{
    m_settings.setValue("General/showAllFiles", value);
}

// ---- Appearance ----

bool AppSettings::darkTheme() const
{
    return m_settings.value("Appearance/darkTheme", false).toBool();
}

void AppSettings::setDarkTheme(bool dark)
{
    m_settings.setValue("Appearance/darkTheme", dark);
}

// ---- Fonts ----

QFont AppSettings::editorFont() const
{
    const QString family = m_settings.value("Fonts/editorFamily", "Courier New").toString();
    const int size       = m_settings.value("Fonts/editorSize",   12).toInt();
    return QFont(family, size);
}

void AppSettings::setEditorFont(const QFont& font)
{
    m_settings.setValue("Fonts/editorFamily", font.family());
    m_settings.setValue("Fonts/editorSize",   font.pointSize());
}

QFont AppSettings::outputFont() const
{
    const QString family = m_settings.value("Fonts/outputFamily", "Courier New").toString();
    const int size       = m_settings.value("Fonts/outputSize",   10).toInt();
    return QFont(family, size);
}

void AppSettings::setOutputFont(const QFont& font)
{
    m_settings.setValue("Fonts/outputFamily", font.family());
    m_settings.setValue("Fonts/outputSize",   font.pointSize());
}

QFont AppSettings::uiFont() const
{
    if (!m_settings.contains("Fonts/uiFamily"))
        return QApplication::font();
    const QString family = m_settings.value("Fonts/uiFamily").toString();
    const int size       = m_settings.value("Fonts/uiSize", 9).toInt();
    return QFont(family, size);
}

void AppSettings::setUiFont(const QFont& font)
{
    m_settings.setValue("Fonts/uiFamily", font.family());
    m_settings.setValue("Fonts/uiSize",   font.pointSize());
}
