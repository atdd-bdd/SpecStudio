# Building SpecStudio™ Distributions

How to produce an installable SpecStudio for Windows, Linux and macOS, and how
to sign each one.

Every package bundles the Qt runtime and the two helper executables SpecStudio
needs beside it — `SpecTableConverter` and `SpecStudioAskPass` — so it runs on a
machine with no Qt installed. The language toolchains used to compile generated
tests (JDK, .NET, Go, Rust, Python, Node, Swift, a C++ compiler) are **not**
included; developers bring their own.

**You need one machine per platform.** Qt applications cannot practically be
cross-compiled: the build, `windeployqt`, `linuxdeploy` and `macdeployqt` all
have to run natively on the target OS. See *Building all three without owning
the machines* at the end for the CI alternative.

## The scripts

| Script | Platform | Produces |
|---|---|---|
| `scripts/package_windows.ps1` | Windows | staged folder, portable zip, installer |
| `scripts/specstudio.iss` | Windows | Inno Setup definition (not run directly) |
| `scripts/sign_windows.ps1` | Windows | Authenticode signing, Sectigo token |
| `scripts/package_linux.sh` | Linux | AppImage, tarball |
| `scripts/package_mac.sh` | macOS | DMG |
| `scripts/notarize_mac.sh` | macOS | Apple codesign, notarize, staple |

Everything lands in `dist/`, which is gitignored — build output does not belong
in the repository. Attach the artefacts to a GitHub release instead.

## Version numbering

Artefact names come from a release tag if the repository has one, otherwise from
`project(SpecStudio VERSION …)` in the top-level `CMakeLists.txt`. With `v0.9.0`
tagged, the tag wins.

The version compiled *into* the application still comes from CMake, via the
`SPECSTUDIO_VERSION` definition. So when bumping a release, move both together:

```bash
# edit CMakeLists.txt to the new version, commit, then
git tag -a v0.10.0 -m "SpecStudio 0.10.0"
git push origin v0.10.0
```

Changing only `CMakeLists.txt` leaves the packages still named after the old
tag; tagging alone leaves the running application reporting the old number.

---

## Documents that ship

All three platforms carry the same three, plus a generated `README-FIRST.txt`:

| Document | |
|---|---|
| `README.md` | what SpecStudio is and where the format comes from |
| `User Guide.md` | using the IDE |
| `spectable syntax v3.3a.md` | the language reference |

Where they land differs, because what a user can actually open differs:

| Package | Location |
|---|---|
| Windows folder, zip, installer | beside the executables |
| Linux tarball | beside the executables |
| Linux AppImage | `usr/share/doc/specstudio` — one file, so only reachable via `--appimage-extract` |
| macOS DMG | loose in the image next to the app, visible on mount |

They are **not** put inside `SpecStudio.app`: dragging the app to Applications
would leave them behind, and inside a bundle they need Show Package Contents.

**Renaming a document breaks the build, deliberately.** Each script names the
files in one list — `DOCS` in the shell scripts, the `foreach` in
`package_windows.ps1` — and stops with the missing name if one is absent. This is
a reaction to the previous behaviour: `package_windows.ps1` copied
`SpecStudio User Guide.md` with `-ErrorAction SilentlyContinue`, and
`package_linux.sh` used `cp … 2>/dev/null || true`. When that guide was renamed,
the Windows copy failed in silence and the 0.9.0 packages shipped the superseded
guide, which still claimed tests generate in "C#, Java, or Rust". A rename should
cost one edit to a list, not a silently incomplete release.

---

## Windows x64

This is the one that has been run end to end.

**Prerequisites**

- Visual Studio 2022 (MSVC toolchain)
- Qt 6.10 for `msvc2022_64`
- CMake — the one at `C:\Qt\Tools\CMake_64\bin\cmake.exe` is fine
- Inno Setup 6, for the installer. Without it you still get the portable zip.
- Windows SDK, for `signtool.exe` (only needed to sign)

**Build**

```powershell
.\scripts\package_windows.ps1
# or
.\scripts\package_windows.ps1 -QtDir C:\Qt\6.10.0\msvc2022_64 -BuildType Release
.\scripts\package_windows.ps1 -SkipBuild     # repackage what is already built
```

