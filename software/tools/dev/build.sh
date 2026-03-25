#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

if [[ $# -eq 0 ]]; then
    (
        cd "$ROOT_DIR"
        cmake --build --preset dev
    )
else
    BUILD_DIR="$1"
    cmake --build "$BUILD_DIR"
fi
