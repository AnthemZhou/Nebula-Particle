#!/bin/bash
set -euo pipefail

PROJECT_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
SDK_ROOT="${AE_SDK_ROOT:-$PROJECT_ROOT/.local/ae-sdk}"
BUILD_ROOT="$PROJECT_ROOT/build"
ARTIFACT_ROOT="$HOME/Library/Application Support/Nebula Particle/Builds"
PLUGIN_NAME="Nebula Particle.plugin"

if [[ "$(uname -s)" != "Darwin" ]]; then
    printf '%s\n' 'This build script currently targets macOS arm64 only.' >&2
    exit 2
fi

if [[ -f "$SDK_ROOT/Examples/Headers/AE_Effect.h" ]]; then
    SDK_EXAMPLES="$SDK_ROOT/Examples"
elif [[ -f "$SDK_ROOT/Headers/AE_Effect.h" ]]; then
    SDK_EXAMPLES="$SDK_ROOT"
else
    printf '%s\n' 'Adobe After Effects C++ SDK was not found.' \
        'Set AE_SDK_ROOT to the official extracted SDK root; see docs/SDK准备.md.' >&2
    exit 2
fi

for required in Headers/AE_Effect.h Headers/AE_EffectVers.h Headers/AE_EffectUI.h Headers/AE_Macros.h \
    Util/entry.h Util/Param_Utils.h Resources/AE_General.r; do
    if [[ ! -f "$SDK_EXAMPLES/$required" ]]; then
        printf 'Missing SDK file: %s\n' "$SDK_EXAMPLES/$required" >&2
        exit 2
    fi
done
for source in src/ae_plugin.cpp src/particle_core.cpp resources/NebulaParticlePiPL.r resources/Info.plist resources/presets.ini; do
    if [[ ! -f "$PROJECT_ROOT/$source" ]]; then
        printf 'Missing project source: %s\n' "$source" >&2
        exit 2
    fi
done

MACOS_SDK="$(xcrun --sdk macosx --show-sdk-path)"
mkdir -p "$BUILD_ROOT" "$ARTIFACT_ROOT"
# Documents may be managed by File Provider, which asynchronously reattaches
# FinderInfo to .plugin directories and invalidates an otherwise valid signature.
# Build and retain the real bundle outside that tree; publish a workspace symlink.
STAGING="$(mktemp -d "$ARTIFACT_ROOT/$(date +%Y%m%d-%H%M%S).XXXXXX")"
PUBLISH_LINK=""
PUBLISHED=0
cleanup() {
    if [[ -n "$PUBLISH_LINK" && -L "$PUBLISH_LINK" ]]; then
        rm -f -- "$PUBLISH_LINK"
    fi
    if [[ "$PUBLISHED" != 1 ]]; then
        rm -rf -- "$STAGING"
    fi
}
trap cleanup EXIT
BUNDLE="$STAGING/$PLUGIN_NAME"
mkdir -p "$BUNDLE/Contents/MacOS" "$BUNDLE/Contents/Resources"

printf '%s\n' 'Compiling Nebula Particle for macOS arm64…'
xcrun --sdk macosx clang++ \
    -std=c++17 -stdlib=libc++ -arch arm64 -mmacosx-version-min=12.0 \
    -isysroot "$MACOS_SDK" -O2 -DNDEBUG -DPF_DEEP_COLOR_AWARE=1 \
    -fvisibility=hidden -fvisibility-inlines-hidden \
    -isystem "$SDK_EXAMPLES/Headers" -isystem "$SDK_EXAMPLES/Headers/SP" \
    -isystem "$SDK_EXAMPLES/Util" -I "$PROJECT_ROOT/src" \
    -bundle -framework Cocoa \
    -Wl,-exported_symbol,_EffectMain -Wl,-exported_symbol,_PluginDataEntryFunction2 \
    "$PROJECT_ROOT/src/ae_plugin.cpp" "$PROJECT_ROOT/src/particle_core.cpp" \
    -o "$BUNDLE/Contents/MacOS/NebulaParticle"

printf '%s\n' 'Compiling PiPL metadata…'
xcrun Rez -useDF -arch arm64 -d __MACH__ -d __arm64__ \
    -i "$SDK_EXAMPLES/Headers" -i "$SDK_EXAMPLES/Resources" -i "$PROJECT_ROOT/src" \
    "$PROJECT_ROOT/resources/NebulaParticlePiPL.r" \
    -o "$BUNDLE/Contents/Resources/NebulaParticle.rsrc"

cp "$PROJECT_ROOT/resources/Info.plist" "$BUNDLE/Contents/Info.plist"
cp "$PROJECT_ROOT/resources/presets.ini" "$BUNDLE/Contents/Resources/presets.ini"
printf 'eFKTFXTC' > "$BUNDLE/Contents/PkgInfo"
plutil -lint "$BUNDLE/Contents/Info.plist"
# Rez may attach Finder metadata, which code signing rejects. The resource
# lives in the data fork (-useDF); strip metadata only from our new artifact.
xattr -cr "$BUNDLE"
codesign --force --sign - --timestamp=none "$BUNDLE"
codesign --verify --strict --verbose=2 "$BUNDLE"
xcrun lipo "$BUNDLE/Contents/MacOS/NebulaParticle" -verify_arch arm64
ditto -c -k --norsrc --noextattr --keepParent "$BUNDLE" "$STAGING/Nebula Particle.zip"

# Publish only after every build and signature check passes.
PUBLISH_LINK="$BUILD_ROOT/.nebula-particle-link.$$"
ln -s "$BUNDLE" "$PUBLISH_LINK"
codesign --verify --strict "$PUBLISH_LINK"
PREVIOUS=""
if [[ -e "$BUILD_ROOT/$PLUGIN_NAME" || -L "$BUILD_ROOT/$PLUGIN_NAME" ]]; then
    PREVIOUS="$BUILD_ROOT/previous/$(date +%Y%m%d-%H%M%S)-$$"
    mkdir -p "$PREVIOUS"
    mv "$BUILD_ROOT/$PLUGIN_NAME" "$PREVIOUS/$PLUGIN_NAME"
fi
if ! mv "$PUBLISH_LINK" "$BUILD_ROOT/$PLUGIN_NAME"; then
    if [[ -n "$PREVIOUS" ]]; then
        mv "$PREVIOUS/$PLUGIN_NAME" "$BUILD_ROOT/$PLUGIN_NAME"
    fi
    exit 1
fi
PUBLISH_LINK=""
if ! codesign --verify --strict --verbose=2 "$BUILD_ROOT/$PLUGIN_NAME"; then
    rm -f -- "$BUILD_ROOT/$PLUGIN_NAME"
    if [[ -n "$PREVIOUS" ]]; then
        mv "$PREVIOUS/$PLUGIN_NAME" "$BUILD_ROOT/$PLUGIN_NAME"
    fi
    exit 1
fi
PUBLISHED=1
cp -X "$STAGING/Nebula Particle.zip" "$BUILD_ROOT/Nebula Particle.zip"
printf 'Built: %s\n' "$BUILD_ROOT/$PLUGIN_NAME"
printf 'Signed bundle: %s\n' "$BUNDLE"
printf 'Portable archive: %s\n' "$BUILD_ROOT/Nebula Particle.zip"
printf '%s\n' 'This verifies packaging; loading and rendering must still be tested inside After Effects.'
