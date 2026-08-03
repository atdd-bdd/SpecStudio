#include "SecretStore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QObject>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

#ifdef Q_OS_WIN
#  include <windows.h>
#  include <wincred.h>
#endif

namespace {

// One flat key, so the three backends agree on what identifies a secret.
//
// Still "SpecStudio:" after the rename to AlignThree, deliberately. This string
// is the lookup key in the Windows Credential Manager, the macOS Keychain and
// the Linux Secret Service -- changing it would not rename the stored entries,
// it would simply stop finding them, and every user would silently lose the git
// token they had already saved. The key is never shown; the --label passed to
// secret-tool, which is what a keyring browser displays, does say AlignThree.
QString targetName(const QString& service, const QString& account)
{
    return QStringLiteral("SpecStudio:") + service + QLatin1Char(':') + account;
}

#if !defined(Q_OS_WIN)
// Run a tool and report success by exit code, keeping the secret off the
// command line where it would be visible in the process list.
bool runTool(const QString& program, const QStringList& args,
             const QByteArray& stdinData, QByteArray* stdoutOut, QString* errorOut)
{
    QProcess proc;
    proc.start(program, args);
    if (!proc.waitForStarted(3000)) {
        if (errorOut) *errorOut = QObject::tr("%1 is not available").arg(program);
        return false;
    }
    if (!stdinData.isNull()) {
        proc.write(stdinData);
        proc.closeWriteChannel();
    }
    if (!proc.waitForFinished(10000)) {
        proc.kill();
        if (errorOut) *errorOut = QObject::tr("%1 did not finish").arg(program);
        return false;
    }
    if (stdoutOut) *stdoutOut = proc.readAllStandardOutput();
    if (proc.exitCode() != 0) {
        if (errorOut) {
            const QString err = QString::fromUtf8(proc.readAllStandardError()).trimmed();
            *errorOut = err.isEmpty() ? QObject::tr("%1 failed").arg(program) : err;
        }
        return false;
    }
    return true;
}
#endif

#if !defined(Q_OS_WIN) && !defined(Q_OS_MACOS)
bool secretToolPresent()
{
    static const bool present = !QStandardPaths::findExecutable("secret-tool").isEmpty();
    return present;
}

// Last resort on a Linux machine with no Secret Service. Not encrypted; the
// only protection is file permissions, which is why isSecure() reports false.
QString fallbackPath(const QString& service, const QString& account)
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                        + QStringLiteral("/credentials");
    QDir().mkpath(dir);
    QString name = targetName(service, account);
    name.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9._-]")), QStringLiteral("_"));
    return dir + QLatin1Char('/') + name;
}
#endif

} // namespace

// ---------------------------------------------------------------------------

QString SecretStore::backendName()
{
#if defined(Q_OS_WIN)
    return QObject::tr("Windows Credential Manager");
#elif defined(Q_OS_MACOS)
    return QObject::tr("macOS Keychain");
#else
    return secretToolPresent() ? QObject::tr("Secret Service (secret-tool)")
                               : QObject::tr("a file readable only by you (not encrypted)");
#endif
}

bool SecretStore::isSecure()
{
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
    return true;
#else
    return secretToolPresent();
#endif
}

// ---------------------------------------------------------------------------

