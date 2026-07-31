#!/usr/bin/env bash
# package_linux.sh - build SpecStudio for Linux and package it.
#
# Produces, in dist/:
#   SpecStudio-<version>-x86_64.AppImage    single self-contained file
#   SpecStudio-<version>-linux-x86_64.tar.gz  the same tree, if you prefer a tarball
#
# The AppImage carries the Qt libraries and plugins, so it runs on any
# reasonably current distribution without Qt installed. It does not carry the
# language toolchains used to compile generated tests -- those stay the
# developer's own.
#
# Nothing is signed. Linux has no Authenticode equivalent and the Sectigo token
# does not apply here; if you want provenance, GPG-sign the AppImage:
#   gpg --detach-sign --armor SpecStudio-<version>-x86_64.AppImage
# which produces a .asc users can check with your public key.
#
#   ./scripts/package_linux.sh
#   ./scripts/package_linux.sh --qt-dir ~/Qt/6.10.0/gcc_64
#   ./scripts/package_linux.sh --skip-build

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$SCRIPT_DIR/.." && pwd)"

QT_DIR=""
BUILD_TYPE="Release"
SKIP_BUILD=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --qt-dir)     QT_DIR="$2"; shift 2 ;;
        --build-type) BUILD_TYPE="$2"; shift 2 ;;
        --skip-build) SKIP_BUILD=1; shift ;;
        -h|--help)    sed -n '2,25p' "$0"; exit 0 ;;
        *) echo "Unknown option: $1" >&2; exit 2 ;;
    esac
done

# ---- locate Qt ---------------------------------------------------------------
if [[ -z "$QT_DIR" ]]; then
    for c in "$HOME/Qt/6.10.0/gcc_64" "/opt/Qt/6.10.0/gcc_64" "/usr/lib/qt6" "/usr"; do
        if [[ -f "$c/lib/cmake/Qt6/Qt6Config.cmake" ]]; then QT_DIR="$c"; break; fi
    done
fi
if [[ -z "$QT_DIR" ]]; then
    echo "ERROR: Qt 6 not found. Pass --qt-dir <path>, e.g. ~/Qt/6.10.0/gcc_64" >&2
    exit 1
fi
echo "Qt: $QT_DIR"

# A release tag if the tree has one, otherwise the project() version in
# CMakeLists.txt. Never a bare commit hash -- these names are user-facing.
VERSION="$(git -C "$REPO" describe --tags --abbrev=0 2>/dev/null || true)"
if [[ -z "$VERSION" ]]; then
    VERSION="$(grep -o 'project(SpecStudio VERSION [0-9.]*' "$REPO/CMakeLists.txt" | grep -o '[0-9][0-9.]*$')"
fi
VERSION="${VERSION:-0.0.0}"
VERSION="${VERSION#v}"
echo "SpecStudio $VERSION ($BUILD_TYPE)"

BUILD_DIR="$REPO/build-linux"
DIST="$REPO/dist"
APPDIR="$BUILD_DIR/AppDir"
mkdir -p "$DIST"

# ---- build -------------------------------------------------------------------
if [[ $SKIP_BUILD -eq 0 ]]; then
    echo "Configuring..."
    cmake -S "$REPO" -B "$BUILD_DIR" \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DQt6_DIR="$QT_DIR/lib/cmake/Qt6"
    echo "Building..."
    cmake --build "$BUILD_DIR" --parallel
fi

# Single-config generators do not nest by build type, so the binaries land
# straight in build-linux/src and build-linux/converter.
SPECSTUDIO="$BUILD_DIR/src/SpecStudio"
CONVERTER="$BUILD_DIR/converter/SpecTableConverter"
ASKPASS="$BUILD_DIR/src/SpecStudioAskPass"
for f in "$SPECSTUDIO" "$CONVERTER" "$ASKPASS"; do
    [[ -x "$f" ]] || { echo "ERROR: missing $f -- build first" >&2; exit 1; }
done

# ---- documents ---------------------------------------------------------------
# The list is named explicitly and every file is checked. It used to be a single
# `cp "$REPO/README.md" ... 2>/dev/null || true`, and the Windows script had the
# same shape while naming a guide that had since been renamed: the copy failed in
# silence and the distribution shipped without it. A missing document must stop
# the build, not quietly drop out of it.
DOCS=("README.md" "Getting Started.md" "User Guide.md" "Configuration Guide.md" "Git Setup.md" "spectable syntax v3.3a.md")

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

# ---- AppDir ------------------------------------------------------------------
# All three executables go in usr/bin together: SpecStudio finds the converter
# and the askpass helper next to itself, through applicationDirPath().
echo "Building AppDir..."
rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin" "$APPDIR/usr/share/applications" \
         "$APPDIR/usr/share/icons/hicolor/256x256/apps"
cp "$SPECSTUDIO" "$CONVERTER" "$ASKPASS" "$APPDIR/usr/bin/"

# An AppImage is one file, so these are only reachable by extracting it
# (--appimage-extract). usr/share/doc is where a user would look for them.
copy_docs "$APPDIR/usr/share/doc/specstudio"

DESKTOP="$APPDIR/usr/share/applications/specstudio.desktop"
cat > "$DESKTOP" <<EOF
[Desktop Entry]
Type=Application
Name=SpecStudio
GenericName=Specification IDE
Comment=Write and share executable specifications
Exec=SpecStudio %f
Icon=specstudio
Categories=Development;IDE;
Terminal=false
MimeType=text/x-spectable;
EOF