Produces in `dist\`:

```
SpecStudio-<version>-setup.exe          the installer
SpecStudio-<version>-windows-x64.zip    portable, no install
SpecStudio-<version>-windows-x64\       the staged folder both came from
```

**The one thing not bundled** is the Microsoft Visual C++ 2015–2022
Redistributable (x64). Most machines have it; the installer checks and points at
`https://aka.ms/vs/17/release/vc_redist.x64.exe` if missing, and the portable
zip says so in its README.

**Verified:** the staged binaries run with Qt stripped off the `PATH`, the
converter generates correct output at exit 0, and the GUI starts and draws.

---

## Linux x64

**Machine.** An x86_64 Linux box or VM. Build on the *oldest* distribution you
intend to support — Ubuntu 22.04 is a sensible floor. glibc is only forward
compatible, so an AppImage built on 24.04 will not run on 22.04.

**Prerequisites**

```bash
sudo apt install build-essential cmake ninja-build git libgl1-mesa-dev

# Qt 6.10 for gcc_64: the Qt online installer, or
pip install aqtinstall
aqt install-qt linux desktop 6.10.0 gcc_64
```

**Build**

```bash
git clone https://github.com/atdd-bdd/SpecStudio && cd SpecStudio
./scripts/package_linux.sh --qt-dir ~/Qt/6.10.0/gcc_64
```

Produces in `dist/`:

```
SpecStudio-<version>-x86_64.AppImage        self-contained, carries Qt
SpecStudio-<version>-linux-x86_64.tar.gz    three binaries only, needs system Qt
```

The script downloads `linuxdeploy` and its Qt plugin on first run, so it needs
network once. If the download fails it still writes the tarball and says so.

**FUSE.** `linuxdeploy` is itself an AppImage and would normally need FUSE to
mount. Ubuntu 22.04+ dropped `libfuse2`, and CI containers rarely have it; the
failure reads as an obscure `libfuse.so.2` error. The script sets
`APPIMAGE_EXTRACT_AND_RUN=1`, which extracts instead — slower, works everywhere.

**Icon.** If `resources/icons/specstudio*.png` exists it is used. Otherwise the
script generates a placeholder (via ImageMagick if present, else a 1×1 PNG) so
packaging is never blocked on artwork. Replace it when there is real artwork.

**Signing.** Linux has no Authenticode equivalent and the Sectigo certificate
does not apply. For provenance, publish a detached GPG signature:

```bash
gpg --detach-sign --armor dist/SpecStudio-<version>-x86_64.AppImage
```

---

## macOS

**Machine.** Any Mac — but read the architecture note, it is easy to get wrong.

**Prerequisites**

```bash
xcode-select --install
brew install cmake ninja
# Qt 6.10 for macos
```

**Build**

```bash
./scripts/package_mac.sh --qt-dir ~/Qt/6.10.0/macos --universal
```

Produces `dist/SpecStudio-<version>-macos-universal.dmg`.

**Use `--universal`.** Without it the build targets whatever Mac it runs on, and
an arm64 build will not launch on an Intel Mac at all — Rosetta translates
Intel→ARM, not the reverse — so a single-architecture build silently excludes
half your users. Qt's macOS libraries ship universal, so one bundle serves both.
The architecture appears in the DMG name (`-universal`, `-arm64`, `-x86_64`) so
a host-only build cannot be mistaken for one that runs everywhere.

The script copies both helper executables into `Contents/MacOS` (which is what
`applicationDirPath()` returns, so it is the only place they can go and still be
found), runs `macdeployqt` with `-executable=` for each so their load paths are
rewritten to the bundled frameworks, then checks `otool -L` for any absolute
path that would dangle on someone else's Mac.

---

## Signing

Three platforms, three different answers. **The Sectigo USB token covers Windows
only.**

Signing is a separate step from building everywhere, on purpose. The key lives
on hardware and needs a PIN, so it cannot run unattended; and keeping it
separate means the build stays reproducible and you sign exactly the bytes you
built. Never re-run a packaging script after signing without re-signing — it
overwrites the files and discards the signatures.

### A release needs two phases

Signing *after* packaging is not enough, and the gap is easy to miss. The zip and
the installer are both built **from the staged folder**, so a single
package-then-sign pass signs the loose staged executables and the installer
wrapper while the copies *embedded in* the zip and the installer stay unsigned.
Install from it and `SpecStudio.exe` on disk is unsigned — the signature on the
installer says nothing about what it unpacked.

