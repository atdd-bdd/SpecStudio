#!/usr/bin/env bash
# package_mac.sh - build SpecStudio for macOS and package it as a .dmg.
#
# Replaces the earlier build_mac_dmg.sh, which shipped a bundle with no code
# generator in it: SpecStudio locates SpecTableConverter and SpecStudioAskPass
# beside its own executable, and neither was ever copied into the .app. Build
# would have reported "No converter found" on every install.
#
# Produces, in dist/:
#   SpecStudio-<version>.dmg
#
# macdeployqt puts the Qt frameworks and plugins inside the bundle, so it runs
# on a Mac with no Qt. Language toolchains are not included.
#
# Nothing is signed here. macOS signing is a separate step and a separate
# certificate: the Sectigo token signs Windows binaries only, and Gatekeeper
# accepts nothing but an Apple Developer ID Application certificate, followed by
# notarization. See scripts/notarize_mac.sh.
#
#   ./scripts/package_mac.sh
#   ./scripts/package_mac.sh --qt-dir ~/Qt/6.10.0/macos
#   ./scripts/package_mac.sh --skip-build

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$SCRIPT_DIR/.." && pwd)"

QT_DIR=""
BUILD_TYPE="Release"
SKIP_BUILD=0
ARCHS=""          # empty means: whatever this Mac is

while [[ $# -gt 0 ]]; do
    case "$1" in
        --qt-dir)     QT_DIR="$2"; shift 2 ;;
        --build-type) BUILD_TYPE="$2"; shift 2 ;;
        --skip-build) SKIP_BUILD=1; shift ;;
        --universal)  ARCHS="x86_64;arm64"; shift ;;
        --arch)       ARCHS="$2"; shift 2 ;;
        -h|--help)    sed -n '2,24p' "$0"; exit 0 ;;
        *) echo "Unknown option: $1" >&2; exit 2 ;;
    esac
done

if [[ -z "$QT_DIR" ]]; then
    for c in "$HOME/Qt/6.10.0/macos" "/usr/local/Qt/6.10.0/macos" \
             "/opt/homebrew/opt/qt@6" "/usr/local/opt/qt@6"; do
        if [[ -f "$c/lib/cmake/Qt6/Qt6Config.cmake" ]]; then QT_DIR="$c"; break; fi
    done
fi
[[ -n "$QT_DIR" ]] || { echo "ERROR: Qt 6 not found. Pass --qt-dir <path>." >&2; exit 1; }
echo "Qt: $QT_DIR"

MACDEPLOYQT="$QT_DIR/bin/macdeployqt"
[[ -x "$MACDEPLOYQT" ]] || { echo "ERROR: macdeployqt not found at $MACDEPLOYQT" >&2; exit 1; }

# A release tag if the tree has one, otherwise the project() version in
# CMakeLists.txt. Never a bare commit hash -- these names are user-facing.
VERSION="$(git -C "$REPO" describe --tags --abbrev=0 2>/dev/null || true)"
if [[ -z "$VERSION" ]]; then
    VERSION="$(grep -o 'project(SpecStudio VERSION [0-9.]*' "$REPO/CMakeLists.txt" | grep -o '[0-9][0-9.]*$')"
fi
VERSION="${VERSION:-0.0.0}"
VERSION="${VERSION#v}"
echo "SpecStudio $VERSION ($BUILD_TYPE)"

BUILD_DIR="$REPO/build-mac"
DIST="$REPO/dist"
STAGING="$BUILD_DIR/dmg-staging"
mkdir -p "$DIST"

# ---- build -------------------------------------------------------------------
if [[ $SKIP_BUILD -eq 0 ]]; then
    GEN=()
    command -v ninja >/dev/null 2>&1 && GEN=(-G Ninja)

    # Without CMAKE_OSX_ARCHITECTURES the build targets whichever Mac it runs
    # on. An arm64 build does not run on an Intel Mac at all -- not even under
    # Rosetta, which goes the other way -- so a single-arch build silently
    # excludes half your users. --universal produces one bundle for both, which
    # Qt's own macOS libraries support since they ship universal.
    ARCH_ARGS=()
    if [[ -n "$ARCHS" ]]; then
        ARCH_ARGS=(-DCMAKE_OSX_ARCHITECTURES="$ARCHS")
        echo "Architectures: $ARCHS"
    else
        echo "Architectures: $(uname -m) only (pass --universal for both)"
    fi

    echo "Configuring..."
    cmake -S "$REPO" -B "$BUILD_DIR" "${GEN[@]}" "${ARCH_ARGS[@]}" \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DQt6_DIR="$QT_DIR/lib/cmake/Qt6"
    echo "Building..."
    cmake --build "$BUILD_DIR" --parallel
fi

APP="$(find "$BUILD_DIR" -maxdepth 4 -name 'SpecStudio.app' -type d | head -1)"
[[ -n "$APP" ]] || { echo "ERROR: SpecStudio.app not found under $BUILD_DIR" >&2; exit 1; }
echo "Bundle: $APP"

