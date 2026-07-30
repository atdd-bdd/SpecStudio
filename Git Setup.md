# Setting Up Git Sharing

For whoever sets SpecStudio™ up on a machine — often a developer helping someone
who does not use git themselves. It is a **one-time** job per person per machine,
and most of it is clicking through a browser sign-in.

Read this only if the solution uses **GitHub** sharing. Solutions that share
through a **shared file system** need none of it — no git, no account, nothing
below.

---

## First: does this person need a GitHub account?

**Yes, unavoidably.** Pushing to GitHub means authenticating as somebody. There
is no anonymous write access, and no arrangement where SpecStudio signs in on
their behalf. One of these has to be true before you start:

- they have a free personal account on `github.com`, **or**
- they have been invited to your organisation, **or**
- their employer has issued them an account on a corporate **GitHub Enterprise
  Server** (a private GitHub at an address like `github.company.com`).

If none of those is true and they do not want an account, stop here and use
**shared file system** sharing instead. That is a legitimate choice, not a
lesser one: SpecStudio supports it fully, and for a small co-located team it is
simpler. You choose it when the solution is created, and it can be changed later
in the solution's settings.

---

## The short version

On Windows, for a `github.com` account, the whole procedure is:

1. Open the solution in SpecStudio.
2. Use **Share Changes**.
3. If git is not installed, accept the offer to install it. Take the defaults.
4. A browser window opens. Sign in to GitHub and approve.
5. Done — permanently, on this machine.

There is no token to create, copy or paste. Steps 3 and 4 happen once.

---

## Why there is nothing to configure

Git for Windows installs **Git Credential Manager**, and SpecStudio installs Git
for Windows. GCM is what opens that browser window, and it stores the resulting
token in the **Windows Credential Manager**, encrypted for that user account.

It also *renews* the token. This is the main reason not to hand-make a personal
access token: a token you create yourself expires, silently, and the failure
arrives weeks later as a sign-in error nobody connects to a decision made months
before.

SpecStudio deliberately does not get in the way of this. Earlier versions
disabled the credential helper for their own git calls and used a password saved
in SpecStudio's settings file instead; that is why pushing from the IDE could
fail while the same push from a terminal worked. It no longer does that — the
credential helper answers first, every time.

---

## macOS

git is not installed by default and SpecStudio will not install it, because on
macOS it comes from Apple.

1. SpecStudio offers to start the **Xcode Command Line Tools** installation
   (`xcode-select --install`). Accept, and let Apple's installer finish — it runs
   in its own window, and SpecStudio cannot tell when it is done. Come back and
   retry afterwards.
2. Apple's git has no browser sign-in. Either install Git Credential Manager,
   which behaves exactly as it does on Windows:

   ```bash
   brew install --cask git-credential-manager
   ```

   or use the Keychain with a personal access token (see below):

   ```bash
   git config --global credential.helper osxkeychain
   ```

GCM is worth the extra step. The Keychain helper *stores* a credential but
cannot *obtain* one, so with it you are back to making and renewing tokens by
hand.

---

## Linux

git comes from the distribution. SpecStudio will name the command but will not
run it, because installing needs root and a desktop application should not be
quietly invoking `sudo`:

```bash
sudo apt install git      # Debian, Ubuntu
sudo dnf install git      # Fedora, RHEL
sudo pacman -S git        # Arch
```

For credentials, in order of preference:

1. **Git Credential Manager** — `.deb` and tarball releases at
   `github.com/git-ecosystem/git-credential-manager`. Browser sign-in, same as
   Windows.
2. **libsecret**, if the desktop runs a keyring (GNOME Keyring, KWallet):
   `git config --global credential.helper libsecret`. Stores securely, but you
   still supply a token yourself.
3. `credential.helper store` — **avoid**. It writes the token to
   `~/.git-credentials` in clear text.

On a headless machine there may be no keyring at all. SpecStudio will tell you
if it has had to fall back to a file readable only by the owner rather than
anything encrypted.

---

## If you do need a personal access token

Only when there is no credential helper that can do a browser sign-in.

**Get it from the same host that holds the repository.** A token is issued by one
server and is valid only there — a `github.com` token means nothing to
`github.company.com`, and the reverse.

| Repository | Token page |
|---|---|
| `github.com` | `https://github.com/settings/tokens` |
| Enterprise Server | `https://<your-host>/settings/tokens` — same path, your server |

Scope: a **classic** token needs `repo`. A **fine-grained** token needs
*Contents: read and write*, plus *Administration: read and write* if SpecStudio
is to create the repository for you.

Set an expiry you will actually remember, and write down when it falls due.

### The one that catches everybody

If the repository belongs to an organisation on **GitHub Enterprise Cloud with
SAML single sign-on**, a valid token is still refused until it is authorised for
that organisation. On the token page, use **Configure SSO → Authorize** next to
the token.

The error you get without this says nothing about SSO. It looks like an ordinary
permission failure, and people burn an afternoon regenerating tokens that were
fine.

---

## When something goes wrong

SpecStudio inspects failed git output and, when it looks like a sign-in problem,
prints what to do next in the Output panel. The messages worth recognising:

| Message | What it means |
|---|---|
| *Password authentication is not supported for Git operations* | An account password was used. GitHub stopped accepting these in August 2021 — no password will ever work. Use a credential helper or a token. |
| *could not read Username* / *terminal prompts disabled* | No credential helper is configured, so nothing can answer the prompt. |
| *Authentication failed* with a helper configured | The stored sign-in expired or was revoked. Clear the saved entry for that host and sign in again. |
| 403 on an organisation repository | Usually the SAML SSO authorisation above. |

To clear a stored credential and start again:

- **Windows** — Control Panel → Credential Manager → Windows Credentials, remove
  the `git:https://github.com` entry
- **macOS** — Keychain Access, search `github`, delete the entry
- **Linux** — `secret-tool clear service github.com`, or the keyring application

---

## Related documents

- `User Guide.md` — using the IDE
- `Configuration Guide.md` — `.specconfig` fields and generating into another repository
- `spectable syntax v3.3a.md` — the language
