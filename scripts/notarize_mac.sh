#!/usr/bin/env bash
# notarize_mac.sh - sign and notarize the macOS build.
#
# READ THIS FIRST IF YOU CAME FROM THE SECTIGO TOKEN:
#
# The Sectigo code-signing certificate does not work on macOS. It signs Windows
# PE binaries through Authenticode; Gatekeeper accepts exactly one thing, an
# "Developer ID Application" certificate issued by Apple. There is no way to
# substitute one for the other. To ship a Mac build you need:
#
#   1. Apple Developer Program membership (99 USD/year).
#   2. A "Developer ID Application" certificate in your login keychain.
#      Xcode > Settings > Accounts > Manage Certificates, or developer.apple.com.
#   3. An app-specific password for notarization, stored in the keychain once:
#        xcrun notarytool store-credentials alignthree-notary \
#            --apple-id you@example.com --team-id ABCDE12345 \
#            --password <app-specific-password>
#      Create the app-specific password at appleid.apple.com, not your Apple ID
#      password. Stored this way it never appears in a script or in shell history.
#
# Signing is separate from packaging for the same reason as on Windows: it needs
# credentials and, for notarization, a round trip to Apple's servers. Build once,
# sign the bytes you built.
#
#   ./scripts/notarize_mac.sh path/to/AlignThree.app path/to/AlignThree-x.y.z.dmg
#   ./scripts/notarize_mac.sh --identity "Developer ID Application: Name (TEAMID)" ...

set -euo pipefail

IDENTITY=""
PROFILE="alignthree-notary"
ENTITLEMENTS=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --identity)     IDENTITY="$2"; shift 2 ;;
        --profile)      PROFILE="$2"; shift 2 ;;
        --entitlements) ENTITLEMENTS="$2"; shift 2 ;;
        -h|--help)      sed -n '2,28p' "$0"; exit 0 ;;
        *) break ;;
    esac
done

APP="${1:-}"
DMG="${2:-}"
[[ -d "$APP" ]] || { echo "Usage: $0 <AlignThree.app> [AlignThree.dmg]" >&2; exit 2; }

# ---- identity ----------------------------------------------------------------
if [[ -z "$IDENTITY" ]]; then
    IDENTITY="$(security find-identity -v -p codesigning 2>/dev/null \
                | grep 'Developer ID Application' | head -1 \
                | sed -E 's/.*"(.*)"/\1/')"
fi
if [[ -z "$IDENTITY" ]]; then
    echo "ERROR: no 'Developer ID Application' certificate in the keychain." >&2
    echo "       security find-identity -v -p codesigning   # to see what is there" >&2
    echo "       A Sectigo certificate will not do -- macOS requires Apple's." >&2
    exit 1
fi
echo "Identity: $IDENTITY"

# ---- sign --------------------------------------------------------------------
# Inside out: nested code must be signed before the bundle that contains it, or
# the outer signature seals a nested one that is not yet valid.
#
# --options runtime turns on the hardened runtime, which notarization requires.
# --timestamp records a trusted timestamp so signatures outlive the certificate.
SIGN_ARGS=(--force --options runtime --timestamp --sign "$IDENTITY")
[[ -n "$ENTITLEMENTS" ]] && SIGN_ARGS+=(--entitlements "$ENTITLEMENTS")

echo "Signing frameworks and helpers..."
find "$APP/Contents/Frameworks" -name '*.framework' -maxdepth 1 -type d 2>/dev/null \
    | while read -r fw; do codesign "${SIGN_ARGS[@]}" "$fw"; done
find "$APP/Contents/Frameworks" -name '*.dylib' 2>/dev/null \
    | while read -r lib; do codesign "${SIGN_ARGS[@]}" "$lib"; done
find "$APP/Contents/PlugIns" -name '*.dylib' 2>/dev/null \
    | while read -r plugin; do codesign "${SIGN_ARGS[@]}" "$plugin"; done

# The two helper executables are signed individually: they are separate
# processes AlignThree launches, and an unsigned child breaks the parent's
# hardened runtime.
for helper in SpecTableConverter AlignThreeAskPass; do
    if [[ -f "$APP/Contents/MacOS/$helper" ]]; then
        echo "Signing $helper..."
        codesign "${SIGN_ARGS[@]}" "$APP/Contents/MacOS/$helper"
    else
        echo "WARNING: $helper is not in the bundle - AlignThree will not find it." >&2
    fi
done

echo "Signing the bundle..."
codesign "${SIGN_ARGS[@]}" "$APP"

echo "Verifying..."
codesign --verify --deep --strict --verbose=2 "$APP"

# ---- notarize ----------------------------------------------------------------
# Apple must see the software before Gatekeeper will let it run on a machine
# that did not build it. Submit the .dmg if there is one, otherwise a zip of the
# .app -- notarytool does not take a bare bundle.
SUBMIT="$DMG"
if [[ -z "$SUBMIT" ]]; then
    SUBMIT="${APP%.app}.zip"
    echo "Zipping for submission..."
    ditto -c -k --keepParent "$APP" "$SUBMIT"
fi

echo "Submitting to Apple (this can take a few minutes)..."
if ! xcrun notarytool submit "$SUBMIT" --keychain-profile "$PROFILE" --wait; then
    echo "" >&2
    echo "Notarization failed. For the reason:" >&2
    echo "  xcrun notarytool history --keychain-profile $PROFILE" >&2
    echo "  xcrun notarytool log <submission-id> --keychain-profile $PROFILE" >&2
    echo "" >&2
    echo "If it says the profile is unknown, store credentials first -- see the" >&2
    echo "header of this script." >&2
    exit 1
fi

# Stapling attaches the notarization ticket to the file, so Gatekeeper can
# check it without the user being online.
echo "Stapling..."
xcrun stapler staple "$SUBMIT"
[[ "$SUBMIT" == *.dmg ]] && xcrun stapler staple "$APP" || true

echo ""
echo "Signed, notarized and stapled: $SUBMIT"
echo "Confirm the way Gatekeeper will see it:"
echo "  spctl --assess --type execute --verbose \"$APP\""
