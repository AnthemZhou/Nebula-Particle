#!/bin/bash
set -euo pipefail

PROJECT_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
SOURCE="${1:-$PROJECT_ROOT/resources/presets.ini}"
PRESET_ROOT="$HOME/Library/Application Support/Nebula Particle"
DESTINATION="$PRESET_ROOT/presets.ini"

if [[ ! -f "$SOURCE" ]]; then
    printf 'Preset source not found: %s\n' "$SOURCE" >&2
    exit 2
fi
if [[ -e "$DESTINATION" && "$SOURCE" -ef "$DESTINATION" ]]; then
    printf '%s\n' 'The source already is the active preset file; no copy needed.'
    exit 0
fi

mkdir -p "$PRESET_ROOT"
STAGING="$(mktemp "$PRESET_ROOT/.presets.XXXXXX")"
trap 'rm -f -- "$STAGING"' EXIT
cp "$SOURCE" "$STAGING"
if [[ -e "$DESTINATION" ]]; then
    BACKUP="$PROJECT_ROOT/.local/preset-backups/$(date +%Y%m%d-%H%M%S)-$$"
    mkdir -p "$BACKUP"
    cp "$DESTINATION" "$BACKUP/presets.ini"
    printf 'Previous presets backed up: %s\n' "$BACKUP/presets.ini"
fi
mv "$STAGING" "$DESTINATION"
printf 'Updated editable presets: %s\n' "$DESTINATION"
printf '%s\n' 'Use the plugin Reload current preset control to invalidate cached frames and read these values. No native bundle was changed.'
