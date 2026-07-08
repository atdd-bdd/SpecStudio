#!/usr/bin/env bash
# build_mac_dmg.sh — Build SpecStudio for macOS and package as a .dmg
# Usage: ./scripts/build_mac_dmg.sh [Qt-install-dir] [build-type]
#   Qt-install-dir: e.g. /usr/local/Qt/6.10.0/macos  (default: auto-detect via brew/default paths)
#   build-type: Debug or Release (default: Release)
#
# Output: SpecStudio-<version>.dmg in the repo root

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# ── Config ───────────────────────────────────────────────────────────────────
QT_DIR="${1:-}"
BUILD_TYPE="${2:-Release}"
APP_NAME="SpecStudio"
VERSION="$(git -C "$REPO_ROOT" describe --tags --always --dirty 2>/dev/null || echo "dev")"
DMG_NAME="${APP_NAME}-${VERSION}.dmg"
BUILD_DIR="$REPO_ROOT/build-mac"
STAGING_DIR="$BUILD_DIR/dmg-staging"

# Auto-detect Qt if not provided
if [[ -z "$QT_DIR" ]]; then
    for candidate in \
        "$HOME/Qt/6.10.0/macos" \
        "/usr/local/Qt/6.10.0/macos" \
        "/opt/homebrew/opt/qt@6" \
        "/usr/local/opt/qt@6"; do
        if [[ -f "$candidate/lib/cmake/Qt6/Qt6Config.cmake" ]]; then
            QT_DIR="$candidate"
            break
        fi
    done
    if [[ -z "$QT_DIR" ]]; then
        echo "ERROR: Qt 6 not found. Pass Qt install dir as first argument." >&2
        echo "       e.g.: ./scripts/build_mac_dmg.sh ~/Qt/6.10.0/macos" >&2
        exit 1
    fi
fi
echo "Using Qt at: $QT_DIR"

QT6_CMAKE="$QT_DIR/lib/cmake/Qt6"
MACDEPLOYQT="$QT_DIR/bin/macdeployqt"
CMAKE="$(command -v cmake || echo /usr/local/bin/cmake)"

# ── Configure ────────────────────────────────────────────────────────────────
echo "Configuring ($BUILD_TYPE)..."
"$CMAKE" -S "$REPO_ROOT" -B "$BUILD_DIR" \
    -G "Ninja" \
    "-DQt6_DIR=$QT6_CMAKE" \
    "-DCMAKE_BUILD_TYPE=$BUILD_TYPE"

# ── Build ────────────────────────────────────────────────────────────────────
echo "Building..."
"$CMAKE" --build "$BUILD_DIR" --config "$BUILD_TYPE" --parallel

APP_BUNDLE="$BUILD_DIR/src/${APP_NAME}.app"
if [[ ! -d "$APP_BUNDLE" ]]; then
    # Some CMake setups nest the .app differently
    APP_BUNDLE="$(find "$BUILD_DIR" -name "${APP_NAME}.app" -maxdepth 5 | head -1)"
fi
if [[ -z "$APP_BUNDLE" || ! -d "$APP_BUNDLE" ]]; then
    echo "ERROR: Could not locate ${APP_NAME}.app under $BUILD_DIR" >&2
    exit 1
fi
echo "App bundle: $APP_BUNDLE"

# ── Deploy Qt frameworks ──────────────────────────────────────────────────────
echo "Running macdeployqt..."
"$MACDEPLOYQT" "$APP_BUNDLE" -verbose=1

# ── Create DMG ───────────────────────────────────────────────────────────────
echo "Creating DMG..."
rm -rf "$STAGING_DIR"
mkdir -p "$STAGING_DIR"
cp -R "$APP_BUNDLE" "$STAGING_DIR/"

# Symlink /Applications so users can drag-and-drop
ln -s /Applications "$STAGING_DIR/Applications"

DMG_OUT="$REPO_ROOT/$DMG_NAME"
rm -f "$DMG_OUT"
hdiutil create \
    -volname "$APP_NAME $VERSION" \
    -srcfolder "$STAGING_DIR" \
    -ov \
    -format UDZO \
    "$DMG_OUT"

echo ""
echo "Done: $DMG_OUT"
