# Build for Mac

*(formerly named SpecStudio — the git repository is still called `specstudio`.)*

A from-scratch walkthrough for building AlignThree on macOS, for both Apple
Silicon (arm64) and Intel (x86_64). For the full release process — version
numbering, what documents ship, signing/notarizing, and the Windows/Linux
sides — see [Building Distributions.md](Building%20Distributions.md). This
document only covers getting a macOS build running from a clean checkout.

## Two kinds of build

- **Local/dev build** — plain `cmake --build`. Fast, runs straight out of the
  build tree, targets whatever Mac you're on. Not something you'd hand to
  someone else.
- **Distributable build** — `scripts/package_mac.sh`. Bundles the Qt runtime
  and the two helper executables into a relocatable `.dmg` that runs on a Mac
  with nothing installed.

## Prerequisites (either architecture)

```bash
xcode-select --install      # Apple Clang + the macOS SDK
brew install cmake          # 3.21+
```

Homebrew itself: [brew.sh](https://brew.sh). `git` ships with the Xcode
Command Line Tools.

## Which Qt install to use

This is the one choice that actually matters, and it decides which
architectures you can produce:

| Source | What you get | Use it for |
|---|---|---|
| `brew install qt@6` | **Native-arch only.** arm64 on an Apple Silicon Mac, x86_64 on an Intel Mac. Homebrew does not ship fat/universal Qt frameworks. | Building for the Mac you're sitting at. |
| Qt Online Installer, or `pip install aqtinstall` | **Universal** (arm64 **and** x86_64 in the same frameworks) | Building for the *other* architecture, or a universal DMG, without owning a second Mac. |

Mixing these up is the whole failure mode: pass `--universal` to
`scripts/package_mac.sh` with Homebrew's Qt and the link fails with
`ld: symbol(s) not found for architecture x86_64` — there's no x86_64 slice of
Qt to link against. (Confirmed by trying it: see *Pitfalls* below.)

---

## Building for Apple Silicon (arm64)

**Verified** on this machine (Aug 2026, an M-series Mac).

### Install Qt

```bash
brew install qt@6
```

Lands at `/opt/homebrew/opt/qt@6`.

### Local dev build

```bash
git clone https://github.com/atdd-bdd/SpecStudio && cd SpecStudio
cmake -S . -B build -DQt6_DIR=/opt/homebrew/opt/qt@6/lib/cmake/Qt6
cmake --build build --parallel
open build/src/AlignThree.app
```

`AlignThree` finds `SpecTableConverter` and `AlignThreeAskPass` via
`applicationDirPath()` (see `src/ToolPath.h`), which on macOS resolves to
`AlignThree.app/Contents/MacOS`. A `POST_BUILD` step in `src/CMakeLists.txt`
copies both helpers there automatically, so Build works out of a plain dev
build with no extra step.

### Distributable DMG

```bash
./scripts/package_mac.sh
```

Auto-detects the Homebrew Qt above and produces
`dist/AlignThree-<version>-macos-arm64.dmg` — a Release build, Qt frameworks
bundled via `macdeployqt`, ad-hoc signed so it actually launches (see
*Pitfalls*). It is **not** notarized; on another Mac, Gatekeeper will block it
until the user right-clicks → Open, or until it goes through
`scripts/notarize_mac.sh` with a real Apple Developer ID (see
*Building Distributions.md*).

---

## Building for Intel (x86_64)

### On an actual Intel Mac

**Unverified** — no Intel Mac was available to test this, but Homebrew's
install layout is the only thing that differs from the arm64 path; the
commands are the same:

```bash
brew install qt@6      # lands at /usr/local/opt/qt@6 on Intel
cmake -S . -B build -DQt6_DIR=/usr/local/opt/qt@6/lib/cmake/Qt6
cmake --build build --parallel
./scripts/package_mac.sh --qt-dir /usr/local/opt/qt@6
```

Produces `dist/AlignThree-<version>-macos-x86_64.dmg`.
`scripts/package_mac.sh` already checks `/usr/local/opt/qt@6` when
auto-detecting, so `--qt-dir` is only needed if Qt lives somewhere else.

### From an Apple Silicon Mac, without owning an Intel Mac

You don't need Rosetta or a second Homebrew prefix for this — Apple's clang
cross-compiles for x86_64 natively. What you need is Qt frameworks that
*contain* an x86_64 slice, which Homebrew's don't. Install Qt via aqtinstall
or the Qt Online Installer instead:

```bash
pip install aqtinstall
aqt install-qt mac desktop 6.10.0 clang_64 -O ~/Qt
```

Then build the x86_64 slice only:

```bash
cmake -S . -B build-x86_64 \
    -DQt6_DIR=~/Qt/6.10.0/macos/lib/cmake/Qt6 \
    -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build-x86_64 --parallel
./scripts/package_mac.sh --qt-dir ~/Qt/6.10.0/macos --arch x86_64 --skip-build
```

**This specific single-arch invocation is unverified**, but it follows
directly from something that *is* proven: `.github/workflows/release.yml`
builds the `--universal` DMG (arm64 **and** x86_64 together) on a
`macos-14` runner — which is Apple Silicon — using this same aqt-installed Qt.
If linking both slices from an arm64 host already works in CI, linking just
the x86_64 slice is the easier case.

---

## Pitfalls hit while writing this (now fixed in the scripts)

Worth knowing if you ever edit `scripts/package_mac.sh`, since removing any of
these silently reintroduces the bug:

- **`macdeployqt` invalidates signatures.** It rewrites load-command paths
  (`install_name_tool`) in every framework and dylib it touches, which strips
  the ad-hoc signature the linker put there. On Apple Silicon the kernel
  refuses to load a Mach-O with an invalid signature — not a Gatekeeper
  warning, a hard `SIGKILL` at launch. The script now runs
  `codesign --force --deep --sign -` right after `macdeployqt`. Without it,
  the DMG looks fine and the app dies the instant you open it.
- **A second run could package its own leftover copy.** `package_mac.sh`
  stages the bundle at `build-mac/dmg-staging/AlignThree.app` before making
  the DMG. The `find` that locates the built bundle wasn't scoped, so a
  second run could match that staging copy instead of the real one in
  `build-mac/src/`. Now excluded explicitly.
- **`macOS`'s default `/bin/bash` is 3.2.** Under `set -u`, expanding an empty
  array (`"${GEN[@]}"` when nothing set `GEN`, e.g. no `ninja` on `PATH`)
  throws `unbound variable` on bash 3.2, even though the same line is fine on
  bash 4+. Fixed with the `${ARR[@]+"${ARR[@]}"}` idiom.

---

## See also

- [Building Distributions.md](Building%20Distributions.md) — versioning,
  which documents ship, signing and notarizing on all three platforms, and
  the CI release workflow.
- `.github/workflows/release.yml` — builds a universal macOS DMG on CI with
  no local Mac required at all.
