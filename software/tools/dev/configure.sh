#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

if [[ $# -eq 0 ]]; then
    (
        cd "$ROOT_DIR"
        cmake --preset dev
    )
else
    BUILD_DIR="$1"
    cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -G Ninja
fi