So stage and package separately, signing in between:

```powershell
.\scripts\package_windows.ps1 -StageOnly      # build + stage, no zip/installer
.\scripts\sign_windows.ps1                    # sign the staged .exe files
.\scripts\package_windows.ps1 -PackageOnly    # zip + installer from signed files
.\scripts\sign_windows.ps1 dist\SpecStudio-0.9.0-setup.exe
```

`-PackageOnly` deliberately does **not** re-stage — re-staging would overwrite
the executables just signed — and it reports whether what it is about to package
is actually signed, so this cannot fail quietly. `-StageOnly` and `-PackageOnly`
are mutually exclusive.

A plain `.\scripts\package_windows.ps1` still does everything in one pass; it is
fine for a test build, and it now tells you to use the two-phase flow for a
release.

### Windows — Sectigo token

Since June 2023 the CA/Browser Forum has required publicly trusted code-signing
keys to live on FIPS 140-2 Level 2 hardware, which is why Sectigo shipped a USB
token rather than a `.pfx` file.

1. Plug in the token; start SafeNet Authentication Client.
2. `.\scripts\sign_windows.ps1 -ListCerts` to confirm the certificate is visible.
3. `.\scripts\sign_windows.ps1`

It signs every `.exe` in `dist\` — `SpecStudio.exe`, `SpecTableConverter.exe`,
`SpecStudioAskPass.exe` and the installer. All four, not just the launcher:
SpecStudio starts the converter and askpass helper as child processes, and an
unsigned child undermines the signature on the parent. The installer is signed
so SmartScreen sees a signed download.

**DLLs are not signed.** Every DLL that ships came from Qt, not from this build;
The Qt Company's signature is the accurate provenance, and re-signing another
party's binary with this certificate would assert authorship we do not have.

To sign a single file, pass it positionally:

```powershell
.\scripts\sign_windows.ps1 dist\SpecStudio-0.9.0-setup.exe
```

#### Choosing the certificate

The no-argument form is correct on a machine with one valid certificate — it
picks the single *unexpired* code-signing certificate in the store. Use
`-Thumbprint` or `-SubjectName` only when there is more than one.

Two traps, both of which report the same unhelpful signtool message, *No
certificates were found that met all the given criteria*:

- signtool's `/n` matches a substring of the subject **as rendered**, and a CN
  containing a comma renders quoted — `CN="Ken Pugh, Inc."`. Passing the full DN
  `CN=Ken Pugh, Inc.` therefore cannot match.
- This store still holds an **expired 2022 certificate with the same CN**, so
  even the bare name matches two certificates.

`sign_windows.ps1` sidesteps both. `-SubjectName` is resolved against the
certificate store — expired certificates excluded, ambiguity reported with the
candidates listed — and signing is always done by thumbprint, so signtool is
never left to disambiguate. It accepts the CN value or a full DN, quoted or not.

`-Thumbprint` is validated before use: a hash copied from the Windows
certificate dialog carries an invisible U+200E mark and spaces between byte
pairs, both of which signtool rejects as *Invalid SHA1 hash format*. Those are
stripped, and anything that is not 40 hex digits is refused with an explanation
rather than passed through.

The PIN is never a parameter and is never written anywhere. If you are prompted
per file rather than once per session, enable **Enable single logon** in the
SafeNet client rather than scripting the PIN.

Everything is timestamped against `http://timestamp.sectigo.com`, so signatures
stay valid after the certificate expires.

### macOS — Apple only

The Sectigo certificate cannot sign macOS binaries. Gatekeeper accepts only an
**Apple Developer ID Application** certificate.

1. Apple Developer Program membership ($99/year).
2. A *Developer ID Application* certificate in the login keychain
   (Xcode → Settings → Accounts → Manage Certificates).
3. An app-specific password, created at appleid.apple.com — **not** your Apple
   ID password — stored once:

```bash
xcrun notarytool store-credentials specstudio-notary \
    --apple-id you@example.com --team-id ABCDE12345 --password <app-specific>
```

4. Sign, notarize and staple:

```bash
./scripts/notarize_mac.sh build-mac/SpecStudio.app dist/SpecStudio-<version>-macos-universal.dmg
```

