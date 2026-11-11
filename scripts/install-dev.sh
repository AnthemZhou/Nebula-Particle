#!/bin/bash
set -euo pipefail

PROJECT_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
PLUGIN_NAME="Nebula Particle.plugin"
LEGACY_PLUGIN_NAME="Particle Atelier.plugin"
SOURCE="$PROJECT_ROOT/build/$PLUGIN_NAME"
DEST_ROOT="$HOME/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore"
DESTINATION="$DEST_ROOT/$PLUGIN_NAME"
# Kept across the rename so the installer can identify genuine older bundles.
EXPECTED_ID="com.particleatelier.particles.dev"
PRESET_ROOT="$HOME/Library/Application Support/Nebula Particle"
LEGACY_PRESET_ROOT="$HOME/Library/Application Support/Particle Atelier"
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

EXISTING_COUNT=0
for name in "$PLUGIN_NAME" "$LEGACY_PLUGIN_NAME"; do
    candidate="$DEST_ROOT/$name"
    if [[ -e "$candidate" || -L "$candidate" ]]; then
        if [[ "$(/usr/libexec/PlistBuddy -c 'Print :CFBundleIdentifier' "$candidate/Contents/Info.plist" 2>/dev/null || true)" != "$EXPECTED_ID" ]]; then
            printf 'A different or unreadable bundle already uses this name; refusing to replace it: %s\n' "$candidate" >&2
            exit 2
        fi
        EXISTING_COUNT=$((EXISTING_COUNT + 1))
    fi
done

AE_RUNNING=0
if pgrep -f '/[^/]*After Effects[^/]*\.app/Contents/MacOS/After Effects([[:space:]]|$)' >/dev/null; then
    AE_RUNNING=1
fi
if [[ "$EXISTING_COUNT" -gt 0 && "$AE_RUNNING" == 1 ]]; then
    printf '%s\n' 'Close After Effects before installing the native plugin update. Your project has not been closed or changed.' >&2
    exit 3
fi

# This installer owns the per-user location only. Do not create a second effect
# if a previous copy was installed for all users by a different installer.
SYSTEM_ROOT="/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore"
for name in "$PLUGIN_NAME" "$LEGACY_PLUGIN_NAME"; do
    candidate="$SYSTEM_ROOT/$name"
    if [[ -e "$candidate" || -L "$candidate" ]]; then
        if [[ "$(/usr/libexec/PlistBuddy -c 'Print :CFBundleIdentifier' "$candidate/Contents/Info.plist" 2>/dev/null || true)" == "$EXPECTED_ID" ]]; then
            printf 'An existing system-wide copy would duplicate this effect. Move it out of MediaCore before installing the per-user copy: %s\n' "$candidate" >&2
            exit 2
        fi
    fi
done

mkdir -p "$DEST_ROOT"
STAGING="$(mktemp -d "$DEST_ROOT/(NebulaParticle-Staging).XXXXXX")"
BACKUP=""
INSTALLED=0
cleanup() {
    local result=$?
    if [[ "$INSTALLED" != 1 && -n "$BACKUP" ]]; then
        for name in "$PLUGIN_NAME" "$LEGACY_PLUGIN_NAME"; do
            if [[ -e "$BACKUP/$name" || -L "$BACKUP/$name" ]]; then
                if [[ ! -e "$DEST_ROOT/$name" && ! -L "$DEST_ROOT/$name" ]]; then
                    if ! mv "$BACKUP/$name" "$DEST_ROOT/$name"; then
                        printf 'Could not restore the previous bundle; it remains at: %s\n' "$BACKUP/$name" >&2
                    fi
                else
                    printf 'Previous bundle retained for recovery: %s\n' "$BACKUP/$name" >&2
                fi
            fi
        done
    fi
    rm -rf -- "$STAGING"
    return "$result"
}
trap cleanup EXIT
ditto "$SOURCE" "$STAGING/$PLUGIN_NAME"
codesign --verify --strict "$STAGING/$PLUGIN_NAME"

if [[ "$EXISTING_COUNT" -gt 0 ]]; then
    BACKUP="$PROJECT_ROOT/.local/plugin-backups/$(date +%Y%m%d-%H%M%S)-$$"
    mkdir -p "$BACKUP"
    for name in "$PLUGIN_NAME" "$LEGACY_PLUGIN_NAME"; do
        if [[ -e "$DEST_ROOT/$name" || -L "$DEST_ROOT/$name" ]]; then
            mv "$DEST_ROOT/$name" "$BACKUP/$name"
            printf 'Previous version backed up outside MediaCore: %s\n' "$BACKUP/$name"
        fi
    done
fi
mv "$STAGING/$PLUGIN_NAME" "$DESTINATION"
INSTALLED=1
mkdir -p "$PRESET_ROOT"
if [[ ! -e "$PRESET_FILE" && ! -L "$PRESET_FILE" ]]; then
    if [[ -f "$LEGACY_PRESET_ROOT/presets.ini" ]]; then
        cp -n "$LEGACY_PRESET_ROOT/presets.ini" "$PRESET_FILE"
        printf 'Copied legacy editable presets; original preserved: %s\n' "$PRESET_FILE"
    else
        cp -n "$DESTINATION/Contents/Resources/presets.ini" "$PRESET_FILE"
        printf 'Initialized editable presets: %s\n' "$PRESET_FILE"
    fi
else
    printf 'Kept existing editable presets: %s\n' "$PRESET_FILE"
fi
printf 'Installed: %s\n' "$DESTINATION"
if [[ "$AE_RUNNING" == 1 ]]; then
    printf '%s\n' 'First installation completed while After Effects was running. The current session was not changed; a new session is required to discover this plugin.'
else
    printf '%s\n' 'Open After Effects and search Effects & Presets for Nebula Particle.'
fi
