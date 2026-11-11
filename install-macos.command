#!/bin/bash
set -euo pipefail
PACKAGE_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

printf '%s\n' 'Nebula Particle installer' 'Save your project and quit After Effects before updating.'
bash "$PACKAGE_ROOT/scripts/install-dev.sh"
bash "$PACKAGE_ROOT/scripts/install-panel.sh" "${AE_VERSION:-23.6}"
printf '%s\n' 'Installation complete. Start After Effects.' \
    'Effect: Nebula Particle' 'Window menu: Nebula Particle Presets.jsx'