Without this, macOS refuses to open the application on any machine that did not
build it. Stapling attaches the notarization ticket so Gatekeeper can check it
offline.

Confirm the way Gatekeeper will see it:

```bash
spctl --assess --type execute --verbose SpecStudio.app
```

### Linux — GPG or nothing

No code-signing equivalent. Publish a detached GPG signature alongside the
AppImage, or sign the repository metadata if distributing through a package
repo.

---

## Building Linux and macOS without owning the machines

`.github/workflows/release.yml` builds both on GitHub Actions —
`ubuntu-22.04` and `macos-14`, free for public repositories.

**Trigger it** by pushing a version tag:

```bash
git tag -a v0.10.0 -m "SpecStudio 0.10.0"
git push origin v0.10.0
```

That builds both platforms, then opens a **draft** release with the artefacts
attached. It is deliberately a draft: the Windows build has to be produced and
signed locally, added by hand, and only then published.

`workflow_dispatch` runs the builds without creating a release, which is the way
to test a change to the packaging scripts.

**Windows is not in the workflow.** Its artefacts must be signed with the
Sectigo hardware token, which no runner can reach, and publishing an unsigned
`.exe` beside signed Linux and Mac builds would be worse than building it by
hand. The division of labour is: CI produces Linux and macOS, you produce
Windows.

### What the workflow checks

Not just that the build succeeded:

- **Linux** extracts the AppImage and runs the bundled converter under a
  scrubbed environment (`env -i`), so a package that only works because the
  runner happens to have a library fails the job.
- **macOS** runs `lipo -archs` on all three executables and fails if any is not
  both `x86_64` and `arm64`. A single-architecture build is indistinguishable
  from a universal one until an Intel Mac tries to open it.

### Secrets for macOS signing

Signing and notarization run only if these repository secrets exist. Without
them the job still produces an unsigned DMG and logs a warning, so a first run
or a fork is not blocked.

| Secret | What it is |
|---|---|
| `APPLE_CERTIFICATE_P12` | Developer ID Application certificate, exported as `.p12`, base64-encoded |
| `APPLE_CERTIFICATE_PASSWORD` | the password set when exporting the `.p12` |
| `APPLE_ID` | Apple ID of the developer account |
| `APPLE_TEAM_ID` | ten-character team identifier |
| `APPLE_APP_PASSWORD` | app-specific password from appleid.apple.com, **not** the account password |

To produce the first one:

```bash
# Keychain Access -> export the Developer ID Application cert as cert.p12
base64 -i cert.p12 | pbcopy      # paste into the repository secret
```

The certificate is imported into a keychain created for that job alone, with a
random password, and deleted with the runner.

---

## What has and has not been verified

**Windows** is verified end to end, as described above.

**Linux and macOS are unverified.** The scripts are syntax-checked (`bash -n`)
and written carefully, but have never been run — there is no Linux or Mac
machine in the development environment. CI (`.github/workflows/release.yml`,
`ubuntu-22.04` and `macos-14`) is where they are first exercised for real.

The `copy_docs` helper added to both is the exception: it was extracted from the
scripts and run directly, covering the success path — including the two document
names containing spaces — and the missing-document path, which exits 1 with the
offending name. The surrounding scripts remain unrun.

Expect the first real execution to surface something. The likeliest candidates:

- the `linuxdeploy` download URL, which points at a `continuous` release that
  can move;
- the exact build-tree paths `find_tool` probes on macOS, which differ between
  Ninja and Xcode generators;
- Qt install paths, if yours are not in the locations the auto-detection tries
  (pass `--qt-dir` explicitly and that stops mattering).

Two Windows-specific traps are already handled, and are worth knowing about if
you extend these scripts: Windows PowerShell 5.1 turns any stderr line from a
native executable into a terminating error under `$ErrorActionPreference =
'Stop'`, even on exit code 0. A harmless `windeployqt` warning about
`dxcompiler.dll` killed packaging after a clean ten-minute build, and
`git describe` on a repository with no tags killed it again. Both now run
through helpers that judge tools by exit code alone. The signing script gets the
same treatment, so one chatty `signtool` line cannot leave half the files
signed. Redirected stderr also has to be unwrapped before it is printed: `2>&1`
wraps each line in an `ErrorRecord` that can render as the bare type name
`System.Management.Automation.RemoteException`, hiding the reason a sign failed.
