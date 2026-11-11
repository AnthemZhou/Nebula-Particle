#!/bin/bash
set -euo pipefail
PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$PROJECT_ROOT/build/tests"
xcrun clang++ -std=c++17 -O2 -Wall -Wextra -Wpedantic \
  "$PROJECT_ROOT/src/particle_core.cpp" "$PROJECT_ROOT/tests/particle_core_test.cpp" \
  -I "$PROJECT_ROOT/src" -o "$PROJECT_ROOT/build/tests/particle_core_test"
"$PROJECT_ROOT/build/tests/particle_core_test"
if [[ "${1:-}" == "--sanitize" ]]; then
  xcrun clang++ -std=c++17 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
    "$PROJECT_ROOT/src/particle_core.cpp" "$PROJECT_ROOT/tests/particle_core_test.cpp" \
    -I "$PROJECT_ROOT/src" -o "$PROJECT_ROOT/build/tests/particle_core_sanitize"
  "$PROJECT_ROOT/build/tests/particle_core_sanitize"
fi
