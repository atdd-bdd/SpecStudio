#include "AppSettings.h"

#include <QCryptographicHash>
#include <QStandardPaths>

AppSettings::AppSettings()
    : m_settings(QSettings::IniFormat,
                 QSettings::UserScope,
                 "SpecStudio", "SpecStudio")
{}

QString AppSettings::projectKey(const QString& projectRoot)
{
    return "Projects/" +
           QString::fromLatin1(
               QCryptographicHash::hash(projectRoot.toUtf8(), QCryptographicHash::Md5).toHex());
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
    return m_settings.value(projectKey(projectRoot) + "/gitPassword").toString();
}

void AppSettings::setGitPassword(const QString& projectRoot, const QString& password)
{
    m_settings.setValue(projectKey(projectRoot) + "/gitPassword", password);
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
    return { ".feature", ".featurex", ".spectable", ".txt", ".md", ".csv", ".xls", ".xlsx" };
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

// ---- Appearance ----

bool AppSettings::darkTheme() const
{
    return m_settings.value("Appearance/darkTheme", false).toBool();
}

void AppSettings::setDarkTheme(bool dark)
{
    m_settings.setValue("Appearance/darkTheme", dark);
}
