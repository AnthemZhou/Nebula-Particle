#!/bin/bash
set -euo pipefail

PROJECT_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
VERSION="${1:-0.5.0}"
[[ "$VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || { printf '%s\n' 'Expected a release version such as 0.5.0.' >&2; exit 2; }
PLUGIN_NAME="Nebula Particle.plugin"
SOURCE="$PROJECT_ROOT/build/$PLUGIN_NAME"
PACKAGE_NAME="Nebula-Particle-$VERSION-macos-arm64"
DIST="$PROJECT_ROOT/dist"
STAGING_ROOT="$HOME/Library/Application Support/Nebula Particle/Release Staging"
[[ -d "$SOURCE" ]] || { printf '%s\n' 'Build the native plugin before packaging.' >&2; exit 2; }
[[ "$(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' "$SOURCE/Contents/Info.plist")" == "$VERSION" ]] || {
    printf '%s\n' 'The compiled bundle version does not match the requested release.' >&2; exit 2;
}
codesign --verify --strict "$SOURCE"
if [[ -n "$(git -C "$PROJECT_ROOT" status --porcelain --untracked-files=no)" ]]; then
    printf '%s\n' 'Commit tracked source changes before creating the release archive.' >&2
    exit 2
fi
COMMIT="$(git -C "$PROJECT_ROOT" rev-parse HEAD)"
mkdir -p "$DIST" "$STAGING_ROOT"
STAGING="$(mktemp -d "$STAGING_ROOT/$VERSION.XXXXXX")"
PACKAGE="$STAGING/$PACKAGE_NAME"
mkdir -p "$PACKAGE/build" "$PACKAGE/scripts"
# Keep real bundles outside sync-managed folders while checking signatures.
ditto --noextattr "$SOURCE" "$PACKAGE/build/$PLUGIN_NAME"
for name in install-dev.sh install-panel.sh 'Nebula Particle Presets.jsx' NebulaParticlePresetLib.jsxinc; do
    cp -X "$PROJECT_ROOT/scripts/$name" "$PACKAGE/scripts/$name"
done
cp -X "$PROJECT_ROOT/install-macos.command" "$PACKAGE/install-macos.command"
cp -X "$PROJECT_ROOT/LICENSE" "$PACKAGE/LICENSE"
for name in README.md README_EN.md CHANGELOG.md CONTRIBUTING.md; do
    cp -X "$PROJECT_ROOT/$name" "$PACKAGE/$name"
done
ditto --noextattr "$PROJECT_ROOT/docs" "$PACKAGE/docs"
chmod +x "$PACKAGE/install-macos.command" "$PACKAGE/scripts/install-dev.sh" "$PACKAGE/scripts/install-panel.sh"
printf 'Nebula Particle %s\n\n' "$VERSION" > "$PACKAGE/INSTALL.txt"
cat >> "$PACKAGE/INSTALL.txt" <<'INSTRUCTIONS'
macOS Apple Silicon (arm64). Tested with After Effects 23.6.

1. Save your project and quit After Effects.
2. Keep the extracted folder intact and run install-macos.command.
   Terminal alternative: bash /path/to/this-folder/install-macos.command
3. Start AE and search Effects & Presets for Nebula Particle.
4. Open Window > Nebula Particle Presets.jsx.

No Adobe SDK or compiler is needed to install this package. The installer uses
your user-level MediaCore and AE 23.6 ScriptUI directories, without sudo.
For another AE preferences version, set AE_VERSION when running the installer;
other AE versions have not been validated by this release.

The old Particle Atelier plugin/panel is backed up during upgrade. Saved AEP
identities remain compatible. Legacy user presets remain readable; new presets
are saved in the Nebula Particle user-data folder.

This package is ad-hoc signed, not Apple-notarized. Source is available for
building locally. Rendering is currently CPU 2D with 8/16 bpc; GPU, optical flow,
32-bpc HDR, depth, clones and a dedicated baking workflow are not implemented.

License: GPL-3.0-only. See LICENSE and SOURCE.txt.
INSTRUCTIONS
printf 'Nebula Particle %s\nSource commit: %s\nRepository: https://github.com/AnthemZhou/Nebula-Particle\nCorresponding source: https://github.com/AnthemZhou/Nebula-Particle/archive/refs/tags/v%s.zip\nLicense: GPL-3.0-only (LICENSE included). Adobe SDK is not included.\n' \
    "$VERSION" "$COMMIT" "$VERSION" > "$PACKAGE/SOURCE.txt"
codesign --verify --strict "$PACKAGE/build/$PLUGIN_NAME"
ARCHIVE="$DIST/$PACKAGE_NAME.zip"
if [[ -f "$ARCHIVE" ]]; then
    mkdir -p "$DIST/previous"
    mv "$ARCHIVE" "$DIST/previous/$PACKAGE_NAME.$(date +%Y%m%d-%H%M%S).zip"
fi
ditto -c -k --norsrc --noextattr --keepParent "$PACKAGE" "$ARCHIVE"
# Extract and verify the actual archive, not just the staging directory.
mkdir "$STAGING/verify"
ditto -x -k "$ARCHIVE" "$STAGING/verify"
codesign --verify --strict "$STAGING/verify/$PACKAGE_NAME/build/$PLUGIN_NAME"
(cd "$DIST" && shasum -a 256 "$PACKAGE_NAME.zip") > "$DIST/$PACKAGE_NAME.sha256"
printf 'Release archive: %s\nSHA-256 file: %s\nSource commit: %s\n' "$ARCHIVE" "$DIST/$PACKAGE_NAME.sha256" "$COMMIT"
