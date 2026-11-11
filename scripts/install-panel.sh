#!/bin/bash
set -euo pipefail

PROJECT_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
VERSION="${1:-${AE_VERSION:-23.6}}"
if [[ $# -gt 1 || ! "$VERSION" =~ ^[0-9]+\.[0-9]+$ ]]; then
    printf '%s\n' 'Usage: ./scripts/install-panel.sh [AE major.minor, default 23.6]' >&2
    exit 2
fi
if [[ "$(uname -s)" != Darwin ]]; then
    printf '%s\n' 'This installer currently targets macOS. Copy both JSX/JSXINC files to the AE ScriptUI Panels folder on other systems.' >&2
    exit 2
fi
# AE 23.6 itself identifies Scripts/ScriptUI Panels in the user preferences
# folder as an installation location. This creates files, not preference keys.
DESTINATION="${AE_SCRIPTUI_DIR:-$HOME/Library/Preferences/Adobe/After Effects/$VERSION/Scripts/ScriptUI Panels}"
FILES=("ParticleAtelierPresetLib.jsxinc" "Particle Atelier Presets.jsx")
for filename in "${FILES[@]}"; do
    [[ -f "$PROJECT_ROOT/scripts/$filename" ]] || { printf 'Missing panel source: %s\n' "$filename" >&2; exit 2; }
    if [[ -L "$DESTINATION/$filename" || ( -e "$DESTINATION/$filename" && ! -f "$DESTINATION/$filename" ) ]]; then
        printf 'Refusing to replace a symbolic link or non-file: %s\n' "$DESTINATION/$filename" >&2
        exit 2
    fi
done
mkdir -p "$DESTINATION"
if [[ ! -w "$DESTINATION" ]]; then
    printf 'The ScriptUI directory is not writable: %s\n' "$DESTINATION" >&2
    exit 2
fi
STAGING="$(mktemp -d "$DESTINATION/.particle-atelier-panel.XXXXXX")"
trap 'rmdir "$STAGING" 2>/dev/null || true' EXIT
BACKUP="$PROJECT_ROOT/.local/panel-backups/$(date +%Y%m%d-%H%M%S)-$$"
for filename in "${FILES[@]}"; do
    cp -X "$PROJECT_ROOT/scripts/$filename" "$STAGING/$filename"
    if [[ -f "$DESTINATION/$filename" ]]; then
        mkdir -p "$BACKUP"
        cp -p "$DESTINATION/$filename" "$BACKUP/$filename"
    fi
done
for filename in "${FILES[@]}"; do
    mv "$STAGING/$filename" "$DESTINATION/$filename"
done
printf 'Installed panel: %s\n' "$DESTINATION/Particle Atelier Presets.jsx"
if [[ -d "$BACKUP" ]]; then printf 'Previous files backed up: %s\n' "$BACKUP"; fi
printf '%s\n' 'A new AE session can discover this panel in the Window menu.' \
    'In the current session, File > Scripts > Run Script File opens the JSX as a floating palette.' \
    'No application was restarted; no preferences, CEP settings, or native bundles were changed.'
