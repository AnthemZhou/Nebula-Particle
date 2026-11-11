#!/bin/bash
set -euo pipefail

PROJECT_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
PLUGIN_NAME="Particle Atelier.plugin"
SOURCE="$PROJECT_ROOT/build/$PLUGIN_NAME"
DEST_ROOT="$HOME/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore"
DESTINATION="$DEST_ROOT/$PLUGIN_NAME"
EXPECTED_ID="com.particleatelier.particles.dev"
PRESET_ROOT="$HOME/Library/Application Support/Particle Atelier"
PRESET_FILE="$PRESET_ROOT/presets.ini"

if [[ ! -d "$SOURCE" ]]; then
    printf '%s\n' 'Build the native plugin first: ./scripts/build.sh' >&2
    exit 2
fi
if [[ "$(/usr/libexec/PlistBuddy -c 'Print :CFBundleIdentifier' "$SOURCE/Contents/Info.plist")" != "$EXPECTED_ID" ]]; then
    printf '%s\n' 'Refusing to install a bundle with an unexpected identifier.' >&2
    exit 2
fi
codesign --verify --strict "$SOURCE"

AE_RUNNING=0
if pgrep -f '/[^/]*After Effects[^/]*\.app/Contents/MacOS/After Effects([[:space:]]|$)' >/dev/null; then
    AE_RUNNING=1
fi
if [[ -e "$DESTINATION" && "$AE_RUNNING" == 1 ]]; then
    printf '%s\n' 'Close After Effects before installing the native plugin update. Your project has not been closed or changed.' >&2
    exit 3
fi

if [[ -e "$DESTINATION" ]]; then
    if [[ "$(/usr/libexec/PlistBuddy -c 'Print :CFBundleIdentifier' "$DESTINATION/Contents/Info.plist" 2>/dev/null || true)" != "$EXPECTED_ID" ]]; then
        printf '%s\n' 'A different bundle already uses this name; refusing to replace it.' >&2
        exit 2
    fi
fi

mkdir -p "$DEST_ROOT"
STAGING="$(mktemp -d "$DEST_ROOT/(ParticleAtelier-Staging).XXXXXX")"
trap 'rm -rf -- "$STAGING"' EXIT
ditto "$SOURCE" "$STAGING/$PLUGIN_NAME"
codesign --verify --strict "$STAGING/$PLUGIN_NAME"

if [[ -e "$DESTINATION" ]]; then
    BACKUP="$PROJECT_ROOT/.local/plugin-backups/$(date +%Y%m%d-%H%M%S)-$$"
    mkdir -p "$BACKUP"
    mv "$DESTINATION" "$BACKUP/$PLUGIN_NAME"
    printf 'Previous version backed up: %s\n' "$BACKUP/$PLUGIN_NAME"
fi
mv "$STAGING/$PLUGIN_NAME" "$DESTINATION"
mkdir -p "$PRESET_ROOT"
if [[ ! -e "$PRESET_FILE" ]]; then
    cp -n "$DESTINATION/Contents/Resources/presets.ini" "$PRESET_FILE"
    printf 'Initialized editable presets: %s\n' "$PRESET_FILE"
else
    printf 'Kept existing editable presets: %s\n' "$PRESET_FILE"
fi
printf 'Installed: %s\n' "$DESTINATION"
if [[ "$AE_RUNNING" == 1 ]]; then
    printf '%s\n' 'First installation completed while After Effects was running. The current session was not changed; a new session is required to discover this plugin.'
else
    printf '%s\n' 'Open After Effects and search Effects & Presets for Particle Atelier.'
fi