# Helper executables are plain binaries, not bundles, so CMake leaves them
# outside. Ninja and Makefiles do not nest by configuration; Xcode does.
find_tool() {  # name subdir
    for p in "$BUILD_DIR/$2/$1" "$BUILD_DIR/$2/$BUILD_TYPE/$1"; do
        [[ -x "$p" ]] && { echo "$p"; return 0; }
    done
    return 1
}
CONVERTER="$(find_tool SpecTableConverter converter)" \
    || { echo "ERROR: SpecTableConverter not built" >&2; exit 1; }
ASKPASS="$(find_tool SpecStudioAskPass src)" \
    || { echo "ERROR: SpecStudioAskPass not built" >&2; exit 1; }

# ---- put the helpers inside the bundle ---------------------------------------
# Contents/MacOS is what applicationDirPath() returns, so this is the one place
# they can go and still be found.
echo "Adding helper executables to the bundle..."
cp "$CONVERTER" "$ASKPASS" "$APP/Contents/MacOS/"

# ---- deploy Qt ---------------------------------------------------------------
# -executable= is given for each helper so macdeployqt rewrites their load paths
# to the frameworks inside the bundle too. Without it they keep absolute
# references to the build machine's Qt and fail to start anywhere else.
echo "Running macdeployqt..."
"$MACDEPLOYQT" "$APP" \
    -executable="$APP/Contents/MacOS/SpecTableConverter" \
    -executable="$APP/Contents/MacOS/SpecStudioAskPass" \
    -verbose=1

# ---- sanity check ------------------------------------------------------------
# A bundle that still points at the build machine's Qt looks fine here and fails
# on the user's Mac, so check before shipping rather than after.
echo "Checking for references outside the bundle..."
leaked=0
for exe in "$APP/Contents/MacOS/"*; do
    [[ -f "$exe" && -x "$exe" ]] || continue
    while read -r lib; do
        case "$lib" in
            /usr/lib/*|/System/*|@rpath/*|@executable_path/*|@loader_path/*|"") ;;
            *) echo "  $(basename "$exe") -> $lib"; leaked=1 ;;
        esac
    done < <(otool -L "$exe" | tail -n +2 | awk '{print $1}')
done
if [[ $leaked -eq 1 ]]; then
    echo "WARNING: the above are absolute paths on this machine and will not" >&2
    echo "         resolve on another Mac. Check the macdeployqt output." >&2
fi

# ---- documents ---------------------------------------------------------------
# Named explicitly and checked. The Windows script carried the same copy with
# `-ErrorAction SilentlyContinue` while naming a guide that had been renamed: the
# copy failed in silence and the distribution shipped without it. A missing
# document must stop the build, not quietly drop out of it.
#
# These go loose in the DMG next to the app, so they are visible the moment the
# volume mounts. Inside the bundle they would be invisible without Show Package
# Contents, and would be thrown away by dragging the app to Applications.
DOCS=("README.md" "User Guide.md" "Configuration Guide.md" "spectable syntax v3.3a.md")

copy_docs() {  # dest-dir
    local dest="$1" doc
    mkdir -p "$dest"
    for doc in "${DOCS[@]}"; do
        if [[ ! -f "$REPO/$doc" ]]; then
            echo "ERROR: document not found: $doc" >&2
            echo "       Update DOCS in $(basename "$0") if it was renamed." >&2
            exit 1
        fi
        cp "$REPO/$doc" "$dest/"
    done
}

# ---- dmg ---------------------------------------------------------------------
echo "Creating DMG..."
rm -rf "$STAGING"; mkdir -p "$STAGING"
cp -R "$APP" "$STAGING/"
ln -s /Applications "$STAGING/Applications"
copy_docs "$STAGING"
cat > "$STAGING/README-FIRST.txt" <<EOF
SpecStudio $VERSION

Drag SpecStudio.app to Applications.

The Qt runtime, the code generator and the git credential helper are all inside
the bundle. The language toolchains used to compile generated tests (JDK, .NET,
Go, Rust, Python, Node, Swift, clang) are not -- install whichever you generate
for.

The documents beside this file stay on the disk image, so copy them out if you
want them: start with "User Guide.md"; "spectable syntax v3.3a.md" is the
language reference.
EOF

# Name the architecture, so an arm64-only build is never mistaken for one that
# runs everywhere.
case "$ARCHS" in
    "x86_64;arm64"|"arm64;x86_64") ARCH_TAG="universal" ;;
    "")                            ARCH_TAG="$(uname -m)" ;;
    *)                             ARCH_TAG="${ARCHS//;/-}" ;;
esac
DMG="$DIST/SpecStudio-$VERSION-macos-$ARCH_TAG.dmg"
rm -f "$DMG"
hdiutil create -volname "SpecStudio $VERSION" -srcfolder "$STAGING" \
               -ov -format UDZO "$DMG"

echo ""
echo "Done: $DMG"
echo ""
echo "Unsigned: macOS will refuse to open it on another machine until it is"
echo "signed with an Apple Developer ID and notarized. Next step:"
echo "  ./scripts/notarize_mac.sh \"$APP\" \"$DMG\""