bool SecretStore::store(const QString& service, const QString& account,
                        const QString& secret, QString* errorOut)
{
#if defined(Q_OS_WIN)
    const QString target = targetName(service, account);
    const QByteArray blob = secret.toUtf8();

    CREDENTIALW cred{};
    cred.Type            = CRED_TYPE_GENERIC;
    cred.TargetName      = const_cast<LPWSTR>(reinterpret_cast<LPCWSTR>(target.utf16()));
    cred.UserName        = const_cast<LPWSTR>(reinterpret_cast<LPCWSTR>(account.utf16()));
    cred.CredentialBlob  = reinterpret_cast<LPBYTE>(const_cast<char*>(blob.constData()));
    cred.CredentialBlobSize = DWORD(blob.size());
    // LOCAL_MACHINE, not SESSION: the credential must survive a logout.
    cred.Persist         = CRED_PERSIST_LOCAL_MACHINE;

    if (!CredWriteW(&cred, 0)) {
        if (errorOut) *errorOut = QObject::tr("CredWrite failed (%1)").arg(GetLastError());
        return false;
    }
    return true;

#elif defined(Q_OS_MACOS)
    // -U updates an existing item instead of failing. -w last so the secret is
    // the final argument; `security` reads it from there.
    return runTool("security",
                   { "add-generic-password", "-U",
                     "-s", targetName(service, account),
                     "-a", account,
                     "-w", secret },
                   QByteArray(), nullptr, errorOut);

#else
    if (secretToolPresent()) {
        // secret-tool reads the secret from stdin, so it never appears in ps.
        return runTool("secret-tool",
                       { "store", "--label=AlignThree",
                         "service", targetName(service, account),
                         "account", account },
                       secret.toUtf8(), nullptr, errorOut);
    }

    QFile f(fallbackPath(service, account));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorOut) *errorOut = f.errorString();
        return false;
    }
    f.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    f.write(secret.toUtf8());
    f.close();
    return true;
#endif
}

QString SecretStore::retrieve(const QString& service, const QString& account,
                              QString* errorOut)
{
#if defined(Q_OS_WIN)
    const QString target = targetName(service, account);
    PCREDENTIALW cred = nullptr;
    if (!CredReadW(reinterpret_cast<LPCWSTR>(target.utf16()), CRED_TYPE_GENERIC, 0, &cred)) {
        if (errorOut) *errorOut = QObject::tr("no stored credential");
        return {};
    }
    const QString secret = QString::fromUtf8(
        reinterpret_cast<const char*>(cred->CredentialBlob), int(cred->CredentialBlobSize));
    CredFree(cred);
    return secret;

#elif defined(Q_OS_MACOS)
    QByteArray out;
    if (!runTool("security",
                 { "find-generic-password", "-w",
                   "-s", targetName(service, account),
                   "-a", account },
                 QByteArray(), &out, errorOut))
        return {};
    return QString::fromUtf8(out).trimmed();

#else
    if (secretToolPresent()) {
        QByteArray out;
        if (!runTool("secret-tool",
                     { "lookup", "service", targetName(service, account),
                       "account", account },
                     QByteArray(), &out, errorOut))
            return {};
        return QString::fromUtf8(out);
    }

    QFile f(fallbackPath(service, account));
    if (!f.open(QIODevice::ReadOnly)) {
        if (errorOut) *errorOut = QObject::tr("no stored credential");
        return {};
    }
    return QString::fromUtf8(f.readAll());
#endif
}

bool SecretStore::remove(const QString& service, const QString& account, QString* errorOut)
{
#if defined(Q_OS_WIN)
    const QString target = targetName(service, account);
    if (!CredDeleteW(reinterpret_cast<LPCWSTR>(target.utf16()), CRED_TYPE_GENERIC, 0)) {
        const DWORD e = GetLastError();
        if (e == ERROR_NOT_FOUND) return true;          // already gone
        if (errorOut) *errorOut = QObject::tr("CredDelete failed (%1)").arg(e);
        return false;
    }
    return true;

#elif defined(Q_OS_MACOS)
    return runTool("security",
                   { "delete-generic-password",
                     "-s", targetName(service, account),
                     "-a", account },
                   QByteArray(), nullptr, errorOut);

#else
    if (secretToolPresent())
        return runTool("secret-tool",
                       { "clear", "service", targetName(service, account),
                         "account", account },
                       QByteArray(), nullptr, errorOut);

    QFile f(fallbackPath(service, account));
    if (!f.exists()) return true;
    if (!f.remove()) {
        if (errorOut) *errorOut = f.errorString();
        return false;
    }
    return true;
#endif
}