# linuxdeploy insists on an icon. Use the project's if there is one, otherwise
# generate a plain placeholder so packaging is never blocked on artwork.
ICON_SRC="$(find "$REPO/resources/icons" -iname 'specstudio*.png' 2>/dev/null | head -1 || true)"
ICON_DST="$APPDIR/usr/share/icons/hicolor/256x256/apps/specstudio.png"
if [[ -n "$ICON_SRC" ]]; then
    cp "$ICON_SRC" "$ICON_DST"
elif command -v convert >/dev/null 2>&1; then
    convert -size 256x256 xc:'#2d5b88' -gravity center \
            -pointsize 96 -fill white -annotate 0 'SS' "$ICON_DST"
else
    # A 1x1 PNG is enough to satisfy the tooling; replace it with real artwork
    # when there is some.
    printf '\x89PNG\r\n\x1a\n\x00\x00\x00\rIHDR\x00\x00\x00\x01\x00\x00\x00\x01\x08\x06\x00\x00\x00\x1f\x15\xc4\x89\x00\x00\x00\nIDATx\x9cc\x00\x01\x00\x00\x05\x00\x01\r\n-\xb4\x00\x00\x00\x00IEND\xaeB`\x82' > "$ICON_DST"
    echo "NOTE: no icon in resources/icons -- using a placeholder." >&2
fi

# ---- tarball (always) --------------------------------------------------------
# Useful on its own, and a fallback when the AppImage tooling cannot be fetched.
TARDIR="$BUILD_DIR/SpecStudio-$VERSION-linux-x86_64"
rm -rf "$TARDIR"; mkdir -p "$TARDIR"
cp "$SPECSTUDIO" "$CONVERTER" "$ASKPASS" "$TARDIR/"
copy_docs "$TARDIR"
cat > "$TARDIR/README-FIRST.txt" <<EOF
SpecStudio $VERSION

This tarball contains the three executables and the documentation; it expects
Qt 6 to be installed on the system. For a self-contained build use the
.AppImage, which carries the Qt runtime with it.

Start with "Getting Started.md". "User Guide.md" is the full guide and
"spectable syntax v3.3a.md" is the language reference.

Keep the three files together -- SpecStudio looks for SpecTableConverter and
SpecStudioAskPass beside itself.
EOF
tar -C "$BUILD_DIR" -czf "$DIST/SpecStudio-$VERSION-linux-x86_64.tar.gz" \
    "SpecStudio-$VERSION-linux-x86_64"
echo "Wrote $DIST/SpecStudio-$VERSION-linux-x86_64.tar.gz"

# ---- AppImage ----------------------------------------------------------------
TOOLS="$BUILD_DIR/tools"
mkdir -p "$TOOLS"
LD="$TOOLS/linuxdeploy-x86_64.AppImage"
LDQT="$TOOLS/linuxdeploy-plugin-qt-x86_64.AppImage"

fetch() {  # url dest
    if [[ -x "$2" ]]; then return 0; fi
    echo "Fetching $(basename "$2")..."
    if command -v curl >/dev/null 2>&1; then curl -fsSL "$1" -o "$2"
    elif command -v wget >/dev/null 2>&1; then wget -q "$1" -O "$2"
    else return 1; fi
    chmod +x "$2"
}

BASE="https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous"
QBASE="https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous"
if ! fetch "$BASE/linuxdeploy-x86_64.AppImage" "$LD" \
   || ! fetch "$QBASE/linuxdeploy-plugin-qt-x86_64.AppImage" "$LDQT"; then
    echo "WARNING: could not fetch linuxdeploy - skipping AppImage." >&2
    echo "         The tarball above is still usable (needs system Qt)." >&2
    exit 0
fi

echo "Building AppImage..."
# linuxdeploy finds qmake through PATH; point it at the Qt we built against so
# it bundles matching libraries.
export PATH="$QT_DIR/bin:$PATH"
export QMAKE="$QT_DIR/bin/qmake6"
[[ -x "$QMAKE" ]] || export QMAKE="$QT_DIR/bin/qmake"

# linuxdeploy and its Qt plugin are themselves AppImages, so they normally need
# FUSE to mount. Containers, CI runners and Ubuntu 22.04+ (which dropped
# libfuse2) often have none, and the failure reads as a bare "dlopen(): error
# loading libfuse.so.2". Extracting instead is slower but works everywhere.
export APPIMAGE_EXTRACT_AND_RUN=1
export OUTPUT="$DIST/SpecStudio-$VERSION-x86_64.AppImage"
export VERSION

# --executable for each helper too, so their own library needs are resolved and
# they end up inside the image rather than dangling.
"$LD" --appdir "$APPDIR" \
      --executable "$APPDIR/usr/bin/SpecStudio" \
      --executable "$APPDIR/usr/bin/SpecTableConverter" \
      --executable "$APPDIR/usr/bin/SpecStudioAskPass" \
      --desktop-file "$DESKTOP" \
      --icon-file "$ICON_DST" \
      --plugin qt \
      --output appimage

echo ""
echo "Done:"
ls -1 "$DIST" | sed 's/^/  /'
echo ""
echo "Unsigned. Linux has no Authenticode equivalent; to attest provenance:"
echo "  gpg --detach-sign --armor $OUTPUT"
