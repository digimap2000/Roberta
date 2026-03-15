#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

if [[ $# -eq 0 ]]; then
    (
        cd "$ROOT_DIR"
        ctest --preset dev
    )
else
    BUILD_DIR="$1"
    ctest --test-dir "$BUILD_DIR" --output-on-failure
fi
