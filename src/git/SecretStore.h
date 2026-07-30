#pragma once

#include <QString>

// Somewhere to keep a git credential that is not a plain-text file.
//
// SpecStudio used to write the password straight into SpecStudio.ini, readable
// by anything running as the user. Each platform already has a proper place for
// this, so use it:
//
//   Windows   Windows Credential Manager (DPAPI-encrypted, per user)
//   macOS     the login Keychain, via the `security` tool
//   Linux     the Secret Service (GNOME Keyring, KWallet), via `secret-tool`
//
// A Linux box with no Secret Service running has nowhere secure to put this. In
// that case store() falls back to a file with 0600 permissions and isSecure()
// returns false, so the UI can say so plainly rather than implying a protection
// that is not there. That is the same trade `git config credential.helper store`
// makes, made visible.
namespace SecretStore
{
    // `service` names what the secret is for -- use the host, e.g. "github.com".
    // `account` is the user it belongs to.
    bool    store   (const QString& service, const QString& account,
                     const QString& secret, QString* errorOut = nullptr);
    QString retrieve(const QString& service, const QString& account,
                     QString* errorOut = nullptr);
    bool    remove  (const QString& service, const QString& account,
                     QString* errorOut = nullptr);

    // Human-readable name of the backend in use, for dialogs and diagnostics.
    QString backendName();

    // False only for the plain-file fallback. Ask before writing, and tell the
    // user what they are getting.
    bool isSecure();
}
