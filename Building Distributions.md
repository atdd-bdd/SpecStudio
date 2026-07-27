# Building SpecStudio Distributions

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
built. Always package first, then sign — re-running a packaging script
overwrites the files and silently discards the signatures.

### Windows — Sectigo token

Since June 2023 the CA/Browser Forum has required publicly trusted code-signing
keys to live on FIPS 140-2 Level 2 hardware, which is why Sectigo shipped a USB
token rather than a `.pfx` file.

1. Plug in the token; start SafeNet Authentication Client.
2. `.\scripts\sign_windows.ps1 -ListCerts` to confirm the certificate is visible.
3. `.\scripts\sign_windows.ps1`

It signs every `.exe` and non-Qt `.dll` in `dist\`, including the installer —
SpecStudio launches the converter and askpass helper as child processes, and an
unsigned child undermines the parent. Qt's own DLLs already carry The Qt
Company's signature and are skipped.

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
machine in the development environment. Expect the first real execution to
surface something. The likeliest candidates:

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
signed.
